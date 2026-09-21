//#include "FontManager.hpp"
#include "vApp.hpp"
#include "dbConnection.hpp"
#include <filesystem>
#ifndef _WIN32
#include <unistd.h>
#endif


// Inițializează pointerul static al instanței în afara clasei.
vApp* vApp::s_instance = nullptr;

// --- Constructor ---
vApp::vApp(HINSTANCE hInstance, RunMode mode)
    : m_instance(hInstance) {
    s_instance = this;
    m_runMode = mode;
}

// --- Metoda Run ---
int vApp::run(int nCmdShow) {
    if (!init()) {
        ConsoleManager::getInstance().log(L"[ERROR] Inițializarea aplicației a eșuat.");
        return -1;
    }
    else if (m_runMode == RunMode::CONSOLE) {
        //LOG_INFO(L"[vApp::run] Rulează în modul consolă...");
       
        return 0;
    }

    else if (m_runMode == RunMode::SERVICE) {
        //LOG_INFO(L"[vApp::run] Rulează în modul service...");
        return 0;
    }

    return -1;
}

bool vApp::init() {
    // Inițializare comună (FontManager, ConsoleManager etc.)
    
    // ... Alte inițializări comune ...
    

    // Ramificarea logicii
    switch (m_runMode) {
    case RunMode::CONSOLE:
        return initConsole(); // Apează inițializarea Console
    case RunMode::SERVICE:
        return initService(); // Apează inițializarea Service
    default:
        ConsoleManager::getInstance().log(L"[ERROR] Mod de rulare necunoscut.");
        return false;
    }
}


// --- Metoda Shutdown ---
void vApp::shutdown() {
    ConsoleManager::getInstance().log(L"[vApp::shutdown] Se inițiază procedura de închidere...");
    ConsoleManager::getInstance().clearExtraOutputs();
    ConsoleManager::getInstance().log(L"[vApp::shutdown] Resurse eliberate.");
    ConsoleManager::getInstance().shutdown();
    exit(0);
}


void vApp::startConsole() {
   ConsoleManager::getInstance().initialize(); 
   ConsoleManager::getInstance().setColor(FOREGROUND_GREEN);
   //ConsoleManager::getInstance().log(L"Consola inițializată! [AppInit] Începe inițializarea aplicației...");
   ConsoleManager::getInstance().resetColor();
   
}

std::wstring vApp::getAppPath() const {
#ifdef _WIN32
    wchar_t buffer[MAX_PATH] = { 0 };
    GetModuleFileNameW(m_instance, buffer, MAX_PATH);

    std::wstring path(buffer);
    size_t pos = path.find_last_of(L"\\/");
    if (pos != std::wstring::npos) {
        return path.substr(0, pos + 1); // Include separatorul final (\)
    }
    return L"";
#else
    std::string buffer(4096, '\0');
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) return L"";
    buffer.resize(static_cast<size_t>(length));
    return std::filesystem::path(buffer).parent_path().wstring() + L"/";
#endif
}

std::string vApp::getAppPathA() const {
#ifdef _WIN32
    char buffer[MAX_PATH] = { 0 };
    GetModuleFileNameA(m_instance, buffer, MAX_PATH);

    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        return path.substr(0, pos + 1); // Include separatorul final (\)
    }
    return "";
#else
    return std::filesystem::path(getAppPath()).string();
#endif
}

std::wstring vApp::getAppSubPath(const std::wstring& relativePath) const {
    std::wstring basePath = getAppPath();

    // Eliminăm eventualele caractere '/' sau '\' de la începutul căii relative
    std::wstring rel = relativePath;
    if (!rel.empty() && (rel[0] == L'/' || rel[0] == L'\\')) {
        rel = rel.substr(1);
    }

    return basePath + rel;
}

std::string vApp::getAppSubPathA(const std::string& relativePath) const {
    std::string basePath = getAppPathA();

    std::string rel = relativePath;
    if (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) {
        rel = rel.substr(1);
    }

    return basePath + rel;
}