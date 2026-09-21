#include "ConsoleManager.hpp"
#include "SqlQueryParser.hpp"
#include "SqlKeyWords.hpp"
#include "stringUtils.hpp"

#include <sstream>
#include <algorithm>
#include <cwctype>
#include <set>


// Constructorul 1: Pentru query-ul principal
SqlQueryParser::SqlQueryParser(std::wstring qry)
    : query_str(qry), query(m_internalQuery)
{
    // ⭐ Protecție împotriva șirurilor goale ⭐
    if (wstr_trim(qry).empty()) {
        return; // Ișim liniștiți fără să setăm erori sau să crăpăm
    }

    LOG_INFO(L"🔍 CONSTRUCTOR 1 QRY: [" + qry + L"]");
    if (parse()) {
        printStructure();
    }
    else {
        LOG_ERROR(L"Parse failed: " + lastError.message);
    }
}

SqlQueryParser::SqlQueryParser(std::wstring qry, Query& targetQuery)
    : query_str(qry), query(targetQuery)
{
    LOG_INFO(L"🔍 CONSTRUCTOR 2 SUBQUERY QRY: [" + qry + L"]"); // <-- Adaugă aici
    if (!parse()) {
        LOG_ERROR(L"Subquery Parse failed: " + lastError.message);
    }
}

size_t findLogicalSplit(const std::wstring& section, std::wstring& foundOp) {
    int bracketLevel = 0;
    std::wstring upperSection = to_upper(section);

    size_t orPos = std::wstring::npos;
    size_t andPos = std::wstring::npos;

    for (size_t i = 0; i < upperSection.size(); ++i) {
        if (upperSection[i] == L'(') bracketLevel++;
        else if (upperSection[i] == L')') bracketLevel--;

        // Căutăm doar la nivelul 0 (în afara oricăror paranteze)
        if (bracketLevel == 0) {
            // Verificăm OR (prioritate mai mică, deci "rupem" aici prima dată)
            // Ne asigurăm că avem spații în jur ca să nu prindem cuvinte care conțin "OR"
            if (i + 3 < upperSection.size() && upperSection.substr(i, 4) == L" OR ") {
                foundOp = L"OR";
                return i + 1; // Returnăm poziția unde începe "OR"
            }

            // Verificăm AND (dacă nu am găsit OR până la final, îl vom folosi pe acesta)
            if (andPos == std::wstring::npos) {
                if (i + 4 < upperSection.size() && upperSection.substr(i, 5) == L" AND ") {
                    andPos = i + 1;
                }
            }
        }
    }

    if (andPos != std::wstring::npos) {
        foundOp = L"AND";
        return andPos;
    }

    return std::wstring::npos;
}


bool isFullyEnclosed(const std::wstring& str) {
    std::wstring s = wstr_trim(str);
    if (s.size() < 2) return false;
    if (s.front() != L'(' || s.back() != L')') return false;

    int bracketLevel = 0;
    // Mergem până la penultimul caracter
    for (size_t i = 0; i < s.size() - 1; ++i) {
        if (s[i] == L'(') bracketLevel++;
        else if (s[i] == L')') bracketLevel--;

        // Dacă nivelul ajunge la zero înainte de ultimul caracter,
        // înseamnă că paranteza inițială s-a închis prematur.
        // Ex: (A=B) AND (C=D) -> la indexul 4, level devine 0.
        if (bracketLevel == 0) return false;
    }

    return (bracketLevel == 1);
    // Dacă am ajuns aici și level e 1, înseamnă că ultima paranteză de la s.back() o va închide pe prima.
}


std::vector<std::wstring> wexplodeSQL(const std::wstring& str, wchar_t ch) {
    std::vector<std::wstring> res;
    std::wstring tempStr = wstr_trim(str);

    std::wstring current;
    int bracketLevel = 0;
    bool inQuotes = false;
    wchar_t quoteChar = 0; // Memorăm tipul de ghilimele deschise

    for (wchar_t c : tempStr) {
        // Toggle ghilimele (simple, duble sau backticks)
        if (c == L'\'' || c == L'\"' || c == L'`') {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c; // reținem cu ce am deschis
            }
            else if (c == quoteChar) {
                inQuotes = false; // închidem doar dacă e același tip
            }
        }

        if (!inQuotes) {
            if (c == L'(') bracketLevel++;
            if (c == L')') bracketLevel--;
        }

        // Tăiem DOAR dacă suntem la nivelul zero și nu în ghilimele
        if (c == ch && bracketLevel == 0 && !inQuotes) {
            res.push_back(wstr_trim(current));
            current.clear();
        }
        else {
            current += c;
        }
    }

    if (!current.empty()) res.push_back(wstr_trim(current));
    return res;
}

bool checkBrackets(const std::wstring& s) {
    int count = 0;
    for (wchar_t c : s) {
        if (c == L'(') count++;
        else if (c == L')') count--;
        if (count < 0) return false;
    }
    return count == 0;
}

std::vector<std::wstring> splitIgnoringQuotes(const std::wstring& input, wchar_t delimiter) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    int bracketLevel = 0;
    bool inQuotes = false;
    wchar_t quoteChar = 0;

    for (size_t i = 0; i < input.size(); ++i) {
        wchar_t c = input[i];

        // 1. Gestionăm ghilimelele (pentru a nu fi păcăliți de virgule în string-uri)
        if ((c == L'\'' || c == L'\"') && (i == 0 || input[i - 1] != L'\\')) {
            if (!inQuotes) {
                inQuotes = true;
                quoteChar = c;
            }
            else if (c == quoteChar) {
                inQuotes = false;
            }
        }

        // 2. Gestionăm parantezele (pentru a nu fi păcăliți de virgule în funcții/subquery)
        if (!inQuotes) {
            if (c == L'(') bracketLevel++;
            else if (c == L')') bracketLevel--;
        }

        // 3. Verificăm dacă am găsit separatorul la nivelul "zero" (nu în paranteze sau ghilimele)
        if (c == delimiter && bracketLevel == 0 && !inQuotes) {
            tokens.push_back(current);
            current.clear();
        }
        else {
            current += c;
        }
    }

    // Adăugăm și ultimul token
    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}


