
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
#include <algorithm>
#include <cstring>
#include <ctime>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

std::wstring getPasswordMasked() {
#ifdef _WIN32
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
#else
    termios original{};
    tcgetattr(STDIN_FILENO, &original);
    termios hidden = original;
    hidden.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &hidden);

    std::wstring pass;
    std::getline(std::wcin, pass);
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    std::wcout << L"\n";
    return pass;
#endif
}

std::wstring lowerCommandArgument(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(), towlower);
    return value;
}

std::string csvField(const std::wstring& value) {
    std::string field = utf8_encode(value);
    if (field.find_first_of(",\"\r\n") == std::string::npos) return field;

    std::string escaped = "\"";
    for (char character : field) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    return escaped + '"';
}

bool saveCsvFile(const std::filesystem::path& path, const vConTable& table) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;

    for (size_t i = 0; i < table.columns.size(); ++i) {
        if (i) output << ',';
        output << csvField(table.columns[i]);
    }
    output << "\r\n";

    for (const auto& row : table.records) {
        for (size_t i = 0; i < table.columns.size(); ++i) {
            if (i) output << ',';
            output << csvField(i < row.size() ? row[i] : L"");
        }
        output << "\r\n";
    }
    return output.good();
}

char getDbfFieldType(const vConTable& table, size_t index) {
    if (index < table.columnTypes.size() && !table.columnTypes[index].empty()) {
        wchar_t type = towupper(table.columnTypes[index][0]);
        if (type == L'N' || type == L'F' || type == L'D' || type == L'L')
            return static_cast<char>(type);
    }
    return 'C';
}

bool saveDbfFile(const std::filesystem::path& path, const vConTable& table) {
    if (table.columns.empty() || table.columns.size() > 255) return false;

    std::vector<char> types(table.columns.size());
    std::vector<uint8_t> widths(table.columns.size());
    for (size_t i = 0; i < table.columns.size(); ++i) {
        types[i] = getDbfFieldType(table, i);
        size_t width = table.columns[i].size();
        for (const auto& row : table.records)
            if (i < row.size()) width = (std::max)(width, utf8_encode(row[i]).size());
        if (types[i] == 'D') width = 8;
        if (types[i] == 'L') width = 1;
        if (types[i] == 'N' || types[i] == 'F') width = (std::max)(width, size_t(18));
        widths[i] = static_cast<uint8_t>((std::min)(width, size_t(254)));
        if (widths[i] == 0) widths[i] = 1;
    }

    DBF_Header header{};
    header.version = 0x03;
    std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    header.lastUpdate[0] = static_cast<uint8_t>(local.tm_year % 100);
    header.lastUpdate[1] = static_cast<uint8_t>(local.tm_mon + 1);
    header.lastUpdate[2] = static_cast<uint8_t>(local.tm_mday);
    header.numRecords = static_cast<uint32_t>(table.records.size());
    header.headerLength = static_cast<uint16_t>(32 + 32 * table.columns.size() + 1);
    uint16_t recordLength = 1;
    for (uint8_t width : widths) recordLength += width;
    header.recordLength = recordLength;

    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));

    for (size_t i = 0; i < table.columns.size(); ++i) {
        DBF_FieldDescriptor field{};
        std::string name = utf8_encode(table.columns[i]);
        std::memcpy(field.fieldName, name.data(), (std::min)(name.size(), size_t(10)));
        field.fieldType = types[i];
        field.fieldLength = widths[i];
        output.write(reinterpret_cast<const char*>(&field), sizeof(field));
    }
    const char descriptorTerminator = 0x0D;
    output.write(&descriptorTerminator, 1);

    for (const auto& row : table.records) {
        const char active = ' ';
        output.write(&active, 1);
        for (size_t i = 0; i < table.columns.size(); ++i) {
            std::string value = i < row.size() ? utf8_encode(row[i]) : std::string();
            if (value.size() > widths[i]) value.resize(widths[i]);
            if (types[i] == 'N' || types[i] == 'F')
                output << std::string(widths[i] - value.size(), ' ');
            else
                output << value << std::string(widths[i] - value.size(), ' ');
            if (types[i] == 'N' || types[i] == 'F') output << value;
        }
    }
    const char endOfFile = 0x1A;
    output.write(&endOfFile, 1);
    return output.good();
}

