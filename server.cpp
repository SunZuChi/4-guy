// server.cpp
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

// --- data structures ---
struct BroadcastTask {
    std::vector<std::string> recipients; // clientids
    std::string message;
};

// registries
std::unordered_map<std::string, mqd_t> client_mq; // clientid -> mqd_t (server's descriptor)
std::unordered_map<std::string, std::unordered_set<std::string>> room_members; // room -> set(clientid)
std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_seen; // clientid -> last heartbeat time

std::shared_mutex clients_mutex;
std::shared_mutex rooms_mutex;
std::shared_mutex seen_mutex;

// task queue
std::queue<BroadcastTask> tasks;
std::mutex tasks_mtx;
std::condition_variable tasks_cv;

// helper: trim
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch){ return !std::isspace(ch); }));
}
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !std::isspace(ch); }).base(), s.end());
}
static inline void trim(std::string &s) { ltrim(s); rtrim(s); }

// helper: send message to one client (by mqd_t)
bool send_to_mqd(mqd_t mqd, const std::string &msg) {
    if (msg.size() > MAX_MSG_SIZE) {
        std::cerr << "Warning: message truncated for mq_send\n";
    }
    if (mq_send(mqd, msg.c_str(), std::min(msg.size(), MAX_MSG_SIZE), 0) == -1) {
        if (errno == EAGAIN) {
            // queue full, message dropped (policy)
            return false;
        } else {
            std::cerr << "mq_send error: " << strerror(errno) << "\n";
            return false;
        }
    }
    return true;
}

// broadcast worker
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
            mqd_t target = (mqd_t) -1;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it == client_mq.end()) continue;
                target = it->second;
            }
            if (target != (mqd_t)-1) {
                bool ok = send_to_mqd(target, t.message);
                if (!ok) {
                    // policy: drop; could also enqueue a system notice back to sender
                }
            }
        }
    }
}

// queue a broadcast by room (server side)
void queue_broadcast_to_room(const std::string &room, const std::string &message) {
    std::vector<std::string> recip;
    {
        std::shared_lock<std::shared_mutex> lock(rooms_mutex);
        auto it = room_members.find(room);
        if (it != room_members.end()) {
            recip.insert(recip.end(), it->second.begin(), it->second.end());
        }
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

// queue a broadcast to explicit recipients
void queue_broadcast_to_clients(const std::vector<std::string> &clients, const std::string &message) {
    BroadcastTask t{clients, message};
    {
        std::lock_guard<std::mutex> lk(tasks_mtx);
        tasks.push(std::move(t));
    }
    tasks_cv.notify_one();
}

// remove client from all rooms (internal)
void remove_client_from_all_rooms(const std::string &clientid) {
    std::unique_lock<std::shared_mutex> lock(rooms_mutex);
    for (auto &p : room_members) {
        p.second.erase(clientid);
    }
}

// router thread: consumes control queue and acts
void router_thread_func(mqd_t control_mqd) {
    char buf[MAX_MSG_SIZE+1];
    while (running) {
        ssize_t n = mq_receive(control_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            if (errno == EINTR) continue;
            std::cerr << "mq_receive error on control: " << strerror(errno) << "\n";
            // Sleep briefly and continue, but you might also choose to break
            std::this_thread::sleep_for(100ms);
            continue;
        }
        buf[n] = '\0';
        std::string line(buf);
        trim(line);
        if (line.empty()) continue;

        // parse: commands start with WORD
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd == "REGISTER") {
            std::string clientid;
            iss >> clientid;
            if (clientid.empty()) continue;
            std::string replyq = "/" + std::string("reply_") + clientid;
            // open reply queue for write
            mqd_t mq = mq_open(replyq.c_str(), O_WRONLY);
            if (mq == (mqd_t)-1) {
                std::cerr << "REGISTER: cannot open client reply queue " << replyq << " : " << strerror(errno) << "\n";
                // ignore; client might not have created it yet; we could retry on first send
            } else {
                {
                    std::unique_lock<std::shared_mutex> lock(clients_mutex);
                    client_mq[clientid] = mq;
                }
                {
                    std::unique_lock<std::shared_mutex> s(seen_mutex);
                    last_seen[clientid] = std::chrono::steady_clock::now();
                }
                std::string sys = "SYSTEM " + clientid + " registered";
                std::cerr << sys << "\n";
            }
        } else if (cmd == "JOIN") {
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                room_members[room].insert(clientid);
            }
            std::string notice = "SYSTEM " + clientid + " joined " + room;
            queue_broadcast_to_room(room, notice);
        } else if (cmd == "LEAVE") {
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                std::unique_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) it->second.erase(clientid);
            }
            std::string notice = "SYSTEM " + clientid + " left " + room;
            queue_broadcast_to_room(room, notice);
        } else if (cmd == "SAY") {
            std::string clientid, room;
            iss >> clientid >> room;
            std::string rest;
            std::getline(iss, rest);
            trim(rest);
            if (clientid.empty() || room.empty() || rest.empty()) continue;
            bool in_room = false;
    {
        std::shared_lock<std::shared_mutex> lock(rooms_mutex);
        auto it = room_members.find(room);
        if (it != room_members.end()) {
            in_room = it->second.count(clientid) > 0;
        }
    }
    if (!in_room) {
        // optionally send a system notice back to client
        mqd_t mq = (mqd_t)-1;
        {
            std::shared_lock<std::shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(clientid);
            if (it != client_mq.end()) mq = it->second;
        }
        if (mq != (mqd_t)-1) send_to_mqd(mq, "SYSTEM: You cannot SAY to a room you are not in");
        continue;
    }
            std::string msg = clientid + " (room " + room + "): " + rest;
            queue_broadcast_to_room(room, msg);
        } else if (cmd == "DM") {
            std::string clientid, target;
            iss >> clientid >> target;
            std::string rest;
            std::getline(iss, rest);
            trim(rest);
            if (clientid.empty() || target.empty() || rest.empty()) continue;
            std::vector<std::string> one = { target };
            std::string msg = clientid + " (DM): " + rest;
            queue_broadcast_to_clients(one, msg);
        } else if (cmd == "WHO") {
            std::string clientid, room;
            iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            std::string members_list = "WHO " + room + ":";
            {
                std::shared_lock<std::shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) {
                    for (auto &m : it->second) members_list += " " + m;
                }
            }
            // send directly to clientid
            mqd_t mq = (mqd_t)-1;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) mq = it->second;
            }
            if (mq != (mqd_t)-1) send_to_mqd(mq, members_list);
        } else if (cmd == "QUIT") {
            std::string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            // remove from clients and rooms
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
            // optionally broadcast "left" to all rooms - for simplicity just announce top-level
            std::string notice = "SYSTEM " + clientid + " quit";
            // broadcast to everyone (collect all clients)
            std::vector<std::string> all;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                for (auto &p : client_mq) all.push_back(p.first);
            }
            queue_broadcast_to_clients(all, notice);
        } else if (cmd == "HEARTBEAT") {
            std::string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            {
                std::unique_lock<std::shared_mutex> s(seen_mutex);
                last_seen[clientid] = std::chrono::steady_clock::now();
            }
        } else {
            std::cerr << "Unknown command: " << cmd << " line=" << line << "\n";
        }
    } // while
}

