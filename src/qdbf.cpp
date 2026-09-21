#include "vShell.hpp"
#include "vSqlShellEngine.hpp"
#include "vNetworkServer.hpp"

#include "ConsoleManager.hpp"
#include "vApp.hpp"
#include "SqlQueryParser.hpp"
//#include "dbfConnection.hpp"
//#include "csvConnection.hpp"
//#include "odbcConnection.hpp"
#include "stringUtils.hpp"
#include "ConfigLoader.hpp"

#include<iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>

#include <mutex>

// Aici instanțiem obiectul pe care vNetworkServer.hpp îl vede ca "extern"
std::mutex engineMutex;

class myApp : public vApp {
    int port;
    std::wstring path;
    std::wstring users_file;
public:
    myApp(HINSTANCE hInstance, RunMode rm, int srv_port, std::wstring srv_path, std::wstring usr_file) : 
        vApp(hInstance), 
        port(srv_port),
        path(srv_path),
        users_file(usr_file)
        { setRunMode(rm); };
    ~myApp() {};

    bool initService() override {
        dbfConnection shEngine("DBF_NATIVE", path);

        vNetworkServer server;
        server.loadUsers(users_file);
            LOG_INFO(L"Starting application in SERVER mode...");
        if (server.init(port)) { // Ascultăm pe portul 8080
            // 3. Rulăm serverul. Acesta va bloca firul de execuție 
            // și va procesa cererile venite prin rețea
            server.run(shEngine);
        }
        else {
            LOG_FATAL(L"Could not initialize the server on port: " + to_wstring<int>(port));
            return false;
        }
        return true;
    }

};


int main(int argc, char* argv[]) {
    
    std::string configPath = "./qdbf.cfg";
    if (argc > 1) {
        configPath = argv[1];
    }

    ConfigLoader loader;

    // Încercăm să încărcăm fișierul. Dacă nu există, folosim valori default.
    if (loader.load("./qdbf.cfg")) {
        std::cout << "[CONFIG] Configuration file loaded successfully.\n";
    }
    else {
        std::cout << "[CONFIG] Warning: qdbf.cfg was not found. Using default values.\n";
    }


    // Extragere parametrii cu valori fallback (default)
    // 1. Portul (il citim ca wstring si il convertim la int)
    std::wstring portStr = loader.get(L"qdbf_port", L"3519");
    int srv_port = std::stoi(portStr);

    // 2. Calea catre DBF-uri
    std::wstring srv_path = loader.get(L"dbf_files_path", L"./dbfs");

    // 3. Fisierul de useri
    std::wstring usr_file = loader.get(L"users_file", L"qdbf_usrs.pwd");

    // Initializare aplicatie cu parametrii din config
    myApp app(NULL, RunMode::SERVICE, srv_port, srv_path, usr_file);

    app.startConsole();
    return app.run();
    
}