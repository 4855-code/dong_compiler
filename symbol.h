#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

struct Symbol {
    std::string name;
    std::string type;       // "ident", "literal"
    std::string value_type; // "int", "real"
};

extern std::vector<Symbol> sym_table;

// 添加符号（string版本）
inline void add_symbol(const std::string &text, const std::string &type, const std::string &value_type = "") {
    for (auto &s : sym_table) {
        if (s.name == text) {
            // ⚡ 如果已有符号但 value_type 为空，则更新
            if (!value_type.empty() && s.value_type.empty()) {
                s.type = type;
                s.value_type = value_type;
            }
            return;
        }
    }
    Symbol s;
    s.name = text;
    s.type = type;
    s.value_type = value_type;
    sym_table.push_back(s);
}

// 添加符号（const char* 版本，供 syntax.y 调用）
inline void add_symbol(const char* name, const std::string &type, const std::string &value_type = "") {
    add_symbol(std::string(name), type, value_type);
}

// 获取符号的 value_type（int / real）
inline std::string get_symbol_type(const std::string &name) {
    for (auto &s : sym_table)
        if (s.name == name)
            return s.value_type;
    return "";
}

// 输出符号表
inline void print_sym_table() {
    std::cout << "\n=== Symbol Table ===\n";

    printf("%-20s %-10s %-10s\n", "Name", "Type", "ValueType");
    printf("%-20s %-10s %-10s\n", "-------------------", "---------", "---------");

    for (auto &s : sym_table) {
        printf("%-20s %-10s %-10s\n",
               s.name.c_str(),
               s.type.c_str(),
               s.value_type.c_str());
    }

    std::cout << "=====================\n";
}


#endif // SYMBOL_H

