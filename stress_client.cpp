// stress_client.cpp
// Compile: g++ -std=c++17 -pthread -lrt stress_client.cpp -O2 -o stress_client
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

const char* CONTROL_QUEUE = "/control_queue";
const size_t MAX_MSG_SIZE = 2048;

void sender_thread(const std::string &clientid, int ops, int rooms_count, int seed) {
    mqd_t control = mq_open(CONTROL_QUEUE, O_WRONLY);
    if (control == (mqd_t)-1) {
        std::cerr << "mq_open control failed: " << strerror(errno) << "\n";
        return;
    }
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> room_dist(1, rooms_count);
    std::uniform_int_distribution<int> op_dist(0, 4);

    // register first
    {
        std::string reg = "REGISTER " + clientid;
        mq_send(control, reg.c_str(), reg.size(), 0);
    }

    for (int i = 0; i < ops; ++i) {
        int op = op_dist(rng);
        int r = room_dist(rng);
        std::ostringstream oss;
        if (op == 0) { // JOIN
            oss << "JOIN " << clientid << " room" << r;
        } else if (op == 1) { // LEAVE
            oss << "LEAVE " << clientid << " room" << r;
        } else if (op == 2) { // SAY
            oss << "SAY " << clientid << " room" << r << " Hello from " << clientid << " #" << i;
        } else if (op == 3) { // WHO
            oss << "WHO " << clientid << " room" << r;
        } else { // DM
            int target_id = (seed + i) % 50; // some target
            oss << "DM " << clientid << " user" << target_id << " Hi DM #" << i;
        }
        std::string s = oss.str();
        if (mq_send(control, s.c_str(), s.size(), 0) == -1) {
            std::cerr << "mq_send failed: " << strerror(errno) << "\n";
        }
        // tiny random sleep
        std::this_thread::sleep_for(std::chrono::microseconds(100 + (rng() % 500)));
    }

    // QUIT
    {
        std::string q = "QUIT " + clientid;
        mq_send(control, q.c_str(), q.size(), 0);
    }

    mq_close(control);
}

int main(int argc, char** argv) {
    int n_threads = 50;
    int ops_each = 200;
    int rooms = 10;

    if (argc >= 2) n_threads = atoi(argv[1]);
    if (argc >= 3) ops_each = atoi(argv[2]);
    if (argc >= 4) rooms = atoi(argv[3]);

    std::vector<std::thread> threads;
    for (int i = 0; i < n_threads; ++i) {
        std::string cid = "test" + std::to_string(i);
        threads.emplace_back(sender_thread, cid, ops_each, rooms, i+1000);
    }
    for (auto &t : threads) t.join();
    std::cout << "Stress test done\n";
    return 0;
}