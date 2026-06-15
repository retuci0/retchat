#include "Server.hpp"

#include "Client.hpp"
#include "Commands.h"
#include "DiffieHellman.hpp"
#include "Logger.hpp"
#include "Room.hpp"
#include "Packet.hpp"

#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>


namespace Retchat {

    Server::Server(int p) : port(p) {
        rooms.emplace("lobby", "lobby");
    }

    Server::~Server() {
        stop();
        if (consoleThread.joinable()) consoleThread.join();
        close(listenFd);
    }

    void Server::stop() {
        running = false;
        // close listening socket to unblock accept
        if (listenFd != -1) {
            shutdown(listenFd, SHUT_RD);
            close(listenFd);
            listenFd = -1;
        }
        // disconnect all clients
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : clients) {
            disconnectClient(pair.second, true);
        }
        clients.clear();
        rooms.clear();
    }

    void Server::run() {
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) { perror("socket"); exit(1); }
        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
        if (listen(listenFd, 10) < 0) { perror("listen"); exit(1); }
        Logger::info("server listening on port " + std::to_string(port));

        // start console thread
        consoleThread = std::thread(&Server::consoleLoop, this);

        while (running) {
            struct sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int clientFd = accept(listenFd, (sockaddr*)&clientAddr, &addrLen);
            if (clientFd < 0) {
                if (!running) break;
                continue;
            }
            std::string ip = inet_ntoa(clientAddr.sin_addr);
            if (isIpBanned(ip)) {
                Logger::warn("Blocked banned IP: " + ip);
                close(clientFd);
                continue;
            }
            Client* client = new Client(clientFd, this, ip);
            {
                std::lock_guard<std::mutex> lock(mutex);
                clients[clientFd] = client;
            }
            client->start();
            Logger::info("new connection (fd=" + std::to_string(clientFd) + ", ip=" + ip + "): " + client->getName() + " joined " + client->getRoom());
        }
    }

    void Server::removeClient(Client* client) {
        int cfd = client->getSockfd();
        std::string cname = client->getName();
        std::lock_guard<std::mutex> lock(mutex);
        auto it = clients.find(cfd);
        if (it != clients.end()) {
            clients.erase(it);
        }
        getOrCreateRoom(client->getRoom()).removeClient(client);
        delete client;
        Logger::info(cname + "(" + std::to_string(cfd) + ") left.");
    }

    void Server::broadcastToRoom(const std::string& roomName, Client* exclude, const Packet& pkt) {
        getRoom(roomName).broadcast(pkt, exclude);
    }

    bool Server::isNicknameTaken(const std::string& nick, const std::string& roomName, Client* exclude) {
        auto& room = getRoom(roomName);
        for (const std::string& name : room.getUserNames()) {
            if (name == nick) {
                if (exclude && exclude->getName() == name) continue;
                return true;
            }
        }
        return false;
    }

    Room& Server::getOrCreateRoom(const std::string& name) {
        auto it = rooms.find(name);
        if (it == rooms.end()) {
            rooms.emplace(name, name);
            it = rooms.find(name);
        }
        return it->second;
    }

    Room& Server::getRoom(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex);
        return getOrCreateRoom(name);
    }



    // -------- CONSOLE --------

    void Server::consoleLoop() {
        std::string line;
        while (running) {
            std::cout << "> " << std::flush;
            if (!std::getline(std::cin, line)) {
                if (running) {
                    Logger::info("stopping server...");
                    stop();
                }
                break;
            }
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == "stop") {
                Logger::info("stopping server...");
                stop();
                break;
            } else if (cmd == "kick") {
                int fd;
                iss >> fd;
                if (!iss.fail() && fd > 0) {
                    kickClient(fd);
                    continue;
                }
            } else if (cmd == "ban") {
                std::string nick;
                iss >> nick;
                if (!nick.empty()) {
                    banNickname(nick);
                    continue;
                }
            } else if (cmd == "ipban") {
                std::string ip;
                iss >> ip;
                if (!ip.empty()) {
                    banIp(ip);
                    std::lock_guard<std::mutex> lock(mutex);
                    for (auto& pair : clients) {
                        if (pair.second->getIp() == ip) {
                            disconnectClient(pair.second, true);
                        }
                    }
                    continue;
                }
            } else if (cmd == "query") {
                std::string sub;
                iss >> sub;
                if (sub == "room") {
                    std::string roomName;
                    iss >> roomName;
                    if (!roomName.empty()) {
                        std::string result = queryRoom(roomName);
                        Logger::info(result);
                        continue;
                    }
                } else if (sub == "client") {
                    int fd;
                    iss >> fd;
                    if (!iss.fail()) {
                        std::string result = queryClient(fd);
                        Logger::info(result);
                        continue;
                    }
                }
            } else if (cmd == "list") {
                std::string sub;
                iss >> sub;
                if (sub == "rooms") {
                    std::string result = listRooms();
                    Logger::info(result);
                } else if (sub == "clients") {
                    std::string result = listClients();
                    Logger::info(result);
                }
                continue;
            } else if (cmd == "help") {
                listCommands();
                continue;
            } else {
                Logger::warn("unknown command. type \"help\" to see available commands.");
                printUsage(cmd);
            }
        }
    }
    void Server::disconnectClient(Client* client, bool sendPacket) {
        if (sendPacket) {
            DisconnectPacket dp;
            client->sendPacket(dp);
        }
        removeClient(client);
    }

    void Server::kickClient(int fd) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : clients) {
            if (pair.second->getSockfd() == fd) {
                Logger::info("Kicking client " + pair.second->getName() + " (fd=" + std::to_string(fd) + ")");
                disconnectClient(pair.second, true);
                return;
            }
        }
        Logger::warn("client with fd=" + std::to_string(fd) + " not found.");
    }

    void Server::banNickname(const std::string& nickname) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            bannedNicks.insert(nickname);
        }
        Logger::info("banned nickname: " + nickname);
    }

    void Server::banIp(const std::string& ip) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            bannedIps.insert(ip);
        }
        Logger::info("banned IP: " + ip);
    }

    bool Server::isIpBanned(const std::string& ip) const {
        std::lock_guard<std::mutex> lock(mutex);
        return bannedIps.find(ip) != bannedIps.end();
    }

    bool Server::isNicknameBanned(const std::string& nick) const {
        std::lock_guard<std::mutex> lock(mutex);
        return bannedNicks.find(nick) != bannedNicks.end();
    }

    std::string Server::listRooms() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::string result = "rooms: ";
        for (const auto& pair : rooms) {
            result += pair.first + " ";
        }
        return result;
    }

    std::string Server::listClients() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::string result = "connected clients:\n";
        for (const auto& pair : clients) {
            result += "  fd=" + std::to_string(pair.first) + " | name=" + pair.second->getName() + " | room=" + pair.second->getRoom() + " | ip=" + pair.second->getIp() + "\n";
        }
        return result;
    }

    std::string Server::queryRoom(const std::string& roomName) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = rooms.find(roomName);
        if (it == rooms.end()) {
            return "room not found: " + roomName;
        }
        auto users = it->second.getUsers();
        std::string result = "users in room '" + roomName + "': ";
        for (const auto& u : users) {
            result += u->getName() + "(" + std::to_string(u->getSockfd()) + ") ";
        }
        return result;
    }

    std::string Server::queryClient(int fd) const {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = clients.find(fd);
        if (it == clients.end()) {
            return "client with fd " + std::to_string(fd) + " not found.";
        }
        Client* c = it->second;
        return "fd=" + std::to_string(fd) + " | name=" + c->getName() + " | room=" + c->getRoom() + " | ip=" + c->getIp();
    }
}


// -------- MAIN ENTRYPOINT --------

int main(int argc, char* argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : Retchat::DEFAULT_PORT;
    Retchat::DH::init();
    Retchat::Server server(port);
    server.run();
    Retchat::DH::free();
    return 0;
}