void QueryColumn::detectType() {
    std::wstring expr = wstr_trim(rawExpression);
    if (expr.empty()) { type = ColumnType::UNKNOWN; return; }
    
    if (expr == L"*") {
        this->type = ColumnType::WILDCARD; // <--- Foarte important!
        return;
    }

    std::wstring upperExpr = expr;
    // Corecție: towupper pentru wide characters
    std::transform(upperExpr.begin(), upperExpr.end(), upperExpr.begin(), ::towupper);

    // --- 1. AGREGARE (Versiunea compactă și flexibilă) ---
    static const std::vector<std::wstring> aggNames = { L"COUNT", L"SUM", L"AVG", L"MIN", L"MAX",L"STRING_AGG"};
   

    for (const auto& name : aggNames) {
        if (upperExpr.compare(0, name.length(), name) == 0) {
            size_t parenPos = name.length();

            // Ignorăm spațiile dintre nume și paranteză: SUM  (col)
            while (parenPos < upperExpr.size() && iswspace(upperExpr[parenPos])) parenPos++;

            if (parenPos < upperExpr.size() && upperExpr[parenPos] == L'(') {
                // Verificăm dacă se închide paranteza la final (simplificat)
                size_t lastParen = expr.find_last_of(L')');
                if (lastParen != std::wstring::npos && lastParen > parenPos) {
                    this->type = ColumnType::AGGREGATE;
                    this->aggregateFunc = name; // Păstrăm numele funcției (ex: STRING_AGG)

                    // Extragem interiorul: "nume, ', '"
                    this->aggregateArg = wstr_trim(expr.substr(parenPos + 1, lastParen - parenPos - 1));
                    return;
                }
            }
        }
    }

    // --- 1.5 SCALAR FUNCTIONS (Noua Secțiune) ---
    static const std::vector<std::wstring> scalarFuncs = { L"CONCAT", L"SUBSTR", L"UPPER", L"LOWER", L"TYPE", L"CONCAT_WS" };
    for (const auto& name : scalarFuncs) {
        if (upperExpr.compare(0, name.length(), name) == 0) {
            size_t parenPos = name.length();
            while (parenPos < upperExpr.size() && iswspace(upperExpr[parenPos])) parenPos++;

            if (parenPos < upperExpr.size() && upperExpr[parenPos] == L'(') {
                // Dacă se termină în paranteză, e clar o funcție scalară
                if (upperExpr.back() == L')') {
                    this->type = ColumnType::SCALAR_FUNCTION;
                    return;
                }
            }
        }
    }


    // --- 2. SUBQUERY ---
    if (expr.front() == L'(' && expr.back() == L')') {
        if (upperExpr.find(L"SELECT") != std::wstring::npos) {
            type = ColumnType::SUBQUERY;
            return;
        }
    }

    // --- 3. LITERAL (String sau Numeric) ---
    if ((expr.front() == L'\'' && expr.back() == L'\'') ||
        (expr.front() == L'\"' && expr.back() == L'\"')) {
        type = ColumnType::LITERAL;
        return;
    }

    // Verificare numerică îmbunătățită
    wchar_t* endPtr = nullptr;
    std::wcstod(expr.c_str(), &endPtr);
    if (endPtr != expr.c_str() && *endPtr == L'\0') {
        type = ColumnType::LITERAL;
        return;
    }

    // --- 4. EXPRESIE / FUNCȚIE NORMALĂ ---
    if (expr.find(L'(') != std::wstring::npos ||
        expr.find_first_of(L"+-*/") != std::wstring::npos) {
        type = ColumnType::EXPRESSION;
        return;
    }

    // --- 5. RAW_FIELD ---
    type = ColumnType::RAW_FIELD;
}

std::wstring SqlQueryParser::getFirstToken(const std::wstring& s) {
    // 1. Eliminăm spațiile de la început (trim left)
    size_t start = s.find_first_not_of(L" \t\n\r");
    if (start == std::wstring::npos) return L"";

    // 2. Găsim următorul spațiu alb după primul caracter valid
    size_t end = s.find_first_of(L" \t\n\r", start);

    // 3. Extragem subșirul
    if (end == std::wstring::npos) {
        return s.substr(start);
    }
    return s.substr(start, end - start);
}

bool SqlQueryParser::parse() {
    // 1. Resetăm obiectul query
    query = Query();

    std::wstring firstWord = getFirstToken(query_str);
    firstWord = to_upper(firstWord);
    
    if (firstWord == L"SELECT") {
        query.type = QueryType::SELECT;
        return parseSelect();
    }
    else if (firstWord == L"DELETE") {
        query.type = QueryType::DELETE_ROWS;
        return parseDelete(); // O metodă nouă pe care o vom scrie
    }
    else if (firstWord == L"UPDATE") {
        query.type = QueryType::UPDATE;
        return parseUpdate();
    }
    else if (firstWord == L"INSERT") {
        query.type = QueryType::INSERT;
        return parseInsert();
    }

    
    return setError(L"Unknown SQL command: " + firstWord);

}

size_t SqlQueryParser::findOutsideParens(const std::wstring& haystack, const std::wstring& needle) {
    int parenLevel = 0;
    bool inQuotes = false;

    if (needle.empty() || haystack.empty()) return std::wstring::npos;

    std::wstring upperHaystack = haystack;
    std::transform(upperHaystack.begin(), upperHaystack.end(), upperHaystack.begin(), ::towupper);
    std::wstring upperNeedle = needle;
    std::transform(upperNeedle.begin(), upperNeedle.end(), upperNeedle.begin(), ::towupper);

    for (size_t i = 0; i < upperHaystack.size(); ++i) {
        wchar_t c = upperHaystack[i];
        if (c == L'\'') inQuotes = !inQuotes;
        if (!inQuotes) {
            if (c == L'(') parenLevel++;
            else if (c == L')') parenLevel--;
        }

        if (parenLevel == 0 && !inQuotes) {
            // Verificăm dacă needle începe la poziția i
            if (upperHaystack.compare(i, upperNeedle.length(), upperNeedle) == 0) {
                return i;
            }
        }
    }
    return std::wstring::npos;
}


