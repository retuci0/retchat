#pragma once

#include "Packet.hpp"

#include <chrono>
#include <netinet/in.h>
#include <string>
#include <vector>
#include <mutex>


namespace Retchat {

    class Server;

    class Client {
    public:
        Client(int sockfd, Server* server);
        ~Client();
        void start();
        void sendPacket(const Packet& pkt);
        void disconnect();
        bool isConnected() const { return connected; }

        int getSockfd() const { return sockfd; }
        std::string getName() const { return name; }
        std::string getRoom() const { return room; }
        void setName(const std::string& n) { name = n; }
        void setRoom(const std::string& r) { room = r; }

    private:
        void run();
        bool handshake();
        bool readFrame(std::vector<uint8_t>& outPlain);
        void processPacket(Packet* pkt);

        int sockfd;
        Server* server;
        std::string name;
        std::string room;
        uint8_t encKey[32];
        uint64_t sendCounter, recvCounter;
        bool connected;
        std::mutex sendMutex;

        // keepalive
        std::chrono::steady_clock::time_point lastRecvTime;
        std::chrono::steady_clock::time_point lastKeepAliveSent;
        bool waitingForAck = false;
    };

}