#ifndef QDBFCONNECTION_HPP
#define QDBFCONNECTION_HPP

#include "../src/platform.hpp"




#include "../src/dbConnection.hpp"
#include <vector>
#include <string>
#include <cwctype>

#pragma comment(lib, "ws2_32.lib")
//#include "dbConnection.hpp"

vConResult deserializeResult(const std::vector<char>& buffer) {
    vConResult res;
    size_t offset = 0;

    auto readRaw = [&](void* dest, size_t size) {
        if (offset + size > buffer.size()) return;
        memcpy(dest, &buffer[offset], size);
        offset += size;
    };

    auto readWString = [&]() -> std::wstring {
        uint32_t byteSize;
        readRaw(&byteSize, sizeof(uint32_t)); // Citim uint32_t, nu size_t!

        if (byteSize == 0) return L"";
        if (offset + byteSize > buffer.size()) return L"ERR_CORRUPT";

        std::wstring s(byteSize / sizeof(wchar_t), 0);
        memcpy(&s[0], &buffer[offset], byteSize);
        offset += byteSize;
        return s;
    };

    // 1. Citire Metadate (ORDINE IDENTICA)
    readRaw(&res.success, sizeof(res.success));

    int64_t execTime, rowsAff;
    readRaw(&execTime, sizeof(int64_t));
    readRaw(&rowsAff, sizeof(int64_t));
    res.executionTimeMs = execTime;
    res.rowsAffected = rowsAff;

    res.message = readWString();

    // 2. Tabel Metadata
    res.table.tableName = readWString();
    uint32_t colCount;
    readRaw(&colCount, sizeof(uint32_t));
    for (uint32_t i = 0; i < colCount; ++i) {
        res.table.columns.push_back(readWString());
    }

    // 3. Date (Records)
    uint32_t rowCount;
    readRaw(&rowCount, sizeof(uint32_t));
    for (uint32_t i = 0; i < rowCount; ++i) {
        std::vector<std::wstring> row;
        for (uint32_t j = 0; j < colCount; ++j) {
            row.push_back(readWString());
        }
        res.table.records.push_back(row);
    }

    return res;
}


class qdbfConnection : public dbConnection {
private:
    SOCKET m_socket = INVALID_SOCKET;
    std::wstring m_serverIP;
    int m_port;

    std::wstring m_user;
    std::wstring m_pass;

    vConResult m_lastResult;
    std::wstring m_error;
public:
    

    qdbfConnection(const std::wstring& connectionStr, int port)
        : m_port(port) {
        parseConnectionString(connectionStr);
    }

    ~qdbfConnection() override {
        closeDatabase();
    }

    void setCredentials(const std::wstring& user, const std::wstring& pass) {
        m_user = user;
        m_pass = pass;
    }

    void closeDatabase() override {
        if (m_socket != INVALID_SOCKET) {
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
        }
        WSACleanup();
    }

    bool isConnected() const {
        if (m_socket == INVALID_SOCKET) return false;

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(m_socket, &readSet);

        timeval timeout;
        timeout.tv_sec = 0;   // 0 secunde
        timeout.tv_usec = 0;  // 0 microsecunde (interogare instantanee)

        // Select returnează > 0 dacă sunt date de citit, 
        // sau dacă socket-ul a fost închis (event-ul de citire se declanșează la EOF)
            int sel = select(
        #ifdef _WIN32
                0,
        #else
                m_socket + 1,
        #endif
                &readSet, nullptr, nullptr, &timeout);

        if (sel > 0) {
            char buf;
            int res = recv(m_socket, &buf, 1, MSG_PEEK);
            if (res == 0) return false;      // Serverul a închis conexiunea (FIN)
            if (res == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSAEWOULDBLOCK) return false;
            }
        }
        else if (sel == SOCKET_ERROR) {
            return false; // Eroare de socket
        }