bool SqlQueryParser::parseSelect() {
    //LOG_INFO(L"vSqlEngine[parseSelect]: " + query_str);
    // 2. Curățăm query-ul de spații inutile sau punct și virgulă
    std::wstring sql = query_str;
    if (!sql.empty() && sql.back() == L';') sql.pop_back();

    // 3. Identificăm blocurile mari
    // Strategie: Căutăm SELECT, FROM, WHERE etc.
    // ATENȚIE: Nu căutăm doar "SELECT", ci ne asigurăm că nu e în interiorul unui subquery (paranteze)

    std::wstring upperSql = sql;
    std::transform(upperSql.begin(), upperSql.end(), upperSql.begin(), ::toupper);
    /*
    size_t selectPos = upperSql.find(L"SELECT");
    size_t fromPos = upperSql.find(L"FROM");
    size_t wherePos = upperSql.find(L"WHERE");
    size_t groupPos = upperSql.find(L"GROUP BY");
    size_t havingPos = upperSql.find(L"HAVING"); // Adăugat
    size_t orderPos = upperSql.find(L"ORDER BY");
    size_t limitPos = upperSql.find(L"LIMIT");
    size_t offsetPos = upperSql.find(L"OFFSET");
    */
    size_t selectPos = findOutsideParens(sql, L"SELECT");
    size_t fromPos = findOutsideParens(sql, L"FROM");
    size_t wherePos = findOutsideParens(sql, L"WHERE");
    size_t groupPos = findOutsideParens(sql, L"GROUP BY");
    size_t havingPos = findOutsideParens(sql, L"HAVING");
    size_t orderPos = findOutsideParens(sql, L"ORDER BY");
    size_t limitPos = findOutsideParens(sql, L"LIMIT");
    size_t offsetPos = findOutsideParens(sql, L"OFFSET");

    if (selectPos == std::wstring::npos) {
        return setError(L"The query must start with SELECT.");
    }
    size_t selectEnd = (fromPos != std::wstring::npos) ? fromPos : sql.size();
    // --- A. Procesăm SELECT (între SELECT și FROM) ---
    std::wstring selectSection = sql.substr(selectPos + 6, selectEnd - (selectPos + 6));
    if (!parseSelect(selectSection)) return false;

    // --- Procesăm FROM doar dacă există ---
    
    if (fromPos != std::wstring::npos) {
        size_t fromEnd = sql.size();

        if (wherePos != std::wstring::npos && wherePos > fromPos)   fromEnd = std::min<size_t>(fromEnd, wherePos);
        if (groupPos != std::wstring::npos && groupPos > fromPos)   fromEnd = std::min<size_t>(fromEnd, groupPos);
        if (havingPos != std::wstring::npos && havingPos > fromPos)  fromEnd = std::min<size_t>(fromEnd, havingPos);
        if (orderPos != std::wstring::npos && orderPos > fromPos)   fromEnd = std::min<size_t>(fromEnd, orderPos);
        if (limitPos != std::wstring::npos && limitPos > fromPos)   fromEnd = std::min<size_t>(fromEnd, limitPos);

        std::wstring fromSection = sql.substr(fromPos + 4, fromEnd - (fromPos + 4));
        if (!parseFrom(fromSection)) return false;
    }
    else {
        // Dacă nu avem FROM, fromTable rămâne goală. 
        // Engine-ul va ști că lucrează în mod "Virtual Table" sau "Dual".
        query.fromTable.name = L"";
        //LOG_INFO(L"Query fără clauză FROM detectat (calcul direct).");
    }
    


    // --- C. Procesăm WHERE ---
    
    if (wherePos != std::wstring::npos) {
        size_t whereEnd = sql.size();

        // Verificăm TOATE clauzele posibile care pot încheia WHERE
        if (groupPos != std::wstring::npos && groupPos > wherePos)   whereEnd = std::min<size_t>(whereEnd, groupPos);
        if (havingPos != std::wstring::npos && havingPos > wherePos)  whereEnd = std::min<size_t>(whereEnd, havingPos);
        if (orderPos != std::wstring::npos && orderPos > wherePos)   whereEnd = std::min<size_t>(whereEnd, orderPos);
        if (limitPos != std::wstring::npos && limitPos > wherePos)   whereEnd = std::min<size_t>(whereEnd, limitPos);
        if (offsetPos != std::wstring::npos && offsetPos > wherePos) whereEnd = std::min<size_t>(whereEnd, offsetPos);

        std::wstring whereSection = sql.substr(wherePos + 5, whereEnd - (wherePos + 5));
        query.whereRoot = parseRecursiveWhere(whereSection);
    }
    // --- E. Procesăm GROUP BY ---
    if (groupPos != std::wstring::npos) {
        // Ordinea corectă: GROUP BY -> HAVING -> ORDER BY
        size_t groupEnd = sql.size();

        // Dacă există HAVING, GROUP BY se termină acolo
        if (havingPos != std::wstring::npos) {
            groupEnd = havingPos;
        }
        // Dacă nu e HAVING, dar e ORDER BY, se termină acolo
        else if (orderPos != std::wstring::npos) {
            groupEnd = orderPos;
        }

        std::wstring groupSection = sql.substr(groupPos + 9, groupEnd - (groupPos + 9));
        parseGroupBy(groupSection);
    }

    // --- F. Procesăm HAVING ---
    if (havingPos != std::wstring::npos) {
        // HAVING se termină la ORDER BY sau la final
        size_t havingEnd = sql.size();
        if (orderPos != std::wstring::npos && orderPos > havingPos)   havingEnd = std::min<size_t>(havingEnd, orderPos);
        if (limitPos != std::wstring::npos && limitPos > havingPos)   havingEnd = std::min<size_t>(havingEnd, limitPos);
        if (offsetPos != std::wstring::npos && offsetPos > havingPos) havingEnd = std::min<size_t>(havingEnd, offsetPos);

        //size_t havingEnd = (orderPos != std::wstring::npos) ? orderPos : sql.size();
        std::wstring havingSection = sql.substr(havingPos + 7, havingEnd - (havingPos + 7));

        query.havingRoot = parseRecursiveWhere(havingSection);
    }

    // --- D. Procesăm ORDER BY (Update pentru a se opri la LIMIT/OFFSET) ---
    if (orderPos != std::wstring::npos) {
        size_t orderEnd = sql.size();
        if (limitPos != std::wstring::npos) orderEnd = limitPos;
        else if (offsetPos != std::wstring::npos) orderEnd = offsetPos;

        std::wstring orderSection = sql.substr(orderPos + 9, orderEnd - (orderPos + 9));
        parseOrderBy(orderSection);
    }

    // --- G. Procesăm LIMIT ---
    if (limitPos != std::wstring::npos) {
        size_t limitEnd = (offsetPos != std::wstring::npos) ? offsetPos : sql.size();
        std::wstring limitStr = wstr_trim(sql.substr(limitPos + 5, limitEnd - (limitPos + 5)));
        query.limit = std::stoi(limitStr);
    }

    // --- H. Procesăm OFFSET ---
    if (offsetPos != std::wstring::npos) {
        std::wstring offsetStr = wstr_trim(sql.substr(offsetPos + 6));
        try {
            query.offset = std::stoi(offsetStr);
        }
        catch (...) { return setError(L"Invalid OFFSET value.", offsetStr); }
    }

    return true;
}



bool SqlQueryParser::parseSelect(std::wstring section) {
    section = wstr_trim(section);
    if (section.empty()) return setError(L"The SELECT clause is empty.");

    // --- 1. Verificare DISTINCT ---
    std::wstring upperSection = to_upper(section);
    if (upperSection.substr(0, 9) == L"DISTINCT ") {
        query.isDistinct = true;
        section = wstr_trim(section.substr(9));
    }

    // --- 2. Spargerea în coloane (protejând parantezele și ghilimelele) ---
    std::vector<std::wstring> tokens = splitIgnoringQuotes(section, L',');

    for (const auto& rawCol : tokens) {
        std::wstring cleanCol = wstr_trim(rawCol);
        if (cleanCol.empty()) continue;

        QueryColumn col;
        std::wstring upperCol = to_upper(cleanCol);

        // --- 3. Extragerea Alias-ului ---
        size_t asPos = upperCol.find(L" AS ");

        if (asPos != std::wstring::npos) {
            // Cazul A: Explicit cu " AS " (ex: UPPER(nume) AS NumeMare)
            col.rawExpression = wstr_trim(cleanCol.substr(0, asPos));
            col.alias = stripQuotes(wstr_trim(cleanCol.substr(asPos + 4)));
        }
        else {
            // Cazul B: Implicit prin spațiu (ex: UPPER(nume) NumeMare)
            // Căutăm ultimul spațiu care NU se află în interiorul unor paranteze
            size_t lastSpace = std::wstring::npos;
            int depth = 0;

            for (int i = (int)cleanCol.size() - 1; i >= 0; --i) {
                if (cleanCol[i] == L')') depth++;
                else if (cleanCol[i] == L'(') depth--;
                else if (depth == 0 && iswspace(cleanCol[i])) { // iswspace prinde space, tab, etc.
                    lastSpace = i;
                    break;
                }
            }

            if (lastSpace != std::wstring::npos) {
                std::wstring potentialExpr = wstr_trim(cleanCol.substr(0, lastSpace));
                std::wstring potentialAlias = wstr_trim(cleanCol.substr(lastSpace + 1));

                // Ne asigurăm că expresia din stânga e completă (paranteze închise corect)
                if (!potentialAlias.empty() && checkBrackets(potentialExpr)) {
                    col.rawExpression = potentialExpr;
                    col.alias = stripQuotes(potentialAlias);
                }
                else {
                    col.rawExpression = cleanCol;
                }
            }
            else {
                col.rawExpression = cleanCol; // Niciun alias prezent
            }
        }

        // --- 4. Validarea expresiei ---
        if (col.rawExpression.empty()) {
            return setError(L"Invalid expression in the SELECT clause.");
        }

        // ⭐ 5. MAGIA NOUĂ: Generarea arborelui AST pentru evaluări complexe ⭐
        col.astRoot = buildExpressionAST(col.rawExpression);

        // --- 6. Compatibilitate Legacy: Setăm variabilele vechi ---
        col.detectType();

        // --- 7. Compatibilitate Legacy: Parsăm Subquery-ul (dacă există) ---
        if (col.type == ColumnType::SUBQUERY) {
            std::wstring subSql = col.rawExpression;

            // Eliminăm parantezele de la marginile subquery-ului
            if (subSql.front() == L'(') subSql.erase(0, 1);
            if (subSql.back() == L')') subSql.pop_back();
            subSql = wstr_trim(subSql);

            // Instanțiem și parsăm query-ul intern
            col.subSelect = std::make_shared<Query>();
            SqlQueryParser subParser(subSql, *col.subSelect);

            if (!subParser.parseSelect()) {
                return setError(L"Error parsing subquery: " + subSql);
            }
        }

        // Adăugăm coloana finalizată la structura query-ului
        query.columns.push_back(col);
    }

    return true;
}


