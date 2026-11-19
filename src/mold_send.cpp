#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <thread>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {

    if (argc < 4) {
        std::cout << "Usage: mold_send <group> <port> <loss>" << std::endl;
        return 1;
    }

    const char* group = argv[1];
    int port = std::stoi(argv[2]);
    double loss = std::strtod(argv[3], nullptr);

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    in_addr lo{};
    lo.s_addr = htonl(INADDR_LOOPBACK);

    unsigned char loop = 1;
    unsigned char ttl = 0;

    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_IF, &lo, sizeof lo);
    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof loop);
    setsockopt(socket_fd, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof ttl);

    sockaddr_in destination{};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    inet_pton(AF_INET, group, &destination.sin_addr);

    std::random_device rd;
    std::mt19937_64 rng(rd());

    std::bernoulli_distribution drop(loss);

    std::uint64_t seq = 1;
    auto next = std::chrono::steady_clock::now();
    for (;;) {
        if (!drop(rng)) {
            std::uint64_t be_seq = htobe64(seq);
            sendto(socket_fd, &be_seq, sizeof be_seq, 0, reinterpret_cast<sockaddr*>(&destination), sizeof destination);
        }
        ++seq;
        next += std::chrono::milliseconds(1);
        std::this_thread::sleep_until(next);
    }
}
