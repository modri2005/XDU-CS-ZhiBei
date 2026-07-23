#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;
namespace fs = std::filesystem;

static void configureConsoleEncoding() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static string toUpper(string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(toupper(c));
    });
    return s;
}

static string trim(const string &s) {
    size_t first = 0;
    while (first < s.size() && isspace(static_cast<unsigned char>(s[first]))) {
        ++first;
    }
    size_t last = s.size();
    while (last > first && isspace(static_cast<unsigned char>(s[last - 1]))) {
        --last;
    }
    return s.substr(first, last - first);
}

static bool iequals(const string &a, const string &b) {
    return toUpper(a) == toUpper(b);
}

static bool isIdentifierName(const string &s) {
    if (s.empty()) {
        return false;
    }
    if (!isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') {
        return false;
    }
    for (char c : s) {
        if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
            return false;
        }
    }
    return true;
}

enum class TokenKind {
    End,
    Identifier,
    Number,
    String,
    Comma,
    Semicolon,
    LParen,
    RParen,
    Star,
    Equal,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    NotEqual
};

struct Token {
    TokenKind kind{};
    string text;
    size_t pos{};
};

class Lexer {
public:
    explicit Lexer(string input) : input_(std::move(input)) {}

    vector<Token> scan() {
        vector<Token> tokens;
        while (true) {
            Token token = nextToken();
            tokens.push_back(token);
            if (token.kind == TokenKind::End) {
                break;
            }
        }
        return tokens;
    }

private:
    string input_;
    size_t pos_ = 0;

    char peek(size_t offset = 0) const {
        size_t index = pos_ + offset;
        return index < input_.size() ? input_[index] : '\0';
    }

    char advance() {
        return pos_ < input_.size() ? input_[pos_++] : '\0';
    }

    void skipSpaceAndComments() {
        while (true) {
            while (isspace(static_cast<unsigned char>(peek()))) {
                advance();
            }
            if (peek() == '-' && peek(1) == '-') {
                while (peek() != '\0' && peek() != '\n') {
                    advance();
                }
                continue;
            }
            break;
        }
    }

    Token nextToken() {
        skipSpaceAndComments();
        size_t start = pos_;
        char c = advance();
        switch (c) {
            case '\0':
                return {TokenKind::End, "", start};
            case ',':
                return {TokenKind::Comma, ",", start};
            case ';':
                return {TokenKind::Semicolon, ";", start};
            case '(':
                return {TokenKind::LParen, "(", start};
            case ')':
                return {TokenKind::RParen, ")", start};
            case '*':
                return {TokenKind::Star, "*", start};
            case '=':
                return {TokenKind::Equal, "=", start};
            case '<':
                if (peek() == '=') {
                    advance();
                    return {TokenKind::LessEqual, "<=", start};
                }
                if (peek() == '>') {
                    advance();
                    return {TokenKind::NotEqual, "<>", start};
                }
                return {TokenKind::Less, "<", start};
            case '>':
                if (peek() == '=') {
                    advance();
                    return {TokenKind::GreaterEqual, ">=", start};
                }
                return {TokenKind::Greater, ">", start};
            case '!':
                if (peek() == '=') {
                    advance();
                    return {TokenKind::NotEqual, "!=", start};
                }
                throw runtime_error("无法识别的字符 '!'");
            case '\'':
            case '"':
                return scanString(c, start);
            default:
                if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    return scanIdentifier(start);
                }
                if (isdigit(static_cast<unsigned char>(c)) ||
                    (c == '-' && isdigit(static_cast<unsigned char>(peek())))) {
                    return scanNumber(start);
                }
                throw runtime_error("无法识别的字符 '" + string(1, c) + "'");
        }
    }

    Token scanIdentifier(size_t start) {
        while (isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
            advance();
        }
        return {TokenKind::Identifier, input_.substr(start, pos_ - start), start};
    }

    Token scanNumber(size_t start) {
        while (isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
        return {TokenKind::Number, input_.substr(start, pos_ - start), start};
    }

    Token scanString(char quote, size_t start) {
        string value;
        while (peek() != '\0') {
            char c = advance();
            if (c == quote) {
                if (peek() == quote) {
                    value.push_back(advance());
                    continue;
                }
                return {TokenKind::String, value, start};
            }
            if (c == '\\') {
                char next = advance();
                switch (next) {
                    case 'n':
                        value.push_back('\n');
                        break;
                    case 't':
                        value.push_back('\t');
                        break;
                    case '\\':
                        value.push_back('\\');
                        break;
                    case '\'':
                        value.push_back('\'');
                        break;
                    case '"':
                        value.push_back('"');
                        break;
                    case '\0':
                        throw runtime_error("字符串字面量未闭合");
                    default:
                        value.push_back(next);
                        break;
                }
            } else {
                value.push_back(c);
            }
        }
        throw runtime_error("字符串字面量未闭合");
    }
};

