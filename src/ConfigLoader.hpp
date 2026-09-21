#ifndef CONFIGLOADER_HPP
#define CONFIGLOADER_HPP
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "stringUtils.hpp" // Presupunând că ai deja str_to_wstr aici

class ConfigLoader {
private:
    std::map<std::wstring, std::wstring> m_settings;
   
public:
    ConfigLoader() = default;

    bool load(const std::string& filepath) {
        std::ifstream fin(filepath);
        if (!fin.is_open()) return false;

        m_settings.clear();
        std::string line;

        while (std::getline(fin, line)) {

            // Eliminăm comentariile inline
            size_t hashPos = line.find('#');
            if (hashPos != std::string::npos)
                line = line.substr(0, hashPos);

            line = trim(line);

            if (line.empty()) continue;

            size_t sep = line.find('=');
            if (sep != std::string::npos) {
                std::string key = trim(line.substr(0, sep));
                std::string val = trim(line.substr(sep + 1));

                if (!val.empty() && val.front() == '\"') val.erase(0, 1);
                if (!val.empty() && val.back() == '\"') val.pop_back();

                if (!key.empty()) {
                    m_settings[str_to_wstr(key)] = str_to_wstr(val);
                }
            }
        }
        return true;
    }


    // Obținere valoare cu fallback
    std::wstring get(const std::wstring& key, const std::wstring& defaultValue = L"") {
        auto it = m_settings.find(key);
        return (it != m_settings.end()) ? it->second : defaultValue;
    }

    // Versiune pentru string-uri normale (overload)
    std::wstring get(const std::string& key, const std::wstring& defaultValue = L"") {
        return get(str_to_wstr(key), defaultValue);
    }
};
#endif