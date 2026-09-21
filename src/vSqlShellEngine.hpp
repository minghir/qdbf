    #ifndef VSQLSHELLENGINE_HPP
    #define VSQLSHELLENGINE_HPP
    #pragma once

    #include <iostream>
    #include <sstream>
    #include <iomanip>
    #include <string>
    #include <vector>
    #include <cwctype>
    #include <cctype>
    #include <map>
    #include <functional>
    #include <variant>

    #pragma once

    #include "IShellEngine.hpp"
    #include "dbConnection.hpp"
    #include "stringUtils.hpp"
    #include "ConsoleManager.hpp"
    #include "vShellCommandParser.hpp"

std::wstring wformat_pretty_table(const vConTable& table);

    class vSqlShellEngine : public IShellEngine {
    protected:
        std::wstring m_accumulator;
        bool m_running = true;
    
        std::vector<std::wstring> m_history;

        std::unique_ptr<dbConnection> con;

        using ShellHandler = std::function<bool(const ShellCommand&)>;
        std::map<std::wstring, ShellHandler> m_handlers;


        vConResult result;

        // Metode Virtuale pe care le poți suprascrie în QdbfEngine
        virtual bool handleClear(const ShellCommand& cmd);
        virtual bool handleExit(const ShellCommand& cmd);
        virtual bool handleHelp(const ShellCommand& cmd);
        virtual bool handleConnect(const ShellCommand& cmd) = 0;
        virtual bool handleAddUserRemote(const ShellCommand& cmd);
        virtual bool handleDropUserRemote(const ShellCommand& cmd);
        virtual bool handleListUsersRemote(const ShellCommand& cmd);
        virtual bool handleShutdownRemote(const ShellCommand& cmd);
        virtual bool handleSessionsRemote(const ShellCommand& cmd);
        // Funcție pentru înregistrarea handlerelor standard
        void registerDefaultHandlers();

    public:

        vSqlShellEngine(std::unique_ptr<dbConnection> c) : con(std::move(c)){
            if (con->openDatabase()) {
                LOG_SUCCESS(L"DBF database a fost deschisa cu succes.");
            }
            else {
                LOG_FATAL(L"Eroare la deschiderea DBF!!!.");
            }

            registerDefaultHandlers();
        };

        std::wstring getPrompt() const override {
            return m_accumulator.empty() ? L"\nsqly# " : L"  -> ";
        }

        bool shouldExit() const override { return !m_running; }
        bool stop() { return m_running = false; }

        void execute(const std::wstring& line);
        //std::wstring executeRemote(const std::wstring& line);

        void addToHistory(const std::wstring& command) {
            m_history.push_back(command);
            // Salvare opțională în fișier
            std::wofstream historyFile("history.txt", std::ios::app);
            if (historyFile.is_open()) {
                historyFile << command << std::endl;
            }
        }

    protected:
        void executeSQLCommand(const std::wstring& command);

        void executeShellCommand(const std::wstring& command);

    };
    #endif