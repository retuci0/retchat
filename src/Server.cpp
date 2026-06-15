#include "Server.hpp"

#include "Client.hpp"
#include "DiffieHellman.hpp"
#include "Logger.hpp"
#include "Room.hpp"

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


namespace Retchat {

    Server::Server(int p) : port(p) {
        rooms.emplace("lobby", "lobby");
    }

    Server::~Server() { close(listenFd); }

    void Server::run() {
        listenFd = socket(AF_INET, SOCK_STREAM, 0);
        if (listenFd < 0) { perror("socket"); exit(1); }
        int opt = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(listenFd, (sockaddr*) &addr, sizeof(addr)) < 0) { perror("bind"); exit(1); }
        if (listen(listenFd, 10) < 0) { perror("listen"); exit(1); }
        Logger::info("server listening on port " + std::to_string(port));

        while (running) {
            int clientFd = accept(listenFd, nullptr, nullptr);
            if (clientFd < 0) continue;
            Client* client = new Client(clientFd, this);
            {
                std::lock_guard<std::mutex> lock(mutex);
                clients[clientFd] = client;
            }
            client->start();
            Logger::info("new connection (fd=" + std::to_string(clientFd) + "): " + client->getName() + " joined " + client->getRoom());
        }
    }

    void Server::removeClient(Client* client) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = clients.find(client->getSockfd());
        if (it != clients.end()) clients.erase(it);
        getOrCreateRoom(client->getRoom()).removeClient(client);  // no double lock
        delete client;
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

}


int main(int argc, char* argv[]) {
    int port = (argc > 1) ? atoi(argv[1]) : Retchat::DEFAULT_PORT;
    Retchat::DH::init();
    Retchat::Server server(port);
    server.run();
    Retchat::DH::free();
    return 0;
}