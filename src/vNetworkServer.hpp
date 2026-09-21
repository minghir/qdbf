
#ifndef VNETWORKSERVER_HPP
#define VNETWORKSERVER_HPP

#include "platform.hpp"
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <sstream>
#include <map>

#include "ConsoleManager.hpp"
#include "vSqlShellEngine.hpp"
#include "dbfConnection.hpp"

#pragma comment(lib, "ws2_32.lib")

// Mutex global pentru sincronizarea accesului la motorul DBF între thread-uri
extern std::mutex engineMutex;

enum class UserRole { ADMIN, USER };

struct UserProfile {
    std::wstring username;
    std::wstring password;
    UserRole role;
};

struct ActiveSession {
    std::wstring username;
    std::wstring ip;
    std::wstring loginTime;
    UserRole role;
};

// --- SERIALIZARE REZULTATE ---
// Transformă obiectul vConResult într-un flux de bytes pentru rețea
static std::vector<char> serializeResult(const vConResult& res) {
    std::vector<char> buffer;

    auto writeRaw = [&](const auto& data) {
        const char* ptr = reinterpret_cast<const char*>(&data);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(data));
    };

    auto writeWString = [&](const std::wstring& s) {
        uint32_t byteSize = static_cast<uint32_t>(s.size() * sizeof(wchar_t));
        writeRaw(byteSize);
        if (byteSize > 0) {
            const char* ptr = reinterpret_cast<const char*>(s.data());
            buffer.insert(buffer.end(), ptr, ptr + byteSize);
        }
    };

    // 1. Metadate
    writeRaw(res.success);
    int64_t execTime = res.executionTimeMs;
    int64_t rowsAff = res.rowsAffected;
    writeRaw(execTime);
    writeRaw(rowsAff);
    writeWString(res.message);

    // 2. Tabel Metadata
    writeWString(res.table.tableName);
    uint32_t colCount = static_cast<uint32_t>(res.table.columns.size());
    writeRaw(colCount);
    for (const auto& col : res.table.columns) {
        writeWString(col);
    }

    // 3. Date (Records)
    uint32_t rowCount = static_cast<uint32_t>(res.table.records.size());
    writeRaw(rowCount);
    for (const auto& row : res.table.records) {
        for (const auto& cell : row) {
            writeWString(cell);
        }
    }

    return buffer;
}

class vNetworkServer {
    SOCKET listenSocket = INVALID_SOCKET;
    bool m_running = false;
    
    std::map<std::wstring, UserProfile> m_users;
    std::mutex m_userMutex;

    std::wstring m_userFilePath;

    std::map<SOCKET, ActiveSession> m_activeSessions;
    std::mutex m_sessionsMutex;

public:
    vNetworkServer() {
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
    }

    ~vNetworkServer() {
        m_running = false;
        if (listenSocket != INVALID_SOCKET) closesocket(listenSocket);
        WSACleanup();
    }

    void loadUsers(const std::wstring& filePath) {
        m_userFilePath = filePath;
        std::ifstream fin(wstr_to_str(filePath));
        if (!fin.is_open()) {
            LOG_ERROR(L"Nu s-a putut deschide fisierul de useri. Fallback: admin/123.");
            m_users[L"admin"] = { L"admin", L"123", UserRole::ADMIN };
            return;
        }

        std::lock_guard<std::mutex> lock(m_userMutex);
        m_users.clear();
        std::string line;
        while (std::getline(fin, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            size_t eqPos = line.find('=');
            size_t commaPos = line.find(',');

            if (eqPos != std::string::npos && commaPos != std::string::npos) {
                std::string user = trim(line.substr(0, eqPos));
                std::string pass = trim(line.substr(eqPos + 1, commaPos - eqPos - 1));
                std::string roleStr = trim(line.substr(commaPos + 1));
                UserRole role = (roleStr == "0") ? UserRole::ADMIN : UserRole::USER;
                m_users[str_to_wstr(user)] = { str_to_wstr(user), str_to_wstr(pass), role };
            }
        }
        LOG_SUCCESS(L"Incarcat " + std::to_wstring(m_users.size()) + L" utilizatori.");
    }

    bool init(int port) {
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) return false;

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
            return false;

        listen(listenSocket, SOMAXCONN);
        m_running = true;
        //LOG_INFO(L"Serverul asculta pe portul " + std::to_wstring(port));
        return true;
    }