struct LoadData {
    vConTable table;
    bool hasHeader = false;
};

std::vector<std::wstring> parseCsvRecord(const std::string& line) {
    std::vector<std::wstring> fields;
    std::string field;
    bool quoted = false;
    for (size_t index = 0; index < line.size(); ++index) {
        char character = line[index];
        if (character == '"') {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                field += '"';
                ++index;
            }
            else {
                quoted = !quoted;
            }
        }
        else if (character == ',' && !quoted) {
            fields.push_back(utf8_to_wstring(field));
            field.clear();
        }
        else {
            field += character;
        }
    }
    fields.push_back(utf8_to_wstring(field));
    return fields;
}

std::wstring safeIdentifier(const std::wstring& value, const std::wstring& fallback) {
    std::wstring identifier;
    for (wchar_t character : value) {
        if (std::iswalnum(character) || character == L'_') identifier += character;
        else if (!identifier.empty()) identifier += L'_';
    }
    if (identifier.empty()) identifier = fallback;
    if (std::iswdigit(identifier.front())) identifier = L"C_" + identifier;
    return identifier;
}

bool loadCsvFile(const std::filesystem::path& path, LoadData& data) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    std::string line;
    if (!std::getline(input, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    data.table.columns = parseCsvRecord(line);
    if (data.table.columns.empty()) return false;

    for (size_t index = 0; index < data.table.columns.size(); ++index) {
        data.table.columns[index] = safeIdentifier(data.table.columns[index], L"COLUMN_" + std::to_wstring(index + 1));
        data.table.columnTypes.push_back(L"C");
        data.table.columnWidths.push_back(static_cast<int>(data.table.columns[index].size()));
        data.table.columnDecimals.push_back(0);
    }

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto row = parseCsvRecord(line);
        row.resize(data.table.columns.size());
        for (size_t index = 0; index < row.size(); ++index)
            data.table.columnWidths[index] = (std::max)(data.table.columnWidths[index], static_cast<int>(utf8_encode(row[index]).size()));
        data.table.records.push_back(std::move(row));
    }

    for (int& width : data.table.columnWidths) width = (std::min)((std::max)(width, 1), 254);
    data.hasHeader = true;
    return true;
}

bool loadDbfFile(const std::filesystem::path& path, LoadData& data) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    DBF_Header header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input || (header.version != 0x03 && header.version != 0x30)) return false;

    std::vector<DBF_FieldDescriptor> fields;
    while (input && input.peek() != 0x0D) {
        DBF_FieldDescriptor field{};
        input.read(reinterpret_cast<char*>(&field), sizeof(field));
        if (!input) return false;
        fields.push_back(field);
    }
    if (fields.empty()) return false;
    input.seekg(header.headerLength, std::ios::beg);

    for (size_t index = 0; index < fields.size(); ++index) {
        std::string rawName(fields[index].fieldName, 11);
        size_t end = rawName.find('\0');
        if (end != std::string::npos) rawName.resize(end);
        data.table.columns.push_back(safeIdentifier(utf8_to_wstring(rawName), L"COLUMN_" + std::to_wstring(index + 1)));
        data.table.columnTypes.push_back(std::wstring(1, static_cast<wchar_t>(fields[index].fieldType)));
        data.table.columnWidths.push_back(fields[index].fieldLength);
        data.table.columnDecimals.push_back(fields[index].decimalCount);
    }

    std::vector<char> record(header.recordLength);
    for (uint32_t rowIndex = 0; rowIndex < header.numRecords; ++rowIndex) {
        if (!input.read(record.data(), record.size())) break;
        if (record[0] == '*') continue;
        if (record[0] != ' ') return false;

        std::vector<std::wstring> row;
        size_t offset = 1;
        for (const auto& field : fields) {
            std::string value(record.data() + offset, field.fieldLength);
            while (!value.empty() && value.back() == ' ') value.pop_back();
            row.push_back(utf8_to_wstring(value));
            offset += field.fieldLength;
        }
        data.table.records.push_back(std::move(row));
    }
    data.hasHeader = true;
    return true;
}