        return true;
    }


    // Restul metodelor virtuale obligatorii (minimale)
    // 1. Managementul conexiunii
    bool reconnect() override { closeDatabase(); return openDatabase(); }
    bool testConnection() override { return isConnected(); }

    // 2. Metadate
    long long execCountQuery(const std::wstring& countQuery) override {
        return m_lastResult.rowsAffected;
    }
    int getRowCount(std::string stm_name = "default") override {
        return (int)m_lastResult.table.records.size();
    }
    const std::vector<vNativeDataType> getColumnTypes(std::string stm_name = "default") override {
        return {}; // Îl poți popula ulterior dacă ai nevoie
    }
    const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") override {
        return {};
    }

    // 3. Fetching (Aici clientul va citi din m_lastResult.table deja descărcată)
    bool fetchNextRow(std::string stm_name = "default") override { return false; }
    std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") override { return L""; }
    std::vector<std::wstring> fetchRow(std::string stm_name = "default") override { return {}; }
    std::wstring fetchFieldByName(const std::wstring& fieldName, std::string stm_name = "default") override { return L""; }
    std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default") override { return {}; }

    // 4. Stare și info
    std::wstring getError() override { return m_error; }
    void clearError() override { m_error = L""; }
    std::string getConnectionType() override { return "QDBF_NET"; }
    std::wstring getConnectionDSN() override { return m_serverIP; }
    void setConnectionDSN(const std::wstring& txt) override { m_serverIP = txt; }

    // LIPSA: getColumnNames (Atenție la referință &)
    const std::vector<std::wstring>& getColumnNames(std::string stm_name = "default") override {
        return m_lastResult.table.columns;
    }

    // LIPSA: openDatabase (ai implementat connectToServer, dar nu și metoda din interfață)
    bool openDatabase() override {
        return connectToServer();
    }

    // LIPSA: execQuery
    // 5. ExecQuery corectat: Returnează TRUE dacă rețeaua a funcționat, 
    // lăsând clientul să verifice `m_lastResult.success` pentru statusul SQL/comandă.
    bool execQuery(const std::wstring& query, std::string stm_name = "default") override {
        if (!isConnected()) {
            m_error = L"Nu există o conexiune activă la server.";
            return false;
        }

        // 1. Trimitem query-ul
        std::string qStr(query.begin(), query.end());
        if (send(m_socket, qStr.c_str(), (int)qStr.length(), 0) == SOCKET_ERROR) {
            m_error = L"Eroare la trimiterea datelor prin socket.";
            return false;
        }

        // 2. Primim dimensiunea pachetului
        uint32_t packetSize = 0;
        int r = recv(m_socket, (char*)&packetSize, sizeof(packetSize), 0);
        if (r <= 0) {
            m_error = L"Conexiunea a fost închisă de server.";
            return false;
        }

        // 3. Primim buffer-ul complet
        std::vector<char> buffer(packetSize);
        uint32_t receivedTotal = 0;
        while (receivedTotal < packetSize) {
            int n = recv(m_socket, buffer.data() + receivedTotal, packetSize - receivedTotal, 0);
            if (n <= 0) {
                m_error = L"Conexiunea a fost întreruptă în timpul recepției datelor.";
                return false;
            }
            receivedTotal += n;
        }

        // 4. Deserializăm rezultatul trimis de server
        m_lastResult = deserializeResult(buffer);

        // ⭐ MODIFICAREA CHEIE: Returnăm întotdeauna true dacă pachetul s-a citit cu succes.
        // Erorile logice sau de SQL vor fi preluate din m_lastResult.success de către client.
        if(!m_lastResult.success) m_error = m_lastResult.message;
        return true;
    }

    bool execQuery(const std::wstring& query, const std::vector<std::wstring>& params, std::string stm_name = "default") override {
        std::wstring processedQuery = query;
        size_t paramIdx = 0;
        size_t pos = 0;

        auto isNumeric = [](const std::wstring& s) {
            if (s.empty()) return false;
            size_t start = (s[0] == L'-' || s[0] == L'+') ? 1 : 0;
            if (start == s.length()) return false;
            bool hasDecimal = false;
            for (size_t i = start; i < s.length(); ++i) {
                if (s[i] == L'.') {
                    if (hasDecimal) return false;
                    hasDecimal = true;
                }
                else if (!std::iswdigit(s[i])) {
                    return false;
                }
            }
            return true;
        };

        while ((pos = processedQuery.find(L'?', pos)) != std::wstring::npos && paramIdx < params.size()) {
            std::wstring rawVal = params[paramIdx];
            std::wstring formattedVal;

            if (isNumeric(rawVal)) {
                formattedVal = rawVal;
            }
            else {
                std::wstring escapedVal = rawVal;
                size_t qPos = 0;
                while ((qPos = escapedVal.find(L'\'', qPos)) != std::wstring::npos) {
                    escapedVal.replace(qPos, 1, L"''");
                    qPos += 2;
                }
                formattedVal = L"'" + escapedVal + L"'";
            }

            processedQuery.replace(pos, 1, formattedVal);
            pos += formattedVal.length();
            paramIdx++;
        }

        return execQuery(processedQuery, stm_name);
    }

    // LIPSA: getLastQueryResult
    vConResult getLastQueryResult() override {
        return m_lastResult;
    }

    
    

