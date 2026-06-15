#pragma once

#include <ctime>
#include <iostream>
#include <ostream>
#include <string>


namespace Color {
    enum Code {
        FG_RED      = 31,
        FG_GREEN    = 32,
        FG_YELLOW   = 33,
        FG_BLUE     = 34,
        FG_DEFAULT  = 39,
        BG_RED      = 41,
        BG_GREEN    = 42,
        BG_YELLOW   = 43,
        BG_BLUE     = 44,
        BG_DEFAULT  = 49
    };
    class Modifier {
        Code code;
    public:
        Modifier(Code pCode) : code(pCode) {}
        friend std::ostream&
        operator<<(std::ostream& os, const Modifier& mod) {
            return os << "\033[" << mod.code << "m";
        }
    };
}

namespace Logger {

    inline const std::string getTime() {
        time_t now = time(0);
        struct tm tstruct;
        char buf[80];
        tstruct = *localtime(&now);
        strftime(buf, sizeof(buf), "%X", &tstruct);
        return buf;
    }

    inline void info(const std::string& msg) {
        std::cout << Color::Modifier(Color::FG_GREEN) 
                  << "[INFO " << getTime() << "] " << msg 
                  << Color::Modifier(Color::FG_DEFAULT) 
                  << std::endl;
    }

    inline void warn(const std::string& msg) {
        std::cout << Color::Modifier(Color::FG_YELLOW)
                  << "[WARN " << getTime() << "] " << msg 
                  << Color::Modifier(Color::FG_DEFAULT)
                  << std::endl;
    }

    inline void error(const std::string& msg) {
        std::cout << Color::Modifier(Color::FG_RED)
                  << "[ERROR " << getTime() << "] " << msg 
                  << Color::Modifier(Color::FG_DEFAULT)
                  << std::endl;
    }
}