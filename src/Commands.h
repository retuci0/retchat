#pragma once

#include "Logger.hpp"
#include <array>
#include <string>


namespace Retchat {

    const std::string CMD_HELP    = "help";
    const std::string CMD_STOP    = "stop";
    const std::string CMD_KICK    = "kick";
    const std::string CMD_BAN     = "ban";
    const std::string CMD_IPBAN   = "ipban";
    const std::string CMD_UNBAN   = "unban";
    const std::string CMD_UNBANIP = "unbanip";
    const std::string CMD_QUERY   = "query";
    const std::string CMD_LIST    = "list";

    const std::array<std::string, 9> CMDS = {
        CMD_HELP, CMD_STOP, CMD_KICK,
        CMD_BAN, CMD_IPBAN, CMD_UNBAN, CMD_UNBANIP,
        CMD_QUERY, CMD_LIST
    };


    inline void printUsage(const std::string& cmd) {
        if (cmd == CMD_KICK) {
            Logger::info("kick <fd|nick>: kick a client by fd or nickname");
        } else if (cmd == CMD_BAN) {
            Logger::info("ban <nick>: ban a nickname (persisted to bans.txt)");
        } else if (cmd == CMD_IPBAN) {
            Logger::info("ipban <ip>: ban an IP address (persisted to bans.txt)");
        } else if (cmd == CMD_UNBAN) {
            Logger::info("unban <nick>: remove a nickname ban");
        } else if (cmd == CMD_UNBANIP) {
            Logger::info("unbanip <ip>: remove an IP ban");
        } else if (cmd == CMD_QUERY) {
            Logger::info("query room <name>: info about a room");
            Logger::info("query client <fd>: info about a client");
        } else if (cmd == CMD_LIST) {
            Logger::info("list rooms: list all rooms");
            Logger::info("list clients: list all connected clients");
            Logger::info("list bans: list all active bans");
        } else if (cmd == CMD_STOP) {
            Logger::info("stop: shut down the server");
        } else if (cmd == CMD_HELP) {
            Logger::info("help: show this help");
        } else {
            Logger::warn("unknown command: \"" + cmd + "\"");
        }
    }

    inline void listCommands() {
        for (auto& cmd : CMDS) printUsage(cmd);
    }

}