bool SqlQueryParser::parseFrom(std::wstring section) {
    std::wstring trimmedSection = wstr_trim(section);
    if (trimmedSection.empty()) return true;

    query.fromTable = QueryTable();
    query.joins.clear();

    // 1. Găsim unde începe primul JOIN (dacă există)
    size_t firstJoinPos = findFirstJoinKeyword(trimmedSection);

    std::wstring mainTablesPart;
    std::wstring joinsPart;

    if (firstJoinPos != std::wstring::npos) {
        mainTablesPart = wstr_trim(trimmedSection.substr(0, firstJoinPos));
        joinsPart = wstr_trim(trimmedSection.substr(firstJoinPos));
    }
    else {
        mainTablesPart = trimmedSection;
    }

    // --- 2. PROCESĂM TABELUL PRINCIPAL / SUBQUERY-UL ---
    if (mainTablesPart.front() == L'(') {
        size_t lastParen = mainTablesPart.find_last_of(L')');
        if (lastParen == std::wstring::npos) return setError(L"Unclosed parenthesis in the subquery.", mainTablesPart);

        QueryTable qt;
        qt.isSubquery = true;
        std::wstring subQueryStr = wstr_trim(mainTablesPart.substr(1, lastParen - 1));
        qt.name = L"derived_table";

        qt.subSelect = std::make_shared<Query>();
        SqlQueryParser subParser(subQueryStr, *qt.subSelect);
        if (!subParser.parseSelect()) return setError(L"Subquery error: " + subParser.getLastError().message);

        std::wstring remaining = wstr_trim(mainTablesPart.substr(lastParen + 1));
        if (remaining.empty()) return setError(L"Missing alias for subquery.", mainTablesPart);

        if (to_upper(remaining).substr(0, 3) == L"AS ") {
            qt.alias = stripQuotes(wstr_trim(remaining.substr(3)));
        }
        else {
            qt.alias = stripQuotes(remaining);
        }
        query.fromTable = qt;
    }
    else {
        // Tabele clasice (cu sau fără virgulă)
        std::vector<std::wstring> tableTokens = wexplodeSQL(mainTablesPart, L',');
        for (auto& token : tableTokens) {
            std::wstring cleanToken = wstr_trim(token);
            if (cleanToken.empty()) continue;

            QueryTable qt;
            if (!parseSingleTableSource(cleanToken, qt)) return setError(L"Invalid source in the FROM clause.", cleanToken);

            if (query.fromTable.name.empty() && !query.fromTable.isSubquery) {
                query.fromTable = qt;
            }
            else {
                JoinClause jc;
                jc.type = JoinType::INNER;
                jc.table = qt;
                query.joins.push_back(jc);
            }
        }
    }

    // --- 3. PROCESĂM JOIN-URILE EXPLICITE ---
    if (!joinsPart.empty()) {
        if (!parseJoinsRecursive(joinsPart)) {
            return false;
        }
    }

    return true;
}

size_t SqlQueryParser::findFirstJoinKeyword(const std::wstring& str) {
    // Ordinea contează: formele lungi primele
    std::vector<std::wstring> keywords = {
        L" INNER JOIN", L" LEFT JOIN", L" RIGHT JOIN", L" FULL JOIN", L" JOIN"
    };

    size_t minPos = std::wstring::npos;
    for (const auto& kw : keywords) {
        // IMPORTANT: Căutăm doar în afara parantezelor!
        size_t pos = findOutsideParens(str, kw);
        if (pos != std::wstring::npos && (minPos == std::wstring::npos || pos < minPos)) {
            minPos = pos;
        }
    }
    return minPos;
}

std::vector<WhereClause> SqlQueryParser::parseOnConditions(const std::wstring& conditionStr) {
    std::vector<WhereClause> conditions;

    // Căutăm operatorul de egalitate
    size_t opPos = conditionStr.find(L'=');
    if (opPos != std::wstring::npos) {
        WhereClause wc;
        wc.oper = L"=";

        std::wstring leftPart = wstr_trim(conditionStr.substr(0, opPos));
        std::wstring rightPart = wstr_trim(conditionStr.substr(opPos + 1));

        // Reutilizăm logica ta de identificare a operanzilor
        wc.leftOperand.rawExpression = leftPart;
        wc.rightOperand.rawExpression = rightPart;

        // Dacă operandul conține punct, e clar un FIELD (ex: persoane.ID)
        wc.leftOperand.type = (leftPart.find(L'.') != std::wstring::npos) ? ColumnType::RAW_FIELD : ColumnType::LITERAL;
        wc.rightOperand.type = (rightPart.find(L'.') != std::wstring::npos) ? ColumnType::RAW_FIELD : ColumnType::LITERAL;

        conditions.push_back(wc);
    }

    return conditions;
}

bool SqlQueryParser::parseJoinsRecursive(std::wstring joinStr) {
    joinStr = wstr_trim(joinStr); // Lucrăm pe string-ul curat direct
    if (joinStr.empty()) return true;

    std::wstring upperJoin = to_upper(joinStr);
    JoinClause jc;
    jc.type = JoinType::INNER;

    // 1. Detectare tip (Folosim compare securizat)
    if (upperJoin.length() >= 5 && upperJoin.compare(0, 5, L"LEFT ") == 0) jc.type = JoinType::LEFT;
    else if (upperJoin.length() >= 6 && upperJoin.compare(0, 6, L"RIGHT ") == 0) jc.type = JoinType::RIGHT;
    else if (upperJoin.length() >= 5 && upperJoin.compare(0, 5, L"FULL ") == 0) jc.type = JoinType::FULL;
    else if (upperJoin.length() >= 6 && upperJoin.compare(0, 6, L"INNER ") == 0) jc.type = JoinType::INNER;

    // 2. Găsire ON
    size_t onPos = upperJoin.find(L" ON ");
    if (onPos == std::wstring::npos) return setError(L"Lipseste clauza ON", joinStr);

    // 3. Extragere Tabel (între JOIN și ON)
    size_t joinWordPos = upperJoin.find(L"JOIN ");
    if (joinWordPos == std::wstring::npos) return setError(L"Sintaxa JOIN invalida", joinStr);

    size_t startTable = joinWordPos + 5;
    std::wstring tablePart = wstr_trim(joinStr.substr(startTable, onPos - startTable));
    parseSingleTableSource(tablePart, jc.table);

    // 4. Extragere Condiție ON și Recursivitate
    std::wstring afterOn = wstr_trim(joinStr.substr(onPos + 4));
    std::wstring upperAfterOn = to_upper(afterOn);

    // Căutăm următorul JOIN în restul string-ului
    size_t nextJoinPos = findFirstJoinKeyword(upperAfterOn);

    if (nextJoinPos == std::wstring::npos) {
        jc.on_conditions = parseOnConditions(afterOn);
        query.joins.push_back(jc);
        return true;
    }
    else {
        // Izolăm condiția până la următorul cuvânt cheie (LEFT JOIN, etc.)
        std::wstring currentOnCondition = wstr_trim(afterOn.substr(0, nextJoinPos));
        jc.on_conditions = parseOnConditions(currentOnCondition);
        query.joins.push_back(jc);

        // RECURSIVITATE: Trimitem restul string-ului începând exact cu următorul JOIN
        return parseJoinsRecursive(afterOn.substr(nextJoinPos));
    }
}


bool SqlQueryParser::parseSingleTableSource(const std::wstring& token, QueryTable& qt) {
    std::wstring cleanToken = wstr_trim(token);
    if (cleanToken.empty()) return false;

    // Eliminăm virgula de la final dacă există (ex: "persoane,")
    if (cleanToken.back() == L',') {
        cleanToken.pop_back();
        cleanToken = wstr_trim(cleanToken);
    }

    std::vector<std::wstring> parts = wexplodeSQL(cleanToken, L' ');
    qt.name = stripQuotes(parts[0]);
    qt.alias = L""; // Reset

    if (parts.size() > 1) {
        std::wstring second = to_upper(parts[1]);
        // Listă neagră pentru Alias-uri (cuvinte care NU pot fi alias)
        static std::set<std::wstring> reserved = { L"JOIN", L"ON", L"INNER", L"LEFT", L"RIGHT", L"FULL", L"WHERE" };

        if (second == L"AS" && parts.size() > 2) {
            qt.alias = stripQuotes(parts[2]);
        }
        else if (reserved.find(second) == reserved.end()) {
            qt.alias = stripQuotes(parts[1]);
        }
    }

    if (qt.alias.empty()) qt.alias = qt.name;
    return true;
}

