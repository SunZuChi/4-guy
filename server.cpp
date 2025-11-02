// server.cpp (fixed version with WHO)
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <algorithm>

#include <iostream>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <chrono>
#include <atomic>
#include <signal.h>

using namespace std::chrono_literals;

const char* CONTROL_QUEUE = "/control_queue";
const size_t MAX_MSG_SIZE = 2048;
const long MAX_MESSAGES = 10;
const int BROADCAST_WORKERS = 4;
const std::chrono::seconds HEARTBEAT_TIMEOUT = 30s;

std::atomic<bool> running{true};

struct BroadcastTask {
    std::vector<std::string> recipients;
    std::string message;
};

std::unordered_map<std::string, mqd_t> client_mq;
std::unordered_map<std::string, std::unordered_set<std::string>> room_members;
std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_seen;

std::shared_mutex clients_mutex;
std::shared_mutex rooms_mutex;
std::shared_mutex seen_mutex;

std::queue<BroadcastTask> tasks;
std::mutex tasks_mtx;
std::condition_variable tasks_cv;

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [](unsigned char ch){ return !std::isspace(ch); }));
}
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
}
static inline void trim(std::string &s) {
    ltrim(s); rtrim(s);
}

bool send_to_mqd(mqd_t mqd, const std::string &msg) {
    if (mq_send(mqd, msg.c_str(), std::min(msg.size(), MAX_MSG_SIZE), 0) == -1) {
        std::cerr << "mq_send error: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

void broadcaster_worker() {
    while (running) {
        BroadcastTask t;
        {
            std::unique_lock<std::mutex> lk(tasks_mtx);
            tasks_cv.wait(lk, [](){ return !tasks.empty() || !running.load(); });
            if (!running && tasks.empty()) break;
            t = std::move(tasks.front());
            tasks.pop();
        }
        for (auto &clientid : t.recipients) {
            mqd_t target = (mqd_t)-1;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) target = it->second;
            }
            if (target != (mqd_t)-1)
                send_to_mqd(target, t.message);
        }
    }
}

void queue_broadcast_to_room(const std::string &room, const std::string &message) {
    std::vector<std::string> recip;
    {
        std::shared_lock<std::shared_mutex> lock(rooms_mutex);
        auto it = room_members.find(room);
        if (it != room_members.end())
            recip.insert(recip.end(), it->second.begin(), it->second.end());
    }
    if (!recip.empty()) {
        BroadcastTask t{std::move(recip), message};
        {
            std::lock_guard<std::mutex> lk(tasks_mtx);
            tasks.push(std::move(t));
        }
        tasks_cv.notify_one();
    }
}

void queue_broadcast_to_clients(const std::vector<std::string> &clients,
                                const std::string &message) {
    BroadcastTask t{clients, message};
    {
        std::lock_guard<std::mutex> lk(tasks_mtx);
        tasks.push(std::move(t));
    }
    tasks_cv.notify_one();
}

void remove_client_from_all_rooms(const std::string &clientid) {
    std::unique_lock<std::shared_mutex> lock(rooms_mutex);
    for (auto &p : room_members) p.second.erase(clientid);
}

