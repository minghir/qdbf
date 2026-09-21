#include "ConsoleManager.hpp"
#include "stringUtils.hpp"
#include <fcntl.h>
#include <io.h>
#include <codecvt>
#include <locale>
#include <filesystem>
#include <sstream>


void ConsoleManager::initialize() {
    AllocConsole();
    SetConsoleOutputCP(CP_UTF8);

    FILE* stream;
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
    freopen_s(&stream, "CONIN$", "r", stdin);

    std::ios::sync_with_stdio(true);
    _setmode(_fileno(stdout), _O_U8TEXT);

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
            SetConsoleMode(hOut, dwMode);
        }
    }

    std::cout.clear();
    std::cerr.clear();
}

void ConsoleManager::setColor(WORD color) {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void ConsoleManager::resetColor() {
    setColor(FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

void ConsoleManager::log(const std::wstring& message, LogLevel level) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);

    // 1. GARDA Numarul 1: Dacă logarea este suspendată general
    if (m_isSuspended) return;

    // --- MODIFICARE 3: FILTRAREA DUPĂ NIVEL ---
    // Dacă mesajul are un nivel mai mic decât nivelul permis curent, îl ignorăm
    if (static_cast<int>(level) < static_cast<int>(m_currentLogLevel)) {
        return;
    }

    if (std::wcout.fail()) {
        std::wcout.clear();
    }

    std::wstring prefix = L"[LOG]";
    WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

    switch (level) {
    case LogLevel::INFO:
        prefix = L"[INFO]";
        break;

    case LogLevel::SUCCESS:
        prefix = L"[SUCCESS]";
        color = FOREGROUND_GREEN;
        break;

    case LogLevel::WARNING:
        prefix = L"[WARNING]";
        color = FOREGROUND_RED | FOREGROUND_GREEN; // Galben
        break;

    case LogLevel::LOG_ERROR:
        prefix = L"[ERROR]";
        color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;

    case LogLevel::FATAL_ERROR:
        prefix = L"[FATAL_ERROR]";
        color = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;

    case LogLevel::DEBUG:
        prefix = L"[DEBUG]";
        color = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    }

    std::wstring timestamp = getTimestamp();

    // Afișare consolă
    setColor(color);
    std::wcout << prefix << L" " << message << std::endl;
    resetColor();

    // Scrierea în fișier (dacă este activat)
    if (logToFileEnabled && !fileLoggingMuted && logFile.is_open()) {
        std::wstring fullWideMessage = L"[" + timestamp + L"] " + prefix + L" " + message;
        std::string utf8Message = utf8_encode(fullWideMessage);
        logFile << utf8Message << std::endl;
        logFile.flush();
    }

    // Trimitere către alte output-uri (UI, ferestre etc.)
    for (size_t i = 0; i < m_extraOutputs.size(); ++i) {
        if (m_extraOutputs[i]) {
            m_extraOutputs[i]->writeLog(message, level);
        }
    }
}

void ConsoleManager::logTest() {
    std::wcout << L"[TEST] Verificare diacritice în consolă: ș ț ă â î" << std::endl;
    std::wcout << L"[TEST] Această linie ar trebui să apară albă." << std::endl;
    setColor(FOREGROUND_GREEN);
    std::wcout << L"[TEST] Această linie ar trebui să apară verde." << std::endl;
    setColor(FOREGROUND_RED);
    std::wcout << L"[TEST] Această linie ar trebui să apară roșie." << std::endl;
    setColor(FOREGROUND_BLUE);
    std::wcout << L"[TEST] Această linie ar trebui să apară albastră." << std::endl;
    resetColor();
    std::wcout << L"[TEST] Culoarea a fost resetată la alb." << std::endl;
}

void ConsoleManager::shutdown() {
    log(L"Consola a fost închisă.");
}

void ConsoleManager::writeRaw(const std::wstring& message, WORD color) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);

    if (m_isSuspended) return;

    if (color != 0) setColor(color);
    std::wcout << message << std::endl;
    if (color != 0) resetColor();

    if (logToFileEnabled && !fileLoggingMuted && logFile.is_open()) {
        std::string utf8Message = utf8_encode(message);
        logFile << utf8Message << std::endl;
        logFile.flush();
    }

    for (size_t i = 0; i < m_extraOutputs.size(); ++i) {
        if (m_extraOutputs[i]) {
            m_extraOutputs[i]->writeLog(message, LogLevel::INFO);
        }
    }
}

void ConsoleManager::clear() {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    std::wcout << L"\033[2J\033[H";
    std::wcout.flush();
}

bool ConsoleManager::enableFileLogging(const std::wstring& filePath) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);

    if (logFile.is_open()) closeLogFile();

    logFile.open(std::filesystem::path(filePath), std::ios::out | std::ios::app);
    if (logFile.is_open() && logFile.tellp() == 0) {
        logFile << "\xEF\xBB\xBF"; // UTF-8 BOM
    }

    if (logFile.is_open()) {
        logToFileEnabled = true;
        log(L"Logarea în fișier a fost activată (UTF-8)", LogLevel::SUCCESS);
        return true;
    }
    return false;
}

void ConsoleManager::closeLogFile() {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    if (logFile.is_open()) {
        logFile.close();
        logToFileEnabled = false;
    }
}

std::wstring ConsoleManager::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
    localtime_s(&bt, &in_time_t);

    std::wstringstream ss;
    ss << std::put_time(&bt, L"%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void ConsoleManager::addOutput(ILogOutput* output) {
    std::lock_guard<std::recursive_mutex> lock(mtxLog);
    if (output) {
        m_extraOutputs.push_back(output);
    }
}