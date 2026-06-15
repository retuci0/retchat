#pragma once

#include "Logger.hpp"
#include <array>
#include <string>


namespace Retchat {

    const std::string CMD_HELP  = "help";
    const std::string CMD_STOP  = "stop";
    const std::string CMD_KICK  = "kick";
    const std::string CMD_BAN   = "ban";
    const std::string CMD_IPBAN = "ipban";
    const std::string CMD_QUERY = "query";
    const std::string CMD_LIST  = "list";

    const std::array<std::string, 7> CMDS = { 
        CMD_HELP, CMD_STOP, CMD_KICK,
        CMD_BAN, CMD_IPBAN, CMD_QUERY,
        CMD_LIST
    };

    
    inline void printUsage(const std::string& cmd) {
        std::cout << cmd << ": ";
        if (cmd == "kick") {
            Logger::error("kicks a client. usage: kick <client fd>");
        } else if (cmd == "ban") {
            Logger::error("bans a nickname. usage: ban <nick>");
        } else if (cmd == "ipban") {
            Logger::error("bans an IP address. usage: ipban <ip>");
        } else if (cmd == "query") {
            Logger::error("queries a client or a room. usage: query [room|client] <room|clientfd>");
        } else if (cmd == "list") {
            Logger::error("lists all rooms or clients. usage: list [rooms|clients]");
        } else {
            Logger::error("unknown command: \"" + cmd + "\"");
        }
    }

    inline void listCommands() {
        for (auto& cmd : CMDS) {
            printUsage(cmd);
        }
    }

}