bool SqlQueryParser::parseWhere(std::wstring section) {
    std::wstring trimmed = wstr_trim(section);
    if (trimmed.empty()) return true;

    auto tokens = wexplodeSQL(trimmed, L' ');

    // Exemplu pentru o condiție simplă: AIRPORT = 'OTP'
    if (tokens.size() >= 3) {
        WhereClause clauza;

        // Setăm operandul stâng
        clauza.leftOperand.rawExpression = tokens[0];
        clauza.leftOperand.detectType(); // Va detecta dacă e FIELD sau EXPRESSION (ex: TYPE)

        // Setăm operatorul
        clauza.oper = to_upper(tokens[1]);

        // Setăm operandul drept
        clauza.rightOperand.rawExpression = tokens[2];
        clauza.rightOperand.detectType(); // Va detecta dacă e LITERAL ('OTP')

        // Acum push_back va funcționa pentru că folosim tipul corect: WhereClause
        query.where_clauses.push_back(clauza);
    }

    return true;
}


std::shared_ptr<WhereClause> SqlQueryParser::parseRecursiveWhere(std::wstring section) {
    section = wstr_trim(section);
    if (section.empty()) return nullptr;

    // 1. Curățăm straturile de paranteze: "((A=B))" -> "A=B"
    while (isFullyEnclosed(section)) {
        section = wstr_trim(section.substr(1, section.size() - 2));
    }

    // 2. DETECȚIE NOT (Negație Unară)
    // Dacă șirul începe cu NOT, marcăm și parsăm restul
    if (to_upper(section).substr(0, 4) == L"NOT ") {
        std::wstring rest = wstr_trim(section.substr(4));
        auto node = parseRecursiveWhere(rest);
        if (node) {
            node->isNegated = !node->isNegated; // Toggle negation
        }
        return node;
    }

    auto node = std::make_shared<WhereClause>();
    std::wstring foundOp;

    // 3. Căutăm cel mai "slab" operator (OR apoi AND) la nivelul 0
    size_t splitPos = findLogicalSplit(section, foundOp);

    if (splitPos != std::wstring::npos) {
        // AM GĂSIT UN GRUP (Nod părinte în arbore)
        node->isGroup = true;
        node->groupConnector = foundOp;

        // splitPos returnează începutul operatorului (ex: poziția lui 'A' în ' AND ')
        // Trebuie să extragem ce e în stânga și ce e în dreapta lui
        std::wstring leftPart = wstr_trim(section.substr(0, splitPos));

        // Calculăm restul șirului după operator (splitPos + lungimea operatorului)
        // Adăugăm 1 dacă splitPos a fost indexul spațiului de dinainte
        size_t rightStart = splitPos + foundOp.size();
        std::wstring rightPart = wstr_trim(section.substr(rightStart));

        // Recursivitate: desfacem în continuare ambele părți
        node->subClauses.push_back(parseRecursiveWhere(leftPart));
        node->subClauses.push_back(parseRecursiveWhere(rightPart));
    }
    else {
        // ESTE O CONDIȚIE FINALĂ (Frunză în arbore)
        // Nu mai avem AND/OR la nivelul 0, deci e ceva de genul "TYPE(ZI) = 'N'"
        node->isGroup = false;
        parseLeafCondition(node, section);
    }

    return node;
}


void SqlQueryParser::parseLeafCondition(std::shared_ptr<WhereClause> node, std::wstring condition) {
    condition = wstr_trim(condition);

    std::vector<std::wstring> operators = { L"NOT LIKE", L"NOT IN", L"!=", L"<=", L">=", L"<>", L"=", L"<", L">", L"LIKE", L"IN", L"BETWEEN" };

    size_t opPos = std::wstring::npos;
    std::wstring foundOp;

    int bracketLevel = 0;
    for (size_t i = 0; i < condition.size(); ++i) {
        if (condition[i] == L'(') bracketLevel++;
        else if (condition[i] == L')') bracketLevel--;

        if (bracketLevel == 0) {
            for (const auto& op : operators) {
                std::wstring sub = to_upper(condition.substr(i, op.size()));
                if (sub == op) {
                    if (iswalpha(op[0])) {
                        if (i > 0 && !iswspace(condition[i - 1])) continue;
                        if (i + op.size() < condition.size() && !iswspace(condition[i + op.size()])) continue;
                    }

                    opPos = i;
                    foundOp = sub;
                    break;
                }
            }
        }
        if (opPos != std::wstring::npos) break;
    }

    if (opPos != std::wstring::npos) {
        std::wstring leftExpr = wstr_trim(condition.substr(0, opPos));
        std::wstring rightExpr = wstr_trim(condition.substr(opPos + foundOp.size()));

        node->oper = foundOp;

        // ⭐ Folosim proprietățile existente din QueryColumn (leftOperand și rightOperand) ⭐
        node->leftOperand.rawExpression = leftExpr;
        node->leftOperand.detectType();
        node->leftOperand.astRoot = buildExpressionAST(leftExpr); // Aici se construiește AST-ul pentru stânga

        node->rightOperand.rawExpression = rightExpr;
        node->rightOperand.detectType();
        node->rightOperand.astRoot = buildExpressionAST(rightExpr); // Aici se construiește AST-ul pentru dreapta (inclusiv SUBQUERY)
    }
    else {
        std::wstring upperCond = to_upper(condition);
        size_t isPos = upperCond.find(L" IS ");
        if (isPos != std::wstring::npos) {
            std::wstring leftExpr = wstr_trim(condition.substr(0, isPos));

            node->leftOperand.rawExpression = leftExpr;
            node->leftOperand.detectType();
            node->leftOperand.astRoot = buildExpressionAST(leftExpr);

            node->oper = wstr_trim(condition.substr(isPos));
            node->isUnary = true;
        }
    }
}


bool SqlQueryParser::parseOrderBy(std::wstring section) {
    std::wstring trimmed = wstr_trim(section);
    if (trimmed.empty()) return true;

    // Spargem după virgulă pentru sortări multiple
    std::vector<std::wstring> parts = wexplodeSQL(trimmed, L',');

    for (auto& part : parts) {
        std::wstring cleanPart = wstr_trim(part);
        if (cleanPart.empty()) continue;

        OrderClause oc;

        // Căutăm dacă există ASC sau DESC la finalul bucății
        std::vector<std::wstring> tokens = wexplodeSQL(cleanPart, L' ');

        if (tokens.size() >= 2) {
            std::wstring lastToken = to_upper(tokens.back());
            if (lastToken == L"DESC") {
                oc.direction = OrderDirection::DESC;
                // Expresia coloanei este totul până la ultimul token
                tokens.pop_back();
            }
            else if (lastToken == L"ASC") {
                oc.direction = OrderDirection::ASC;
                tokens.pop_back();
            }
        }

        // Reconstruim expresia coloanei (în caz că era ceva de genul TYPE(X))
        std::wstring colExpr;
        for (const auto& t : tokens) colExpr += t + L" ";

        oc.column.rawExpression = wstr_trim(colExpr);
        oc.column.detectType(); // Foarte important: poți face ORDER BY TYPE(Coloana)!

        query.order_clauses.push_back(oc);
    }

    return true;
}


bool SqlQueryParser::parseGroupBy(std::wstring section) {
    std::wstring trimmed = wstr_trim(section);
    if (trimmed.empty()) return true;

    std::vector<std::wstring> parts = wexplodeSQL(trimmed, L',');

    for (auto& part : parts) {
        std::wstring cleanPart = wstr_trim(part);
        if (cleanPart.empty()) continue;

        GroupClause gc;
        // ACCESĂM rawExpression prin membrul 'column'
        gc.column.rawExpression = cleanPart;
        gc.column.detectType();

        // Opțional: poți inițializa gc.oper dacă ai nevoie
        gc.oper = L"";

        query.group_clauses.push_back(gc);
    }

    return true;
}

