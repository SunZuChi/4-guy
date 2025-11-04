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

using namespace std;
using namespace std::chrono_literals;

// --- constants ---
const char* CONTROL_QUEUE = "/control_queue";
const size_t MAX_MSG_SIZE = 8192;
const long MAX_MESSAGES = 10;
const int BROADCAST_WORKERS = 4;
const chrono::seconds HEARTBEAT_TIMEOUT = 30s;

// --- globals ---
atomic<bool> running{true};
mqd_t control_mqd = (mqd_t)-1;

// --- data structures ---
struct BroadcastTask {
    vector<string> recipients;
    string message;
};

unordered_map<string, mqd_t> client_mq; 
unordered_map<string, unordered_set<string>> room_members;
unordered_map<string, chrono::steady_clock::time_point> last_seen;

shared_mutex clients_mutex;
shared_mutex rooms_mutex;
shared_mutex seen_mutex;

queue<BroadcastTask> tasks;
mutex tasks_mtx;
condition_variable tasks_cv;

// --- helper ---
static inline void trim(string &s) {
    s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch){ return !isspace(ch); }));
    s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch){ return !isspace(ch); }).base(), s.end());
}

bool send_to_mqd(mqd_t mqd, const string &msg) {
    if (msg.empty()) return true;
    string out = msg.substr(0, MAX_MSG_SIZE - 1);
    if (mq_send(mqd, out.c_str(), out.size() + 1, 0) == -1) {
        if (errno != EAGAIN) cerr << "mq_send error: " << strerror(errno) << "\n";
        return false;
    }
    return true;
}

// --- broadcast worker ---
void broadcaster_worker() {
    while (running) {
        BroadcastTask t;
        {
            unique_lock<mutex> lk(tasks_mtx);
            tasks_cv.wait(lk, [](){ return !tasks.empty() || !running.load(); });
            if (!running && tasks.empty()) return;
            t = move(tasks.front());
            tasks.pop();
        }
        for (auto &clientid : t.recipients) {
            shared_lock<shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(clientid);
            if (it != client_mq.end())
                send_to_mqd(it->second, t.message);
        }
    }
}

// --- helpers ---
void queue_broadcast_to_room(const string &room, const string &message) {
    vector<string> recip;
    {
        shared_lock<shared_mutex> lock(rooms_mutex);
        auto it = room_members.find(room);
        if (it != room_members.end())
            recip.insert(recip.end(), it->second.begin(), it->second.end());
    }
    if (!recip.empty()) {
        lock_guard<mutex> lk(tasks_mtx);
        tasks.push({recip, message});
        tasks_cv.notify_one();
    }
}

void remove_client_from_all_rooms(const string &clientid) {
    unique_lock<shared_mutex> lock(rooms_mutex);
    for (auto &p : room_members) p.second.erase(clientid);
}

