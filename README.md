# QDBF - Ultra-Fast C++ SQL Server for Legacy DBF Files 🚀

**QDBF** is a lightweight, high-performance database server written entirely from scratch in C++. It breathes new life into legacy dBase/FoxPro (`.dbf`) files by exposing them through a modern, robust SQL network interface. 

If you are dealing with legacy accounting software, old POS systems, or government databases and are tired of crashing 32-bit ODBC drivers, QDBF provides a modern bridge to query and manipulate your data reliably over a network.

## ✨ Key Features

* **Custom SQL Engine:** Built from scratch with a custom Lexer, Parser, and Abstract Syntax Tree (AST) evaluator.
* **Blazing Fast:** Executes complex queries (with joins and subqueries) in sub-milliseconds.
* **Client-Server Architecture:** Runs as an independent TCP server with a provided interactive CLI shell.
* **No Heavy Dependencies:** Operates directly on the physical `.dbf` files without relying on legacy Windows ODBC/OLEDB drivers.
* **Robust Parser:** Handles complex nested parentheses, subqueries (derived tables), aliases, and tables with spaces in their names.

## 🛠️ Supported SQL Features

QDBF supports a wide range of SQL commands for both DML (Data Manipulation) and querying:

* **CRUD Operations:** `SELECT`, `INSERT`, `UPDATE`, `DELETE`
* **Joins:** `INNER JOIN`, `LEFT JOIN`, `RIGHT JOIN`, `FULL JOIN`
* **Subqueries:** Derived tables in the `FROM` clause (e.g., `SELECT * FROM (SELECT ... ) AS temp`)
* **Filtering & Logic:** Complex `WHERE` clauses with `AND`, `OR`, `NOT`, `IN`, `LIKE`, `BETWEEN`, and nested parentheses.
* **Aggregations:** `GROUP BY`, `HAVING`, `SUM`, `COUNT`, `AVG`, `MIN`, `MAX`, `STRING_AGG`
* **Scalar Functions:** `UPPER`, `LOWER`, `CONCAT`, `SUBSTR`, `TYPE`, etc.
* **Sorting & Paging:** `ORDER BY` (ASC/DESC), `LIMIT`, `OFFSET`

## 💻 Usage Examples

Start the server, connect using the provided client shell, and run your standard SQL queries seamlessly:

```sql
-- Standard querying
SELECT id, nume FROM persoane WHERE varsta > 18 ORDER BY nume ASC LIMIT 2;

-- Complex Joins with tables containing spaces in their names
SELECT p.nume, r.calitate 
FROM "persoane - Copy" p 
INNER JOIN rude r ON p.id = r.idpers;

-- Subqueries (Derived Tables)
SELECT * FROM persoane WHERE varsta > (SELECT AVG(varsta) FROM persoane)
SELECT nume FROM (SELECT * FROM persoane WHERE varsta = 44 ) AS prs;
```
## 🚀 Getting Started
Prerequisites
A C++17 (or newer) compatible compiler (MSVC, GCC, Clang).

Add any specific libraries you used here (if any, like WinSock for networking).

# Build Instructions
* Clone the repository:
```bash
git clone [https://github.com/minghir/qdbf.git](https://github.com/minghir/qdbf.git)
```
* Open the project in your IDE (e.g., Visual Studio) or build using your Make/CMake setup.
* Build the Server and Client executables.

# Running the Application
* Place your .dbf files in the defined working directory (e.g., .\dbfs).
* Start the QDBF Server.
* Start the QDBF Client, login with your credentials (e.g., admin), and start typing SQL queries!

## 🛣️ Roadmap
* Move the core database engine into a standalone static library for easier integration into other C++ projects.
* Implement nested function calls in the expression evaluator (e.g., UPPER(SUBSTR(nume, 1, 3))).
* Advanced indexing for even faster data retrieval.
  
 ## 📝 License
This project is licensed under the MIT License - see the LICENSE file for details.
  


```txt
--- QDBF Network Login ---

User: admin
Password: ***
IP Server [default 127.0.0.1]:
[SUCCESS] DBF database opened successfully.
[SUCCESS] Session started for: 127.0.0.1
[SUCCESS] --- Shell Interface Started ---

qdbf# /help

Client commands:
  /help                         Displays this help
  /connect user:password@ip [port]   Connect to a QDBF server
  /save csv <path>              Saves the last result as CSV
  /save dbf <path>              Saves the last result as DBF
  /load <file> into <table>     Imports a CSV or DBF into the server
  /clear                        Clears the console
  /exit, /quit                  Closes the client

Server commands (authentication required):
  /adduser <name> <password> <role>   Adds a user
  /dropuser <name>                    Removes a user
  /list_users                         Lists all users
  /sessions                           Shows active sessions
  /shutdown                           Shuts down the server

SQL:
  SELECT * FROM people;
  INSERT, UPDATE, DELETE, CREATE TABLE, DROP TABLE and SHOW TABLES
  SHOW TABLES



qdbf# show tables;
+--------------------+
| Tables_in_database |
+--------------------+
| pers_csv           |
| pers_dbf           |
| persoane           |
| persoane - Copy    |
| rude               |
| test               |
| test2              |
| test3              |
| test4              |
| test_drop          |
| testdate           |
+--------------------+

[SUCCESS] (0 rows in set, 0.000 sec)

qdbf# select * from persoane;
+----+------------------+--------+----------+-----------+
| ID | NUME             | VARSTA | DATAN    | ORAS      |
+----+------------------+--------+----------+-----------+
| 1  | Ion Popescu      | 45     | 19800512 | Bucuresti |
| 2  | Maria Ionescu    | 30     | 19951001 | Cluj      |
| 3  | Vasile Georgescu | 52     | 19730322 | Iasi      |
| 4  | Elena Stan       | 28     | 19970715 | Constanta |
| 6  | Andra Dumitru    | 44     | 19891109 | Brasov    |
| 5  | Apostol Catalin  | 23     | 19771203 | Bucuresti |
+----+------------------+--------+----------+-----------+

[SUCCESS] (6 rows in set, 0.000 sec)

qdbf# SELECT * FROM persoane WHERE varsta > (SELECT AVG(varsta) FROM persoane);
+----+------------------+--------+----------+-----------+
| ID | NUME             | VARSTA | DATAN    | ORAS      |
+----+------------------+--------+----------+-----------+
| 1  | Ion Popescu      | 45     | 19800512 | Bucuresti |
| 3  | Vasile Georgescu | 52     | 19730322 | Iasi      |
| 6  | Andra Dumitru    | 44     | 19891109 | Brasov    |
+----+------------------+--------+----------+-----------+

[SUCCESS] (3 rows in set, 0.002 sec)

qdbf#
```
