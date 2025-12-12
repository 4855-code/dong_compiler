//main.cpp
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include "node.h"
#include "symbol.h"
#include "asm_generator.h"

// -------------------- 全局变量 --------------------
std::vector<Symbol> sym_table;
Program* g_program = nullptr;  // ✅ 实际定义

extern int yyparse();
extern FILE* yyin;
extern void yyrestart(FILE*);

// -------------------- TAC 生成 --------------------
int temp_counter = 0;
std::vector<std::string> tac_list;
std::string newTemp() { return "t" + std::to_string(temp_counter++); }

std::string generateTAC(Node* node) {
    if (!node) return "";

    if (auto assign = dynamic_cast<AssignStmt*>(node)) {
        std::string rhs = generateTAC(assign->expr.get());
        tac_list.push_back(assign->name + " = " + rhs);
        return assign->name;
    } else if (auto binary = dynamic_cast<Binary*>(node)) {
        std::string left = generateTAC(binary->left.get());
        std::string right = generateTAC(binary->right.get());
        std::string tmp = newTemp();
        tac_list.push_back(tmp + " = " + left + " " + binary->op + " " + right);
        return tmp;
    } else if (auto integer = dynamic_cast<Integer*>(node)) return std::to_string(integer->val);
    else if (auto real = dynamic_cast<Real*>(node)) return std::to_string(real->val);
    else if (auto var = dynamic_cast<Var*>(node)) return var->name;
    else if (auto strNode = dynamic_cast<StringNode*>(node)) {
        std::string tmp = newTemp();
        tac_list.push_back(tmp + " = \"" + strNode->value + "\"");
        return tmp;
    } else if (auto input = dynamic_cast<InputNode*>(node)) {
        std::string tmp = newTemp();
        tac_list.push_back(tmp + " = input(" + input->prompt + ")");
        return tmp;
    } else if (auto printList = dynamic_cast<PrintStmtList*>(node)) {
        for (auto &e : printList->exprs) {
            std::string var = generateTAC(e.get());
            tac_list.push_back("print " + var);
        }
        return "";
    } else if (auto program = dynamic_cast<Program*>(node)) {
        for (auto &stmt : program->stmts) generateTAC(stmt.get());
        return "";
    }
    return "";
}

void outputTAC(Program* root) {
    tac_list.clear();
    temp_counter = 0;
    generateTAC(root);
    std::cout << "\n=== Three-Address Code ===\n";
    for (auto &line : tac_list) std::cout << line << "\n";
    std::cout << "==========================\n";
}

// -------------------- Main --------------------
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " source.fang\n";
        return 1;
    }

    yyin = fopen(argv[1], "r");
    if (!yyin) { perror("fopen"); return 1; }
    yyrestart(yyin);

    auto start = std::chrono::high_resolution_clock::now();
    int parse_result = yyparse();
    auto end = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();

    if (yyin) { fclose(yyin); yyin = nullptr; }

    if (parse_result == 0) {
        std::cout << "\nParse succeeded. AST:\n";
        if (g_program) g_program->dump_tree();

        if (g_program) outputTAC(g_program);

        // ------------------ 汇编生成 ------------------
        const std::string asm_out = "out.s";
        if (g_program && generate_asm(g_program, asm_out)) {
            std::cout << "\n✅ Assembly file generated: " << asm_out << "\n";

            const std::string exe_out = "out";
            std::string cmd = "gcc -no-pie " + asm_out + " -o " + exe_out;

            std::cout << "🔧 Assembling & linking...\n";
            int rc = system(cmd.c_str());
            if (rc == 0) {
                std::cout << "✅ Executable generated: " << exe_out << "\n";
                std::cout << "You can run it with: ./out\n";

                std::cout << "\n▶ Running program:\n";
                std::cout << "(Please enter values when prompted)\n";
                std::cout << "-----------------------------------\n";
                //system(("./" + exe_out).c_str());自动运行程序
                std::cout << "-----------------------------------\n";
            } else {
                std::cerr << "❌ gcc failed (exit code " << rc << ")\n";
            }
        } else {
            std::cerr << "❌ Failed to generate assembly\n";
        }
    } else {
        std::cerr << "❌ Parse failed.\n";
        return 1;
    }

    std::cout << "\nCompilation time: " << elapsed << " seconds\n";
    return 0;
}

