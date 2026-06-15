#pragma once

#include "Packet.hpp"

#include <mutex>
#include <string>
#include <vector>


namespace Retchat {

    class Client;

    class Room {
    public:
        Room(const std::string& name);
        void addClient(Client* client);
        void removeClient(Client* client);
        void broadcast(const Packet& pkt, Client* exclude);
        std::vector<Client*> getUsers() const;
        std::vector<std::string> getUserNames() const;
        const std::string& getName() const { return name; }
        bool hasClient(Client* client) const;

    private:
        std::string name;
        std::vector<Client*> clients;
        mutable std::mutex mutex;
    };

}