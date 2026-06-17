#include "Server.hpp"

#include "Client.hpp"
#include "Commands.h"
#include "DiffieHellman.hpp"
#include "Logger.hpp"
#include "Room.hpp"
#include "Packet.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>


namespace Retchat {

    Server::Server(int p, const std::string& bansFile) : port(p), bansFilePath(bansFile) {
        rooms.emplace("lobby", "lobby");
        if (!bansFilePath.empty()) loadBans(bansFilePath);
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
                Logger::warn("blocked banned IP: " + ip);
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

    void Server::sendImageDm(Client* from, const std::string& targetNick, const ImagePacket& imgPkt) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : clients) {
            if (pair.second->getName() == targetNick) {
                pair.second->sendPacket(imgPkt);
                return;
            }
        }
        SystemPacket err;
        err.isError = true;
        err.code = MSG_DM_TARGET_NOT_FOUND;
        err.params = { targetNick };
        from->sendPacket(err);
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
                if (running) { Logger::info("EOF — stopping server..."); stop(); }
                break;
            }
            if (line.empty()) continue;
            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            if (cmd == CMD_STOP) {
                Logger::info("stopping server...");
                stop();
                break;

            } else if (cmd == CMD_KICK) {
                std::string arg;
                iss >> arg;
                if (arg.empty()) { printUsage(cmd); continue; }
                bool isNum = !arg.empty() && std::all_of(arg.begin(), arg.end(), ::isdigit);
                if (isNum) {
                    kickClient(std::stoi(arg));
                } else {
                    std::lock_guard<std::mutex> lock(mutex);
                    bool found = false;
                    for (auto& pair : clients) {
                        if (pair.second->getName() == arg) {
                            Logger::info("kicking " + arg);
                            KickPacket kp; kp.reason = "pa tu casa";
                            pair.second->sendPacket(kp);
                            disconnectClient(pair.second, false);
                            found = true;
                            break;
                        }
                    }
                    if (!found) Logger::warn("user \"" + arg + "\" not found.");
                }

            } else if (cmd == CMD_BAN) {
                std::string nick; iss >> nick;
                if (nick.empty()) { printUsage(cmd); continue; }
                banNickname(nick);

            } else if (cmd == CMD_IPBAN) {
                std::string ip; iss >> ip;
                if (ip.empty()) { printUsage(cmd); continue; }
                banIp(ip);

            } else if (cmd == CMD_UNBAN) {
                std::string nick; iss >> nick;
                if (nick.empty()) { printUsage(cmd); continue; }
                unbanNickname(nick);

            } else if (cmd == CMD_UNBANIP) {
                std::string ip; iss >> ip;
                if (ip.empty()) { printUsage(cmd); continue; }
                unbanIp(ip);

            } else if (cmd == CMD_QUERY) {
                std::string sub; iss >> sub;
                if (sub == "room") {
                    std::string roomName; iss >> roomName;
                    if (!roomName.empty()) Logger::info(queryRoom(roomName));
                    else printUsage(cmd);
                } else if (sub == "client") {
                    int fd; iss >> fd;
                    if (!iss.fail()) Logger::info(queryClient(fd));
                    else printUsage(cmd);
                } else { printUsage(cmd); }

            } else if (cmd == CMD_LIST) {
                std::string sub; iss >> sub;
                if (sub == "rooms")   Logger::info(listRooms());
                else if (sub == "clients") Logger::info(listClients());
                else if (sub == "bans")    Logger::info(listBans());
                else printUsage(cmd);

            } else if (cmd == CMD_HELP) {
                listCommands();

            } else {
                Logger::warn("unknown command. type \"help\" for commands.");
                printUsage(cmd);
            }
        }
    }

    void Server::disconnectClient(Client* client, bool sendPacket) {
        if (sendPacket) {
            DisconnectPacket dp;
            client->sendPacket(dp);
        }
        shutdown(client->getSockfd(), SHUT_RDWR);
        close(client->getSockfd());
    }

    void Server::kickClient(int fd, const std::string& reason) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : clients) {
            if (pair.second->getSockfd() == fd) {
                Logger::info("kicking " + pair.second->getName() + " (fd=" + std::to_string(fd) + "): " + reason);
                KickPacket kp;
                kp.reason = reason;
                pair.second->sendPacket(kp);
                disconnectClient(pair.second, false);
                return;
            }
        }
        Logger::warn("client with fd=" + std::to_string(fd) + " not found.");
    }

    void Server::banNickname(const std::string& nickname, const std::string& reason) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            bannedNicks.insert(nickname);
            // kick any currently connected client with that nick
            for (auto& pair : clients) {
                if (pair.second->getName() == nickname) {
                    Logger::info("banning and kicking " + nickname);
                    BanPacket bp;
                    bp.reason = reason;
                    pair.second->sendPacket(bp);
                    disconnectClient(pair.second, false);
                }
            }
        }
        Logger::info("banned nickname: " + nickname);
        if (!bansFilePath.empty()) saveBans(bansFilePath);
    }

    void Server::banIp(const std::string& ip, const std::string& reason) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            bannedIps.insert(ip);
            for (auto& pair : clients) {
                if (pair.second->getIp() == ip) {
                    Logger::info("banning and kicking IP " + ip);
                    BanPacket bp;
                    bp.reason = reason;
                    pair.second->sendPacket(bp);
                    disconnectClient(pair.second, false);
                }
            }
        }
        Logger::info("banned IP: " + ip);
        if (!bansFilePath.empty()) saveBans(bansFilePath);
    }

    void Server::unbanNickname(const std::string& nick) {
        std::lock_guard<std::mutex> lock(mutex);
        bannedNicks.erase(nick);
        Logger::info("unbanned nickname: " + nick);
        if (!bansFilePath.empty()) saveBans(bansFilePath);
    }

    void Server::unbanIp(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex);
        bannedIps.erase(ip);
        Logger::info("unbanned IP: " + ip);
        if (!bansFilePath.empty()) saveBans(bansFilePath);
    }

    bool Server::isIpBanned(const std::string& ip) const {
        std::lock_guard<std::mutex> lock(mutex);
        return bannedIps.find(ip) != bannedIps.end();
    }

    bool Server::isNicknameBanned(const std::string& nick) const {
        std::lock_guard<std::mutex> lock(mutex);
        return bannedNicks.find(nick) != bannedNicks.end();
    }

    std::string Server::listBans() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::string result = "banned nicks: ";
        for (const auto& n : bannedNicks) result += n + " ";
        result += "\nbanned IPs: ";
        for (const auto& ip : bannedIps) result += ip + " ";
        return result;
    }

    void Server::sendDm(Client* from, const std::string& targetNick, const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex);
        for (auto& pair : clients) {
            if (pair.second->getName() == targetNick) {
                DmMsgPacket dm;
                dm.senderNick = from->getName();
                dm.text = text;
                pair.second->sendPacket(dm);
                return;
            }
        }
        // target not found
        SystemPacket err;
        err.isError = true;
        err.code = MSG_DM_TARGET_NOT_FOUND;
        err.params = { targetNick };
        from->sendPacket(err);
    }

    void Server::loadBans(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            if (line.substr(0, 5) == "nick:") {
                bannedNicks.insert(line.substr(5));
            } else if (line.substr(0, 3) == "ip:") {
                bannedIps.insert(line.substr(3));
            }
        }
        Logger::info("loaded bans from " + path +
                     " (" + std::to_string(bannedNicks.size()) + " nicks, " +
                     std::to_string(bannedIps.size()) + " IPs)");
    }

    void Server::saveBans(const std::string& path) const {
        std::ofstream f(path, std::ios::trunc);
        if (!f.is_open()) { Logger::warn("could not write bans to " + path); return; }
        for (const auto& n : bannedNicks) f << "nick:" << n << "\n";
        for (const auto& ip : bannedIps)  f << "ip:"   << ip  << "\n";
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

int main(int argc, char** argv) {
    int port = (argc > 1) ? atoi(argv[1]) : Retchat::DEFAULT_PORT;
    std::string bansFile = (argc > 2) ? argv[2] : Retchat::DEFAULT_BANS_FILE;
    std::signal(SIGPIPE, SIG_IGN);
    Retchat::DH::init();
    Retchat::Server server(port, bansFile);
    server.run();
    Retchat::DH::free();
    return 0;
}