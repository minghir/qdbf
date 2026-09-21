#include "vSqlShellEngine.hpp"
#include <thread>  // <--- ADAUGĂ ACEASTĂ LINIE
#include <chrono>  // <--- ADAUGĂ ȘI ACEASTA PENTRU std::chrono::milliseconds
void vSqlShellEngine::execute(const std::wstring& line) {
    std::wstring cleanLine = normalizeSpaces(line);

    // 1) Comenzile de shell se execută imediat
    if (!cleanLine.empty() && cleanLine[0] == L'/') {
        addToHistory(cleanLine);
        executeShellCommand(cleanLine);
        return;
    }

    // 2) SQL multi-line: dacă linia e goală și nu avem nimic acumulat, nu facem nimic
    if (cleanLine.empty() && m_accumulator.empty())
        return;

    // 3) Adăugăm linia în acumulator
    if (!m_accumulator.empty() && !cleanLine.empty())
        m_accumulator += L" ";
    m_accumulator += cleanLine;

    // 4) Verificăm dacă SQL-ul se termină cu ';'
    bool endsWithSemicolon = false;
    if (!m_accumulator.empty() && m_accumulator.back() == L';') {
        endsWithSemicolon = true;
        m_accumulator.pop_back(); // scoatem ';'
    }

    // 5) Dacă nu avem ';', așteptăm următoarea linie
    if (!endsWithSemicolon) {
        return;
    }

    // 6) Avem o comandă SQL completă
    std::wstring fullCommand = normalizeSpaces(m_accumulator);
    m_accumulator.clear();

    if (fullCommand.empty())
        return;

    addToHistory(fullCommand);
    executeSQLCommand(fullCommand);
}

void print_pretty_table(const vConTable& table) {
    const auto& headers = table.columns;
    const auto& rows = table.records;

    if (headers.empty()) {
        ConsoleManager::getInstance().log(L"No columns to display.", LogLevel::WARNING);
        return;
    }

    // Folosim un buffer pentru a construi tot tabelul înainte de a-l trimite la GUI
    std::wstringstream ss;

    // 1. Calculăm lățimea maximă pentru fiecare coloană
    std::vector<size_t> colWidths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        colWidths[i] = headers[i].length();
        for (const auto& row : rows) {
            if (i < row.size()) {
                colWidths[i] = (std::max)(colWidths[i], row[i].length());
            }
        }
        colWidths[i] += 2; // Padding (un spațiu la stânga și unul la dreapta)
    }

    // 2. Funcție lambda internă pentru a adăuga linii orizontale decorative în buffer
    auto add_separator_line = [&]() {
        ss << L"+";
        for (size_t width : colWidths) {
            ss << std::wstring(width, L'-') << L"+";
        }
        ss << L"\n";
    };

    // 3. Construim Antetul (Headers)
    add_separator_line();
    ss << L"|";
    for (size_t i = 0; i < headers.size(); ++i) {
        ss << L" " << std::left << std::setw((int)colWidths[i] - 1) << headers[i] << L"|";
    }
    ss << L"\n";
    add_separator_line();

    // 4. Construim Datele (Rows)
    for (const auto& row : rows) {
        ss << L"|";
        for (size_t i = 0; i < headers.size(); ++i) {
            std::wstring cellValue = (i < row.size()) ? row[i] : L"";
            ss << L" " << std::left << std::setw((int)colWidths[i] - 1) << cellValue << L"|";
        }
        ss << L"\n";
    }

    // 5. Linia de final
    add_separator_line();

    // 6. TRIMITEM TOTUL CATRE GUI SI CONSOLA printr-un singur apel
    // Folosim o culoare neutră (alb/gri deschis) pentru tabel
    ConsoleManager::getInstance().writeRaw(ss.str(), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
}

std::wstring wformat_pretty_table(const vConTable& table) {
    const auto& headers = table.columns;
    const auto& rows = table.records;

    if (headers.empty()) return L"";

    std::wstringstream ss; // Folosim stream în loc de wcout

    // 1. Calculăm lățimea maximă
    std::vector<size_t> colWidths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        colWidths[i] = headers[i].length();
        for (const auto& row : rows) {
            if (i < row.size())
                colWidths[i] = (std::max)(colWidths[i], row[i].length());
        }
        colWidths[i] += 2;
    }

    // 2. Linie decorativă
    auto add_line = [&]() {
        ss << L"+";
        for (size_t width : colWidths) {
            ss << std::wstring(width, L'-') << L"+";
        }
        ss << L"\n";
    };

    // 3. Antet
    add_line();
    ss << L"|";
    for (size_t i = 0; i < headers.size(); ++i) {
        ss << L" " << std::left << std::setw((int)colWidths[i] - 1) << headers[i] << L"|";
    }
    ss << L"\n";
    add_line();

    // 4. Date
    for (const auto& row : rows) {
        ss << L"|";
        for (size_t i = 0; i < row.size(); ++i) {
            ss << L" " << std::left << std::setw((int)colWidths[i] - 1) << row[i] << L"|";
        }
        ss << L"\n";
    }
    add_line();

    return ss.str();
}