bool SqlQueryParser::parseHaving(std::wstring section) {
    std::wstring trimmed = wstr_trim(section);
    if (trimmed.empty()) return true;

    // Magie: refolosim parserul recursiv de WHERE
    query.havingRoot = parseRecursiveWhere(trimmed);

    return (query.havingRoot != nullptr);
}

// Helper pentru a curăța caracterele nedorite (cum ar fi virgulele după numele coloanelor)
std::wstring trimComma(std::wstring s) {
    if (!s.empty() && s.back() == L',') s.pop_back();
    return s;
}




void Query::printColumns() {
    LOG_INFO(L"--- [ Query Columns Structure ] ---");

    // Header tabel
    std::wstring header = L"Index | Type       | Alias          | Raw Expression";
    LOG(header);
    LOG(L"------------------------------------------------------------");

    for (size_t i = 0; i < columns.size(); ++i) {
        const auto& col = columns[i];

        // Convertim enum-ul în string pentru citibilitate
        std::wstring typeStr;
        switch (col.type) {
        case ColumnType::RAW_FIELD:       typeStr = L"FIELD     "; break;
        case ColumnType::LITERAL:         typeStr = L"LITERAL   "; break;
        case ColumnType::EXPRESSION:      typeStr = L"EXPRESSION"; break;
        case ColumnType::SCALAR_FUNCTION: typeStr = L"SCALAR_F  "; break; // Adăugat pentru funcții (ex: TYPE, UPPER)
        case ColumnType::AGGREGATE:       typeStr = L"AGGREGATE "; break; // Adăugat pentru SUM, COUNT
        case ColumnType::SUBQUERY:        typeStr = L"SUBQUERY  "; break;
        case ColumnType::WILDCARD:        typeStr = L"WILDCARD  "; break;
        default:                          typeStr = L"UNKNOWN   "; break;
        }

        std::wstring aliasStr = col.alias.empty() ? L"(none)" : col.alias;

        // Construim linia de log principală
        // Ajustare minoră pentru formatare la index >= 10
        std::wstring spaceAfterIndex = (i < 10) ? L"      | " : L"     | ";
        std::wstring line = std::to_wstring(i) + spaceAfterIndex +
            typeStr + L" | " +
            aliasStr + (aliasStr.length() < 14 ? std::wstring(14 - aliasStr.length(), L' ') : L"") + L" | " +
            col.rawExpression;

        // Folosim culori diferite pentru tipuri diferite
        if (col.type == ColumnType::SUBQUERY) {
            ConsoleManager::getInstance().writeRaw(line + L"\n", 11); // Cyan pentru subqueries
        }
        else if (col.type == ColumnType::EXPRESSION || col.type == ColumnType::SCALAR_FUNCTION) {
            ConsoleManager::getInstance().writeRaw(line + L"\n", 14); // Galben pentru funcții/calcule
        }
        else {
            LOG(line); // Default (Alb)
        }

        // --- ADAUGĂRI AST: Desenăm arborele dacă expresia a fost parsată ---
        if (col.astRoot != nullptr) {
            // Lăsăm un mic spațiu (prefix) pentru a se alinia frumos vizual sub linia de tabel
            printExprAST(col.astRoot, L"          ", true);
        }
    }
    LOG(L"------------------------------------------------------------");
}


void Query::printJoins() {
    LOG_INFO(L"--- [ Query Sources & Joins ] ---");

    if (fromTable.name.empty() && joins.empty()) {
        LOG_WARNING(L"Empty source (Virtual Dual Table / Constant Select).");
        return;
    }

    auto printTableInfo = [&](const QueryTable& qt, const std::wstring& prefix) {
        std::wstring typeStr = qt.isSubquery ? L"[SUBQUERY]" : L"[FILE]    ";
        std::wstring aliasStr = qt.alias.empty() ? L"(no alias)" : qt.alias;

        std::wstring line = prefix + L": " + typeStr + L" Name: " + qt.name +
            L" | Alias: " + aliasStr;

        if (qt.isSubquery) {
            ConsoleManager::getInstance().writeRaw(L"[DEBUG] " + line + L"\n", 11); // Cyan pentru subqueries
        }
        else {
            LOG(line); // Default
        }
    };

    // 1. Afișăm tabela principală (FROM)
    if (!fromTable.name.empty()) {
        printTableInfo(fromTable, L"MAIN (FROM)");
    }

    // 2. Afișăm Join-urile
    for (size_t i = 0; i < joins.size(); ++i) {
        std::wstring prefix = L"JOIN #" + std::to_wstring(i + 1);
        printTableInfo(joins[i].table, prefix);

        // Dacă avem condiții ON (pentru JOIN-uri explicite), le putem lista aici
        if (!joins[i].on_conditions.empty()) {
            LOG(L"       ON: (conditions present)");
        }
    }
    LOG(L"------------------------------------------------------------");
}

void Query::printWhere(std::shared_ptr<WhereClause> node, int level) {
    if (!node) return;

    std::wstring indent = std::wstring(level * 4, L' ');

    if (node->isGroup) {
        LOG_INFO(indent + L"GROUP [" + node->groupConnector + L"]");
        for (auto& sub : node->subClauses) {
            printWhere(sub, level + 1);
        }
    }
    else {
        std::wstring leafInfo = L"LEAF: " + node->leftOperand.rawExpression +
            L" " + node->oper +
            L" " + node->rightOperand.rawExpression;
        LOG_INFO(indent + L"├── " + leafInfo);
    }
}

void Query::printOrder() {
    if (order_clauses.empty()) return;
    LOG_INFO(L"--- [ Order By Clauses ] ---");
    for (auto& oc : order_clauses) {
        std::wstring dir = (oc.direction == OrderDirection::ASC) ? L"ASC" : L"DESC";
        LOG_INFO(L"Col: " + oc.column.rawExpression + L" | Direction: " + dir +
            L" | Type: " + std::to_wstring((int)oc.column.type));
    }
}

void Query::printGroup() {
    if (group_clauses.empty()) return;
    LOG_INFO(L"--- [ Group By Clauses ] ---");
    for (size_t i = 0; i < group_clauses.size(); ++i) {
        // Folosim group_clauses[i].column.rawExpression
        LOG_INFO(L"Index " + std::to_wstring(i) +
            L" | Expr: " + group_clauses[i].column.rawExpression +
            L" | Type: " + std::to_wstring((int)group_clauses[i].column.type));
    }
}