void router_thread_func(mqd_t control_mqd) {
    char buf[MAX_MSG_SIZE+1];

    while (running) {
        ssize_t n = mq_receive(control_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            std::this_thread::sleep_for(100ms);
            continue;
        }
        buf[n] = '\0';
        std::string line(buf);
        trim(line);
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        // ---------------- REGISTER ----------------
        if (cmd == "REGISTER") {
            std::string clientid;
            iss >> clientid;
            if (clientid.empty()) continue;

            std::string replyq = "/reply_" + clientid;
            mqd_t mq = mq_open(replyq.c_str(), O_WRONLY);

            if (mq != (mqd_t)-1) {
                {
                    std::unique_lock<std::shared_mutex> lock(clients_mutex);
                    client_mq[clientid] = mq;
                }
                {
                    std::unique_lock<std::shared_mutex> s(seen_mutex);
                    last_seen[clientid] = std::chrono::steady_clock::now();
                }

                std::cerr << "[SERVER] REGISTER: " << clientid << "\n";
            }
        }

        // ---------------- JOIN ----------------
        else if (cmd == "JOIN") {
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;

            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                room_members[room].insert(clientid);
            }

            std::string notice = "SYSTEM " + clientid + " joined " + room;
            queue_broadcast_to_room(room, notice);

            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end())
                    send_to_mqd(it->second, "SYSTEM: You joined " + room);
            }

            std::cerr << "[SERVER] " << clientid << " joined " << room << "\n";
        }

        // ---------------- LEAVE ----------------
        else if (cmd == "LEAVE") {
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;

            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end())
                    it->second.erase(clientid);
            }

            std::string notice = "SYSTEM " + clientid + " left " + room;
            queue_broadcast_to_room(room, notice);

            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end())
                    send_to_mqd(it->second, "SYSTEM: You left " + room);
            }

            std::cerr << "[SERVER] " << clientid << " left " << room << "\n";
        }

        // ---------------- SAY ----------------
        else if (cmd == "SAY") {
            std::string clientid, room;
            iss >> clientid >> room;
            std::string msg;
            std::getline(iss, msg);
            trim(msg);
            if (clientid.empty() || room.empty() || msg.empty()) continue;

            std::string notice = clientid + ": " + msg;
            queue_broadcast_to_room(room, "SAY " + room + " " + notice);

            std::cerr << "[SERVER] " << clientid << " said in " << room << ": " << msg << "\n";
        }

        // ---------------- DM ----------------
        else if (cmd == "DM") {
            std::string sender, target;
            iss >> sender >> target;
            std::string msg;
            std::getline(iss, msg);
            trim(msg);
            if (sender.empty() || target.empty() || msg.empty()) continue;

            std::string fullmsg = "DM from " + sender + ": " + msg;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(target);
                if (it != client_mq.end())
                    send_to_mqd(it->second, fullmsg);
            }

            std::cerr << "[SERVER] " << sender << " -> " << target << ": " << msg << "\n";
        }

        // ---------------- WHO ----------------
        else if (cmd == "WHO") {
            // Expected from client: "WHO <clientid> <room>"
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;

            std::vector<std::string> members;
            {
                std::shared_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) {
                    members.insert(members.end(), it->second.begin(), it->second.end());
                }
            }

            std::string reply;
            if (members.empty()) {
                reply = "SYSTEM: Room '" + room + "' has no members.";
            } else {
                // join members with ", "
                std::ostringstream oss;
                for (size_t i = 0; i < members.size(); ++i) {
                    if (i) oss << ", ";
                    oss << members[i];
                }
                reply = "SYSTEM: Members in " + room + ": " + oss.str();
            }

            // send reply only to requesting client
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) {
                    send_to_mqd(it->second, reply);
                }
            }

            std::cerr << "[SERVER] WHO " << clientid << " for room " << room << "\n";
        }

        // ---------------- QUIT ----------------
        else if (cmd == "QUIT") {
            std::string clientid;
            iss >> clientid;
            if (clientid.empty()) continue;

            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end())
                    send_to_mqd(it->second, "SYSTEM: Bye! disconnected");
            }

            {
                std::unique_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) {
                    mq_close(it->second);
                    client_mq.erase(it);
                }
            }

            remove_client_from_all_rooms(clientid);

            {
                std::unique_lock<std::shared_mutex> s(seen_mutex);
                last_seen.erase(clientid);
            }

            std::string notice = "SYSTEM " + clientid + " quit";

            std::vector<std::string> all;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                for (auto &p : client_mq) all.push_back(p.first);
            }
            queue_broadcast_to_clients(all, notice);

            std::cerr << "[SERVER] " << clientid << " quit\n";
        }

        // ---------------- HEARTBEAT ----------------
        else if (cmd == "HEARTBEAT") {
            std::string clientid;
            iss >> clientid;
            if (clientid.empty()) continue;

            {
                std::unique_lock<std::shared_mutex> s(seen_mutex);
                last_seen[clientid] = std::chrono::steady_clock::now();
            }
        }
    }
}

// heartbeat cleaner (optional)
void heartbeat_checker() {
    while (running) {
        std::this_thread::sleep_for(5s);
    }
}

mqd_t control_mqd = (mqd_t)-1;

void cleanup_and_exit(int) {
    running = false;
    tasks_cv.notify_all();
    mq_close(control_mqd);
    mq_unlink(CONTROL_QUEUE);

    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex);
        for (auto &p : client_mq) mq_close(p.second);
        client_mq.clear();
    }

    std::cerr << "Server shutting down\n";
    fflush(stderr);
    exit(0);
}

int main() {
    signal(SIGINT, cleanup_and_exit);

    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    control_mqd = mq_open(CONTROL_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);

    std::cerr << "Control queue created.\n";

    std::vector<std::thread> workers;
    for (int i=0;i<BROADCAST_WORKERS;i++)
        workers.emplace_back(broadcaster_worker);

    std::thread router(router_thread_func, control_mqd);
    std::thread heart(heartbeat_checker);

    router.join();
    heart.join();
    for (auto &t : workers) t.join();

    cleanup_and_exit(0);
    return 0;
}