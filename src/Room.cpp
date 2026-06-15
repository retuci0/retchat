#include "Room.hpp"

#include "Client.hpp"
#include "Logger.hpp"

#include <algorithm>


namespace Retchat {

    Room::Room(const std::string& n) : name(n) {}

    void Room::addClient(Client* client) {
        std::lock_guard<std::mutex> lock(mutex);
        if (std::find(clients.begin(), clients.end(), client) == clients.end()) {
            clients.push_back(client);
            Logger::info(client->getName() + " joined room " + getName());
        }
    }

    void Room::removeClient(Client* client) {
        std::lock_guard<std::mutex> lock(mutex);
        clients.erase(std::remove(clients.begin(), clients.end(), client), clients.end());
        Logger::info(client->getName() + " left room " + getName());
    }

    void Room::broadcast(const Packet& pkt, Client* exclude) {
        std::lock_guard<std::mutex> lock(mutex);
        for (Client* c : clients) {
            if (c != exclude) c->sendPacket(pkt);
        }
    }

    std::vector<std::string> Room::getUserNames() const {
        std::lock_guard<std::mutex> lock(mutex);
        std::vector<std::string> names;
        for (Client* c : clients) names.push_back(c->getName());
        return names;
    }

    bool Room::hasClient(Client* client) const {
        std::lock_guard<std::mutex> lock(mutex);
        return std::find(clients.begin(), clients.end(), client) != clients.end();
    }

}