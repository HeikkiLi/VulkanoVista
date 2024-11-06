#pragma once

#include <iostream>
#include <string>

class Logger {
public:
    static void info(const std::string& message) {
        log(message, "[INFO]");
    }

    static void warning(const std::string& message) {
        log(message, "[WARNING]");
    }

    static void error(const std::string& message) {
        log(message, "[ERROR]");
    }

    static void debug(const std::string& message) {
        log(message, "[DEBUG]");
    }

private:
    static void log(const std::string& message, const std::string& levelPrefix) {
        std::cout << levelPrefix << " " << message << std::endl;
    }
};
