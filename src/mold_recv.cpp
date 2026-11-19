#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <format>
#include <mutex>
#include <thread>

#include "includes/httplib.h"

std::atomic<std::uint64_t> packets{0}, gaps{0}, missing{0};
std::mutex snap_mu;
std::string snapshot = "{}";

int main(int argc, char *argv[]) {

    if (argc < 4) {
        std::cout << "Usage: mold_recv <group> <port> <http_port>" << std::endl;
        return 1;
    }

    const char* group = argv[1];
    int port = std::stoi(argv[2]);
    int http_port = std::stoi(argv[3]);


    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int on = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(socket_fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        perror("bind() failed");
        return 1;
    }

    ip_mreqn mc_req{};
    inet_pton(AF_INET, group, &mc_req.imr_multiaddr);
    mc_req.imr_address.s_addr = htonl(INADDR_LOOPBACK);
    if (setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc_req, sizeof mc_req) < 0) {
        perror("join"); return 1;
    }

    std::thread([] {
        std::uint64_t last = 0;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto p = packets.load(std::memory_order_relaxed);
            auto json = std::format(
                R"({{"packets_per_s":{},"total_packets":{},"gaps":{},"missing":{}}})",
                p - last, p, gaps.load(std::memory_order_relaxed), missing.load(std::memory_order_relaxed));
            last = p;
            std::lock_guard lk(snap_mu);
            snapshot = std::move(json);
        }
    }).detach();

    std::thread([http_port] {
        httplib::Server svr;
        svr.Get("/stats", [](const httplib::Request&, httplib::Response& res) {
            std::lock_guard lk(snap_mu);
            res.set_content(snapshot + "\n", "application/json");
        });
        svr.listen("0.0.0.0", http_port);
    }).detach();

    std::uint64_t expected = 0;
    unsigned char buf[2048];
    for (;;) {
        ssize_t n = recv(socket_fd, buf, sizeof buf, 0);
        if (n < 8) {
            continue;
        }

        std::uint64_t be;
        std::memcpy(&be, buf, 8);
        std::uint64_t seq = be64toh(be);

        if (expected != 0 && seq > expected) {
            gaps.fetch_add(1, std::memory_order_relaxed);
            missing.fetch_add(seq - expected, std::memory_order_relaxed);
        }

        if (seq >= expected) {
            expected = seq + 1;
        }
        packets.fetch_add(1, std::memory_order_relaxed);
    }
}
