#ifndef CONSOLE_MANAGER_HPP
#define CONSOLE_MANAGER_HPP

#pragma once


#include <iostream>  // Pentru std::wcout, std::endl
#include <fstream>   // Pentru std::ofstream
#include <string>    // Pentru std::wstring
#include <vector>
#include <mutex>     // Pentru std::recursive_mutex
#include <chrono>
#include <iomanip>
#include <algorithm>

#include "platform.hpp"

enum class LogLevel {
    DEBUG = 0,      // Mesaje detaliate pentru dezvoltare (cel mai mic nivel)
    INFO = 1,       // Informații generale (default)
    SUCCESS = 2,    // Operație reușită
    WARNING = 3,    // Avertisment
    LOG_ERROR = 4,  // Eroare non-fatală
    FATAL_ERROR = 5 // Eroare critică (cel mai înalt nivel)
};

class ILogOutput {
public:
    virtual ~ILogOutput() = default;
    virtual void writeLog(const std::wstring& message, LogLevel level) = 0;
};

#define LOG_INFO(msg)         ConsoleManager::getInstance().log((msg), LogLevel::INFO)
#define LOG_SUCCESS(msg)      ConsoleManager::getInstance().log((msg), LogLevel::SUCCESS)
#define LOG_WARNING(msg)      ConsoleManager::getInstance().log((msg), LogLevel::WARNING)
#define LOG_ERROR(msg)        ConsoleManager::getInstance().log((msg), LogLevel::LOG_ERROR)
#define LOG_FATAL(msg)        ConsoleManager::getInstance().log((msg), LogLevel::FATAL_ERROR)
#define LOG_DEBUG(msg)        ConsoleManager::getInstance().log((msg), LogLevel::DEBUG)
#define LOG(msg)              ConsoleManager::getInstance().log((msg), LogLevel::INFO) 
#define LOG_RAW(msg)          ConsoleManager::getInstance().writeRaw((msg)) 

class ConsoleManager {
private:
    std::recursive_mutex mtxLog;

    ConsoleManager() = default;
    ConsoleManager(const ConsoleManager&) = delete;
    ConsoleManager& operator=(const ConsoleManager&) = delete;

    ~ConsoleManager() { closeLogFile(); }

    std::ofstream logFile;
    bool logToFileEnabled = false;
    bool fileLoggingMuted = false;
    bool m_isSuspended = false;

    // --- MODIFICARE 1: Nivelul curent de logare ---
    LogLevel m_currentLogLevel = LogLevel::DEBUG;

    std::vector<ILogOutput*> m_extraOutputs;

public:
    static ConsoleManager& getInstance() {
        static ConsoleManager instance;
        return instance;
    }

    void initialize();
    void setColor(WORD color);
    void resetColor();
    void log(const std::wstring& message, LogLevel level = LogLevel::INFO);
    void logTest();
    void shutdown();
    void clear();
    void writeRaw(const std::wstring& message, WORD color = 0);

    bool enableFileLogging(const std::wstring& filePath);
    void closeLogFile();
    std::wstring getTimestamp();

    // --- MODIFICARE 2: Metode de setat/obținut nivelul de logare ---
    void setLogLevel(LogLevel level) {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
        m_currentLogLevel = level;
    }

    LogLevel getLogLevel() const {
        return m_currentLogLevel;
    }

    void suspendLogging() { m_isSuspended = true; }
    void resumeLogging() { m_isSuspended = false; }
    bool isSuspended() const { return m_isSuspended; }

    void suspendFileLogging() { fileLoggingMuted = true; }
    void resumeFileLogging() { fileLoggingMuted = false; }

    void addOutput(ILogOutput* output);
    void removeExtraOutput(ILogOutput* output) {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
        m_extraOutputs.erase(
            std::remove(m_extraOutputs.begin(), m_extraOutputs.end(), output),
            m_extraOutputs.end()
        );
    }
    void clearExtraOutputs() {
        std::lock_guard<std::recursive_mutex> lock(mtxLog);
        m_extraOutputs.clear();
    }
};

#endif // CONSOLE_MANAGER_HPP