enum class FieldType {
    Int,
    Char
};

struct ColumnDef {
    string name;
    FieldType type{};
    int length = 0;
};

struct Literal {
    bool isNumber = false;
    string text;
};

enum class CompareOp {
    Eq,
    Ne,
    Lt,
    Le,
    Gt,
    Ge
};

struct Condition {
    string column;
    CompareOp op{};
    Literal value;
};

struct Assignment {
    string column;
    Literal value;
};

enum class StatementKind {
    CreateTable,
    DropTable,
    ShowTables,
    DescribeTable,
    Insert,
    Select,
    DeleteRows,
    UpdateRows,
    Help,
    Exit
};

struct Statement {
    StatementKind kind{};
    string table;
    vector<ColumnDef> columns;
    vector<Literal> values;
    vector<string> selectColumns;
    vector<Condition> conditions;
    vector<Assignment> assignments;
    bool selectAll = false;
    bool ifExists = false;
};

class Parser {
public:
    explicit Parser(vector<Token> tokens) : tokens_(std::move(tokens)) {}

    Statement parseStatement() {
        if (matchKeyword("CREATE")) {
            return parseCreate();
        }
        if (matchKeyword("DROP")) {
            return parseDrop();
        }
        if (matchKeyword("SHOW")) {
            return parseShow();
        }
        if (matchKeyword("DESC") || matchKeyword("DESCRIBE")) {
            return parseDescribe();
        }
        if (matchKeyword("INSERT")) {
            return parseInsert();
        }
        if (matchKeyword("SELECT")) {
            return parseSelect();
        }
        if (matchKeyword("DELETE")) {
            return parseDelete();
        }
        if (matchKeyword("UPDATE")) {
            return parseUpdate();
        }
        if (matchKeyword("HELP")) {
            Statement stmt;
            stmt.kind = StatementKind::Help;
            consumeOptionalSemicolon();
            expectEnd();
            return stmt;
        }
        if (matchKeyword("EXIT") || matchKeyword("QUIT")) {
            Statement stmt;
            stmt.kind = StatementKind::Exit;
            consumeOptionalSemicolon();
            expectEnd();
            return stmt;
        }
        throw error("未知语句，请输入 HELP 查看支持的 SQL");
    }

private:
    vector<Token> tokens_;
    size_t current_ = 0;

    const Token &peek(size_t offset = 0) const {
        size_t index = current_ + offset;
        if (index >= tokens_.size()) {
            return tokens_.back();
        }
        return tokens_[index];
    }

    const Token &previous() const {
        return tokens_[current_ - 1];
    }

    bool isAtEnd() const {
        return peek().kind == TokenKind::End;
    }

    const Token &advance() {
        if (!isAtEnd()) {
            ++current_;
        }
        return previous();
    }

    bool check(TokenKind kind) const {
        return peek().kind == kind;
    }

    bool match(TokenKind kind) {
        if (!check(kind)) {
            return false;
        }
        advance();
        return true;
    }

    bool checkKeyword(const string &keyword) const {
        return peek().kind == TokenKind::Identifier && iequals(peek().text, keyword);
    }

