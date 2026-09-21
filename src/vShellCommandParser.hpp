
#ifndef vShellEngineCOMMANDPARSER_HPP
#define vShellEngineCOMMANDPARSER_HPP

#include <string>
#include <vector>

#include "vKeyWords.hpp"
#include "ConsoleManager.hpp"

struct ShellCommand {
    std::wstring name;
    std::vector<std::wstring> args;
    bool isValid = false;
};

class vShellEngineCommandParser {
private:

    // --- TOKENIZER ---
    static std::vector<std::wstring> tokenize(const std::wstring& line) {
        std::vector<std::wstring> tokens;
        std::wstring tok;
        bool inQuotes = false;

        auto flush = [&]() {
            if (!tok.empty()) {
                tokens.push_back(tok);
                tok.clear();
            }
            };

        for (size_t i = 0; i < line.size(); ++i) {
            wchar_t c = line[i];

            // toggle quotes
            if (c == L'"') {
                inQuotes = !inQuotes;
                continue;
            }

            if (!inQuotes) {
                // operators of 2 chars
                if (i + 1 < line.size()) {
                    std::wstring two = line.substr(i, 2);
                    if (two == L"==" || two == L"!=" || two == L">=" || two == L"<=" ||
                        two == L"&&" || two == L"||" || two == L">>") {
                        flush();
                        tokens.push_back(two);
                        i++;
                        continue;
                    }
                }

                // operators of 1 char
                if (wcschr(L"=+-*<>|;()", c)) {
                    flush();
                    tokens.push_back(std::wstring(1, c));
                    continue;
                }

                // whitespace
                if (iswspace(c)) {
                    flush();
                    continue;
                }
            }

            tok += c;
        }

        flush();
        return tokens;
    }

public:

    // --- PARSER ---
    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;

        if (line.empty() || line[0] != L'/')
            return cmd;

        auto tokens = tokenize(line);

        if (tokens.empty())
            return cmd;

        cmd.name = tokens[0];

        // validate command
        if (!vKeyWords::isShellCommand(cmd.name)) {
            LOG_ERROR(L"Unknown command: " + cmd.name);
            return cmd;
        }

        // rest are args
        for (size_t i = 1; i < tokens.size(); ++i)
            cmd.args.push_back(tokens[i]);

        cmd.isValid = true;
        return cmd;
    }
};

#endif

/*

#ifndef vShellEngineCOMMANDPARSER_HPP
#define vShellEngineCOMMANDPARSER_HPP

#include <string>
#include <vector>
#include <sstream>

#include "vKeyWords.hpp"
#include "ConsoleManager.hpp"

struct ShellCommand {
    std::wstring name;               // Ex: L"/imp"
    std::vector<std::wstring> args;   // [L"dbf", L"C:\My Data\file.dbf", L"public.table"]
    bool isValid = false;
};

class vShellEngineCommandParser {
public:
    static ShellCommand parse(const std::wstring& line) {
        ShellCommand cmd;

        if (line.empty() || line[0] != L'/') return cmd;

        if (cmd.isValid) {
            if (!vKeyWords::isShellCommand(cmd.name)) {
                LOG_ERROR(L"Unknown command: " + cmd.name);
                cmd.isValid = false;
            }
        }
       

        std::wstring currentToken;
        bool inQuotes = false;
        
        for (size_t i = 0; i < line.length(); ++i) {
            wchar_t c = line[i];

            if (c == L'\"') {
                inQuotes = !inQuotes;
                currentToken += c; // ADĂUGĂM ghilimeaua, nu o sărim!
                continue;
            }

            if (iswspace(c) && !inQuotes) { // iswspace e mai sigur decât L' '
                if (!currentToken.empty()) {
                    if (cmd.name.empty()) cmd.name = currentToken;
                    else cmd.args.push_back(currentToken);
                    currentToken.clear();
                }
            }
            else {
                currentToken += c;
            }
        }
        // Adăugăm ultimul token dacă există
        if (!currentToken.empty()) {
            if (cmd.name.empty()) cmd.name = currentToken;
            else cmd.args.push_back(currentToken);
        }

        cmd.isValid = !cmd.name.empty();
        return cmd;
    }

    

};

#endif
*/