// --- main router thread ---
void router_thread_func(mqd_t control_mqd) {
    char buf[MAX_MSG_SIZE+1];

    while (running) {
        ssize_t n = mq_receive(control_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            if (errno == EINTR) continue;
            this_thread::sleep_for(100ms);
            continue;
        }
        buf[n] = '\0';
        string line(buf);
        trim(line);
        if (line.empty()) continue;

        istringstream iss(line);
        string cmd;
        iss >> cmd;

        // --- REGISTER ---
        if (cmd == "REGISTER") {
            string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            string replyq = "/reply_" + clientid;
            mqd_t mq = mq_open(replyq.c_str(), O_WRONLY);
            if (mq == (mqd_t)-1) {
                cerr << "REGISTER: failed for " << clientid << ": " << strerror(errno) << "\n";
                continue;
            }
            {
                unique_lock<shared_mutex> lock(clients_mutex);
                client_mq[clientid] = mq;
            }
            {
                unique_lock<shared_mutex> s(seen_mutex);
                last_seen[clientid] = chrono::steady_clock::now();
            }
            cerr << "[SERVER] REGISTERED: " << clientid << "\n";
        }

        // --- JOIN ---
        else if (cmd == "JOIN") {
            string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                unique_lock<shared_mutex> lock(rooms_mutex);
                room_members[room].insert(clientid);
            }
            queue_broadcast_to_room(room, "SYSTEM: " + clientid + " joined " + room);
        }

        // --- LEAVE ---
        else if (cmd == "LEAVE") {
            string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            {
                unique_lock<shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end()) it->second.erase(clientid);
            }
            queue_broadcast_to_room(room, "SYSTEM: " + clientid + " left " + room);
        }

        // --- SAY ---
        else if (cmd == "SAY") {
            string clientid, room; iss >> clientid >> room;
            string msg; getline(iss, msg); trim(msg);
            if (clientid.empty() || room.empty() || msg.empty()) continue;
            bool in_room = false;
            {
                shared_lock<shared_mutex> lock(rooms_mutex);
                auto it = room_members.find(room);
                if (it != room_members.end())
                    in_room = it->second.count(clientid) > 0;
            }
            if (!in_room) {
                shared_lock<shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end())
                    send_to_mqd(it->second, "SYSTEM: You are not in room " + room);
                continue;
            }
            queue_broadcast_to_room(room, clientid + ": " + msg);
        }

        // --- DM ---
        else if (cmd == "DM") {
            string sender, target;
            iss >> sender >> target;
            string msg; getline(iss, msg); trim(msg);
            if (sender.empty() || target.empty() || msg.empty()) continue;
            shared_lock<shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(target);
            if (it != client_mq.end())
                send_to_mqd(it->second, "DM from " + sender + ": " + msg);
        }

        // --- WHO ---
        else if (cmd == "WHO") {
            string clientid, room; iss >> clientid >> room;
            if (clientid.empty() || room.empty()) continue;
            ostringstream oss;
            {
                shared_lock<shared_mutex> lock(rooms_mutex);
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
            shared_lock<shared_mutex> lock(clients_mutex);
            auto it = client_mq.find(clientid);
            if (it != client_mq.end())
                send_to_mqd(it->second, oss.str());
        }

        // --- QUIT ---
        else if (cmd == "QUIT") {
            string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            {
                unique_lock<shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(clientid);
                if (it != client_mq.end()) {
                    mq_close(it->second);
                    client_mq.erase(it);
                }
            }
            remove_client_from_all_rooms(clientid);
            {
                unique_lock<shared_mutex> s(seen_mutex);
                last_seen.erase(clientid);
            }
            string replyq = "/reply_" + clientid;
            if (mq_unlink(replyq.c_str()) == 0)
                cerr << "[SERVER] Unlinked " << replyq << "\n";
            else
                cerr << "[SERVER] Failed to unlink " << replyq << ": " << strerror(errno) << "\n";
            cerr << "[SERVER] " << clientid << " quit\n";
        }

        // --- HEARTBEAT ---
        else if (cmd == "HEARTBEAT") {
            string clientid; iss >> clientid;
            if (clientid.empty()) continue;
            unique_lock<shared_mutex> s(seen_mutex);
            last_seen[clientid] = chrono::steady_clock::now();
        }
    }
}

// --- heartbeat checker ---
void heartbeat_checker() {
    while (running) {
        this_thread::sleep_for(5s);
        vector<string> dead;
        auto now = chrono::steady_clock::now();
        {
            shared_lock<shared_mutex> lock(seen_mutex);
            for (auto &p : last_seen)
                if (now - p.second > HEARTBEAT_TIMEOUT) dead.push_back(p.first);
        }
        for (auto &id : dead) {
            cerr << "[SERVER] " << id << " timed out\n";
            remove_client_from_all_rooms(id);
            {
                unique_lock<shared_mutex> lock(clients_mutex);
                auto it = client_mq.find(id);
                if (it != client_mq.end()) {
                    mq_close(it->second);
                    client_mq.erase(it);
                }
            }
            {
                unique_lock<shared_mutex> s(seen_mutex);
                last_seen.erase(id);
            }
            string replyq = "/reply_" + id;
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
        unique_lock<shared_mutex> lock(clients_mutex);
        for (auto &p : client_mq) mq_close(p.second);
        client_mq.clear();
    }
    cerr << "Server shutting down\n";
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
        cerr << "Hint: Make sure /dev/mqueue is mounted (--ipc=host in Docker)\n";
        return 1;
    }

    cerr << "[SERVER] Control queue ready.\n";

    vector<thread> workers;
    for (int i = 0; i < BROADCAST_WORKERS; ++i)
        workers.emplace_back(broadcaster_worker);

    thread router(router_thread_func, control_mqd);
    thread heart(heartbeat_checker);

    router.join();
    heart.join();
    for (auto &t : workers) t.join();

    cleanup_and_exit(0);
    return 0;
}