    void run(dbfConnection& engine) {
        while (m_running) {
            SOCKET clientSocket = accept(listenSocket, NULL, NULL);
            if (clientSocket == INVALID_SOCKET) continue;

            //LOG_INFO(L"Client nou detectat. Alocam thread...");
            std::thread([this, clientSocket, &engine]() {
                this->handleClient(clientSocket, engine);
                }).detach();
        }
    }

private:
    // Helper pentru citirea sigură a string-urilor cu lungime prefixată (Handshake-safe)
    std::wstring receiveWString(SOCKET clientSocket) {
        uint32_t byteSize = 0;
        int r = recv(clientSocket, (char*)&byteSize, sizeof(byteSize), 0);
        if (r <= 0 || byteSize == 0 || byteSize > 4096) return L"";

        std::vector<char> buffer(byteSize);
        int received = 0;
        while (received < (int)byteSize) {
            int n = recv(clientSocket, buffer.data() + received, byteSize - received, 0);
            if (n <= 0) break;
            received += n;
        }
        return std::wstring(reinterpret_cast<wchar_t*>(buffer.data()), byteSize / sizeof(wchar_t));
    }

    void handleClient(SOCKET clientSocket, dbfConnection& engine) {
        // 1. Obținere IP Client (Varianta Modernă)
        sockaddr_in addr;
        socklen_t addrLen = sizeof(addr);
        getpeername(clientSocket, (sockaddr*)&addr, &addrLen);
        char ipBuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, ipBuf, INET_ADDRSTRLEN);
        std::wstring clientIP = str_to_wstr(ipBuf);

        //LOG_INFO(L"[" + clientIP + L"] Începe procedura de autentificare...");

        // --- ETAPA 1: AUTENTIFICARE ---
        send(clientSocket, "User: ", 6, 0);
        std::wstring user = receiveWString(clientSocket);

        send(clientSocket, "Pass: ", 6, 0);
        std::wstring pass = receiveWString(clientSocket);

        UserProfile currentUser;
        bool authenticated = false;

        {
            std::lock_guard<std::mutex> lock(m_userMutex);
            if (m_users.count(user) && m_users[user].password == pass) {
                currentUser = m_users[user];
                authenticated = true;
            }
        }

        if (!authenticated) {
            LOG_ERROR(L"[" + clientIP + L"] Acces respins pentru utilizatorul: " + (user.empty() ? L"(null)" : user));
            send(clientSocket, "Login Failed!\n", 14, 0);
            closesocket(clientSocket);
            return;
        }

       