std::wstring sqlLiteral(const std::wstring& value, wchar_t type) {
    if ((type == L'N' || type == L'F') && !value.empty()) return value;
    std::wstring escaped = value;
    size_t position = 0;
    while ((position = escaped.find(L'\'', position)) != std::wstring::npos) {
        escaped.insert(position, L"'");
        position += 2;
    }
    return L"'" + escaped + L"'";
}

class QdbfClient : public vSqlShellEngine {

public:
   

    QdbfClient(const std::wstring& initialConnStr)
        : vSqlShellEngine(std::make_unique<qdbfConnection>(initialConnStr, 3519)) {

        //m_handlers[L"/shutdown"] = [this](auto& args) { return handleShutdownRemote(args); };
        //m_handlers[L"/sessions"] = [this](auto& args) { return handleSessionsRemote(args); };

        // Încercăm conectarea imediat după creare
        if (!con->openDatabase()) {
            LOG_ERROR(L"Initial authentication failed: " + con->getError());
            LOG_INFO(L"You can try again using: /connect user:pass@IP");
        }
        else {
            LOG_SUCCESS(L"Session started for: " + con->getConnectionDSN());
        }
    }


    bool handleConnect(const ShellCommand& cmd) override {
        // Utilizare: /connect admin:123@10.9.50.241 3519
        if (cmd.args.empty()) {
            LOG_ERROR(L"Usage: /connect user:pass@IP [port]");
            return false;
        }

        std::wstring connStr = cmd.args[0];
        int port = 3519; // Portul tău default

        if (cmd.args.size() > 1) {
            try {
                port = std::stoi(cmd.args[1]);
            }
            catch (...) {
                LOG_ERROR(L"Invalid port. Using 3519.");
            }
        }

        // 1. Salvăm conexiunea actuală pentru fallback
        auto oldCon = std::move(con);

        // 2. Creăm noua conexiune (parsing-ul se face în constructor)
        con = std::make_unique<qdbfConnection>(connStr, port);

        // 3. Încercăm deschiderea (Handshake-ul de login are loc aici)
        if (con->openDatabase()) {
            // Folosim getConnectionDSN() pentru a afișa IP-ul extras din URI
            LOG_SUCCESS(L"Connected successfully to: " + con->getConnectionDSN() + L":" + std::to_wstring(port));
            return true;
        }
        else {
            LOG_ERROR(L"Connection error: " + con->getError());

            // 4. Restaurăm vechea conexiune dacă noua a eșuat
            con = std::move(oldCon);

            if (con) {
                LOG_INFO(L"Restored the previous connection: " + con->getConnectionDSN());
            }
            return false;
        }
    }

    bool handleHelp(const ShellCommand& cmd) override {
        (void)cmd;
        ConsoleManager::getInstance().writeRaw(
            L"\nClient commands:\n"
            L"  /help                         Displays this help\n"
            L"  /connect user:password@ip [port]   Connect to a QDBF server\n"
            L"  /save csv <path>              Saves the last result as CSV\n"
            L"  /save dbf <path>              Saves the last result as DBF\n"
            L"  /load <file> into <table>     Imports a CSV or DBF into the server\n"
            L"  /clear                        Clears the console\n"
            L"  /exit, /quit                  Closes the client\n"
            L"\nServer commands (authentication required):\n"
            L"  /adduser <name> <password> <role>   Adds a user\n"
            L"  /dropuser <name>                    Removes a user\n"
            L"  /list_users                         Lists all users\n"
            L"  /sessions                           Shows active sessions\n"
            L"  /shutdown                           Shuts down the server\n"
            L"\nSQL:\n"
            L"  SELECT * FROM people;\n"
            L"  INSERT, UPDATE, DELETE, CREATE TABLE, DROP TABLE and SHOW TABLES\n"
            L"  SHOW TABLES\n\n",
            FOREGROUND_GREEN | FOREGROUND_INTENSITY
        );

        return true;
    }

