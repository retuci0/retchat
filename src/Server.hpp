#pragma once

#include "Room.hpp"

#include <map>
#include <string>
#include <mutex>


namespace Retchat {

    constexpr int DEFAULT_PORT = 6677;

    class Client;

    class Server {
    public:
        Server(int port);
        ~Server();
        void run();

        void removeClient(Client* client);
        void broadcastToRoom(const std::string& roomName, Client* exclude, const Packet& pkt);
        bool isNicknameTaken(const std::string& nick, const std::string& roomName, Client* exclude);
        
        Room& getRoom(const std::string& name);

    private:
        int port;
        int listenFd = -1;
        std::map<int, Client*> clients;
        std::map<std::string, Room> rooms;
        std::mutex mutex;
        bool running = true;

        Room& getOrCreateRoom(const std::string& name);
    };

}