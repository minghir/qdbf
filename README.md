Markdown
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
SELECT id, nume FROM persoane WHERE varsta > 18 ORDER BY nume ASC LIMIT 10;

-- Complex Joins with tables containing spaces in their names
SELECT p.nume, r.grad_rudenie 
FROM "persoane - Copy" p 
INNER JOIN rude r ON p.id = r.persoana_id;

-- Subqueries (Derived Tables)
SELECT nume FROM (SELECT * FROM persoane WHERE activ = 1) AS prs;
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