void SqlQueryParser::printStructure() {
    LOG_INFO(L"--- [ SQL QUERY STRUCTURE ] ---");
    LOG_INFO(L"Raw SQL: " + query_str);

    std::wstring typeStr = L"UNKNOWN";
    switch (query.type) {
    case QueryType::SELECT:      typeStr = L"SELECT"; break;
    case QueryType::INSERT:      typeStr = L"INSERT"; break;
    case QueryType::UPDATE:      typeStr = L"UPDATE"; break;
    case QueryType::DELETE_ROWS: typeStr = L"DELETE"; break;
    }
    LOG_INFO(L"Query Type: [" + typeStr + L"]");
    if (query.type == QueryType::UPDATE) {
        LOG_INFO(L"--- [ UPDATE SETS ] ---");
        LOG_INFO(L"Target Table: " + query.fromTable.name);
        for (auto const& [col, val] : query.updateSets) {
            LOG_INFO(L"   SET " + col + L" = " + val);
        }
    }

    // 1. Specifice pentru INSERT
    if (query.type == QueryType::INSERT) {
        LOG_INFO(L"--- [ INSERT DATA ] ---");
        LOG_INFO(L"Target Table: " + query.fromTable.name);

        if (!query.columns.empty()) {
            std::wstring cols = L"Target Columns: ";
            for (const auto& col : query.columns) cols += col.rawExpression + L", ";
            LOG_INFO(cols.substr(0, cols.length() - 2));
        }

        LOG_INFO(L"Rows to insert: " + std::to_wstring(query.insertValues.size()));
        for (size_t i = 0; i < query.insertValues.size(); ++i) {
            std::wstring rowVals = L"  Row [" + std::to_wstring(i) + L"]: ";
            for (const auto& val : query.insertValues[i]) rowVals += L"'" + val + L"', ";
            LOG_INFO(rowVals.substr(0, rowVals.length() - 2));
        }
    }

    // 2. Logica pentru SELECT / DELETE / UPDATE (Partajarea WHERE și JOINS)
    if (query.type == QueryType::SELECT || query.type == QueryType::DELETE_ROWS || query.type == QueryType::UPDATE) {

        if (query.type == QueryType::SELECT) {
            if (query.isDistinct) LOG_INFO(L"Option: [DISTINCT]");
            query.printColumns();
            query.printJoins();
        }
        else {
            LOG_INFO(L"Target Table: " + query.fromTable.name);
        }

        if (query.whereRoot) {
            LOG_INFO(L"--- [ WHERE Tree ] ---");
            query.printWhere();
        }

        if (query.type == QueryType::SELECT) {
            if (!query.group_clauses.empty()) query.printGroup();
            if (query.havingRoot) {
                LOG_INFO(L"--- [ HAVING Tree ] ---");
                query.printHaving();
            }
            if (!query.order_clauses.empty()) query.printOrder();
        }
    }

    // 3. Afișare tabele participante (Comun)
    if (query.type != QueryType::INSERT) {
        LOG_INFO(L"--- [ PARTICIPATING TABLES ] ---");
        std::vector<QueryTable> allTbls;
        query.collectTablesRecursive(allTbls);
        for (const auto& tbl : allTbls) {
            std::wstring source = tbl.isSubquery ? L"[Subquery]" : L"[File]";
            LOG_INFO(L"Source: " + tbl.name + L" " + source + L" Alias: " + tbl.alias);
        }
    }

    LOG_INFO(L"--- [ END STRUCTURE ] ---");
}


void Query::collectTablesRecursive(std::vector<QueryTable>& allTables) {
    // 1. Tabela principală
    if (!fromTable.name.empty() || fromTable.isSubquery) {
        allTables.push_back(fromTable);
        if (fromTable.isSubquery && fromTable.subSelect) {
            fromTable.subSelect->collectTablesRecursive(allTables);
        }
    }

    // 2. JOIN-uri
    for (auto& join : joins) {
        allTables.push_back(join.table);
        if (join.table.isSubquery && join.table.subSelect) {
            join.table.subSelect->collectTablesRecursive(allTables);
        }
    }
    // 2. Căutăm în coloanele din SELECT (proiecție)
    for (const auto& col : columns) {
        if (col.type == ColumnType::SUBQUERY && col.subSelect) {
            col.subSelect->collectTablesRecursive(allTables);
        }
    }

    // 3. WHERE & HAVING
    collectTablesFromWhere(whereRoot, allTables);
    collectTablesFromWhere(havingRoot, allTables);
    // 3. WHERE
    collectTablesFromWhere(whereRoot, allTables);
}

// Metodă pentru a extrage tabele din arborele WHERE
void Query::collectTablesFromWhere(std::shared_ptr<WhereClause> node, std::vector<QueryTable>& allTables) {
    if (!node) return;

    if (node->isGroup) {
        for (const auto& child : node->subClauses) {
            collectTablesFromWhere(child, allTables);
        }
    }
    else {
        // Verificăm leftOperand: folosim enum-ul ColumnType::SUBQUERY
        if (node->leftOperand.type == ColumnType::SUBQUERY && node->leftOperand.subSelect) {
            node->leftOperand.subSelect->collectTablesRecursive(allTables);
        }

        // Verificăm rightOperand
        if (node->rightOperand.type == ColumnType::SUBQUERY && node->rightOperand.subSelect) {
            node->rightOperand.subSelect->collectTablesRecursive(allTables);
        }
    }
}

bool SqlQueryParser::parseInsert() {
    std::wstring sql = query_str;
    std::wstring upperSql = to_upper(sql);

    size_t intoPos = upperSql.find(L"INTO");
    size_t valuesPos = upperSql.find(L"VALUES");

    if (intoPos == std::wstring::npos || valuesPos == std::wstring::npos)
        return setError(L"Syntax error in INSERT statement");

    // --- 1. Extragere Tabel și Coloane ---
    std::wstring tablePart = wstr_trim(sql.substr(intoPos + 4, valuesPos - (intoPos + 4)));
    size_t openParen = tablePart.find(L'(');

    if (openParen != std::wstring::npos) {
        //query.fromTable.name = wstr_trim(tablePart.substr(0, openParen));
        std::wstring rawTableName = wstr_trim(tablePart.substr(0, openParen));
        query.fromTable.name = stripQuotes(rawTableName);

        size_t closeParen = tablePart.find(L')', openParen);

        if (closeParen != std::wstring::npos) {
            std::wstring colsStr = tablePart.substr(openParen + 1, closeParen - openParen - 1);
            // Folosim wexplode aici pentru că numele coloanelor nu conțin virgule
            std::vector<std::wstring> colNames = wexplode(colsStr, L',');
            for (const auto& name : colNames) {
                QueryColumn col;
                col.rawExpression = name;
                col.type = ColumnType::RAW_FIELD;
                query.columns.push_back(col);
            }
        }
    }
    else {
        //query.fromTable.name = tablePart;
        query.fromTable.name = stripQuotes(tablePart);
    }

    // --- 2. Extragere Valori (Folosind o variantă safe de explode) ---
    std::wstring valuesContent = wstr_trim(sql.substr(valuesPos + 6));
    size_t currentPos = 0;
    while ((currentPos = valuesContent.find(L'(', currentPos)) != std::wstring::npos) {
        size_t endParen = valuesContent.find(L')', currentPos);
        if (endParen == std::wstring::npos) break;

        std::wstring rowStr = valuesContent.substr(currentPos + 1, endParen - currentPos - 1);

        // IMPORTANT: Aici folosim varianta "Quote Safe" pentru a nu tăia în interiorul '...'
        std::vector<std::wstring> rowValues = wexplodeQuoteSafe(rowStr, L',');

        for (auto& v : rowValues) {
            if (!v.empty() && v.front() == L'\'' && v.back() == L'\'')
                v = v.substr(1, v.length() - 2);
        }

        query.insertValues.push_back(rowValues);
        currentPos = endParen + 1;
    }

    return !query.insertValues.empty();
}


bool SqlQueryParser::parseDelete() {
    std::wstring sql = query_str;
    std::wstring upperSql = to_upper(sql);

    // 1. Găsim "FROM" pentru a identifica tabelul
    size_t fromPos = upperSql.find(L"FROM");
    if (fromPos == std::wstring::npos) {
        return setError(L"Syntax error: Missing FROM in DELETE statement");
    }

    // 2. Căutăm clauza WHERE (opțională, dar critică)
    size_t wherePos = upperSql.find(L"WHERE");

    std::wstring tableName;
    if (wherePos != std::wstring::npos) {
        // Avem WHERE, deci tabelul e între FROM și WHERE
        tableName = wstr_trim(sql.substr(fromPos + 4, wherePos - (fromPos + 4)));

        // Parsăm restul ca fiind clauza WHERE folosind metoda existentă
        std::wstring whereSection = wstr_trim(sql.substr(wherePos + 5));
        if (!whereSection.empty()) {
            query.whereRoot = parseRecursiveWhere(whereSection);
        }
    }
    else {
        // Nu avem WHERE, ștergem tot din tabel
        tableName = wstr_trim(sql.substr(fromPos + 4));
    }

    query.fromTable.name = stripQuotes(tableName);
    query.type = QueryType::DELETE_ROWS;

    LOG_INFO(L"vSqlEngine[parseDelete]: Table=" + tableName +
        (query.whereRoot ? L" with WHERE clause" : L" (ALL records)"));

    return true;
}

