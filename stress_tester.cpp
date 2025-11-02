// stress_tester.cpp (auto-safe version)
// Compile:
// g++ -std=c++17 -pthread -lrt stress_tester.cpp -O2 -o stress_tester
//
// Usage:
// ./stress_tester <num_clients> <ops_per_client> <rooms>
// Example:
// ./stress_tester 20 100 5

#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fstream>

#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <sstream>
#include <string>
#include <random>

using namespace std::chrono_literals;

const char* CONTROL_QUEUE = "/control_queue";
const size_t DEFAULT_MSGSIZE = 2048;
const long DEFAULT_MAXMSG = 50;

std::atomic<uint64_t> total_sent{0};
std::atomic<uint64_t> total_received{0};

struct ClientParams {
    int id;
    int ops;
    int rooms;
};

long read_sys_limit(const std::string &path, long fallback) {
    std::ifstream f(path);
    long val = 0;
    if (f && (f >> val)) return val;
    return fallback;
}

void listener_func(const std::string &replyq_name, std::atomic<bool> &run_flag, std::atomic<uint64_t> &recv_count) {
    mqd_t mq = mq_open(replyq_name.c_str(), O_RDONLY);
    if (mq == (mqd_t)-1) {
        std::cerr << "[listener] mq_open failed for " << replyq_name << ": " << strerror(errno) << "\n";
        return;
    }
    char buf[8192];
    while (run_flag.load()) {
        ssize_t n = mq_receive(mq, buf, sizeof(buf), nullptr);
        if (n == -1) {
            if (errno == EINTR) continue;
            std::this_thread::sleep_for(1ms);
            continue;
        }
        ++recv_count;
        ++total_received;
    }
    mq_close(mq);
}

void client_thread_func(ClientParams p, long maxmsg, long msgsize) {
    std::string clientid = "st" + std::to_string(p.id);
    std::string replyq = "/" + std::string("reply_") + clientid;

    // create reply queue with safe attributes
    struct mq_attr attr{};
    attr.mq_flags = 0;
    attr.mq_maxmsg = std::min<long>(maxmsg, 10);  // ลดจำนวนลงเล็กน้อยเพื่อความแน่ใจ
    attr.mq_msgsize = std::min<long>(msgsize, 1024);
    attr.mq_curmsgs = 0;

    mqd_t reply_mqd = mq_open(replyq.c_str(), O_CREAT | O_RDONLY, 0666, &attr);
    if (reply_mqd == (mqd_t)-1) {
        std::cerr << "[client " << clientid << "] Failed to create reply queue: "
                  << strerror(errno)
                  << " (maxmsg=" << attr.mq_maxmsg
                  << ", msgsize=" << attr.mq_msgsize << ")\n";
        return;
    }

    mqd_t control_mqd = mq_open(CONTROL_QUEUE, O_WRONLY);
    if (control_mqd == (mqd_t)-1) {
        std::cerr << "[client " << clientid << "] Failed to open control queue: " << strerror(errno) << "\n";
        mq_close(reply_mqd);
        mq_unlink(replyq.c_str());
        return;
    }

    // start listener
    std::atomic<bool> run_listener{true};
    std::atomic<uint64_t> recv_count{0};
    std::thread listener(listener_func, replyq, std::ref(run_listener), std::ref(recv_count));

    // REGISTER
    {
        std::string reg = "REGISTER " + clientid;
        mq_send(control_mqd, reg.c_str(), reg.size(), 0);
    }

    // choose a room
    int room_id = (p.id % p.rooms) + 1;
    std::string room = "room" + std::to_string(room_id);

    // JOIN
    {
        std::string join = "JOIN " + clientid + " " + room;
        mq_send(control_mqd, join.c_str(), join.size(), 0);
    }

    // send ops messages
    for (int i = 0; i < p.ops; ++i) {
        std::ostringstream oss;
        oss << "SAY " << clientid << " " << room << " msg_" << i;
        std::string m = oss.str();
        if (mq_send(control_mqd, m.c_str(), m.size(), 0) == -1) {
            std::cerr << "[client " << clientid << "] mq_send error: " << strerror(errno) << "\n";
        } else {
            ++total_sent;
        }
        if ((i & 0x3) == 0) std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    std::this_thread::sleep_for(500ms);

    // LEAVE & QUIT
    {
        std::string leave = "LEAVE " + clientid + " " + room;
        mq_send(control_mqd, leave.c_str(), leave.size(), 0);
    }
    {
        std::string q = "QUIT " + clientid;
        mq_send(control_mqd, q.c_str(), q.size(), 0);
    }

    run_listener = false;
    listener.join();

    mq_close(reply_mqd);
    mq_unlink(replyq.c_str());
    mq_close(control_mqd);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <num_clients> <ops_per_client> <rooms>\n";
        std::cout << "Example: ./stress_tester 20 100 5\n";
        return 1;
    }

    int num_clients = atoi(argv[1]);
    int ops_per_client = atoi(argv[2]);
    int rooms = atoi(argv[3]);

    // อ่านค่า limit ระบบ
    long sys_maxmsg = read_sys_limit("/proc/sys/fs/mqueue/msg_max", DEFAULT_MAXMSG);
    long sys_msgsize = read_sys_limit("/proc/sys/fs/mqueue/msgsize_max", DEFAULT_MSGSIZE);

    std::cout << "[System limits] msg_max=" << sys_maxmsg
              << " msgsize_max=" << sys_msgsize << "\n";

    std::cout << "Stress tester: clients=" << num_clients
              << " ops/client=" << ops_per_client
              << " rooms=" << rooms << "\n";

    total_sent = 0;
    total_received = 0;

    std::vector<std::thread> threads;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < num_clients; ++i) {
        ClientParams p{i, ops_per_client, rooms};
        threads.emplace_back(client_thread_func, p, sys_maxmsg, sys_msgsize);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (auto &t : threads) t.join();

    auto end = std::chrono::steady_clock::now();
    double sec = std::chrono::duration_cast<std::chrono::duration<double>>(end - start).count();

    uint64_t sent = total_sent.load();
    uint64_t recv = total_received.load();

    std::cout << "\n=== Test Results ===\n";
    std::cout << "Elapsed: " << sec << " sec\n";
    std::cout << "Sent: " << sent << "\n";
    std::cout << "Received: " << recv << "\n";
    std::cout << "Send throughput: " << (sent / sec) << " msgs/sec\n";
    std::cout << "Receive throughput: " << (recv / sec) << " msgs/sec\n";

    if (recv < sent)
        std::cout << "X  Some messages may not have been echoed back by server.\n";
    else
        std::cout << "/ All messages appear to have been received.\n";

    return 0;
}