void vSqlShellEngine::executeSQLCommand(const std::wstring& command) {
    // 1. Verificare conexiune înainte de trimitere
    if (!con || !con->isConnected()) {
        LOG_ERROR(L"EROARE: Serverul este offline!");
        m_running = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        exit(0);
        return;
    }

    // 2. Execuția pe server
    if (con->execQuery(command)) {
        result = con->getLastQueryResult();

        // Detectare semnal de broadcasting (Shutdown dat de altcineva)
        if (result.message == L"SERVER_SHUTDOWN_SIGNAL" ||
            result.message == L"SERVER_SHUTDOWN_FORCE_CLOSE") {
            LOG_WARNING(L"Deconectare: Serverul a fost oprit de administrator.");
            m_running = false;
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
            exit(0);
            return;
        }

        if (!result.success) {
            LOG_ERROR(L"Eroare Server: " + con->getError());

            return;
        }

        // Afișare tabel (Codul tău de formatare...)
        if (!result.table.records.empty()) {
            std::wstring tableStr = wformat_pretty_table(result.table);
            ConsoleManager::getInstance().writeRaw(tableStr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
        }

        // Metadate
        std::wstringstream ssMeta;
        ssMeta << L"(" << result.rowsAffected << L" rows in set, "
            << std::fixed << std::setprecision(3)
            << (double)result.executionTimeMs / 1000.0 << L" sec)";
        LOG_SUCCESS(ssMeta.str());
    }
    else {
        // Dacă execQuery a eșuat (ex: timeout sau socket closed)
        if (!con->isConnected()) {
            LOG_ERROR(L"Conexiune pierdută cu serverul!");
            exit(0);
        }
        LOG_ERROR(L"Eroare SQL: " + con->getError());
    }
}


void vSqlShellEngine::executeShellCommand(const std::wstring& command) {
        auto cmd = vShellEngineCommandParser::parse(command);

        if (!cmd.isValid) return;

        auto it = m_handlers.find(cmd.name);
        if (it != m_handlers.end()) {
            // EROAREA ERA AICI: Trebuie să trimiți cmd.args, nu cmd (care e tot obiectul)
            it->second(cmd);
        }
        else {
            LOG_ERROR(L"Comanda " + cmd.name + L" nu are un handler înregistrat.");
        }
}

void vSqlShellEngine::registerDefaultHandlers() {
    m_handlers[L"/exit"] = [this](auto& args) { return handleExit(args); };
    m_handlers[L"/quit"] = [this](auto& args) { return handleExit(args); };
    m_handlers[L"/clear"] = [this](auto& args) { return handleClear(args); };
    m_handlers[L"/help"] = [this](auto& args) { return handleHelp(args); };
    m_handlers[L"/connect"] = [this](auto& args) { return handleConnect(args); };
    m_handlers[L"/adduser"] = [this](auto& args) { return handleAddUserRemote(args); };
    m_handlers[L"/dropuser"] = [this](auto& args) { return handleDropUserRemote(args); };
    m_handlers[L"/list_users"] = [this](auto& args) { return handleListUsersRemote(args); };
    m_handlers[L"/shutdown"] = [this](auto& args) { return handleShutdownRemote(args); };
    m_handlers[L"/sessions"] = [this](auto& args) { return handleSessionsRemote(args); };
}

bool vSqlShellEngine::handleClear(const ShellCommand& cmd) {
    return true;
}

bool vSqlShellEngine::handleExit(const ShellCommand& cmd) {
    m_running = false;
    return true;
}

bool vSqlShellEngine::handleHelp(const ShellCommand& cmd) {
    LOG_INFO(L"PRITN HELP");
    return true;
}


bool vSqlShellEngine::handleAddUserRemote(const ShellCommand& cmd) {
    if (cmd.args.size() < 3) {
        LOG_ERROR(L"Utilizare: /adduser <nume> <parola> <rol>");
        return false;
    }

    // Construim string-ul
    std::wstring fullCmd = L"/adduser " + cmd.args[0] + L" " + cmd.args[1] + L" " + cmd.args[2];

    // Folosim execQuery-ul existent! 
    // Serverul o va primi și va trebui să știe să o trateze.
    if (con->execQuery(fullCmd)) {
        LOG_SUCCESS(L"Comanda a fost executată cu succes pe server.");
        return true;
    }
    else {
        LOG_ERROR(L"Serverul a respins comanda: " + con->getError());
        return false;
    }
}

bool vSqlShellEngine::handleListUsersRemote(const ShellCommand& cmd) {
    std::wstring fullCmd = L"/list_users";

    if (con->execQuery(fullCmd)) {
        // 1. Luăm rezultatul proaspăt descărcat de pe server
        vConResult res = con->getLastQueryResult();

        if (!res.success) {
            LOG_ERROR(L"Eroare server: " + res.message);
            return false;
        }

        // 2. Verificăm dacă avem date (tabelul cu useri)
        if (!res.table.records.empty()) {
            // 3. Folosim funcția ta de formatare și afișare
            std::wstring tableStr = wformat_pretty_table(res.table);
            ConsoleManager::getInstance().writeRaw(tableStr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);

            // Afișăm și metadatele (câți useri sunt)
            LOG_SUCCESS(L" (" + std::to_wstring(res.rowsAffected) + L" users in system)");
        }
        else {
            //LOG_INFO(L"Nu există utilizatori înregistrați.");
        }
        return true;
    }
    else {
        LOG_ERROR(L"Serverul a respins comanda: " + con->getError());
        return false;
    }
}

bool vSqlShellEngine::handleDropUserRemote(const ShellCommand& cmd) {
    // 1. Verificăm dacă avem argumentul necesar (numele utilizatorului)
    if (cmd.args.empty()) {
        LOG_ERROR(L"Utilizare: /dropuser <nume_utilizator>");
        return false;
    }

    // 2. Construim comanda pe care o trimitem la server
    std::wstring userToDrop = cmd.args[0];
    std::wstring fullCmd = L"/dropuser " + userToDrop;

    // 3. Trimitem comanda prin conexiunea de rețea existentă
    if (con->execQuery(fullCmd)) {
        // Luăm rezultatul serializat de server (vConResult)
        vConResult res = con->getLastQueryResult();

        if (res.success) {
            LOG_SUCCESS(res.message); // Exemplu: "User 'test' dropped successfully."
            return true;
        }
        else {
            LOG_ERROR(L"Eroare server: " + res.message);
            return false;
        }
    }
    else {
        LOG_ERROR(L"Eroare retea: " + con->getError());
        return false;
    }
}

bool vSqlShellEngine::handleShutdownRemote(const ShellCommand& cmd) {
    ConsoleManager::getInstance().writeRaw(L"[INFO] Trimitere cerere de oprire server...\n", FOREGROUND_INTENSITY);

    if (con->execQuery(L"/shutdown")) {
        vConResult res = con->getLastQueryResult();
        if (res.success) {
            LOG_SUCCESS(res.message); // "SERVER_SHUTDOWN_SIGNAL"

            // Setăm flag-ul de oprire
            m_running = false;

            LOG_WARNING(L"Serverul se oprește. Închidem consola...");
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            // EXIT(0) aici este obligatoriu pentru că vShell::run() 
            // este blocat în std::getline și nu va vedea m_running = false altfel.
            exit(0);
            return true;
        }
    }
    return false;
}

bool vSqlShellEngine::handleSessionsRemote(const ShellCommand& cmd) {
    if (con->execQuery(L"/sessions")) {
        vConResult res = con->getLastQueryResult();
        if (res.success) {
            std::wstring tableStr = wformat_pretty_table(res.table);
            ConsoleManager::getInstance().writeRaw(tableStr, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            LOG_SUCCESS(L"Total: " + std::to_wstring(res.rowsAffected) + L" utilizatori online.");
            return true;
        }
        LOG_ERROR(res.message);
    }
    return false;
}
/*
bool vSqlShellEngine::shouldExit() const {
    // Dacă am setat manual m_running = false SAU dacă am pierdut conexiunea
    if (!m_running) return true;
    if (con && !con->isConnected()) {
        LOG_ERROR(L"Conexiunea cu serverul a fost pierdută.");
        return true;
    }
    return false;
}
*/