bool SqlQueryParser::parseUpdate() {
    std::wstring sql = query_str;
    std::wstring upperSql = to_upper(sql);

    size_t updatePos = upperSql.find(L"UPDATE");
    size_t setPos = upperSql.find(L"SET");
    size_t wherePos = upperSql.find(L"WHERE");

    if (updatePos == std::wstring::npos || setPos == std::wstring::npos)
        return setError(L"Syntax error: Missing UPDATE or SET");

    // 1. Extragem tabelul (între UPDATE și SET)
    //query.fromTable.name = wstr_trim(sql.substr(updatePos + 6, setPos - (updatePos + 6)));
    std::wstring rawTableName = wstr_trim(sql.substr(updatePos + 6, setPos - (updatePos + 6)));
    query.fromTable.name = stripQuotes(rawTableName);

    // 2. Extragem clauza SET (perechi coloana = valoare)
    std::wstring setSection;
    if (wherePos != std::wstring::npos) {
        setSection = sql.substr(setPos + 3, wherePos - (setPos + 3));
        // Parsăm și WHERE-ul
        std::wstring whereSection = wstr_trim(sql.substr(wherePos + 5));
        query.whereRoot = parseRecursiveWhere(whereSection);
    }
    else {
        setSection = sql.substr(setPos + 3);
    }

    // Spargem setSection în perechi după virgulă
    std::vector<std::wstring> pairs = wexplodeQuoteSafe(setSection, L',');
    for (const auto& pair : pairs) {
        size_t eqPos = pair.find(L'=');
        if (eqPos != std::wstring::npos) {
            std::wstring col = to_upper(wstr_trim(pair.substr(0, eqPos)));
            std::wstring val = wstr_trim(pair.substr(eqPos + 1));

            // Curățăm ghilimelele de la valori dacă există
            if (val.size() >= 2 && val.front() == L'\'' && val.back() == L'\'')
                val = val.substr(1, val.size() - 2);

            query.updateSets[col] = val;
        }
    }

    query.type = QueryType::UPDATE;
    return !query.updateSets.empty();
}
// O funcție ajutătoare în SqlQueryParser pentru a construi AST-ul unei expresii
std::shared_ptr<ExprASTNode> SqlQueryParser::buildExpressionAST(std::wstring expr) {
    expr = wstr_trim(expr);
    if (expr.empty()) return nullptr;

    std::wstring upperExpr = to_upper(expr);

    // 1. Verificare prioritară SUBQUERY: Dacă e încadrat în paranteze și începe cu SELECT
    if (upperExpr.front() == L'(' && upperExpr.back() == L')') {
        std::wstring innerTrimmed = wstr_trim(expr.substr(1, expr.size() - 2));
        if (to_upper(innerTrimmed).find(L"SELECT") == 0) {
            auto node = std::make_shared<ExprASTNode>(ExprNodeType::SUBQUERY, L"SUBQUERY");
            node->subQuery = std::make_shared<Query>();
            

            
            // Asigură-te că innerTrimmed nu e gol înainte de parsare!
            if (!innerTrimmed.empty()) {
                SqlQueryParser subParser(innerTrimmed, *node->subQuery);
            }
            return node;
        }
    }

    // 2. Curățăm parantezele exterioare doar dacă încadrează o expresie normală (ex: (5 + 3))
    if (isFullyEnclosed(expr)) {
        expr = wstr_trim(expr.substr(1, expr.size() - 2));
        upperExpr = to_upper(expr);
    }

    // 3. Căutăm operatori binari (+, -, *, /) în AFARA parantezelor
    // Începem cu cei cu prioritate mică (+, -)
    std::vector<std::wstring> operators = { L"+", L"-", L"*", L"/" };

    for (const auto& op : operators) {
        int bracketLevel = 0;
        bool inQuotes = false;

        // Parcurgem invers pentru a asocia corect la stânga (ex: 5 - 3 - 1 devine (5 - 3) - 1)
        for (int i = (int)expr.size() - 1; i >= 0; --i) {
            if (expr[i] == L'\'') inQuotes = !inQuotes;
            if (!inQuotes) {
                if (expr[i] == L')') bracketLevel++;
                else if (expr[i] == L'(') bracketLevel--;
            }

            if (bracketLevel == 0 && !inQuotes) {
                // Dacă am găsit operatorul
                if (expr.compare(i, op.length(), op) == 0) {
                    auto node = std::make_shared<ExprASTNode>(ExprNodeType::BINARY_OP, op);

                    std::wstring leftPart = expr.substr(0, i);
                    std::wstring rightPart = expr.substr(i + op.length());

                    // Parsăm recursiv stânga și dreapta
                    node->children.push_back(buildExpressionAST(leftPart));
                    node->children.push_back(buildExpressionAST(rightPart));

                    return node;
                }
            }
        }
    }

    // 4. Verificăm dacă e FUNCȚIE (ex: TYPE(varsta), UPPER(nume))
    size_t firstParen = expr.find(L'(');
    if (firstParen != std::wstring::npos && expr.back() == L')') {
        std::wstring funcName = wstr_trim(expr.substr(0, firstParen));
        // Dacă numele funcției e valid (alfanumeric)
        if (!funcName.empty() && iswalpha(funcName[0])) {
            auto node = std::make_shared<ExprASTNode>(ExprNodeType::FUNCTION_CALL, to_upper(funcName));

            std::wstring argsStr = wstr_trim(expr.substr(firstParen + 1, expr.size() - firstParen - 2));
            std::vector<std::wstring> args = splitIgnoringQuotes(argsStr, L',');
            for (const auto& arg : args) {
                node->children.push_back(buildExpressionAST(arg));
            }
            return node;
        }
    }

    // Funcție lambda pentru a detecta dacă un șir este numeric
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

    // 5. Dacă nu e niciuna, e LITERAL sau COLUMN_REF
    if ((expr.front() == L'\'' && expr.back() == L'\'') || isNumeric(expr)) {
        return std::make_shared<ExprASTNode>(ExprNodeType::LITERAL, expr);
    }

    return std::make_shared<ExprASTNode>(ExprNodeType::COLUMN_REF, expr);
}


void Query::printExprAST(std::shared_ptr<ExprASTNode> node, std::wstring prefix, bool isLast) {
    if (!node) return;

    // 1. Traducem tipul nodului într-un text lizibil
    std::wstring typeStr;
    switch (node->type) {
    case ExprNodeType::LITERAL:       typeStr = L"LITERAL"; break;
    case ExprNodeType::COLUMN_REF:    typeStr = L"COLUMN"; break;
    case ExprNodeType::FUNCTION_CALL: typeStr = L"FUNCTION"; break;
    case ExprNodeType::BINARY_OP:     typeStr = L"OPERATOR"; break;
    case ExprNodeType::SUBQUERY:      typeStr = L"SUBQUERY"; break;
    default:                          typeStr = L"UNKNOWN"; break;
    }

    // 2. Alegem graficele pentru ramuri
    std::wstring marker = isLast ? L"└── " : L"├── ";

    // 3. Afișăm nodul curent
    std::wstring nodeText = prefix + marker + L"[" + typeStr + L"] " + node->value;

    // Dacă e subquery, putem pune un indicator
    if (node->type == ExprNodeType::SUBQUERY) {
        nodeText += L" (Complex SQL Expression)";
    }

    // Folosim o culoare diferită pentru a-l evidenția (opțional)
    // Sau pur și simplu LOG_INFO
    ConsoleManager::getInstance().writeRaw(nodeText + L"\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);

    // 4. Pregătim prefixul pentru copiii nodului curent
    std::wstring childPrefix = prefix + (isLast ? L"    " : L"│   ");

    // 5. Apelăm recursiv pentru fiecare copil
    for (size_t i = 0; i < node->children.size(); ++i) {
        bool isLastChild = (i == node->children.size() - 1);
        printExprAST(node->children[i], childPrefix, isLastChild);
    }
}