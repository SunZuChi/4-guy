// client.cpp
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <cctype>
#include <iostream>
#include <thread>
#include <string>
#include <chrono>
#include <atomic>
#include <sstream>

using namespace std::chrono_literals;

const char* CONTROL_QUEUE = "/control_queue";
const size_t MAX_MSG_SIZE = 8192;
const long MAX_MESSAGES = 10;

std::atomic<bool> running{true};

void listener_thread_func(mqd_t reply_mqd) {
    char buf[MAX_MSG_SIZE+1];
    while (running) {
        ssize_t n = mq_receive(reply_mqd, buf, MAX_MSG_SIZE, nullptr);
        if (n == -1) {
            if (errno == EINTR) continue;
            std::cerr << "mq_receive (reply) error: " << strerror(errno) << "\n";
            std::this_thread::sleep_for(100ms);
            continue;
        }
        buf[n] = '\0';
        std::cout << buf << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./client <clientid>\n";
        return 1;
    }
    std::string clientid = argv[1];
    std::string replyq = "/" + std::string("reply_") + clientid;

    // create reply queue
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = MAX_MSG_SIZE;
    attr.mq_curmsgs = 0;

    mqd_t reply_mqd = mq_open(replyq.c_str(), O_CREAT | O_RDONLY, 0666, &attr);
    if (reply_mqd == (mqd_t)-1) {
        std::cerr << "Failed to create reply queue: " << strerror(errno) << "\n";
        return 1;
    }

    // open control queue for writing
    mqd_t control_mqd = mq_open(CONTROL_QUEUE, O_WRONLY);
    if (control_mqd == (mqd_t)-1) {
        std::cerr << "Failed to open control queue: " << strerror(errno) << "\n";
        mq_close(reply_mqd);
        mq_unlink(replyq.c_str());
        return 1;
    }

    // send REGISTER
    std::string reg = "REGISTER " + clientid;
    mq_send(control_mqd, reg.c_str(), reg.size(), 0);

    // start listener
    std::thread listener(listener_thread_func, reply_mqd);

    // start heartbeat sender
    std::thread hb([&](){
        while (running) {
            std::string hbmsg = "HEARTBEAT " + clientid;
            mq_send(control_mqd, hbmsg.c_str(), hbmsg.size(), 0);
            std::this_thread::sleep_for(10s);
        }
    });

    // input loop: simple commands:
    // JOIN <room>
    // LEAVE <room>
    // SAY <room> <text...>
    // DM <target> <text...>
    // WHO <room>
    // QUIT
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!running) break;
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;
        if (cmd.empty()) continue;
        if (cmd == "JOIN" || cmd == "LEAVE" || cmd == "WHO") {
            std::string room; iss >> room;
            if (room.empty()) { std::cout << "usage: " << cmd << " <room>\n"; continue; }
            std::string out = cmd + " " + clientid + " " + room;
            mq_send(control_mqd, out.c_str(), out.size(), 0);
        } else if (cmd == "SAY") {
            std::string room; iss >> room;
            std::string rest; std::getline(iss, rest);
            if (room.empty() || rest.empty()) { std::cout << "usage: SAY <room> <message>\n"; continue; }
            std::string out = "SAY " + clientid + " " + room + " " + rest;
            mq_send(control_mqd, out.c_str(), out.size(), 0);
        } else if (cmd == "DM") {
            std::string target; iss >> target; std::string rest; std::getline(iss, rest);
            if (target.empty() || rest.empty()) { std::cout << "usage: DM <target> <message>\n"; continue; }
            std::string out = "DM " + clientid + " " + target + " " + rest;
            mq_send(control_mqd, out.c_str(), out.size(), 0);
        } else if (cmd == "QUIT") {
            std::string out = "QUIT " + clientid;
            mq_send(control_mqd, out.c_str(), out.size(), 0);
            running = false;
            break;
        } else {
            std::cout << "Unknown command. Use JOIN/LEAVE/SAY/DM/WHO/QUIT\n";
        }
    }

    // cleanup
    running = false;
    // close and unlink reply queue
    mq_close(reply_mqd);
    mq_unlink(replyq.c_str());

    mq_close(control_mqd);

    listener.join();
    hb.join();

    std::cout << "Client exiting\n";
    return 0;
}
