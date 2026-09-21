#include "SqlQueryEngine.hpp"
#include "stringUtils.hpp"
#include "ConsoleManager.hpp"
//#include "..\math\vmath.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cwctype>


void vSqlEngine::registerHandlers() {
    // Egalitate
    m_opHandlers[L"="] = [](const std::wstring& l, const std::wstring& r) {
        return l == r;
    };

    // Diferit
    m_opHandlers[L"!="] = [](const std::wstring& l, const std::wstring& r) {
        return l != r;
    };
    m_opHandlers[L"<>"] = m_opHandlers[L"!="];


    // Operatori numerici (folosind asNumber definit anterior)
    m_opHandlers[L">"] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) > asNumber(r);
    };


    m_opHandlers[L"<"] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) < asNumber(r);
    };

    m_opHandlers[L"<="] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) <= asNumber(r);
    };

    m_opHandlers[L">="] = [this](const std::wstring& l, const std::wstring& r) {
        return asNumber(l) >= asNumber(r);
    };

    // Operatorul LIKE
    m_opHandlers[L"LIKE"] = [this](const std::wstring& l, const std::wstring& r) {
        return this->handleLikeOp(l, r);
    };

    // Înregistrăm NOT LIKE (pur și simplu negăm rezultatul aceleiași metode)
    m_opHandlers[L"NOT LIKE"] = [this](const std::wstring& l, const std::wstring& r) {
        return !this->handleLikeOp(l, r);
    };


    // FUNCTII AGREGARE
    // COUNT: incrementăm valoarea numerică existentă
    m_aggHandlers[L"COUNT"] = [](const std::wstring& current, const std::wstring& next) -> std::wstring {
        // Dacă e prima dată (current e gol), pornim de la 0, altfel incrementăm
        long long count = current.empty() ? 0 : std::stoll(current);
        return std::to_wstring(count + 1);
    };

    m_aggHandlers[L"SUM"] = [this](const std::wstring& current, const std::wstring& next) {
        double sum = current.empty() ? 0.0 : asNumber(current);
        double val = next.empty() ? 0.0 : asNumber(next);

        // Formatare fără zerouri inutile
        std::wstringstream ss;
        ss << (sum + val);
        return ss.str();
    };

    m_aggHandlers[L"MIN"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        if (next.empty()) return current;
        if (current.empty()) return next; // Prima valoare devine minimul curent

        double curMin = asNumber(current);
        double val = asNumber(next);

        std::wstringstream ss;
        ss << (val < curMin ? val : curMin);
        return ss.str();
    };

    m_aggHandlers[L"MAX"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        if (next.empty()) return current;
        if (current.empty()) return next; // Prima valoare devine maximul curent

        double curMax = asNumber(current);
        double val = asNumber(next);

        std::wstringstream ss;
        ss << (val > curMax ? val : curMax);
        return ss.str();
    };

    m_aggHandlers[L"AVG"] = [this](const std::wstring& current, const std::wstring& next) -> std::wstring {
        double sum = 0.0;
        long long count = 0;

        if (!current.empty()) {
            size_t sep = current.find(L':');
            if (sep != std::wstring::npos) {
                sum = std::stod(current.substr(0, sep));
                count = std::stoll(current.substr(sep + 1));
            }
        }

        if (!next.empty()) {
            sum += asNumber(next);
            count++;
        }

        std::wstringstream ss;
        ss << sum << L":" << count;
        return ss.str();
    };

    m_aggHandlers[L"STRING_AGG"] = [](const std::wstring& current, const std::wstring& nextRaw) -> std::wstring {
        if (nextRaw.empty()) return current;

        std::wstring val = nextRaw;
        std::wstring sep = L", "; // Default

        // Căutăm delimitatorul nostru special
        size_t sepPos = nextRaw.find(L"|SEP|");
        if (sepPos != std::wstring::npos) {
            val = nextRaw.substr(0, sepPos);
            std::wstring rawSep = nextRaw.substr(sepPos + 5); // +5 pentru lungimea "|SEP|"

            // Curățăm separatorul de ghilimele (ex: ', ' devine , )
            if (rawSep.size() >= 2 && (rawSep.front() == L'\'' || rawSep.front() == L'\"')) {
                rawSep = rawSep.substr(1, rawSep.size() - 2);
            }
            sep = rawSep;
        }

        if (current.empty()) return val;
        return current + sep + val;
    };


    // --- Funcții scalare ---

    // UPPER(str)
    m_funcHandlers[L"UPPER"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        std::wstring res = args[0];
        std::transform(res.begin(), res.end(), res.begin(), ::towupper);
        return res;
    };

    // UPPER(str)
    m_funcHandlers[L"LOWER"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        std::wstring res = args[0];
        std::transform(res.begin(), res.end(), res.begin(), ::tolower);
        return res;
    };

    // SUBSTR(str, start, [len])
    m_funcHandlers[L"SUBSTR"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.size() < 2) return L"";
        std::wstring str = args[0];
        try {
            int start = std::stoi(args[1]) - 1;
            int len = (args.size() > 2) ? std::stoi(args[2]) : (int)str.size();

            if (start < 0) start = 0;
            if (start >= (int)str.size()) return L"";

            return str.substr(start, len); // Acum compilatorul știe că vrei wstring
        }
        catch (...) {
            return L"";
        }
    };

    // TRIM(str)
    m_funcHandlers[L"TRIM"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"";
        return wstr_trim(args[0]);
    };

    // TYPE(val) - Returnează tipul datei
    m_funcHandlers[L"TYPE"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        if (args.empty()) return L"U"; // Unknown
        return args[0]; // Returnăm tipul pe care resolveExpression l-a identificat deja
    };

    m_funcHandlers[L"CONCAT"] = [](const std::vector<std::wstring>& args) -> std::wstring {
        std::wstring result = L"";
        for (const auto& arg : args) {
            result += arg;
        }
        return result;
    };
}

vConTable* vSqlEngine::findTableInUniverse(const std::wstring& nameOrAlias) {
    std::wstring search = to_upper(nameOrAlias);
    for (auto& table : m_sourceTables) {
        if (to_upper(table.tableAlias) == search || to_upper(table.tableName) == search) {
            return &table;
        }
    }
    return nullptr;
}

