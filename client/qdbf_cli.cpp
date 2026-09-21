
#include "qdbfConnection.hpp"


#include "../src/vShell.hpp"
#include "../src/vSqlShellEngine.hpp"
#include "../src/ConsoleManager.hpp"
#include "../src/vApp.hpp"
#include "../src/SqlQueryParser.hpp"
#include "../src/stringUtils.hpp"
#include "../src/ConfigLoader.hpp"


#include<iostream>
#include <sstream>
#include <filesystem>
#include <string>
#include <chrono>
#include <iomanip>


#include <conio.h> // Pentru _getch()

std::wstring getPasswordMasked() {
    std::wstring pass;
    wchar_t ch;
    while ((ch = _getwch()) != L'\r') { // Până la Enter
        if (ch == L'\b') { // Backspace
            if (!pass.empty()) {
                pass.pop_back();
                std::wcout << L"\b \b";
            }
        }
        else {
            pass.push_back(ch);
            std::wcout << L'*';
        }
    }
    std::wcout << L"\n";
    return pass;
}

class QdbfClient : public vSqlShellEngine {

public:
   

    QdbfClient(const std::wstring& initialConnStr)
        : vSqlShellEngine(std::make_unique<qdbfConnection>(initialConnStr, 3519)) {

        //m_handlers[L"/shutdown"] = [this](auto& args) { return handleShutdownRemote(args); };
        //m_handlers[L"/sessions"] = [this](auto& args) { return handleSessionsRemote(args); };

        // Încercăm conectarea imediat după creare
        if (!con->openDatabase()) {
            LOG_ERROR(L"Autentificare initiala esuata: " + con->getError());
            LOG_INFO(L"Puteti incerca din nou folosind: /connect user:pass@IP");
        }
        else {
            LOG_SUCCESS(L"Sesiune pornita pentru: " + con->getConnectionDSN());
        }
    }


    bool handleConnect(const ShellCommand& cmd) override {
        // Utilizare: /connect admin:123@10.9.50.241 3519
        if (cmd.args.empty()) {
            LOG_ERROR(L"Utilizare: /connect user:pass@IP [Port]");
            return false;
        }

        std::wstring connStr = cmd.args[0];
        int port = 3519; // Portul tău default

        if (cmd.args.size() > 1) {
            try {
                port = std::stoi(cmd.args[1]);
            }
            catch (...) {
                LOG_ERROR(L"Port invalid. Se foloseste 3519.");
            }
        }

        // 1. Salvăm conexiunea actuală pentru fallback
        auto oldCon = std::move(con);

        // 2. Creăm noua conexiune (parsing-ul se face în constructor)
        con = std::make_unique<qdbfConnection>(connStr, port);

        // 3. Încercăm deschiderea (Handshake-ul de login are loc aici)
        if (con->openDatabase()) {
            // Folosim getConnectionDSN() pentru a afișa IP-ul extras din URI
            LOG_SUCCESS(L"Conectat cu succes la: " + con->getConnectionDSN() + L":" + std::to_wstring(port));
            return true;
        }
        else {
            LOG_ERROR(L"Eroare conectare: " + con->getError());

            // 4. Restaurăm vechea conexiune dacă noua a eșuat
            con = std::move(oldCon);

            if (con) {
                LOG_INFO(L"S-a revenit la conexiunea anterioară: " + con->getConnectionDSN());
            }
            return false;
        }
    }
    
    std::wstring getPrompt() const override {
        return m_accumulator.empty() ? L"\nqdbf# " : L"  -> ";
    }
};

class myApp : public vApp {
public:
    myApp(HINSTANCE hInstance, RunMode rm) :vApp(hInstance) { setRunMode(rm); };
    ~myApp() {};

    bool initConsole() override {
        // 1. Cerem datele de logare de la utilizator
        std::wstring user, pass, ip;

        // Curățăm ecranul pentru un aspect profesional
        ConsoleManager::getInstance().writeRaw(L"--- QDBF Network Login ---\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        std::wcout << L"Utilizator: ";
        std::wcin >> user;
        std::wcout << L"Parola: ";
        // Notă: std::wcin nu ascunde caracterele. 
        // Pentru "stele" ar fi nevoie de funcții specifice Windows (GetConsoleMode), 
        // dar pentru moment mergem pe varianta simplă.
        //std::wcin >> pass;

        pass = getPasswordMasked();

        std::wcout << L"IP Server [default 127.0.0.1]: ";
        std::wcin.ignore(); // curățăm buffer-ul
        std::getline(std::wcin, ip);
        if (ip.empty()) ip = L"127.0.0.1";

        // 2. Construim string-ul pentru constructorul QdbfClient
        std::wstring connectionUri = user + L":" + pass + L"@" + ip;

        // 3. Inițializăm clientul cu aceste date
        QdbfClient shClient(connectionUri);

        // 4. Pornim Shell-ul
        vShell shell(shClient);
        shell.run();
        return true;
    }

};


int main(int argc, char* argv[]) {
    myApp app(NULL, RunMode::CONSOLE);
    app.startConsole();
    return app.run();
}