#pragma once

#include "Room.hpp"

#include <map>
#include <string>
#include <mutex>
#include <thread>
#include <unordered_set>


namespace Retchat {

    constexpr int DEFAULT_PORT = 6677;

    class Client;

    class Server {
    public:
        Server(int port);
        ~Server();
        void run();
        void stop();

        void removeClient(Client* client);
        void broadcastToRoom(const std::string& roomName, Client* exclude, const Packet& pkt);
        bool isNicknameTaken(const std::string& nick, const std::string& roomName, Client* exclude);
        
        Room& getRoom(const std::string& name);


        // console commands

        void kickClient(int fd);
        void banNickname(const std::string& nick);
        void banIp(const std::string& ip);
        
        bool isNicknameBanned(const std::string& nick) const;
        bool isIpBanned(const std::string& ip) const;

        std::string listClients() const;
        std::string listRooms() const;
        std::string queryClient(int fd) const;
        std::string queryRoom(const std::string& room) const;

    private:
        int port;
        int listenFd = -1;
        std::map<int, Client*> clients;
        std::map<std::string, Room> rooms;
        mutable std::mutex mutex;
        bool running = true;

        std::unordered_set<std::string> bannedNicks;
        std::unordered_set<std::string> bannedIps;

        std::thread consoleThread;
        void consoleLoop();

        Room& getOrCreateRoom(const std::string& name);
        void disconnectClient(Client* client, bool sendPacket = true);
    };

}