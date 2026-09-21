#ifndef SQLKEYWORDS_HPP
#define SQLKEYWORDS_HPP

#include <string>
#include <unordered_set>
#include <algorithm>

class SqlKeywords {
private:
    // Cuvinte cheie SQL Standard
    static inline const std::unordered_set<std::wstring> RESERVED_SQL = {
        L"SELECT", L"FROM", L"WHERE", L"INSERT", L"UPDATE", L"DELETE",
        L"CREATE", L"DROP", L"TABLE", L"SCHEMA", L"AS", L"AND", L"OR",
        L"NOT", L"NULL", L"IN", L"INTO", L"VALUES", L"JOIN", L"ON"
    };

    // Comenzi interne de Shell (Slash Commands)
    static inline const std::unordered_set<std::wstring> SHELL_COMMANDS = {
        L"/IMP", L"/CSV", L"/SAVE", L"/LOAD", L"/INFO", L"/D",
        L"/DESCRIBE", L"/T", L"/TABLES", L"/CLEAR", L"/HELP", L"/H"
    };

    static inline const std::unordered_set<std::wstring> DATA_TYPES = {
    L"INTEGER", L"DOUBLE", L"TEXT", L"DATE", L"BOOLEAN"
    };

    static inline const std::unordered_set<std::wstring> RESERVED = {
        L"SELECT", L"FROM", L"WHERE", L"AS", L"AND", L"OR", L"NOT",
        L"INSERT", L"INTO", L"VALUES", L"UPDATE", L"SET", L"DELETE",
        L"CREATE", L"TABLE", L"DROP", L"JOIN", L"ON", L"LIMIT"
    };

public:
    static bool isSqlKeyword(std::wstring word) {
        transformToUpper(word);
        return RESERVED_SQL.find(word) != RESERVED_SQL.end();
    }

    static bool isShellCommand(std::wstring word) {
        transformToUpper(word);
        return SHELL_COMMANDS.find(word) != SHELL_COMMANDS.end();
    }

    static bool isReserved(const std::wstring& word) {
        return isSqlKeyword(word) || isShellCommand(word);
    }
    
    static bool isKeyword(const std::wstring& word) {
        std::wstring upper = word;
        std::transform(upper.begin(), upper.end(), upper.begin(), ::towupper);
        return RESERVED.find(upper) != RESERVED.end();
    }

    static bool is(const std::wstring& word, const std::wstring& targetKeyword) {
        std::wstring w = word;
        std::transform(w.begin(), w.end(), w.begin(), ::towupper);
        return w == targetKeyword;
    }
    


private:
    static void transformToUpper(std::wstring& s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towupper);
    }
};

#endif