vConResult vSqlEngine::execute(const SqlQueryParser& parser) {

    const Query& q = parser.getQuery();
    //const Query& q = getCurrentQuery();

    switch (q.type) {
    case QueryType::SELECT:
        return executeSelect(parser);
    case QueryType::INSERT:
        return executeInsert(parser);
    case QueryType::UPDATE:
        return executeUpdate(parser);
    case QueryType::DELETE_ROWS:
        return executeDelete(parser);
        // ...
    }
}

vConResult vSqlEngine::executeInsert(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();
        //const Query& q = getCurrentQuery();
        if (m_sourceTables.empty())
            throw std::runtime_error("Tabelul tinta nu a putut fi incarcat.");

        // Tabelul în care inserăm este întotdeauna primul (fromTable)
        const vConTable& target = m_sourceTables[0];

        // Pregătim un vConTable care va conține DOAR rândurile noi
        // Acesta va fi "pachetul" pe care dbfConnection îl va scrie pe disc
        vConTable rowsToAdd;
        rowsToAdd.tableName = target.tableName;
        rowsToAdd.columns = target.columns;
        rowsToAdd.columnTypes = target.columnTypes;
        rowsToAdd.columnWidths = target.columnWidths;

        // Procesăm fiecare set de valori (INSERT INTO ... VALUES (...), (...))
        // q.insertValues este std::vector<std::vector<std::wstring>>
        for (const auto& valueRow : q.insertValues) {

            // Cream un rând gol cu dimensiunea corectă a tabelului țintă
            std::vector<std::wstring> newRecord(target.columns.size(), L"");

            // Dacă avem coloane specificate: INSERT INTO tab (col1, col3) VALUES (val1, val3)
            if (!q.columns.empty()) {
                for (size_t i = 0; i < q.columns.size(); ++i) {
                    // Găsim unde trebuie să ajungă valoarea în structura DBF
                    int targetIdx = target.getColumnIndex(to_upper(q.columns[i].rawExpression));
                    if (targetIdx != -1 && i < valueRow.size()) {
                        newRecord[targetIdx] = valueRow[i];
                    }
                }
            }
            else {
                // Dacă nu avem coloane: INSERT INTO tab VALUES (val1, val2, ...)
                // Valorile se mapează 1 la 1 în ordinea coloanelor din fișier
                for (size_t i = 0; i < valueRow.size() && i < newRecord.size(); ++i) {
                    newRecord[i] = valueRow[i];
                }
            }

            rowsToAdd.records.push_back(std::move(newRecord));
        }

        result.table = std::move(rowsToAdd);
        result.rowsAffected = result.table.records.size();
        result.success = true;
        result.message = L"Insert rows prepared successfully.";

    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Insert Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


vConResult vSqlEngine::executeUpdate(const SqlQueryParser& parser) {
    vConResult result;
    const Query& q = parser.getQuery();
    //const Query& q = getCurrentQuery();
    const vConTable& target = m_sourceTables[0]; // Tabela deja încărcată

    // Colectăm rândurile care trebuie modificate și valorile lor noi
    // Map: Index_Rând -> Vector_Valori_Noi
    std::map<int, std::vector<std::wstring>> updates;

    for (int i = 0; i < (int)target.records.size(); ++i) {
        if (!q.whereRoot || evaluateCondition(q.whereRoot, target.records[i], target)) {
            std::vector<std::wstring> updatedRow = target.records[i];

            for (auto const& [colName, newValue] : q.updateSets) {
                int colIdx = target.getColumnIndex(colName);
                if (colIdx != -1) {
                    // Aici putem folosi loop-ul de re-evaluare pentru newValue!
                    updatedRow[colIdx] = resolveExpression(newValue, target.records[i], target);
                }
            }
            updates[i] = updatedRow;
        }
    }

    result.updatedRecordsMap = updates; // Ai nevoie de acest membru nou în vConResult
    result.rowsAffected = updates.size();
    result.success = true;
    return result;
}

vConResult vSqlEngine::executeDelete(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();
        //const Query& q = getCurrentQuery();
        if (m_sourceTables.empty()) throw std::runtime_error("No target table.");

        const vConTable& target = m_sourceTables[0];
        std::vector<int> indicesToDelete;

        // Identificăm rândurile care respectă WHERE
        for (int i = 0; i < (int)target.records.size(); ++i) {
            if (!q.whereRoot || evaluateCondition(q.whereRoot, target.records[i], target)) {
                indicesToDelete.push_back(i);
            }
        }

        result.affectedIndices = indicesToDelete; // Adăugăm un nou membru în vConResult: std::vector<int>
        result.rowsAffected = indicesToDelete.size();
        result.success = true;
    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Delete Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


vConResult vSqlEngine::executeSelect(const SqlQueryParser& parser) {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        const Query& q = parser.getQuery();

        // Verificăm dacă avem un query de tip Virtual Table (fără clauză FROM și fără Subquery)
        // EX: SELECT 1+1
        bool isVirtualTable = q.fromTable.name.empty() && q.fromTable.subSelect == nullptr;

        vConTable workTable;

        if (isVirtualTable) {
            workTable.tableName = L"DUAL";
            // Adăugăm un rând gol fictiv pentru ca buclele de evaluare să aibă context de rulare
            workTable.records.push_back({});
        }
        else {
            // --- STEP 0: CONSTRUCȚIA UNIVERSULUI (FROM + JOIN) ---
            bool antiCartesianShield = false; // Scutul împotriva parser-ului

            // 1. Verificăm dacă clauza FROM conține un SUBQUERY (Derived Table)
            if (q.fromTable.subSelect != nullptr) {
                // Instanțiem un motor izolat pentru a evalua subquery-ul interior
                vSqlEngine subEngine(this->m_sourceTables, q.fromTable.subSelect);
                vConResult subRes = subEngine.executeSubquery();

                if (!subRes.success) {
                    throw std::runtime_error("Eroare in subquery-ul din FROM: " + wstr_to_str(subRes.message));
                }

                // Tabelul rezultat devine baza noastră de lucru
                workTable = std::move(subRes.table);

                // Îi setăm alias-ul (ex: 'prs') ca să poată fi găsit corect de coloanele din SELECT
                workTable.tableName = q.fromTable.alias.empty() ? L"SUBQUERY" : q.fromTable.alias;
                workTable.tableAlias = workTable.tableName;

                antiCartesianShield = true; // Activăm scutul
            }
            else {
                // 2. Cazul clasic: tabel normal citit de pe disc (ex: FROM persoane)
                vConTable* mainTable = findTableInUniverse(q.fromTable.getEffectiveName());
                if (mainTable) {
                    workTable = *mainTable;
                }
                else {
                    // Fallback de siguranță legacy
                    if (m_sourceTables.empty()) throw std::runtime_error("No source tables available");
                    workTable = m_sourceTables[0];

                    antiCartesianShield = true; // Activăm scutul pentru siguranță
                }
            }

            // 3. Aplicăm JOIN-urile rând pe rând (peste workTable-ul deja stabilit)
            for (size_t i = 0; i < q.joins.size(); ++i) {
                const JoinClause& join = q.joins[i];

                vConTable* rightTable = findTableInUniverse(join.table.getEffectiveName());
                if (!rightTable) {
                    // Dacă e activ scutul și tabelul din join nu există fizic, îl sărim
                    if (antiCartesianShield) continue;
                    throw std::runtime_error("Table not found in universe: " + wstr_to_str(join.table.getEffectiveName()));
                }

                // --- HACK ANTI-CARTESIAN PENTRU PARSER ---
                // Dacă parserul a generat un join fără clauză ON către tabelul de bază, îl ignorăm
                if (antiCartesianShield && join.on_conditions.empty() &&
                    !m_sourceTables.empty() && to_upper(rightTable->tableName) == to_upper(m_sourceTables[0].tableName)) {
                    continue;
                }

                workTable = performJoin(workTable, *rightTable, join);
            }
        }

        // --- STEP 1: Pregătirea coloanelor (Expandare SELECT *) ---
        std::vector<QueryColumn> projectedColumns = expandWildcards(q.columns, workTable);

        // --- STEP 2: Filtrare (WHERE) ---
        std::vector<const std::vector<std::wstring>*> filteredRows;
        filteredRows.reserve(workTable.records.size());

        for (const auto& row : workTable.records) {
            if (!q.whereRoot || evaluateCondition(q.whereRoot, row, workTable)) {
                filteredRows.push_back(&row);
            }
        }

        // --- STEP 3: Sortare (ORDER BY) ---
        if (!q.order_clauses.empty() && !isVirtualTable) {
            applySorting(filteredRows, q, workTable);
        }

        // --- STEP 4: Pregătirea tabelului de lucru (workTable) ---
        vConTable finalTable;
        finalTable.tableName = workTable.tableName;

        // Stabilim coloanele finale (Headerele)
        for (const auto& col : projectedColumns) {
            finalTable.columns.push_back(col.alias.empty() ? col.rawExpression : col.alias);

            if (!isVirtualTable) {
                int srcIdx = workTable.getColumnIndex(to_upper(col.rawExpression));
                if (srcIdx != -1) {
                    finalTable.columnTypes.push_back(workTable.columnTypes[srcIdx]);
                }
                else {
                    finalTable.columnTypes.push_back(L"C"); // Default Character
                }
            }
            else {
                finalTable.columnTypes.push_back(L"C"); // Default pentru Virtual Table
            }
        }

        // Copiem rândurile filtrate în tabelul temporar pentru procesare
        for (const auto* rowPtr : filteredRows) {
            finalTable.records.push_back(*rowPtr);
        }

        // --- STEP 5: EVALUAREA EXPRESIILOR (AST / Legacy) ---
        evaluateExpressions(q, finalTable, workTable, projectedColumns);

        // --- STEP 6: LIMIT/OFFSET ---
        applyLimitOffset(finalTable.records, q.limit, q.offset);

        result.table = std::move(finalTable);
        result.rowsAffected = result.table.records.size();
        result.success = true;

    }
    catch (const std::exception& e) {
        result.success = false;
        std::wstring errorMsg = L"Execution Error: " + str_to_wstr(e.what());

        // Afișăm eroarea direct în consola serverului
        LOG_ERROR(errorMsg);

        result.message = errorMsg;
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}


bool vSqlEngine::evaluateJoinCondition(const JoinClause& join, const std::vector<std::wstring>& combinedRow, const vConTable& joinedSchema) {
    if (join.on_conditions.empty()) return true;

    for (const auto& condition : join.on_conditions) {
        // 1. Extragem valorile folosind resolveExpression pe rândul combinat
        std::wstring leftVal = resolveExpression(condition.leftOperand.rawExpression, combinedRow, joinedSchema);
        std::wstring rightVal = resolveExpression(condition.rightOperand.rawExpression, combinedRow, joinedSchema);


        // LOG DE DEBUG:
         //LOG_INFO(L"JOIN Compare: [" + condition.leftOperand.rawExpression + L":" + leftVal + L"] == [" + 
           //       condition.rightOperand.rawExpression + L":" + rightVal + L"]");

        // 2. Căutăm handler-ul pentru operatorul specific (ex: "=", "LIKE", ">")
        std::wstring op = to_upper(condition.oper);
        if (m_opHandlers.count(op)) {
            // Executăm handler-ul (care știe deja să facă conversia la număr sau LIKE)
            if (!m_opHandlers[op](leftVal, rightVal)) {
                return false; // Una din condiții nu e satisfăcută
            }
        }
        else {
            // Dacă nu avem handler, putem decide să dăm eroare sau să comparăm ca string
            if (leftVal != rightVal) return false;
        }
    }

    return true; // Toate condițiile ON au trecut
}

vConTable vSqlEngine::performJoin(const vConTable& left, const vConTable& right, const JoinClause& join) {
    vConTable res;
    res.tableName = L"JOIN_RESULT";

    // 1. Construim Metadata (Header + Tipuri)
    auto addMeta = [&](const vConTable& src) {
        std::wstring pref = src.tableAlias.empty() ? to_upper(src.tableName) : to_upper(src.tableAlias);
        for (size_t i = 0; i < src.columns.size(); ++i) {
            res.columns.push_back(pref + L"." + to_upper(src.columns[i]));
            res.columnTypes.push_back(src.columnTypes[i]);
        }
    };
    addMeta(left);
    addMeta(right);

    // 2. Pregătim monitorizarea pentru Right/Full Join
    // rightMatched[j] va fi true dacă rândul j din right a fost folosit măcar o dată
    std::vector<bool> rightMatched(right.records.size(), false);

    // 3. Procesăm tabela din STÂNGA (Left Source)
    for (size_t i = 0; i < left.records.size(); ++i) {
        bool leftMatched = false;

        for (size_t j = 0; j < right.records.size(); ++j) {
            // Combinăm datele celor două rânduri
            std::vector<std::wstring> combinedRow = left.records[i];
            combinedRow.insert(combinedRow.end(), right.records[j].begin(), right.records[j].end());

            // Evaluăm condiția ON folosind schema tabelului rezultat (res)
            if (evaluateJoinCondition(join, combinedRow, res)) {
                res.records.push_back(combinedRow);
                leftMatched = true;
                rightMatched[j] = true; // Marcăm că acest rând din dreapta are pereche
            }
        }

        // Logică pentru LEFT / FULL JOIN: dacă rândul din stânga e "singur"
        if (!leftMatched && (join.type == JoinType::LEFT || join.type == JoinType::FULL)) {
            std::vector<std::wstring> nullRow = left.records[i];
            // Padding cu string-uri goale pentru toate coloanele din dreapta
            for (size_t k = 0; k < right.columns.size(); ++k) {
                nullRow.push_back(L"");
            }
            res.records.push_back(nullRow);
        }
    }

    // 4. Procesăm tabela din DREAPTA pentru RIGHT / FULL JOIN
    // Adăugăm rândurile din dreapta care NU au găsit pereche în stânga
    if (join.type == JoinType::RIGHT || join.type == JoinType::FULL) {
        for (size_t j = 0; j < right.records.size(); ++j) {
            if (!rightMatched[j]) {
                // Padding cu string-uri goale pentru toate coloanele din stânga
                std::vector<std::wstring> nullRow(left.columns.size(), L"");
                // Adăugăm datele reale din rândul de la dreapta
                nullRow.insert(nullRow.end(), right.records[j].begin(), right.records[j].end());
                res.records.push_back(nullRow);
            }
        }
    }

    return res;
}


void vSqlEngine::evaluateExpressions(const Query& query, vConTable& targetTable, const vConTable& sourceRef, const std::vector<QueryColumn>& projectedColumns) {

    // --- HELPER ELEGANT: Alege automat evaluarea prin AST (dacă există) sau fallback legacy ---
    auto evalCol = [&](const QueryColumn& c, const std::vector<std::wstring>& r) -> std::wstring {
        if (c.astRoot != nullptr) {
            std::wstring val = evaluateASTNode(c.astRoot, r, sourceRef);
            // Plasa de siguranță legacy fallback
            if (val.empty() && c.astRoot->type == ExprNodeType::COLUMN_REF) {
                val = resolveExpression(c.rawExpression, r, sourceRef);
            }
            return val;
        }
        return resolveExpression(c.rawExpression, r, sourceRef);
    };

    // 1. Mod normal (Fără agregare și fără GROUP BY)
    if (query.group_clauses.empty() && !query.isAggregate()) {

        // Cazul special: Virtual Table (ex: SELECT 1 + 2) - nu avem clauza FROM
        if (targetTable.records.empty() && query.fromTable.name.empty() && query.fromTable.subSelect == nullptr) {
            std::vector<std::wstring> singleRow; // rând gol fictiv
            std::vector<std::wstring> processedRow;
            for (const auto& col : projectedColumns) {
                processedRow.push_back(evalCol(col, singleRow));
            }
            targetTable.records.push_back(processedRow);
            return;
        }

        // Cazul clasic: SELECT calcul FROM tabel
        // IMPORTANT: Iterăm peste rândurile existente în targetTable și le SUPRASCRIEM cu proiecția!
        for (auto& row : targetTable.records) {
            std::vector<std::wstring> processedRow;
            for (const auto& col : projectedColumns) {
                // Folosim 'row' care e deja un rând complet (după join/where) din sourceRef (workTable original)
                processedRow.push_back(evalCol(col, row));
            }
            // Înlocuim rândul complet cu cel proiectat (doar coloanele cerute)
            row = std::move(processedRow);
        }
        return;
    }

    // 2. LOGICA DE GRUPARE (Bucketizing)
    std::map<std::wstring, std::vector<std::vector<std::wstring>>> groups;
    for (const auto& row : targetTable.records) {
        std::wstring groupKey = query.group_clauses.empty() ? L"GLOBAL" : L"";
        if (!query.group_clauses.empty()) {
            for (const auto& gc : query.group_clauses) {
                groupKey += evalCol(gc.column, row) + L"|";
            }
        }
        groups[groupKey].push_back(row);
    }

    // 3. GENERAREA REZULTATELOR DIN GRUPURI
    std::vector<std::vector<std::wstring>> finalRecords;

    for (auto& pair : groups) {
        auto& groupRows = pair.second;
        std::vector<std::wstring> resultRow(projectedColumns.size(), L"");

        for (size_t i = 0; i < projectedColumns.size(); ++i) {
            const auto& col = projectedColumns[i];

            if (col.type == ColumnType::AGGREGATE) {
                // --- FAZA A: ACUMULARE ---
                std::wstring aggVal = L"";
                std::wstring funcUpper = to_upper(col.aggregateFunc);
                auto it = m_aggHandlers.find(funcUpper);

                std::vector<std::wstring> args = splitSqlArguments(col.aggregateArg);

                for (const auto& row : groupRows) {
                    std::wstring nextVal;

                    if (funcUpper == L"STRING_AGG") {
                        std::wstring data = (args.size() > 0) ? resolveExpression(args[0], row, sourceRef) : L"";
                        std::wstring sep = (args.size() > 1) ? args[1] : L"', '";
                        nextVal = data + L"|SEP|" + sep;
                    }
                    else {
                        nextVal = (col.aggregateArg == L"*") ? L"1" : resolveExpression(col.aggregateArg, row, sourceRef);
                    }

                    if (it != m_aggHandlers.end()) {
                        aggVal = it->second(aggVal, nextVal);
                    }
                }

                // --- FAZA B: FINALIZARE (Specific pentru AVG) ---
                if (funcUpper == L"AVG") {
                    size_t sep = aggVal.find(L':');
                    if (sep != std::wstring::npos) {
                        double sum = std::stod(aggVal.substr(0, sep));
                        long long count = std::stoll(aggVal.substr(sep + 1));
                        double result = (count > 0) ? (sum / count) : 0.0;

                        std::wstringstream ss;
                        ss << std::fixed << std::setprecision(4) << result;
                        std::wstring s = ss.str();
                        s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
                        if (!s.empty() && s.back() == L'.') s.pop_back();
                        aggVal = s;
                    }
                }
                resultRow[i] = aggVal;
            }
            else {
                resultRow[i] = evalCol(col, groupRows[0]);
            }
        }

        // 4. FILTRARE HAVING
        if (query.havingRoot) {
            vConTable tempTable;
            for (auto& col : projectedColumns) tempTable.columns.push_back(col.rawExpression);
            if (evaluateCondition(query.havingRoot, resultRow, tempTable)) {
                finalRecords.push_back(resultRow);
            }
        }
        else {
            finalRecords.push_back(resultRow);
        }
    }

    // AICI ERA PROBLEMA! Acum suprascriem structura pe care o așteaptă executeSelect!
    targetTable.records = std::move(finalRecords);
}


// În SqlQueryEngine.cpp
bool vSqlEngine::evaluateCondition(
    std::shared_ptr<WhereClause> node,
    const std::vector<std::wstring>& row,
    const vConTable& table,
    const std::vector<std::wstring>& outerRow, // <-- Adăugat pentru corelare
    const vConTable& outerTable               // <-- Adăugat pentru corelare
) {
    if (!node) return true;
    bool result = false;

    if (node->isGroup) {
        if (node->groupConnector == L"AND") {
            result = true;
            for (auto& child : node->subClauses) {
                // Pasăm contextul mai departe recursiv
                if (!evaluateCondition(child, row, table, outerRow, outerTable)) {
                    result = false;
                    break;
                }
            }
        }
        else { // OR
            result = false;
            for (auto& child : node->subClauses) {
                if (evaluateCondition(child, row, table, outerRow, outerTable)) {
                    result = true;
                    break;
                }
            }
        }
    }
    
    else {
        // --- LOGICA PENTRU LEAF BAZATĂ PE AST ---
        std::wstring left, right;

        // Partea Stângă: Evaluăm prin AST dacă există, altfel fallback pe context
        if (node->leftOperand.astRoot) {
            left = wstr_trim(evaluateASTNode(node->leftOperand.astRoot, row, table));
        }
        else {
            left = wstr_trim(resolveExpressionWithContext(node->leftOperand.rawExpression, row, table, outerRow, outerTable));
        }

        // Partea Dreaptă: Evaluăm prin AST (Aici rulează subquery-ul nostru!)
        if (node->rightOperand.astRoot) {
            right = wstr_trim(evaluateASTNode(node->rightOperand.astRoot, row, table));
        }
        else {
            right = wstr_trim(resolveExpressionWithContext(node->rightOperand.rawExpression, row, table, outerRow, outerTable));
        }

        auto it = m_opHandlers.find(to_upper(node->oper));
        if (it != m_opHandlers.end()) {
            result = it->second(left, right);
        }
        else {
            LOG_ERROR(L"Operator necunoscut: " + node->oper);
            result = false;
        }
    }

    return node->isNegated ? !result : result;
}

std::vector<std::wstring> vSqlEngine::splitSqlArguments(const std::wstring& s) {
    std::vector<std::wstring> args;
    std::wstring current;
    int parenLevel = 0;
    bool inQuotes = false;

    for (wchar_t c : s) {
        if (c == L'\'') inQuotes = !inQuotes;

        if (!inQuotes) {
            if (c == L'(') parenLevel++;
            else if (c == L')') parenLevel--;
        }

        if (c == L',' && parenLevel == 0 && !inQuotes) {
            args.push_back(wstr_trim(current));
            current.clear();
        }
        else {
            current += c;
        }
    }
    if (!current.empty()) args.push_back(wstr_trim(current));
    return args;
}



std::wstring vSqlEngine::resolveExpression(std::wstring expr, const std::vector<std::wstring>& row, const vConTable& table) {
    expr = wstr_trim(expr);
    if (expr.empty()) return L"";

    // --- 1. LOGICA PENTRU SUBQUERY (Rămâne neschimbată) ---
    std::wstring upperExpr = to_upper(expr);
    // --- 1. LOGICA PENTRU SUBQUERY (Îmbunătățită) ---
    if (upperExpr.size() > 7 && upperExpr.find(L"(SELECT") != std::wstring::npos) {
        // Extragem subquery-ul dintre paranteze dacă e nevoie
        std::wstring subSql = expr;
        if (subSql.front() == L'(' && subSql.back() == L')') {
            subSql = subSql.substr(1, subSql.size() - 2);
        }

        // Creăm un parser temporar pentru acest subquery specific
        // sau refolosim obiectul col.subSelect dacă îl găsim parțial în expr
        for (const auto& col : m_queryParser.getQuery().columns) {
            // Verificăm dacă subquery-ul parsat este CONȚINUT în expresia curentă
            if (col.type == ColumnType::SUBQUERY && expr.find(col.rawExpression) != std::wstring::npos) {

                if (!col.subSelect) return L"NULL";

                std::wstring targetName = to_upper(col.subSelect->fromTable.name);
                vConTable* targetTable = nullptr;
                for (auto& sourceTbl : m_sourceTables) {
                    if (to_upper(sourceTbl.tableName) == targetName || to_upper(sourceTbl.tableAlias) == targetName) {
                        targetTable = &sourceTbl;
                        break;
                    }
                }

                if (targetTable) {
                    for (const auto& innerRow : targetTable->records) {
                        if (evaluateCondition(col.subSelect->whereRoot, innerRow, *targetTable, row, table)) {
                            if (!col.subSelect->columns.empty()) {
                                // Executăm și returnăm valoarea
                                return resolveExpression(col.subSelect->columns[0].rawExpression, innerRow, *targetTable);
                            }
                        }
                    }
                }
                return L"0"; // Returnăm 0 în loc de NULL pentru a nu strica aritmetica (+1)
            }
        }
    }

    // --- 2. LOGICA DE EVALUARE RECURSIVĂ ---
    bool reevaluate = true;
    int safety_net = 0;

    while (reevaluate && safety_net < 10) {
        reevaluate = false;
        safety_net++;

        // A. Curățare paranteze
        if (!expr.empty() && expr.front() == L'(' && expr.back() == L')') {
            // Verificăm dacă sunt paranteze de grupare (nu funcție)
            size_t openP = expr.find(L'(');
            if (openP == 0) {
                expr = wstr_trim(expr.substr(1, expr.size() - 2));
                reevaluate = true;
                continue;
            }
        }

        // B. Coloană directă
        int colIdx = table.getColumnIndex(to_upper(expr));
        if (colIdx != -1) return (colIdx < (int)row.size()) ? row[colIdx] : L"";

        // C. Funcții (UPPER, TYPE, etc.)
        size_t funcOpenP = expr.find(L'(');
        size_t funcCloseP = expr.find_last_of(L')');
        if (funcOpenP != std::wstring::npos && funcCloseP != std::wstring::npos && funcCloseP > funcOpenP) {
            std::wstring funcName = to_upper(wstr_trim(expr.substr(0, funcOpenP)));
            // Verificăm dacă e un nume de funcție valid (nu doar o paranteză matematică)
            if (!funcName.empty() && std::iswalpha(funcName[0])) {
                std::wstring inner = expr.substr(funcOpenP + 1, funcCloseP - funcOpenP - 1);

                if (funcName == L"TYPE") {
                    std::wstring colName = wstr_trim(inner);
                    if (colName.size() >= 2 && (colName.front() == L'\"' || colName.front() == L'\''))
                        colName = colName.substr(1, colName.size() - 2);
                    int cIdx = table.getColumnIndex(to_upper(colName));
                    return (cIdx != -1) ? table.columnTypes[cIdx] : L"U";
                }

                auto it = m_funcHandlers.find(funcName);
                if (it != m_funcHandlers.end()) {
                    std::vector<std::wstring> rawArgs = splitSqlArguments(inner);
                    std::vector<std::wstring> resolvedArgs;
                    for (const auto& arg : rawArgs) resolvedArgs.push_back(resolveExpression(arg, row, table));
                    return it->second(resolvedArgs);
                }
            }
        }

        // D. ARITMETICĂ (Mutată în interiorul buclei pentru a permite compunerea)
        // Căutăm operatorii în ordinea inversă a priorității (+/- apoi */)
        std::vector<std::wstring> ops = { L"+", L"-", L"*", L"/" };
        for (const auto& op : ops) {
            size_t opPos = expr.find(op);
            if (opPos != std::wstring::npos && opPos > 0) {
                std::wstring leftStr = wstr_trim(expr.substr(0, opPos));
                std::wstring rightStr = wstr_trim(expr.substr(opPos + 1));

                std::wstring leftVal = resolveExpression(leftStr, row, table);
                std::wstring rightVal = resolveExpression(rightStr, row, table);

                try {
                    double leftNum = std::stod(leftVal);
                    double rightNum = std::stod(rightVal);
                    double res = 0;
                    if (op == L"+") res = leftNum + rightNum;
                    else if (op == L"-") res = leftNum - rightNum;
                    else if (op == L"*") res = leftNum * rightNum;
                    else if (op == L"/") res = (rightNum != 0) ? leftNum / rightNum : 0;

                    std::wstring resStr = std::to_wstring(res);
                    resStr.erase(resStr.find_last_not_of(L'0') + 1, std::string::npos);
                    if (resStr.back() == L'.') resStr.pop_back();
                    return resStr;
                }
                catch (...) { /* Mergem mai departe dacă nu e numeric */ }
            }
        }

        // E. Literali
        if (expr.size() >= 2 && (expr.front() == L'\"' || expr.front() == L'\'')) {
            return expr.substr(1, expr.size() - 2);
        }
    }

    return expr;
}

void vSqlEngine::printResult() {
   
}

void vSqlEngine::projectColumns(vConTable& table) {
   
}


double vSqlEngine::asNumber(const std::wstring& s) {
    if (s.empty()) return 0.0;
    try {
        return std::stod(s);
    }
    catch (...) {
        return 0.0; // Sau NaN, depinde cum vrei să tratezi erorile
    }
}


bool vSqlEngine::handleLikeOp(const std::wstring& left, const std::wstring& right) {
    if (right.empty()) return false;

    std::wstring target = to_upper(left);
    std::wstring pattern = to_upper(right);

    bool startWild = (pattern.front() == L'%');
    bool endWild = (pattern.back() == L'%');

    if (startWild && endWild) {
        // %STRING% -> căutare oriunde
        std::wstring search = pattern.substr(1, pattern.size() - 2);
        return target.find(search) != std::wstring::npos;
    }
    if (startWild) {
        // %STRING -> se termină cu
        std::wstring search = pattern.substr(1);
        if (target.size() < search.size()) return false;
        return target.compare(target.size() - search.size(), search.size(), search) == 0;
    }
    if (endWild) {
        // STRING% -> începe cu
        std::wstring search = pattern.substr(0, pattern.size() - 1);
        return target.find(search) == 0;
    }

    // Fără wildcard-uri -> egalitate exactă
    return target == pattern;
}


std::vector<QueryColumn> vSqlEngine::expandWildcards(const std::vector<QueryColumn>& columns, const vConTable& source) {
    std::vector<QueryColumn> expanded;
    for (const auto& col : columns) {
        if (col.type == ColumnType::WILDCARD || col.rawExpression == L"*") {
            for (const auto& realColName : source.columns) {
                QueryColumn newCol;
                newCol.rawExpression = realColName;
                newCol.type = ColumnType::RAW_FIELD;
                expanded.push_back(newCol);
            }
        }
        else {
            expanded.push_back(col);
        }
    }
    return expanded;
}

void vSqlEngine::applySorting(std::vector<const std::vector<std::wstring>*>& rows, const Query& q, const vConTable& source) {
    size_t numToSort = rows.size();
    bool usePartial = false;

    if (q.limit >= 0) {
        numToSort = std::min<size_t>(rows.size(), static_cast<size_t>(q.offset + q.limit));
        usePartial = numToSort < rows.size();
    }

    auto comp = [&](const std::vector<std::wstring>* a, const std::vector<std::wstring>* b) {
        for (const auto& order : q.order_clauses) {
            std::wstring valA = resolveExpression(order.column.rawExpression, *a, source);
            std::wstring valB = resolveExpression(order.column.rawExpression, *b, source);
            if (valA == valB) continue;

            bool isLess = (isNumber(valA) && isNumber(valB)) ? std::stod(valA) < std::stod(valB) : valA < valB;
            return (order.direction == OrderDirection::ASC) ? isLess : !isLess;
        }
        return false;
    };

    if (usePartial) {
        std::partial_sort(rows.begin(), rows.begin() + numToSort, rows.end(), comp);
        rows.resize(numToSort);
    }
    else {
        std::stable_sort(rows.begin(), rows.end(), comp);
    }
}

void vSqlEngine::applyLimitOffset(std::vector<std::vector<std::wstring>>& records, int limit, int offset) {
    if (offset > 0) {
        if (static_cast<size_t>(offset) >= records.size()) {
            records.clear();
            return;
        }
        records.erase(records.begin(), records.begin() + offset);
    }
    if (limit >= 0 && static_cast<size_t>(limit) < records.size()) {
        records.resize(limit);
    }
}

std::wstring vSqlEngine::resolveExpressionWithContext(
    std::wstring expr,
    const std::vector<std::wstring>& row, const vConTable& table,
    const std::vector<std::wstring>& outerRow, const vConTable& outerTable)
{
    expr = wstr_trim(expr);
    std::wstring upperExpr = to_upper(expr);

    // --- LOGICA NOUĂ: Gestionare Prefix Tabel (Tabel.Coloana) ---
    size_t dotPos = upperExpr.find(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring tblPrefix = upperExpr.substr(0, dotPos);
        std::wstring colName = upperExpr.substr(dotPos + 1);

        // 1. Verificăm dacă prefixul e tabelul curent (ex: PERSOANE)
        if (tblPrefix == to_upper(table.tableName) || tblPrefix == to_upper(table.tableAlias)) {
            int idx = table.getColumnIndex(colName);
            if (idx != -1) return (idx < row.size()) ? row[idx] : L"";
        }

        // 2. Verificăm dacă prefixul e tabelul exterior (ex: RUDE)
        if (!outerRow.empty()) {
            if (tblPrefix == to_upper(outerTable.tableName) || tblPrefix == to_upper(outerTable.tableAlias)) {
                int oIdx = outerTable.getColumnIndex(colName);
                if (oIdx != -1) return (oIdx < outerRow.size()) ? outerRow[oIdx] : L"";
            }
        }
    }

    // --- LOGICA EXISTENTĂ: Căutare fără prefix ---
    int idx = table.getColumnIndex(upperExpr);
    if (idx != -1) return (idx < row.size()) ? row[idx] : L"";

    if (!outerRow.empty()) {
        int oIdx = outerTable.getColumnIndex(upperExpr);
        if (oIdx != -1) return (oIdx < outerRow.size()) ? outerRow[oIdx] : L"";
    }

    // 3. Dacă e funcție sau literal
    return resolveExpression(expr, row, table);
}

// O metodă internă pentru a extrage valoarea, ținând cont de contextul dublu
std::wstring vSqlEngine::getValWithContext(std::wstring identifier,
    const std::vector<std::wstring>& row, const vConTable& table,
    const std::vector<std::wstring>& outerRow, const vConTable& outerTable)
{
    identifier = to_upper(wstr_trim(identifier));

    // 1. Verificăm dacă avem formatul Tabel.Coloana (ex: RUDE.IDPERS)
    size_t dotPos = identifier.find(L'.');
    if (dotPos != std::wstring::npos) {
        std::wstring tblPart = identifier.substr(0, dotPos);
        std::wstring colPart = identifier.substr(dotPos + 1);

        // Este tabelul curent (interior)?
        if (tblPart == to_upper(table.tableName) || tblPart == to_upper(table.tableAlias)) {
            int idx = table.getColumnIndex(colPart);
            if (idx != -1) return row[idx];
        }

        // Este tabelul exterior (corelare)?
        if (!outerRow.empty()) {
            if (tblPart == to_upper(outerTable.tableName) || tblPart == to_upper(outerTable.tableAlias)) {
                int idx = outerTable.getColumnIndex(colPart);
                if (idx != -1) return outerRow[idx];
            }
        }
    }

    // 2. Dacă e un nume simplu de coloană, căutăm întâi în interior, apoi în exterior
    int idx = table.getColumnIndex(identifier);
    if (idx != -1) return row[idx];

    if (!outerRow.empty()) {
        idx = outerTable.getColumnIndex(identifier);
        if (idx != -1) return outerRow[idx];
    }

    // 3. Dacă nu e coloană, înseamnă că e un literal sau o funcție
    // Folosim resolveExpression-ul standard (fără contextul de corelare)
    return resolveExpression(identifier, row, table);
}


std::wstring vSqlEngine::formatDouble(double val) {
    std::wstringstream ss;
    ss << std::fixed << std::setprecision(4) << val;
    std::wstring s = ss.str();
    // Scoatem zero-urile inutile de la coadă
    s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
    // Dacă a rămas punctul zecimal singur, îl scoatem și pe el
    if (!s.empty() && s.back() == L'.') s.pop_back();
    return s;
}


std::wstring vSqlEngine::evaluateASTNode(std::shared_ptr<ExprASTNode> node, const std::vector<std::wstring>& row, const vConTable& sourceRef) {
    if (!node) return L"";

    switch (node->type) {
        // --- 1. CONSTANTE (1, 2, 'TEXT') ---
    case ExprNodeType::LITERAL: {
        std::wstring val = node->value;
        // Curățăm ghilimelele pentru string-uri ('1977' -> 1977)
        if (val.size() >= 2 && val.front() == L'\'' && val.back() == L'\'') {
            return val.substr(1, val.size() - 2);
        }
        return val;
    }

    // --- 2. CÂMPURI DIN TABEL (ex: varsta, nume) ---
    case ExprNodeType::COLUMN_REF: {
        // 1. Curățăm spațiile parazite
        std::wstring colUpper = to_upper(wstr_trim(node->value));

        // 2. Încercăm metoda nativă a tabelului (dacă e implementată smart)
        int colIdx = sourceRef.getColumnIndex(colUpper);
        if (colIdx != -1) {
            return (colIdx < (int)row.size()) ? row[colIdx] : L"";
        }

        // 3. Căutare robustă (Iertătoare cu alias-urile)
        for (size_t i = 0; i < sourceRef.columns.size(); ++i) {
            std::wstring srcCol = to_upper(wstr_trim(sourceRef.columns[i]));

            // Match exact ("NUME" == "NUME")
            if (srcCol == colUpper) {
                return (i < row.size()) ? row[i] : L"";
            }

            // Sursa are prefix (ex: tabelul are "PERS.NUME", dar query-ul cere "NUME")
            size_t srcDot = srcCol.find(L'.');
            if (srcDot != std::wstring::npos && srcCol.substr(srcDot + 1) == colUpper) {
                return (i < row.size()) ? row[i] : L"";
            }

            // Query-ul are prefix (ex: query-ul cere "PERS.NUME", dar tabelul are doar "NUME")
            size_t reqDot = colUpper.find(L'.');
            if (reqDot != std::wstring::npos && colUpper.substr(reqDot + 1) == srcCol) {
                return (i < row.size()) ? row[i] : L"";
            }
        }

        return L""; // Fallback final dacă chiar nu există coloana
    }

                                 // --- 3. MATEMATICĂ ȘI LOGICĂ (+, -, *, /) ---
    case ExprNodeType::BINARY_OP: {
        // MAGIC: RECURSIVITATE! Evaluăm stânga și dreapta înainte!
        std::wstring leftStr = evaluateASTNode(node->children[0], row, sourceRef);
        std::wstring rightStr = evaluateASTNode(node->children[1], row, sourceRef);

        if (node->value == L"+" || node->value == L"-" || node->value == L"*" || node->value == L"/") {
            // Convertim string-urile în numere (dacă sunt goale, punem 0)
            double leftNum = leftStr.empty() ? 0.0 : std::stod(leftStr);
            double rightNum = rightStr.empty() ? 0.0 : std::stod(rightStr);
            double res = 0.0;

            if (node->value == L"+") res = leftNum + rightNum;
            else if (node->value == L"-") res = leftNum - rightNum;
            else if (node->value == L"*") res = leftNum * rightNum;
            else if (node->value == L"/") res = (rightNum != 0.0) ? (leftNum / rightNum) : 0.0;

            return formatDouble(res);
        }
        return L"";
    }

                                // --- 4. FUNCȚII (ex: UPPER) ---
    case ExprNodeType::FUNCTION_CALL: {
        std::wstring funcName = node->value;

        if (funcName == L"UPPER" && !node->children.empty()) {
            std::wstring argVal = evaluateASTNode(node->children[0], row, sourceRef);
            return to_upper(argVal);
        }
        // AICI VOM ADAUGA "TYPE" MAI TÂRZIU
        return L"";
    }

                                    // --- 5. SUBQUERIES ---
    // --- 5. SUBQUERY MIRACOL: Executăm un sub-select la runtime! ---
    case ExprNodeType::SUBQUERY: {
        if (!node->subQuery) {
            LOG_ERROR(L"Eroare: node->subQuery este null!");
            return L"0";
        }

        // --- 1. MEMOIZATION: Verificăm dacă l-am calculat deja ---
        auto it = m_astSubqueryCache.find(node.get());
        if (it != m_astSubqueryCache.end()) {
            // L-am găsit! Returnăm instant valoarea, zero procesare!
            return it->second;
        }

        // --- 2. EXECUȚIA (ajungem aici O SINGURĂ DATĂ per subquery) ---
        vSqlEngine subEngine(this->m_sourceTables, node->subQuery);
        vConResult subRes = subEngine.executeSubquery();

        std::wstring finalVal = L"0";
        if (subRes.success && !subRes.table.records.empty()) {
            if (!subRes.table.records[0].empty()) {
                finalVal = subRes.table.records[0][0];
                LOG_INFO(L"SUBQUERY CALCULAT NOU: " + finalVal);
            }
        }
        else {
            LOG_ERROR(L"Eroare executie subquery: " + subRes.message);
        }

        // --- 3. SALVĂM ÎN CACHE PENTRU URMĂTOARELE RÂNDURI ---
        m_astSubqueryCache[node.get()] = finalVal;

        return finalVal;
    }
    }
    return L"";
}


vConResult vSqlEngine::executeSubquery() {
    vConResult result;
    auto start = std::chrono::high_resolution_clock::now();

    try {
        if (!m_subQueryObj) {
            throw std::runtime_error("No subquery object provided");
        }

        const Query& q = *m_subQueryObj;
        bool isVirtualTable = q.fromTable.name.empty();

        vConTable workTable;

        if (isVirtualTable) {
            workTable.tableName = L"DUAL_SUB";
            workTable.records.push_back({}); // Rând gol pentru context scalar
        }
        else {
            if (m_sourceTables.empty()) throw std::runtime_error("No source tables available for subquery");

            // Construcția universului / join-uri pentru subquery
            vConTable* targetTable = findTableInUniverse(q.fromTable.getEffectiveName());
            if (!targetTable) {
                throw std::runtime_error("Tabelul pentru subquery nu a fost gasit: " + wstr_to_str(q.fromTable.getEffectiveName()));
            }
            workTable = *targetTable; // <--- CORECT: Acum targetează 'persoane', nu 'departamente'

            for (size_t i = 0; i < q.joins.size(); ++i) {
                const JoinClause& join = q.joins[i];
                vConTable* rightTable = findTableInUniverse(join.table.getEffectiveName());
                if (!rightTable) {
                    throw std::runtime_error("Subquery Table not found in universe: " + wstr_to_str(join.table.getEffectiveName()));
                }
                workTable = performJoin(workTable, *rightTable, join);
            }
        }

        // --- Expansiune și Filtrare ---
        std::vector<QueryColumn> projectedColumns = expandWildcards(q.columns, workTable);

        std::vector<const std::vector<std::wstring>*> filteredRows;
        filteredRows.reserve(workTable.records.size());

        for (const auto& row : workTable.records) {
            if (!q.whereRoot || evaluateCondition(q.whereRoot, row, workTable)) {
                filteredRows.push_back(&row);
            }
        }

        // --- Pregătire tabel final ---
        vConTable finalTable;
        finalTable.tableName = workTable.tableName;

        for (const auto& col : projectedColumns) {
            finalTable.columns.push_back(col.alias.empty() ? col.rawExpression : col.alias);
            if (!isVirtualTable) {
                int srcIdx = workTable.getColumnIndex(to_upper(col.rawExpression));
                finalTable.columnTypes.push_back((srcIdx != -1) ? workTable.columnTypes[srcIdx] : L"C");
            }
            else {
                finalTable.columnTypes.push_back(L"C");
            }
        }

        for (const auto* rowPtr : filteredRows) {
            finalTable.records.push_back(*rowPtr);
        }

        // Evaluarea expresiilor/agregărilor din subquery
        evaluateExpressions(q,finalTable, workTable, projectedColumns);

        // Limit / Offset
        applyLimitOffset(finalTable.records, q.limit, q.offset);

        result.table = std::move(finalTable);
        result.rowsAffected = result.table.records.size();
        result.success = true;
    }
    catch (const std::exception& e) {
        result.success = false;
        result.message = L"Subquery Execution Error: " + str_to_wstr(e.what());
    }

    auto end = std::chrono::high_resolution_clock::now();
    result.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}