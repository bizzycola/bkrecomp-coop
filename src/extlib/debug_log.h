// debug_log.h
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <fstream>
#include <string>

inline void debug_log(const std::string& msg) {
    static std::ofstream logFile("bkrecomp_coop_debug.log", std::ios::app);
    if (logFile.is_open()) {
        logFile << msg << std::endl;
        logFile.flush();
    }
}

inline void debug_log_clear() {
    std::ofstream logFile("bkrecomp_coop_debug.log", std::ios::trunc);
}

#endif