// heartbeat checker (cleans up dead clients)
void heartbeat_checker() {
    while (running) {
        std::this_thread::sleep_for(5s);
        std::vector<std::string> to_remove;
        auto now = std::chrono::steady_clock::now();
        {
            std::shared_lock<std::shared_mutex> lock(seen_mutex);
            for (auto &p : last_seen) {
                if (now - p.second > HEARTBEAT_TIMEOUT) to_remove.push_back(p.first);
            }
        }
        for (auto &clientid : to_remove) {
            // announce leave
            remove_client_from_all_rooms(clientid);
            {
                std::unique_lock<std::shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) {
                    mq_close(it->second);
                    client_mq.erase(it);
                }
            }
            {
                std::unique_lock<std::shared_mutex> s(seen_mutex);
                last_seen.erase(clientid);
            }
            // notify all
            std::vector<std::string> all;
            {
                std::shared_lock<std::shared_mutex> lock(clients_mutex);
                for (auto &p : client_mq) all.push_back(p.first);
            }
            queue_broadcast_to_clients(all, "SYSTEM " + clientid + " timed out/disconnected");
        }
    }
}

// cleanup on SIGINT
mqd_t control_mqd = (mqd_t)-1;
void cleanup_and_exit(int) {
    running = false;
    tasks_cv.notify_all();
    if (control_mqd != (mqd_t)-1) {
        mq_close(control_mqd);
        mq_unlink(CONTROL_QUEUE);
    }
    // close other client mqd's
    {
        std::unique_lock<std::shared_mutex> lock(clients_mutex);
        for (auto &p : client_mq) mq_close(p.second);
        client_mq.clear();
    }
    std::cerr << "Server shutting down\n";
}

int main() {
    signal(SIGINT, cleanup_and_exit);

    // create control queue
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;
    struct stat sb{};
    if (stat("/dev/mqueue", &sb) != 0) {
        std::cerr << "Error: /dev/mqueue not mounted inside container.\n";
        return 1;
    }
    control_mqd = mq_open(CONTROL_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    if (control_mqd == (mqd_t)-1) {
        std::cerr << "Failed to create control queue: " << strerror(errno) << "\n";
        return 1;
    }
    std::cerr << "Control queue created: " << CONTROL_QUEUE << "\n";

    // spawn broadcasters
    std::vector<std::thread> workers;
    for (int i=0;i<BROADCAST_WORKERS;i++) workers.emplace_back(broadcaster_worker);

    std::thread router(router_thread_func, control_mqd);
    std::thread heart(heartbeat_checker);

    // main thread waits for router to finish
    router.join();
    heart.join();
    for (auto &t : workers) {
        t.join();
    }

    cleanup_and_exit(0);
    return 0;
}
