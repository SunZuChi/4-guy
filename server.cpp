// server.cpp — POSIX MQ chat server (improved version)
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

// --- constants ---
const char* CONTROL_QUEUE = "/control_queue";
const size_t MAX_MSG_SIZE = 8192;
const long MAX_MESSAGES = 10;
const int BROADCAST_WORKERS = 4;
const std::chrono::seconds HEARTBEAT_TIMEOUT = 30s;

// --- globals ---
std::atomic<bool> running{true};
mqd_t control_mqd = (mqd_t)-1;

// --- data structures ---
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

// --- helper ---
static inline void trim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
}

bool send_to_mqd(mqd_t mqd, const std::string &msg) {
    if (msg.empty()) return true;
    std::string out = msg.substr(0, MAX_MSG_SIZE - 1);
    if (mq_send(mqd, out.c_str(), out.size() + 1, 0) == -1) {
        if (errno != EAGAIN) std::cerr << "mq_send error: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

// --- broadcast worker ---
void broadcaster_worker() {
    while (running) {
        BroadcastTask t;
        {
            std::unique_lock<std::mutex> lk(tasks_mtx);
            tasks_cv.wait(lk, [](){ return !tasks.empty() || !running.load(); });
            if (!running && tasks.empty()) return;
            t = std::move(tasks.front());
            tasks.pop();
        }
        for (auto &clientid : t.recipients) {
            std::shared_lock<std::shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(clientid);
            if (it != client_mq.end())
                send_to_mqd(it->second, t.message);
        }
    }
}

// --- helpers ---
void queue_broadcast_to_room(const std::string &room, const std::string &message) {
    std::vector<std::string> recip;
    {
        std::shared_lock<std::shared_mutex> lock(rooms_mutex);
        auto it = room_members.find(room);
        if (it != room_members.end())
            recip.insert(recip.end(), it->second.begin(), it->second.end());
    }
    if (!recip.empty()) {
        std::lock_guard<std::mutex> lk(tasks_mtx);
        tasks.push({recip, message});
        tasks_cv.notify_one();
    }
}

void remove_client_from_all_rooms(const std::string &clientid) {
    std::unique_lock<std::shared_mutex> lock(rooms_mutex);
    for (auto &p : room_members) p.second.erase(clientid);
}

// --- main router thread ---
void router_thread_func(mqd_t control_mqd) {
    char buf[MAX_MSG_SIZE+1];

    while (running) {
        ssize_t n = mq_receive(control_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            if (errno == EINTR) continue;
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

        // --- REGISTER ---
        if (cmd == "REGISTER") {
            std::string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            std::string replyq = "/reply_" + clientid;
            mqd_t mq = mq_open(replyq.c_str(), O_WRONLY);
            if (mq == (mqd_t)-1) {
                std::cerr << "REGISTER: failed for " << clientid << ": " << strerror(errno) << "\n";
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
            std::cerr << "[SERVER] REGISTERED: " << clientid << "\n";
        }

        // --- JOIN ---
        else if (cmd == "JOIN") {
            std::string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                room_members[room].insert(clientid);
            }
            queue_broadcast_to_room(room, "SYSTEM: " + clientid + " joined " + room);
        }

        // --- LEAVE ---
        else if (cmd == "LEAVE") {
            std::string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) it->second.erase(clientid);
            }
            queue_broadcast_to_room(room, "SYSTEM: " + clientid + " left " + room);
        }

        // --- SAY ---
        else if (cmd == "SAY") {
            std::string clientid, room; iss >> clientid >> room;
            std::string msg; std::getline(iss, msg); trim(msg);
            if (clientid.empty() || room.empty() || msg.empty()) continue;
            bool in_room = false;
            {
                std::shared_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end())
                    in_room = it->second.count(clientid) > 0;
            }
            if (!in_room) {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end())
                    send_to_mqd(it->second, "SYSTEM: You are not in room " + room);
                continue;
            }
            queue_broadcast_to_room(room, clientid + ": " + msg);
        }

        // --- DM ---
        else if (cmd == "DM") {
            std::string sender, target;
            iss >> sender >> target;
            std::string msg; std::getline(iss, msg); trim(msg);
            if (sender.empty() || target.empty() || msg.empty()) continue;
            std::shared_lock<std::shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(target);
            if (it != client_mq.end())
                send_to_mqd(it->second, "DM from " + sender + ": " + msg);
        }

        // --- WHO ---
        else if (cmd == "WHO") {
            std::string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            std::ostringstream oss;
            {
                std::shared_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) {
                    oss << "SYSTEM: Members in " << room << ": ";
                    bool first = true;
                    for (auto &m : it->second) {
                        if (!first) oss << ", ";
                        oss << m;
                        first = false;
                    }
                } else {
                    oss << "SYSTEM: Room '" << room << "' has no members.";
                }
            }
            std::shared_lock<std::shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(clientid);
            if (it != client_mq.end())
                send_to_mqd(it->second, oss.str());
        }

        // --- QUIT ---
        else if (cmd == "QUIT") {
            std::string clientid; iss >> clientid;
            if (clientid.empty()) continue;
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
            std::string replyq = "/reply_" + clientid;
            if (mq_unlink(replyq.c_str()) == 0)
                std::cerr << "[SERVER] Unlinked " << replyq << "\n";
            else
                std::cerr << "[SERVER] Failed to unlink " << replyq << ": " << strerror(errno) << "\n";
            std::cerr << "[SERVER] " << clientid << " quit\n";
        }

        // --- HEARTBEAT ---
        else if (cmd == "HEARTBEAT") {
            std::string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            std::unique_lock<std::shared_mutex> s(seen_mutex);
            last_seen[clientid] = std::chrono::steady_clock::now();
        }
    }
}