    bool handleSave(const ShellCommand& cmd) override {
        if (cmd.args.size() != 2 ||
            (lowerCommandArgument(cmd.args[0]) != L"csv" &&
             lowerCommandArgument(cmd.args[0]) != L"dbf")) {
            LOG_ERROR(L"Usage: /save csv <file.csv> or /save dbf <file.dbf>");
            return false;
        }
        if (result.table.columns.empty()) {
            LOG_ERROR(L"There is no tabular result to save. Run a SELECT first.");
            return false;
        }

        const std::filesystem::path path(cmd.args[1]);
        const bool saved = lowerCommandArgument(cmd.args[0]) == L"csv"
            ? saveCsvFile(path, result.table)
            : saveDbfFile(path, result.table);
        if (!saved) {
            LOG_ERROR(L"Could not save file: " + path.wstring());
            return false;
        }
        LOG_SUCCESS(L"Result saved to: " + path.wstring());
        return true;
    }

    bool handleLoad(const ShellCommand& cmd) override {
        if (cmd.args.size() != 3 || lowerCommandArgument(cmd.args[1]) != L"into") {
            LOG_ERROR(L"Usage: /load <file.csv|file.dbf> into <table>");
            return false;
        }
        if (!con || !con->isConnected()) {
            LOG_ERROR(L"There is no active server connection.");
            return false;
        }

        const std::filesystem::path path(cmd.args[0]);
        const std::wstring extension = lowerCommandArgument(path.extension().wstring());
        LoadData data;
        bool loaded = extension == L".csv" ? loadCsvFile(path, data)
            : extension == L".dbf" ? loadDbfFile(path, data) : false;
        if (!loaded) {
            LOG_ERROR(L"Could not load file: " + path.wstring());
            return false;
        }

        const std::wstring tableName = safeIdentifier(cmd.args[2], L"IMPORTED_TABLE");
        std::wstring createQuery = L"CREATE TABLE " + tableName + L" (";
        for (size_t index = 0; index < data.table.columns.size(); ++index) {
            if (index) createQuery += L", ";
            wchar_t type = data.table.columnTypes[index].empty() ? L'C' : data.table.columnTypes[index][0];
            int width = data.table.columnWidths[index];
            int decimals = data.table.columnDecimals[index];
            if (type == L'D') createQuery += data.table.columns[index] + L" D";
            else if (type == L'L') createQuery += data.table.columns[index] + L" L(1)";
            else if (type == L'N' || type == L'F')
                createQuery += data.table.columns[index] + L" " + type + L"(" + std::to_wstring(width) + L"," + std::to_wstring(decimals) + L")";
            else
                createQuery += data.table.columns[index] + L" C(" + std::to_wstring((std::max)(width, 1)) + L")";
        }
        createQuery += L")";

        if (!con->execQuery(createQuery) || !con->getLastQueryResult().success) {
            LOG_ERROR(L"Could not create table " + tableName + L": " + con->getError());
            return false;
        }

        size_t importedRows = 0;
        for (const auto& row : data.table.records) {
            std::wstring insertQuery = L"INSERT INTO " + tableName + L" VALUES (";
            for (size_t index = 0; index < data.table.columns.size(); ++index) {
                if (index) insertQuery += L", ";
                wchar_t type = data.table.columnTypes[index].empty() ? L'C' : data.table.columnTypes[index][0];
                insertQuery += sqlLiteral(index < row.size() ? row[index] : L"", type);
            }
            insertQuery += L")";

            if (!con->execQuery(insertQuery) || !con->getLastQueryResult().success) {
                LOG_ERROR(L"Import stopped at row " + std::to_wstring(importedRows + 1) + L": " + con->getError());
                return false;
            }
            ++importedRows;
        }

        LOG_SUCCESS(L"Table " + tableName + L" created; imported " + std::to_wstring(importedRows) + L" rows.");
        return true;
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

        std::wcout << L"User: ";
        std::wcin >> user;
        std::wcout << L"Password: ";
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