    bool matchKeyword(const string &keyword) {
        if (!checkKeyword(keyword)) {
            return false;
        }
        advance();
        return true;
    }

    runtime_error error(const string &message) const {
        ostringstream oss;
        oss << message << "，位置 " << peek().pos;
        if (peek().kind != TokenKind::End) {
            oss << "，当前 Token: " << peek().text;
        }
        return runtime_error(oss.str());
    }

    string expectIdentifier(const string &message) {
        if (!check(TokenKind::Identifier)) {
            throw error(message);
        }
        return advance().text;
    }

    void expectKeyword(const string &keyword) {
        if (!matchKeyword(keyword)) {
            throw error("期望关键字 " + keyword);
        }
    }

    void expect(TokenKind kind, const string &message) {
        if (!match(kind)) {
            throw error(message);
        }
    }

    void consumeOptionalSemicolon() {
        match(TokenKind::Semicolon);
    }

    void expectEnd() {
        if (!isAtEnd()) {
            throw error("语句结尾存在多余内容");
        }
    }

    Statement parseCreate() {
        Statement stmt;
        stmt.kind = StatementKind::CreateTable;
        expectKeyword("TABLE");
        stmt.table = expectIdentifier("期望表名");
        expect(TokenKind::LParen, "期望 '('");
        do {
            ColumnDef column;
            column.name = expectIdentifier("期望字段名");
            if (matchKeyword("INT")) {
                column.type = FieldType::Int;
            } else if (matchKeyword("CHAR")) {
                column.type = FieldType::Char;
                expect(TokenKind::LParen, "CHAR 类型需要长度，如 CHAR(20)");
                if (!check(TokenKind::Number)) {
                    throw error("期望 CHAR 长度");
                }
                column.length = stoi(advance().text);
                if (column.length <= 0) {
                    throw error("CHAR 长度必须大于 0");
                }
                expect(TokenKind::RParen, "期望 ')'");
            } else {
                throw error("期望字段类型 INT 或 CHAR(n)");
            }
            stmt.columns.push_back(column);
        } while (match(TokenKind::Comma));
        expect(TokenKind::RParen, "期望 ')'");
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseDrop() {
        Statement stmt;
        stmt.kind = StatementKind::DropTable;
        expectKeyword("TABLE");
        if (matchKeyword("IF")) {
            expectKeyword("EXISTS");
            stmt.ifExists = true;
        }
        stmt.table = expectIdentifier("期望表名");
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseShow() {
        Statement stmt;
        stmt.kind = StatementKind::ShowTables;
        expectKeyword("TABLES");
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseDescribe() {
        Statement stmt;
        stmt.kind = StatementKind::DescribeTable;
        stmt.table = expectIdentifier("期望表名");
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseInsert() {
        Statement stmt;
        stmt.kind = StatementKind::Insert;
        expectKeyword("INTO");
        stmt.table = expectIdentifier("期望表名");
        expectKeyword("VALUES");
        expect(TokenKind::LParen, "期望 '('");
        if (!check(TokenKind::RParen)) {
            do {
                stmt.values.push_back(parseLiteral());
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::RParen, "期望 ')'");
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseSelect() {
        Statement stmt;
        stmt.kind = StatementKind::Select;
        if (match(TokenKind::Star)) {
            stmt.selectAll = true;
        } else {
            do {
                stmt.selectColumns.push_back(expectIdentifier("期望查询字段名"));
            } while (match(TokenKind::Comma));
        }
        expectKeyword("FROM");
        stmt.table = expectIdentifier("期望表名");
        parseOptionalWhere(stmt.conditions);
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseDelete() {
        Statement stmt;
        stmt.kind = StatementKind::DeleteRows;
        expectKeyword("FROM");
        stmt.table = expectIdentifier("期望表名");
        parseOptionalWhere(stmt.conditions);
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    Statement parseUpdate() {
        Statement stmt;
        stmt.kind = StatementKind::UpdateRows;
        stmt.table = expectIdentifier("期望表名");
        expectKeyword("SET");
        do {
            Assignment assignment;
            assignment.column = expectIdentifier("期望字段名");
            expect(TokenKind::Equal, "期望 '='");
            assignment.value = parseLiteral();
            stmt.assignments.push_back(assignment);
        } while (match(TokenKind::Comma));
        parseOptionalWhere(stmt.conditions);
        consumeOptionalSemicolon();
        expectEnd();
        return stmt;
    }

    void parseOptionalWhere(vector<Condition> &conditions) {
        if (!matchKeyword("WHERE")) {
            return;
        }
        do {
            Condition condition;
            condition.column = expectIdentifier("WHERE 条件中期望字段名");
            condition.op = parseCompareOp();
            condition.value = parseLiteral();
            conditions.push_back(condition);
        } while (matchKeyword("AND"));
    }

    CompareOp parseCompareOp() {
        if (match(TokenKind::Equal)) {
            return CompareOp::Eq;
        }
        if (match(TokenKind::NotEqual)) {
            return CompareOp::Ne;
        }
        if (match(TokenKind::Less)) {
            return CompareOp::Lt;
        }
        if (match(TokenKind::LessEqual)) {
            return CompareOp::Le;
        }
        if (match(TokenKind::Greater)) {
            return CompareOp::Gt;
        }
        if (match(TokenKind::GreaterEqual)) {
            return CompareOp::Ge;
        }
        throw error("期望比较运算符 =、<>、!=、<、<=、>、>=");
    }

    Literal parseLiteral() {
        if (check(TokenKind::Number)) {
            return {true, advance().text};
        }
        if (check(TokenKind::String)) {
            return {false, advance().text};
        }
        if (check(TokenKind::Identifier)) {
            return {false, advance().text};
        }
        throw error("期望字面量，如 20 或 'Alice'");
    }
};

struct Table {
    string name;
    vector<ColumnDef> columns;
};

static string fieldTypeName(FieldType type) {
    return type == FieldType::Int ? "INT" : "CHAR";
}

static string escapeCell(const string &s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '\t':
                out += "\\t";
                break;
            case '\n':
                out += "\\n";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

static string unescapeCell(const string &s) {
    string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            if (n == 't') {
                out.push_back('\t');
            } else if (n == 'n') {
                out.push_back('\n');
            } else {
                out.push_back(n);
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static vector<string> splitTabLine(const string &line) {
    vector<string> parts;
    string current;
    bool escaped = false;
    for (char c : line) {
        if (escaped) {
            current.push_back('\\');
            current.push_back(c);
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        if (c == '\t') {
            parts.push_back(unescapeCell(current));
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (escaped) {
        current.push_back('\\');
    }
    parts.push_back(unescapeCell(current));
    return parts;
}

static string joinTabLine(const vector<string> &row) {
    string line;
    for (size_t i = 0; i < row.size(); ++i) {
        if (i > 0) {
            line.push_back('\t');
        }
        line += escapeCell(row[i]);
    }
    return line;
}

class MiniDBMS {
public:
    explicit MiniDBMS(fs::path dataDir = "data")
        : dataDir_(std::move(dataDir)), catalogFile_(dataDir_ / "catalog.dbms") {
        fs::create_directories(dataDir_);
        loadCatalog();
    }

    bool execute(const Statement &stmt) {
        switch (stmt.kind) {
            case StatementKind::CreateTable:
                createTable(stmt);
                break;
            case StatementKind::DropTable:
                dropTable(stmt.table, stmt.ifExists);
                break;
            case StatementKind::ShowTables:
                showTables();
                break;
            case StatementKind::DescribeTable:
                describeTable(stmt.table);
                break;
            case StatementKind::Insert:
                insertRow(stmt);
                break;
            case StatementKind::Select:
                selectRows(stmt);
                break;
            case StatementKind::DeleteRows:
                deleteRows(stmt);
                break;
            case StatementKind::UpdateRows:
                updateRows(stmt);
                break;
            case StatementKind::Help:
                printHelp();
                break;
            case StatementKind::Exit:
                cout << "Bye.\n";
                return false;
        }
        return true;
    }

    static void printHelp() {
        cout << "支持的语句:\n";
        cout << "  CREATE TABLE name (col INT, col2 CHAR(20));\n";
        cout << "  INSERT INTO name VALUES (1, 'text');\n";
        cout << "  SELECT * FROM name WHERE col >= 1 AND col2 = 'text';\n";
        cout << "  UPDATE name SET col2 = 'new' WHERE col = 1;\n";
        cout << "  DELETE FROM name WHERE col = 1;\n";
        cout << "  SHOW TABLES;  DESC name;  DROP TABLE name;  HELP;  EXIT;\n";
    }

private:
    fs::path dataDir_;
    fs::path catalogFile_;
    map<string, Table> tables_;

    static string keyOf(const string &name) {
        return toUpper(name);
    }

    fs::path tableFile(const string &name) const {
        return dataDir_ / (keyOf(name) + ".tbl");
    }

    Table &requireTable(const string &name) {
        auto it = tables_.find(keyOf(name));
        if (it == tables_.end()) {
            throw runtime_error("表不存在: " + name);
        }
        return it->second;
    }

    const Table &requireTable(const string &name) const {
        auto it = tables_.find(keyOf(name));
        if (it == tables_.end()) {
            throw runtime_error("表不存在: " + name);
        }
        return it->second;
    }

    int columnIndex(const Table &table, const string &column) const {
        for (size_t i = 0; i < table.columns.size(); ++i) {
            if (iequals(table.columns[i].name, column)) {
                return static_cast<int>(i);
            }
        }
        throw runtime_error("字段不存在: " + column);
    }

    void loadCatalog() {
        tables_.clear();
        ifstream in(catalogFile_);
        if (!in) {
            return;
        }
        string word;
        while (in >> word) {
            if (word != "TABLE") {
                throw runtime_error("Catalog 文件损坏: 期望 TABLE");
            }
            Table table;
            size_t columnCount = 0;
            in >> table.name >> columnCount;
            for (size_t i = 0; i < columnCount; ++i) {
                string marker;
                ColumnDef column;
                string type;
                in >> marker >> column.name >> type >> column.length;
                if (marker != "COLUMN") {
                    throw runtime_error("Catalog 文件损坏: 期望 COLUMN");
                }
                if (type == "INT") {
                    column.type = FieldType::Int;
                    column.length = 0;
                } else if (type == "CHAR") {
                    column.type = FieldType::Char;
                } else {
                    throw runtime_error("Catalog 文件损坏: 未知类型 " + type);
                }
                table.columns.push_back(column);
            }
            tables_[keyOf(table.name)] = table;
        }
    }

    void saveCatalog() const {
        ofstream out(catalogFile_, ios::trunc);
        if (!out) {
            throw runtime_error("无法写入 Catalog 文件");
        }
        for (const auto &[_, table] : tables_) {
            out << "TABLE " << table.name << ' ' << table.columns.size() << '\n';
            for (const auto &column : table.columns) {
                out << "COLUMN " << column.name << ' ' << fieldTypeName(column.type) << ' '
                    << column.length << '\n';
            }
        }
    }

    vector<vector<string>> loadRows(const Table &table) const {
        vector<vector<string>> rows;
        ifstream in(tableFile(table.name));
        if (!in) {
            return rows;
        }
        string line;
        while (getline(in, line)) {
            if (line.empty()) {
                continue;
            }
            vector<string> row = splitTabLine(line);
            if (row.size() != table.columns.size()) {
                throw runtime_error("数据文件损坏: " + table.name);
            }
            rows.push_back(row);
        }
        return rows;
    }

    void saveRows(const Table &table, const vector<vector<string>> &rows) const {
        ofstream out(tableFile(table.name), ios::trunc);
        if (!out) {
            throw runtime_error("无法写入数据文件: " + table.name);
        }
        for (const auto &row : rows) {
            out << joinTabLine(row) << '\n';
        }
    }

    string normalizeLiteral(const ColumnDef &column, const Literal &literal) const {
        if (column.type == FieldType::Int) {
            if (!literal.isNumber) {
                throw runtime_error("字段 " + column.name + " 需要 INT 值");
            }
            try {
                size_t pos = 0;
                int value = stoi(literal.text, &pos);
                if (pos != literal.text.size()) {
                    throw runtime_error("");
                }
                return to_string(value);
            } catch (...) {
                throw runtime_error("字段 " + column.name + " 的整数格式不合法: " + literal.text);
            }
        }
        string value = literal.text;
        if (static_cast<int>(value.size()) > column.length) {
            ostringstream oss;
            oss << "字段 " << column.name << " 超过 CHAR(" << column.length << ") 长度";
            throw runtime_error(oss.str());
        }
        return value;
    }

    bool evaluateConditions(const Table &table, const vector<string> &row,
                            const vector<Condition> &conditions) const {
        for (const auto &condition : conditions) {
            int idx = columnIndex(table, condition.column);
            const ColumnDef &column = table.columns[idx];
            string rhs = normalizeLiteral(column, condition.value);
            if (!compareValues(column, row[idx], rhs, condition.op)) {
                return false;
            }
        }
        return true;
    }

    bool compareValues(const ColumnDef &column, const string &lhs, const string &rhs,
                       CompareOp op) const {
        int cmp = 0;
        if (column.type == FieldType::Int) {
            int a = stoi(lhs);
            int b = stoi(rhs);
            cmp = (a > b) - (a < b);
        } else {
            cmp = lhs.compare(rhs);
            cmp = (cmp > 0) - (cmp < 0);
        }
        switch (op) {
            case CompareOp::Eq:
                return cmp == 0;
            case CompareOp::Ne:
                return cmp != 0;
            case CompareOp::Lt:
                return cmp < 0;
            case CompareOp::Le:
                return cmp <= 0;
            case CompareOp::Gt:
                return cmp > 0;
            case CompareOp::Ge:
                return cmp >= 0;
        }
        return false;
    }

    void createTable(const Statement &stmt) {
        if (!isIdentifierName(stmt.table)) {
            throw runtime_error("表名不合法: " + stmt.table);
        }
        if (stmt.columns.empty()) {
            throw runtime_error("CREATE TABLE 至少需要一个字段");
        }
        string key = keyOf(stmt.table);
        if (tables_.count(key)) {
            throw runtime_error("表已存在: " + stmt.table);
        }
        Table table;
        table.name = stmt.table;
        for (const auto &column : stmt.columns) {
            if (!isIdentifierName(column.name)) {
                throw runtime_error("字段名不合法: " + column.name);
            }
            for (const auto &existing : table.columns) {
                if (iequals(existing.name, column.name)) {
                    throw runtime_error("字段重复: " + column.name);
                }
            }
            table.columns.push_back(column);
        }
        tables_[key] = table;
        saveCatalog();
        ofstream(tableFile(table.name), ios::app).close();
        cout << "OK, 表 " << table.name << " 创建成功。\n";
    }

    void dropTable(const string &name, bool ifExists) {
        auto it = tables_.find(keyOf(name));
        if (it == tables_.end()) {
            if (ifExists) {
                cout << "OK, 表 " << name << " 不存在，跳过删除。\n";
                return;
            }
            throw runtime_error("表不存在: " + name);
        }
        Table table = it->second;
        tables_.erase(keyOf(name));
        saveCatalog();
        fs::remove(tableFile(table.name));
        cout << "OK, 表 " << table.name << " 已删除。\n";
    }

    void showTables() const {
        if (tables_.empty()) {
            cout << "(无表)\n";
            return;
        }
        cout << left << setw(24) << "Table" << "Columns\n";
        cout << string(36, '-') << '\n';
        for (const auto &[_, table] : tables_) {
            cout << left << setw(24) << table.name << table.columns.size() << '\n';
        }
    }

    void describeTable(const string &name) const {
        const Table &table = requireTable(name);
        cout << "Table: " << table.name << '\n';
        cout << left << setw(18) << "Field" << setw(10) << "Type" << "Length\n";
        cout << string(36, '-') << '\n';
        for (const auto &column : table.columns) {
            cout << left << setw(18) << column.name << setw(10) << fieldTypeName(column.type)
                 << (column.type == FieldType::Char ? column.length : 0) << '\n';
        }
    }

    void insertRow(const Statement &stmt) {
        const Table &table = requireTable(stmt.table);
        if (stmt.values.size() != table.columns.size()) {
            throw runtime_error("插入值数量与字段数量不一致");
        }
        vector<string> row;
        for (size_t i = 0; i < table.columns.size(); ++i) {
            row.push_back(normalizeLiteral(table.columns[i], stmt.values[i]));
        }
        ofstream out(tableFile(table.name), ios::app);
        if (!out) {
            throw runtime_error("无法写入数据文件: " + table.name);
        }
        out << joinTabLine(row) << '\n';
        cout << "OK, 插入 1 行。\n";
    }

    void selectRows(const Statement &stmt) const {
        const Table &table = requireTable(stmt.table);
        vector<int> projectIndexes;
        if (stmt.selectAll) {
            for (size_t i = 0; i < table.columns.size(); ++i) {
                projectIndexes.push_back(static_cast<int>(i));
            }
        } else {
            for (const auto &column : stmt.selectColumns) {
                projectIndexes.push_back(columnIndex(table, column));
            }
        }

        vector<vector<string>> result;
        for (const auto &row : loadRows(table)) {
            if (!evaluateConditions(table, row, stmt.conditions)) {
                continue;
            }
            vector<string> projected;
            for (int idx : projectIndexes) {
                projected.push_back(row[idx]);
            }
            result.push_back(projected);
        }
        printRows(table, projectIndexes, result);
        cout << result.size() << " row(s) selected.\n";
    }

    void deleteRows(const Statement &stmt) {
        const Table &table = requireTable(stmt.table);
        vector<vector<string>> rows = loadRows(table);
        vector<vector<string>> kept;
        size_t removed = 0;
        for (const auto &row : rows) {
            if (evaluateConditions(table, row, stmt.conditions)) {
                ++removed;
            } else {
                kept.push_back(row);
            }
        }
        saveRows(table, kept);
        cout << "OK, 删除 " << removed << " 行。\n";
    }

    void updateRows(const Statement &stmt) {
        const Table &table = requireTable(stmt.table);
        vector<pair<int, string>> updates;
        for (const auto &assignment : stmt.assignments) {
            int idx = columnIndex(table, assignment.column);
            string value = normalizeLiteral(table.columns[idx], assignment.value);
            updates.push_back({idx, value});
        }
        vector<vector<string>> rows = loadRows(table);
        size_t changed = 0;
        for (auto &row : rows) {
            if (!evaluateConditions(table, row, stmt.conditions)) {
                continue;
            }
            for (const auto &[idx, value] : updates) {
                row[idx] = value;
            }
            ++changed;
        }
        saveRows(table, rows);
        cout << "OK, 更新 " << changed << " 行。\n";
    }

    void printRows(const Table &table, const vector<int> &indexes,
                   const vector<vector<string>> &rows) const {
        if (indexes.empty()) {
            cout << "(无字段)\n";
            return;
        }
        vector<size_t> widths(indexes.size(), 0);
        for (size_t i = 0; i < indexes.size(); ++i) {
            widths[i] = table.columns[indexes[i]].name.size();
        }
        for (const auto &row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                widths[i] = max(widths[i], row[i].size());
            }
        }
        for (size_t i = 0; i < indexes.size(); ++i) {
            cout << left << setw(static_cast<int>(widths[i] + 2))
                 << table.columns[indexes[i]].name;
        }
        cout << '\n';
        for (size_t width : widths) {
            cout << string(width, '-') << "  ";
        }
        cout << '\n';
        for (const auto &row : rows) {
            for (size_t i = 0; i < row.size(); ++i) {
                cout << left << setw(static_cast<int>(widths[i] + 2)) << row[i];
            }
            cout << '\n';
        }
    }
};

static vector<string> splitStatements(const string &script) {
    vector<string> statements;
    string current;
    bool inString = false;
    char quote = '\0';
    for (size_t i = 0; i < script.size(); ++i) {
        char c = script[i];
        current.push_back(c);
        if (inString) {
            if (c == '\\' && i + 1 < script.size()) {
                current.push_back(script[++i]);
                continue;
            }
            if (c == quote) {
                if (i + 1 < script.size() && script[i + 1] == quote) {
                    current.push_back(script[++i]);
                } else {
                    inString = false;
                }
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            inString = true;
            quote = c;
            continue;
        }
        if (c == ';') {
            string statement = trim(current);
            if (!statement.empty()) {
                statements.push_back(statement);
            }
            current.clear();
        }
    }
    string rest = trim(current);
    if (!rest.empty()) {
        statements.push_back(rest);
    }
    return statements;
}

static bool runStatement(MiniDBMS &db, const string &sql) {
    Lexer lexer(sql);
    Parser parser(lexer.scan());
    Statement statement = parser.parseStatement();
    return db.execute(statement);
}

static int runScript(MiniDBMS &db, const string &script, bool echo) {
    bool keepRunning = true;
    int failures = 0;
    for (const auto &sql : splitStatements(script)) {
        if (!keepRunning) {
            break;
        }
        if (echo) {
            cout << "dbms> " << sql << '\n';
        }
        try {
            keepRunning = runStatement(db, sql);
        } catch (const exception &ex) {
            ++failures;
            cout << "ERROR: " << ex.what() << '\n';
        }
    }
    return failures;
}

static string readWholeFile(const fs::path &path) {
    ifstream in(path);
    if (!in) {
        throw runtime_error("无法打开脚本文件: " + path.string());
    }
    ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static void runRepl(MiniDBMS &db) {
    cout << "MiniDBMS started. 输入 HELP 查看帮助，输入 EXIT 退出。\n";
    string line;
    string buffer;
    while (true) {
        cout << (buffer.empty() ? "dbms> " : "   .. ");
        if (!getline(cin, line)) {
            break;
        }
        buffer += line;
        buffer.push_back('\n');
        vector<string> statements = splitStatements(buffer);
        string trimmed = trim(buffer);
        bool endsWithSemicolon = !trimmed.empty() && trimmed.back() == ';';
        if (!endsWithSemicolon) {
            continue;
        }
        buffer.clear();
        for (const auto &sql : statements) {
            try {
                if (!runStatement(db, sql)) {
                    return;
                }
            } catch (const exception &ex) {
                cout << "ERROR: " << ex.what() << '\n';
            }
        }
    }
}

int main(int argc, char *argv[]) {
    configureConsoleEncoding();
    try {
        fs::path dataDir = "data";
        optional<fs::path> scriptFile;
        for (int i = 1; i < argc; ++i) {
            string arg = argv[i];
            if (arg == "--data") {
                if (i + 1 >= argc) {
                    throw runtime_error("--data 需要目录参数");
                }
                dataDir = argv[++i];
            } else if (arg == "--help" || arg == "-h") {
                MiniDBMS::printHelp();
                cout << "用法: mini_dbms [--data data_dir] [script.sql]\n";
                return 0;
            } else {
                scriptFile = arg;
            }
        }

        MiniDBMS db(dataDir);
        if (scriptFile) {
            string script = readWholeFile(*scriptFile);
            int failures = runScript(db, script, true);
            return failures == 0 ? 0 : 1;
        }
        runRepl(db);
        return 0;
    } catch (const exception &ex) {
        cerr << "FATAL: " << ex.what() << '\n';
        return 1;
    }
}