// --- heartbeat checker ---
void heartbeat_checker() {
    while (running) {
        std::this_thread::sleep_for(5s);
        std::vector<std::string> dead;
        auto now = std::chrono::steady_clock::now();
        {
            std::shared_lock<std::shared_mutex> lock(seen_mutex);
            for (auto &p : last_seen)
                if (now - p.second > HEARTBEAT_TIMEOUT) dead.push_back(p.first);
        }
        for (auto &id : dead) {
            std::cerr << "[SERVER] " << id << " timed out\n";
            remove_client_from_all_rooms(id);
            {
                std::unique_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(id);
                if (it != client_mq.end()) {
                    mq_close(it->second);
                    client_mq.erase(it);
                }
            }
            {
                std::unique_lock<std::shared_mutex> s(seen_mutex);
                last_seen.erase(id);
            }
            std::string replyq = "/reply_" + id;
            mq_unlink(replyq.c_str());
        }
    }
}

// --- cleanup ---
void cleanup_and_exit(int) {
    running = false;
    tasks_cv.notify_all();
    if (control_mqd != (mqd_t)-1) {
        mq_close(control_mqd);
        mq_unlink(CONTROL_QUEUE);
    }
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex);
        for (auto &p : client_mq) mq_close(p.second);
        client_mq.clear();
    }
    std::cerr << "Server shutting down\n";
    exit(0);
}

int main() {
    signal(SIGINT, cleanup_and_exit);

    struct mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mq_unlink(CONTROL_QUEUE); // remove old
    control_mqd = mq_open(CONTROL_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (control_mqd == (mqd_t)-1) {
        perror("Failed to create control queue");
        std::cerr << "Hint: Make sure /dev/mqueue is mounted (--ipc=host in Docker)\n";
        return 1;
    }

    std::cerr << "[SERVER] Control queue ready.\n";

    std::vector<std::thread> workers;
    for (int i = 0; i < BROADCAST_WORKERS; ++i)
        workers.emplace_back(broadcaster_worker);

    std::thread router(router_thread_func, control_mqd);
    std::thread heart(heartbeat_checker);

    router.join();
    heart.join();
    for (auto &t : workers) t.join();

    cleanup_and_exit(0);
    return 0;
}