private:
    bool sendWString(const std::wstring& s) {
        uint32_t byteSize = static_cast<uint32_t>(s.size() * sizeof(wchar_t));
        // 1. Trimitem lungimea
        if (send(m_socket, (const char*)&byteSize, sizeof(byteSize), 0) == SOCKET_ERROR)
            return false;
        // 2. Trimitem datele (dacă există)
        if (byteSize > 0) {
            if (send(m_socket, (const char*)s.data(), byteSize, 0) == SOCKET_ERROR)
                return false;
        }
        return true;
    }

    bool connectToServer() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            m_error = L"WSAStartup failed";
            return false;
        }

        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET) {
            m_error = L"Socket creation failed: " + std::to_wstring(WSAGetLastError());
            return false;
        }

        std::string ipStr(m_serverIP.begin(), m_serverIP.end());
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_port);
        inet_pton(AF_INET, ipStr.c_str(), &serverAddr.sin_addr);

        if (connect(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            m_error = L"Connection failed: " + std::to_wstring(WSAGetLastError());
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        // --- ETAPA DE HANDSHAKE (Sincronizată cu Serverul) ---

        // 1. Așteptăm prompt-ul "User: " de la server (opțional, dar serverul tău îl trimite)
        char junk[16];
        recv(m_socket, junk, 6, 0); // "User: "

        // 2. Trimitem Username
        if (!sendWString(m_user)) return false;

        // 3. Așteptăm prompt-ul "Pass: "
        recv(m_socket, junk, 6, 0); // "Pass: "

        // 4. Trimitem Parola
        if (!sendWString(m_pass)) return false;

        // 5. Verificăm dacă am primit "Login Success!"
        char response[128] = { 0 };
        int bytes = recv(m_socket, response, sizeof(response) - 1, 0);
        if (bytes > 0) {
            std::string respStr(response);
            if (respStr.find("Success") == std::string::npos) {
                m_error = L"Autentificare eșuată: " + std::wstring(respStr.begin(), respStr.end());
                closesocket(m_socket);
                m_socket = INVALID_SOCKET;
                return false;
            }
        }

        return true;
    }

    void parseConnectionString(const std::wstring& connStr) {
        // Format asteptat: "user:pass@ip"
        size_t colonPos = connStr.find(L':');
        size_t atPos = connStr.find(L'@');

        if (colonPos != std::wstring::npos && atPos != std::wstring::npos && atPos > colonPos) {
            m_user = connStr.substr(0, colonPos);
            m_pass = connStr.substr(colonPos + 1, atPos - colonPos - 1);
            m_serverIP = connStr.substr(atPos + 1);
        }
        else {
            // Fallback daca string-ul nu are formatul corect (doar IP)
            m_user = L"guest";
            m_pass = L"";
            m_serverIP = connStr;
        }
    }
    std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName) override {
        std::vector<vExternalColumnInfo> schema;
        // TODO: populate schema from m_lastResult.table or server metadata
        return schema;
    }

    void clearStatement(std::string stm_name = "default") {
        // În acest model de conexiune prin rețea, m_lastResult reține datele
        // primite de la server. Pentru a elibera memoria:

        m_lastResult.table.records.clear();
        m_lastResult.table.columns.clear();
        m_lastResult.message.clear();

        // Forțăm eliberarea capacității vectorilor (shrink_to_fit) 
        // pentru a da memoria înapoi sistemului imediat.
        m_lastResult.table.records.shrink_to_fit();
        m_lastResult.table.columns.shrink_to_fit();

        // LOG_DEBUG(L"qdbfConnection::clearStatement: Memoria buffer-ului de rețea a fost eliberată.");
    }
};

#endif