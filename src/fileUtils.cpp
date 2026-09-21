
#include <filesystem>
#include <fstream>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#include <fstream>
#include <iostream>

#include "platform.hpp"


#include "fileUtils.hpp"
#include "stringUtils.hpp"
#include "ConsoleManager.hpp" // Adjust the path

//#include <sys/types.h>
//#include <sched.h>
//#include <exception>



    namespace fs = std::filesystem;

    //std::string csv_directory_path = ".";
    //std::string pdf_directory_path = ".";
    //std::string reports_directory_path = ".";

    std::map<std::wstring, std::wstring> cfg_file_vars;

    void create_dir_if_missing(const std::string& path) {
        //std::cout << "Verific directorul: " << path << std::endl;
        ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Verific directorul: " + str_to_wstr(path));

        if (path.empty()) {
            ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Path-ul este gol. Nu se poate crea directorul.");
            //std::cerr << "Path-ul este gol. Nu se poate crea directorul.\n";
            return;
        }

        try {
            if (!fs::exists(path)) {
                if (fs::create_directory(path)) {
                    //std::cout << "Directorul \"" << path << "\" a fost creat cu succes.\n";
                    ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Directorul \"" + str_to_wstr(path) + L"\" a fost creat cu succes.");
                }
                else {
                    std::cerr << "Eroare la crearea directorului \"" << path << "\".\n";
                }
            }
            else {
                //std::cout << "Directorul \"" << path << "\" exista deja.\n";
                ConsoleManager::getInstance().log(L"[LOG] create_dir_if_missing: Directorul \"" + str_to_wstr(path) + L"\" exista deja.");
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Exceptie: " << e.what() << "\n";
        }
    }





   

    std::string sanitizePath(const std::string& path) {
        if (!path.empty() && path.back() == '\\') {
            return path.substr(0, path.size() - 1);
        }
        return path;
    }


    


    





    bool copyFile(const std::string& source, const std::string& destination) {
        try {
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
            return true;  // Copiere reușită
        }
        catch (const std::exception& e) {
            std::cerr << "Eroare la copiere: " << e.what() << std::endl;
            return false;  // Copiere eșuată
        }
    }

    bool removeFile(const std::string& filePath) {
        std::filesystem::path path(filePath);
        return std::filesystem::remove(path);
    }

    std::string getCurrentDirectory() {
        return std::filesystem::current_path().string();
    }


    std::wstring utf8ToWstring(const std::string& utf8Str) {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, NULL, 0);
        std::wstring wstr(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), -1, &wstr[0], size_needed);
        return wstr;
    }

    // Funcție pentru citirea unui fișier CSV în std::vector<std::wstring>
    std::vector<std::wstring> readCSVFile(const std::string& filename) {
        std::vector<std::wstring> lines;
        std::ifstream file(filename);

        if (!file) {
            std::cerr << "Eroare la deschiderea fișierului: " << filename << std::endl;
            return lines;
        }

        std::string line;
        while (std::getline(file, line)) {
            // line = rm_char(line, '/"');
            lines.push_back(processStringForRTF(utf8ToWstring(line)));  // Convertim fiecare linie la std::wstring
          //  std::wcout << utf8ToWstring(line) << std::endl;
        }

        std::wcout << L"Am citit: " << lines.size() << std::endl;
        file.close();
        std::wcout << L"Am inchis fisierul cu: " << lines.size() << std::endl;

        return lines;
    }

    std::vector<std::wstring> readCSVFile2(const std::string& filename) {
        std::vector<std::wstring> lines;
        std::ifstream file(filename);

        if (!file) {
            std::cerr << "Eroare la deschiderea fisierului: " << filename << std::endl;
            return lines;
        }

        std::string line;
        while (std::getline(file, line)) {
            // NU eliminăm ghilimelele — sunt esențiale pentru parsarea corectă a valorilor cu virgulă
            std::wstring wline = utf8ToWstring(line);

            // Dacă ai nevoie de procesare suplimentară (ex: pentru afișare RTF), o poți aplica aici
            wline = processStringForRTF(wline);
            //std::wcout << "INCARC LINIA::::" << wline << std::endl;
            lines.push_back(wline);
        }

        std::wcout << L"Am citit: " << lines.size() << L" linii din fisier." << std::endl;
        file.close();
        std::wcout << L"AM INCHIS FISIERUL" << std::endl;
        return lines;
    }




    std::vector<std::string> getCsvFiles(const std::string& folderPath) {
        std::vector<std::string> csvFiles;
        try {
            for (const auto& entry : fs::directory_iterator(folderPath)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == ".csv") {
                        csvFiles.push_back(entry.path().filename().string());
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Eroare la accesarea directorului: " << e.what() << '\n';
        }


        return csvFiles;
    }

    bool load_options_file(std::string cfg_filepath) {
        std::ifstream fin(cfg_filepath);
        if (!fin) {
            std::cerr << "FILE ERROR: " << cfg_filepath << std::endl;
            return false;
        }
        std::string tmp;
        std::vector <std::string> vtmp;
        while (getline(fin, tmp)) {

            if (!tmp.empty()) {
                if (tmp[0] == '#') {
                    std::cerr << tmp << std::endl;
                    continue;
                }
            }
            else {
                continue;
            }

            //vtmp = explode(tmp, '=');
            vtmp = explode(rm_char(tmp, '\"'), '=');

            //if (vtmp[0] == "csv_directory_path") csv_directory_path = vtmp[1];
            //if (vtmp[0] == "pdf_directory_path") pdf_directory_path = vtmp[1];
            //if (vtmp[0] == "reports_directory_path") reports_directory_path = vtmp[1];
            if (vtmp[0].size() > 0 && vtmp[1].size() > 0)
                cfg_file_vars[str_to_wstr(vtmp[0])] = str_to_wstr(vtmp[1]);


        }
        fin.close();
        return true;
    }


    bool populate_config_map_from_file(const std::string& cfg_filepath, std::map<std::wstring, std::wstring>& out_map) {
        out_map.clear();
        std::ifstream fin(cfg_filepath);
        if (!fin) {
            std::cerr << "FILE ERROR: " << cfg_filepath << std::endl;
            return false;
        }

        std::string tmp;
        while (getline(fin, tmp)) {
            if (tmp.empty()) continue;
            if (tmp[0] == '#') {
                std::cerr << tmp << std::endl;
                continue;
            }

            std::vector<std::string> vtmp = explode(rm_char(tmp, '\"'), '=');
            if (vtmp.size() >= 2 && !vtmp[0].empty() && !vtmp[1].empty()) {
                out_map[str_to_wstr(vtmp[0])] = str_to_wstr(vtmp[1]);
            }
        }

        fin.close();
        return true;
    }


    std::string getFilenameWithoutExtension(const std::string& filepath) {
        std::filesystem::path p(filepath);
        return p.stem().string(); // .stem() returnează numele fără extensie
    }

    std::wstring getFilenameWithoutExtension(const std::wstring& filepath) {
        std::filesystem::path p(filepath);
        return p.stem().wstring(); // .stem() = nume fără extensie
    }



    bool fileExists(const std::wstring& path) {
        return std::filesystem::exists(path);
    }

    bool directoryExists(const std::wstring& path) {
        return std::filesystem::exists(path) && std::filesystem::is_directory(path);
    }


    std::vector<std::wstring> getFilesWithExtension(const std::wstring& directory, const std::wstring& extension) {
        std::vector<std::wstring> matchingFiles;
        std::wcout << "CAUT " << extension << " IN:" << directory << std::endl;
        try {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_regular_file() && entry.path().extension() == extension) {
                    matchingFiles.push_back(entry.path().wstring());
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::wcerr << L"Eroare la accesarea directorului: " << e.what() << std::endl;
        }

        return matchingFiles;
    }

    std::wstring get_file_name(const std::wstring& path) {
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            return path.substr(pos + 1);
        }
        return path; // Dacă nu există separator, presupunem că e deja numele fișierului
    }

    bool delete_files_by_extension(const std::wstring& dir_path, const std::wstring& extensie) {
        namespace fs = std::filesystem;
        bool sters = false;

        try {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == extensie) {
                        fs::remove(entry.path());
                        std::wcout << L"Sters: " << entry.path().filename() << std::endl;
                        sters = true;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::wcerr << L"Eroare: " << e.what() << std::endl;
            return false;
        }

        return sters;
    }

    bool renameFile(const std::string& oldName, const std::string& newName) {
        if (std::rename(oldName.c_str(), newName.c_str()) != 0) {
            std::cerr << "Eroare la redenumire: " << oldName << " → " << newName << std::endl;
            return false;
        }
        return true;
    }

    bool wrenameFile(const std::wstring& oldName, const std::wstring& newName) {
#ifdef _WIN32
        if (_wrename(oldName.c_str(), newName.c_str()) != 0) {
            std::wcerr << L"Eroare la redenumirea fișierului: " << oldName << L" → " << newName << std::endl;
            return false;
        }
#else
        try {
            fs::rename(fs::path(oldName), fs::path(newName));
        }
        catch (const fs::filesystem_error&) {
            std::wcerr << L"Eroare la redenumirea fișierului: " << oldName << L" → " << newName << std::endl;
            return false;
        }
#endif
        return true;
    }


    bool wcopyFile(const std::wstring& source, const std::wstring& destination) {
        try {
            fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
            std::wcerr << L"Am copiat: " << source << " in " << destination << std::endl;
            return true;  // Copiere reușită
        }
        catch (const fs::filesystem_error& e) {
            std::wcerr << L"Eroare la copiere: " << e.what() << std::endl;
            return false;  // Copiere eșuată
        }
    }

    int numaraFisiereCuExtensie(const std::string& caleDirector, const std::string& extensie) {
        int count = 0;

        try {
            for (const auto& entry : fs::directory_iterator(caleDirector)) {
                if (entry.is_regular_file()) {
                    if (entry.path().extension() == extensie) {
                        ++count;
                    }
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            std::cerr << "Eroare la accesarea directorului: " << e.what() << std::endl;
        }

        return count;
    }

    std::wstring getTempPath() {
#ifdef _WIN32
        std::vector<wchar_t> tempPath(MAX_PATH);
        DWORD length = GetTempPathW(MAX_PATH, tempPath.data());
        if (length > 0 && length < MAX_PATH) {
            return std::wstring(tempPath.data(), length);
        }
        return L""; // Returnează un șir gol în caz de eroare
    #else
        return fs::temp_directory_path().wstring();
    #endif
    }

    std::wstring getUniqueTempFilePath(const std::wstring& tempDir, const std::wstring& prefix) {
#ifdef _WIN32
        std::vector<wchar_t> tempFilePath(MAX_PATH);
        UINT result = GetTempFileNameW(
            tempDir.c_str(), // Directorul temporar
            prefix.c_str(),  // Prefixul fișierului
            0,               // Număr unic (0 pentru a lăsa sistemul să-l genereze)
            tempFilePath.data()
        );

        if (result != 0) {
            return std::wstring(tempFilePath.data());
        }
        return L""; // Returnează un șir gol în caz de eroare
    #else
        const auto path = fs::temp_directory_path() / (prefix + L"-qdbf-temp");
        return path.wstring();
    #endif
    }


    std::wstring ensureExtension(const std::wstring& name, const std::wstring& extension) {

        if (name.size() >= extension.size() &&
            name.compare(name.size() - extension.size(), extension.size(), extension) == 0) {
            return name; // deja se termină în .dbf
        }

        return name + extension; // adaugă extensia
    }




std::wstring citeste_fisier_utf8(const std::wstring& filePath) {
    // Deschidem fișierul în mod binar pentru a nu altera caracterele speciale
    std::ifstream file(fs::path(filePath), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return L"";
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (content.empty()) return L"";

    // Verificăm și eliminăm UTF-8 BOM dacă există (\xEF\xBB\xBF)
    size_t offset = 0;
    if (content.size() >= 3 && 
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) 
    {
        offset = 3;
    }

    // Conversie UTF-8 (std::string) -> UTF-16 (std::wstring) folosind API-ul Windows
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, content.c_str() + offset, static_cast<int>(content.size() - offset), NULL, 0);
    if (size_needed <= 0) return L"";

    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, content.c_str() + offset, static_cast<int>(content.size() - offset), &wstrTo[0], size_needed);

    return wstrTo;
}

