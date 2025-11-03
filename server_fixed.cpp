// server_fixed.cpp
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <chrono>
#include <condition_variable>
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
const int BROADCAST_WORKERS = 10;
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

void broadcast_worker(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [] { return !message_queue.empty() || done; });

        if (done && message_queue.empty())
            break;

        std::string msg = message_queue.front();
        message_queue.pop();
        lock.unlock();

        // Simulate sending message (add artificial delay)
        std::this_thread::sleep_for(500ms); // you can change delay to test

        // Print timestamp and thread info
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now.time_since_epoch()).count();

        std::cout << "[" << ms << "] Thread " << id
                  << " broadcasting: " << msg << "\n";
    }
}
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
    if (mqd == (mqd_t)-1) return false;
    ssize_t to_send = std::min(msg.size(), MAX_MSG_SIZE);
    if (mq_send(mqd, msg.c_str(), static_cast<size_t>(to_send), 0) == -1) {
        std::cerr << "mq_send error: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

// Safe helper: get mqd_t copy for a client id (returns (mqd_t)-1 if not found)
mqd_t get_client_mqd_copy(const std::string &clientid) {
    mqd_t target = (mqd_t)-1;
    std::shared_lock<std::shared_mutex> lock(clients_mutex);
    auto it = client_mq.find(clientid);
    if (it != client_mq.end()) target = it->second;
    return target;
}

// Safe send to a client by id (copies mqd_t under lock, then sends)
bool safe_send_to_client(const std::string &clientid, const std::string &msg) {
    mqd_t target = get_client_mqd_copy(clientid);
    if (target == (mqd_t)-1) return false;
    return send_to_mqd(target, msg);
}

void broadcaster_worker() {
    while (running) {
        BroadcastTask t;
        {
            std::unique_lock<std::mutex> lk(tasks_mtx);
            tasks_cv.wait(lk, [](){ return !tasks.empty() || !running.load(); });
            if (!running && tasks.empty()) break;
            if (tasks.empty()) continue;
            t = std::move(tasks.front());
            tasks.pop();
        }
        for (auto &clientid : t.recipients) {
            mqd_t target = get_client_mqd_copy(clientid);
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

// Remove client safely: erase from client_mq, rooms and last_seen under locks,
// then notify/close the queue after locks are released.
// If `announce` is true, send SYSTEM quit notice to remaining clients.
void remove_client(const std::string &clientid, bool announce = true) {
    mqd_t captured_mqd = (mqd_t)-1;
    {
        // remove from client_mq and capture mqd
        std::unique_lock<std::shared_mutex> lock(clients_mutex);
        auto it = client_mq.find(clientid);
        if (it != client_mq.end()) {
            captured_mqd = it->second;
            client_mq.erase(it);
        }
    }

    {
        // remove from all rooms
        std::unique_lock<std::shared_mutex> lock(rooms_mutex);
        for (auto &p : room_members) p.second.erase(clientid);
    }

    {
        // remove from last_seen
        std::unique_lock<std::shared_mutex> s(seen_mutex);
        last_seen.erase(clientid);
    }

    // notify the client (if we still have its queue handle)
    if (captured_mqd != (mqd_t)-1) {
        send_to_mqd(captured_mqd, "SYSTEM: Bye! disconnected");
        mq_close(captured_mqd);
    }

    if (announce) {
        std::string notice = "SYSTEM " + clientid + " quit";
        std::vector<std::string> all;
        {
            std::shared_lock<std::shared_mutex> lock(clients_mutex);
            for (auto &p : client_mq) all.push_back(p.first);
        }
        if (!all.empty()) queue_broadcast_to_clients(all, notice);
    }

    std::cerr << "[SERVER] Removed client " << clientid << "\n";
}

void router_thread_func(mqd_t control_mqd) {
    char buf[MAX_MSG_SIZE+1];

    while (running) {
        ssize_t n = mq_receive(control_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            if (errno == EINTR || errno == EAGAIN) {
                std::this_thread::sleep_for(100ms);
                continue;
            } else {
                std::cerr << "mq_receive error: " << strerror(errno) << "\n";
                std::this_thread::sleep_for(100ms);
                continue;
            }
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
            if (mq == (mqd_t)-1) {
                std::cerr << "[SERVER] REGISTER: cannot open reply queue " << replyq
                          << " error: " << strerror(errno) << "\n";
                continue;
            }

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

            safe_send_to_client(clientid, "SYSTEM: You joined " + room);

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

            safe_send_to_client(clientid, "SYSTEM: You left " + room);

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
            safe_send_to_client(target, fullmsg);

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
                std::ostringstream oss;
                for (size_t i = 0; i < members.size(); ++i) {
                    if (i) oss << ", ";
                    oss << members[i];
                }
                reply = "SYSTEM: Members in " + room + ": " + oss.str();
            }

            safe_send_to_client(clientid, reply);

            std::cerr << "[SERVER] WHO " << clientid << " for room " << room << "\n";
        }

        // ---------------- QUIT ----------------
        else if (cmd == "QUIT") {
            std::string clientid;
            iss >> clientid;
            if (clientid.empty()) continue;

            // Use remove_client to safely erase and announce
            remove_client(clientid, true);

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

// heartbeat cleaner: remove clients which missed heartbeat
void heartbeat_checker() {
    while (running) {
        std::this_thread::sleep_for(5s);
        auto now = std::chrono::steady_clock::now();
        std::vector<std::string> expired;

        {
            std::shared_lock<std::shared_mutex> s(seen_mutex);
            for (auto &p : last_seen) {
                if (now - p.second > HEARTBEAT_TIMEOUT) {
                    expired.push_back(p.first);
                }
            }
        }

        for (auto &cid : expired) {
            std::cerr << "[SERVER] Client " << cid << " timed out (no heartbeat)\n";
            // remove_client will erase entry from last_seen too
            remove_client(cid, true);
        }
    }
}

mqd_t control_mqd = (mqd_t)-1;

void cleanup_and_exit(int) {
    running = false;
    tasks_cv.notify_all();

    if (control_mqd != (mqd_t)-1) {
        mq_close(control_mqd);
        mq_unlink(CONTROL_QUEUE);
        control_mqd = (mqd_t)-1;
    }

    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex);
        for (auto &p : client_mq) {
            if (p.second != (mqd_t)-1) mq_close(p.second);
        }
        client_mq.clear();
    }

    {
        std::unique_lock<std::shared_mutex> lock(rooms_mutex);
        room_members.clear();
    }

    {
        std::unique_lock<std::shared_mutex> lock(seen_mutex);
        last_seen.clear();
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
    if (control_mqd == (mqd_t)-1) {
        std::cerr << "Failed to create control queue " << CONTROL_QUEUE << ": "
                  << strerror(errno) << "\n";
        return 1;
    }

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
