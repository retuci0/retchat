#pragma once

#include "Room.hpp"

#include <map>
#include <string>
#include <mutex>
#include <thread>
#include <unordered_set>


namespace Retchat {

    constexpr int DEFAULT_PORT = 6677;
    const std::string DEFAULT_BANS_FILE = "bans.txt";

    class Client;

    class Server {
    public:
        Server(int port, const std::string& bansFile = "bans.txt");
        ~Server();
        void run();
        void stop();

        void removeClient(Client* client);
        void broadcastToRoom(const std::string& roomName, Client* exclude, const Packet& pkt);
        void sendImageDm(Client* from, const std::string& targetNick, const ImagePacket& imgPkt);
        bool isNicknameTaken(const std::string& nick, const std::string& roomName, Client* exclude);
        
        Room& getRoom(const std::string& name);


        // console commands

        void kickClient(int fd, const std::string& reason = "kicked by admin");
        void banNickname(const std::string& nick, const std::string& reason = "banned");
        void banIp(const std::string& ip, const std::string& reason = "banned");
        void unbanNickname(const std::string& nick);
        void unbanIp(const std::string& ip);

        bool isNicknameBanned(const std::string& nick) const;
        bool isIpBanned(const std::string& ip) const;

        void sendDm(Client* from, const std::string& targetNick, const std::string& text);

        std::string listClients() const;
        std::string listRooms() const;
        std::string listBans() const;
        std::string queryClient(int fd) const;
        std::string queryRoom(const std::string& room) const;

        void loadBans(const std::string& path);
        void saveBans(const std::string& path) const;

    private:
        int port;
        int listenFd = -1;
        std::map<int, Client*> clients;
        std::map<std::string, Room> rooms;
        mutable std::mutex mutex;
        bool running = true;

        std::unordered_set<std::string> bannedNicks;
        std::unordered_set<std::string> bannedIps;
        std::string bansFilePath;

        std::thread consoleThread;
        void consoleLoop();

        Room& getOrCreateRoom(const std::string& name);
        void disconnectClient(Client* client, bool sendPacket = true);
    };

}