        std::wstring roleStr = (currentUser.role == UserRole::ADMIN) ? L"ADMIN" : L"USER";
        LOG_SUCCESS(L"[" + clientIP + L"] " + user + L" s-a logat ca " + roleStr);
        send(clientSocket, "Login Success!\n", 15, 0);

        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);

            // 1. Siguranță: Dacă socket-ul cumva există deja (nu ar trebui), îl ștergem
            m_activeSessions.erase(clientSocket);

            // 2. Opțional: Prevenim login-ul dublu cu același cont de pe același IP
            // (Uneori clientul trimite pachete de sincronizare care pot declanșa thread-uri noi)
            for (auto it = m_activeSessions.begin(); it != m_activeSessions.end(); ) {
                if (it->second.username == user && it->second.ip == clientIP) {
                    it = m_activeSessions.erase(it); // Închidem sesiunea veche dacă e identică
                }
                else {
                    ++it;
                }
            }

            // 3. Adăugăm sesiunea actuală
            m_activeSessions[clientSocket] = { user, clientIP, L"Acum", currentUser.role };
        }


        // --- ETAPA 2: SESIUNE ACTIVĂ ---
        bool sessionActive = true;
        while (sessionActive) {
            char buffer[2048] = { 0 };
            int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) break;

            std::string msg(buffer, bytes);

            msg.erase(msg.find_last_not_of(" \n\r\t") + 1);
            if (msg.empty()) continue;

            //LOG_INFO(L"[" + user + L"] Comandă primită: " + str_to_wstr(msg));


            //LOG_INFO(L"[" + user + L"] Comandă primită: " + str_to_wstr(msg));

            // 1. Verificăm dacă este o comandă administrativă (cele cu '/')
            bool isAdminCommand = (msg.find("/adduser") == 0 ||
                msg.find("/list_users") == 0 ||
                msg.find("/dropuser") == 0 || 
                msg.find("/shutdown") == 0 ||
                msg.find("/sessions") == 0);

            if (isAdminCommand) {
                // Dacă e comandă de admin, verificăm rolul
                if (currentUser.role != UserRole::ADMIN) {
                    vConResult res;
                    res.success = false;
                    res.message = L"Eroare: Această comandă necesită drepturi de ADMINISTRATOR.";

                    LOG_WARNING(L"[" + user + L"] Tentativă neautorizată la comandă de sistem!");

                    std::vector<char> data = serializeResult(res);
                    uint32_t packetSize = static_cast<uint32_t>(data.size());
                    send(clientSocket, (char*)&packetSize, sizeof(packetSize), 0);
                    send(clientSocket, data.data(), (int)data.size(), 0);
                    continue; // Nu executăm, trecem la următoarea comandă
                }

                // Logică Admin / Comenzi speciale
                if (msg.find("/adduser") == 0) {
                    vConResult res; // Creăm un obiect de rezultat standard

                    if (currentUser.role != UserRole::ADMIN) {
                        LOG_WARNING(L"[" + user + L"] Tentativă neautorizată!");
                        res.success = false;
                        res.message = L"Error: Admin rights required.";
                    }
                    else {
                        processAddUser(msg);
                        res.success = true;
                        res.message = L"User added successfully.";
                        res.rowsAffected = 1;
                    }

                    // ACUM: Trimitem rezultatul serializat, nu un simplu string!
                    // Folosim logica pe care o ai deja pentru SQL
                    std::vector<char> data = serializeResult(res);
                    uint32_t packetSize = static_cast<uint32_t>(data.size());

                    // Trimitem dimensiunea + datele
                    send(clientSocket, reinterpret_cast<const char*>(&packetSize), sizeof(packetSize), 0);
                    send(clientSocket, data.data(), (int)data.size(), 0);

                    continue; // Trecem la următoarea comandă
                }
                if (msg.find("/list_users") == 0) {
                    vConResult res;
                    res.success = true;
                    res.message = L"Lista utilizatori sistem:";

                    // Definim coloanele tabelului de rezultate
                    res.table.columns = { L"USERNAME", L"ROLE" };
                    res.table.tableName = L"SystemUsers";

                    {
                        std::lock_guard<std::mutex> lock(m_userMutex);
                        for (auto const& [name, profile] : m_users) {
                            std::vector<std::wstring> row;
                            row.push_back(profile.username);
                            row.push_back(profile.role == UserRole::ADMIN ? L"ADMIN" : L"USER");
                            res.table.records.push_back(row);
                        }
                    }

                    res.rowsAffected = res.table.records.size();

                    // Serializare și trimitere (Protocolul Unificat)
                    std::vector<char> data = serializeResult(res);
                    uint32_t packetSize = static_cast<uint32_t>(data.size());

                    send(clientSocket, reinterpret_cast<const char*>(&packetSize), sizeof(packetSize), 0);
                    send(clientSocket, data.data(), (int)data.size(), 0);

                    continue;
                }

                if (msg.find("/dropuser") == 0) {
                    vConResult res;

                    if (currentUser.role != UserRole::ADMIN) {
                        res.success = false;
                        res.message = L"Error: Admin rights required.";
                    }
                    else {
                        // Parsăm numele utilizatorului (ex: /dropuser test)
                        std::stringstream ss(msg);
                        std::string tag, usernameToDrop;
                        ss >> tag >> usernameToDrop;

                        std::wstring wUserToDrop = str_to_wstr(usernameToDrop);

                        if (wUserToDrop == L"admin") {
                            res.success = false;
                            res.message = L"Error: Cannot drop the main admin account.";
                        }
                        else {
                            std::lock_guard<std::mutex> lock(m_userMutex);
                            if (m_users.erase(wUserToDrop)) {
                                saveUsersInternal(); // Salvăm modificarea pe disc
                                res.success = true;
                                res.message = L"User '" + wUserToDrop + L"' dropped successfully.";
                                res.rowsAffected = 1;
                            }
                            else {
                                res.success = false;
                                res.message = L"Error: User not found.";
                            }
                        }
                    }

                    // Trimitem rezultatul înapoi la client (folosind protocolul tău binar)
                    std::vector<char> data = serializeResult(res);
                    uint32_t packetSize = static_cast<uint32_t>(data.size());
                    send(clientSocket, reinterpret_cast<const char*>(&packetSize), sizeof(packetSize), 0);
                    send(clientSocket, data.data(), (int)data.size(), 0);

                    continue;
                }

                if (msg == "/shutdown") {
                    if (currentUser.role != UserRole::ADMIN) {
                        vConResult res;
                        res.success = false;
                        res.message = L"Eroare: Doar administratorul poate opri serverul.";

                        std::vector<char> data = serializeResult(res);
                        uint32_t packetSize = static_cast<uint32_t>(data.size());
                        send(clientSocket, (char*)&packetSize, sizeof(packetSize), 0);
                        send(clientSocket, data.data(), (int)data.size(), 0);
                    }
                    else {
                        LOG_WARNING(L"!!! Comandă SHUTDOWN primită de la " + user + L" !!!");

                        // 1. Pregătim mesajul de notificare pentru TOȚI clienții
                        vConResult shutRes;
                        shutRes.success = false; // Folosim false pentru a indica o stare de terminare
                        shutRes.message = L"SERVER_SHUTDOWN_SIGNAL";

                        std::vector<char> data = serializeResult(shutRes);
                        uint32_t packetSize = static_cast<uint32_t>(data.size());

                        // 2. Notificăm TOȚI clienții conectați
                        {
                            std::lock_guard<std::mutex> lock(m_sessionsMutex);
                            //LOG_INFO(L"Anunțăm " + std::to_wstring(m_activeSessions.size()) + L" sesiuni de închidere...");

                            for (auto const& [sock, session] : m_activeSessions) {
                                // Trimitem mărimea pachetului și datele către fiecare socket
                                send(sock, (char*)&packetSize, sizeof(packetSize), 0);
                                send(sock, data.data(), (int)data.size(), 0);

                                // Opțional: trimitem și un mesaj text simplu pentru siguranță
                                shutdown(sock, SD_BOTH);
                                // send(sock, "FORCE_CLOSE", 11, 0); 
                            }
                        }

                        // 3. Sincronizăm datele pe disc
                        {
                            std::lock_guard<std::mutex> lock(engineMutex);
                            saveUsersInternal();
                            // engine.closeAll(); // Dacă ai implementat închiderea tabelelor
                        }

                        // 4. Lăsăm un mic buffer (200ms) pentru ca pachetele să plece prin rețea
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));

                        // 5. Oprim totul
                        m_running = false;
                        closesocket(listenSocket);

                        LOG_SUCCESS(L"Serverul s-a oprit controlat.");
                        exit(0);
                    }
                    continue;
                }

                if (msg.find("/sessions") == 0) {
                    vConResult res;
                    res.success = true;
                    res.message = L"Sesiuni active pe server:";
                    res.table.tableName = L"ActiveSessions";
                    res.table.columns = { L"USER", L"IP ADDRESS", L"ROLE" };

                    {
                        std::lock_guard<std::mutex> lock(m_sessionsMutex);
                        for (auto const& [sock, session] : m_activeSessions) {
                            std::vector<std::wstring> row;
                            row.push_back(session.username);
                            row.push_back(session.ip);
                            row.push_back(session.role == UserRole::ADMIN ? L"ADMIN" : L"USER");
                            res.table.records.push_back(row);
                        }
                    }
                    res.rowsAffected = res.table.records.size();

                    std::vector<char> data = serializeResult(res);
                    uint32_t packetSize = static_cast<uint32_t>(data.size());
                    send(clientSocket, (char*)&packetSize, sizeof(packetSize), 0);
                    send(clientSocket, data.data(), (int)data.size(), 0);
                    continue;
                }
            }
            if (msg == "/exit" || msg == "/quit") {
                sessionActive = false;
                send(clientSocket, "Goodbye!\n", 9, 0);
                continue;
            }

            // Execuție Query SQL
            executeAndSendQuery(clientSocket, msg, engine);
        }

        //LOG_INFO(L"[" + clientIP + L"] Sesiune terminată pentru " + user);

        {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            m_activeSessions.erase(clientSocket);
        }

        closesocket(clientSocket);
    }

    void executeAndSendQuery(SOCKET clientSocket, const std::string& msg, dbfConnection& engine) {
        std::wstring query = str_to_wstr(msg);
        vConResult result = {};

        {
            std::lock_guard<std::mutex> lock(engineMutex);
            if (engine.execQuery(query)) {
                result = engine.getLastQueryResult();
            }
            else {
                result.success = false;
                result.message = engine.getError();
            }
        }

        std::vector<char> data = serializeResult(result);
        uint32_t packetSize = static_cast<uint32_t>(data.size());

        // Trimitem lungimea pachetului
        if (send(clientSocket, reinterpret_cast<const char*>(&packetSize), sizeof(packetSize), 0) == SOCKET_ERROR) return;

        // Trimitem corpul pachetului într-o buclă (pentru date mari)
        int totalSent = 0;
        int dataSize = static_cast<int>(data.size());
        while (totalSent < dataSize) {
            int sent = send(clientSocket, data.data() + totalSent, dataSize - totalSent, 0);
            if (sent <= 0) break;
            totalSent += sent;
        }
    }

    void processAddUser(const std::string& cmd) {
        std::stringstream ss(cmd);
        std::string tag, u, p;
        std::string rStr; // Citim rolul ca string întâi pentru siguranță

        if (!(ss >> tag >> u >> p >> rStr)) {
            LOG_ERROR(L"Format invalid pentru /adduser!");
            return;
        }

        int r = std::stoi(rStr); // Convertim manual

        std::lock_guard<std::mutex> lock(m_userMutex);
        m_users[str_to_wstr(u)] = { str_to_wstr(u), str_to_wstr(p), (r == 0 ? UserRole::ADMIN : UserRole::USER) };

        if (!m_userFilePath.empty()) {
            // ATENȚIE: Nu apela saveUsers aici pentru că saveUsers încearcă să ia m_userMutex DIN NOU!
            // Va rezulta un DEADLOCK (blocaj total).
            saveUsersInternal();
            //LOG_INFO(L"Sistem: Utilizator nou salvat: " + str_to_wstr(u));
        }
    }

    void saveUsersInternal() {
        std::ofstream fout(wstr_to_str(m_userFilePath));
        if (!fout.is_open()) return;
        for (auto const& [name, profile] : m_users) {
            int r = (profile.role == UserRole::ADMIN) ? 0 : 1;
            fout << wstr_to_str(profile.username) << "=" << wstr_to_str(profile.password) << "," << r << "\n";
        }
    }

    // Funcția publică rămâne cu lock
    void saveUsers(const std::wstring& filePath) {
        std::lock_guard<std::mutex> lock(m_userMutex);
        if (m_users.empty()) return;
        saveUsersInternal();
    }
};

#endif // VNETWORKSERVER_HPP
