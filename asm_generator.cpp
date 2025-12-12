// =============================
// asm_generator.cpp (最终修正版)
// 自动检测变量类型并正确选择 scanf / printf
// =============================
#include "node.h"
#include "symbol.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
using namespace std;

// ===== 数据结构 =====
struct StringData { string label; string text; };
struct RealData { string label; double value; };

static vector<StringData> ro_strings;
static vector<RealData> ro_reals;
static int str_counter = 0;
static int real_counter = 0;
static int input_cleanup_counter = 0;

// ===== 工具函数 =====
static string strip_quotes(const string &s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')))
        return s.substr(1, s.size() - 2);
    return s;
}
static string make_str_label() { ostringstream oss; oss << "str_" << str_counter++; return oss.str(); }
static string make_real_label() { ostringstream oss; oss << "LC_real_" << real_counter++; return oss.str(); }

static string intern_string(const string &raw) {
    string txt = strip_quotes(raw);
    for (auto &s : ro_strings) if (s.text == txt) return s.label;
    string lbl = make_str_label();
    ro_strings.push_back({lbl, txt});
    return lbl;
}
static string intern_real(double v) {
    for (auto &r : ro_reals) if (r.value == v) return r.label;
    string lbl = make_real_label();
    ro_reals.push_back({lbl, v});
    return lbl;
}

// ===== 收集 rodata =====
static void collect_rodata_expr(Node* node) {
    if (!node) return;
    if (auto r = dynamic_cast<Real*>(node)) { intern_real(r->val); return; }
    if (auto s = dynamic_cast<StringNode*>(node)) { intern_string(s->value); return; }
    if (auto in = dynamic_cast<InputNode*>(node)) { intern_string(in->prompt); return; }
    if (auto b = dynamic_cast<Binary*>(node)) {
        collect_rodata_expr(b->left.get());
        collect_rodata_expr(b->right.get());
    }
}
static void collect_rodata_stmt(Node* stmt) {
    if (!stmt) return;
    if (auto as = dynamic_cast<AssignStmt*>(stmt)) collect_rodata_expr(as->expr.get());
    else if (auto pl = dynamic_cast<PrintStmtList*>(stmt)) for (auto &e : pl->exprs) collect_rodata_expr(e.get());
    else if (auto ps = dynamic_cast<PrintStmt*>(stmt)) collect_rodata_expr(ps->expr.get());
    else if (auto prog = dynamic_cast<Program*>(stmt)) for (auto &s : prog->stmts) collect_rodata_stmt(s.get());
}

// ===== 收集符号表变量 =====
static set<string> collect_idents() {
    set<string> ids;
    for (auto &s : sym_table)
        if (s.type == "ident") ids.insert(s.name);
    return ids;
}

// ===== 汇编头尾 =====
static void emit_prologue(ostream &os, const set<string> &vars) {
    os << "\t.section .rodata\n";
    os << "fmt_int_print:\n\t.asciz \"%ld\\n\"\n";
    os << "fmt_int_scanf:\n\t.asciz \"%ld\"\n";
    os << "fmt_double_print:\n\t.asciz \"%f\\n\"\n";
    os << "fmt_double_scanf:\n\t.asciz \"%lf\"\n";
    os << "fmt_str_print:\n\t.asciz \"%s\\n\"\n";

    for (auto &s : ro_strings) {
        string safe = s.text;
        size_t pos;
        while ((pos = safe.find('\n')) != string::npos) safe.replace(pos, 1, "\\n");
        os << s.label << ":\n\t.asciz \"" << safe << "\"\n";
    }
    for (auto &r : ro_reals) {
        ostringstream oss; oss << setprecision(12) << r.value;
        os << r.label << ":\n\t.double " << oss.str() << "\n";
    }

    os << "\n\t.section .data\n";
    os << "\t.globl input_val_int\ninput_val_int:\n\t.quad 0\n";
    os << "\t.globl input_val_real\ninput_val_real:\n\t.double 0.0\n";

    for (auto &v : vars) {
        string vt = get_symbol_type(v);
        os << "\t.globl " << v << "\n";
        if (vt == "real" || vt == "double" || vt == "float")
            os << v << ":\n\t.double 0.0\n";
        else
            os << v << ":\n\t.quad 0\n";
    }

    os << "\n\t.section .text\n\t.globl main\n\t.type main, @function\n";
    os << "main:\n\tpushq %rbp\n\tmovq %rsp, %rbp\n";
}
static void emit_epilogue(ostream &os) { os << "\tleave\n\tret\n"; }

// ===== 输入函数 =====
static void emit_input_int(const string &prompt_label, ostream &os) {
    os << "\tleaq " << prompt_label << "(%rip), %rdi\n";
    os << "\txor %rax,%rax\n\tcall printf@PLT\n";
    os << "\txor %rdi,%rdi\n\tcall fflush@PLT\n";
    os << "\tleaq input_val_int(%rip), %rsi\n";
    os << "\tleaq fmt_int_scanf(%rip), %rdi\n";
    os << "\txor %rax,%rax\n\tcall scanf@PLT\n";
    int id = input_cleanup_counter++;
    os << ".__cleanup_input_int_" << id << ":\n\tcall getchar@PLT\n";
    os << "\tcmp $-1,%eax\n\tje .__cleanup_input_int_" << id << "_end\n";
    os << "\tcmp $10,%eax\n\tje .__cleanup_input_int_" << id << "_end\n";
    os << "\tjmp .__cleanup_input_int_" << id << "\n";
    os << ".__cleanup_input_int_" << id << "_end:\n";
    os << "\tmovq input_val_int(%rip), %rax\n";
}
static void emit_input_real(const string &prompt_label, ostream &os) {
    os << "\tleaq " << prompt_label << "(%rip), %rdi\n";
    os << "\txor %rax,%rax\n\tcall printf@PLT\n";
    os << "\txor %rdi,%rdi\n\tcall fflush@PLT\n";
    os << "\tleaq input_val_real(%rip), %rsi\n";
    os << "\tleaq fmt_double_scanf(%rip), %rdi\n";
    os << "\txor %rax,%rax\n\tcall scanf@PLT\n";
    int id = input_cleanup_counter++;
    os << ".__cleanup_input_real_" << id << ":\n\tcall getchar@PLT\n";
    os << "\tcmp $-1,%eax\n\tje .__cleanup_input_real_" << id << "_end\n";
    os << "\tcmp $10,%eax\n\tje .__cleanup_input_real_" << id << "_end\n";
    os << "\tjmp .__cleanup_input_real_" << id << "\n";
    os << ".__cleanup_input_real_" << id << "_end:\n";
    os << "\tmovsd input_val_real(%rip), %xmm0\n";
}

// ===== 类型判断 =====
static bool expr_is_real(Node* node) {
    if (!node) return false;
    if (dynamic_cast<Real*>(node)) return true;
    if (auto v = dynamic_cast<Var*>(node)) {
        string vt = get_symbol_type(v->name);
        return (vt == "real" || vt == "double" || vt == "float");
    }
    if (auto b = dynamic_cast<Binary*>(node))
        return expr_is_real(b->left.get()) || expr_is_real(b->right.get());
    if (dynamic_cast<InputNode*>(node)) return true;
    return false;
}

// ===== 表达式生成 =====
static void emit_expr(Node* node, ostream &os, bool expect_real = false) {
    if (!node) { os << "\tmovq $0, %rax\n"; return; }

    if (auto i = dynamic_cast<Integer*>(node)) { os << "\tmovq $" << i->val << ", %rax\n"; return; }
    if (auto r = dynamic_cast<Real*>(node)) { string lbl = intern_real(r->val); os << "\tmovsd " << lbl << "(%rip), %xmm0\n"; return; }
    if (auto v = dynamic_cast<Var*>(node)) {
        string vt = get_symbol_type(v->name);
        if (vt == "real" || vt == "double" || vt == "float")
            os << "\tmovsd " << v->name << "(%rip), %xmm0\n";
        else
            os << "\tmovq " << v->name << "(%rip), %rax\n";
        return;
    }
    if (auto in = dynamic_cast<InputNode*>(node)) {
        string lbl = intern_string(in->prompt);
        if (expect_real) emit_input_real(lbl, os);
        else emit_input_int(lbl, os);
        return;
    }
    if (auto b = dynamic_cast<Binary*>(node)) {
        bool is_real = expr_is_real(b) || expect_real;
        if (is_real) {
            emit_expr(b->left.get(), os, true);
            os << "\tmovsd %xmm0, %xmm2\n";
            emit_expr(b->right.get(), os, true);
            os << "\tmovsd %xmm2, %xmm1\n";
            if (b->op == "+") os << "\taddsd %xmm0, %xmm1\n";
            else if (b->op == "-") os << "\tsubsd %xmm0, %xmm1\n";
            else if (b->op == "*") os << "\tmulsd %xmm0, %xmm1\n";
            else if (b->op == "/") os << "\tdivsd %xmm0, %xmm1\n";
            os << "\tmovsd %xmm1, %xmm0\n";
        } else {
            emit_expr(b->left.get(), os, false); os << "\tpushq %rax\n";
            emit_expr(b->right.get(), os, false); os << "\tpopq %rcx\n";
            if (b->op == "+") os << "\taddq %rax, %rcx\n\tmovq %rcx, %rax\n";
            else if (b->op == "-") os << "\tsubq %rax, %rcx\n\tmovq %rcx, %rax\n";
            else if (b->op == "*") os << "\timulq %rax, %rcx\n\tmovq %rcx, %rax\n";
            else if (b->op == "/") { os << "\tmovq %rax, %rbx\n\tmovq %rcx, %rax\n\tcqto\n\tidivq %rbx\n"; }
        }
        return;
    }
    os << "\tmovq $0, %rax\n";
}

// ===== print 语句 =====
static void emit_print_list(PrintStmtList* pl, ostream &os) {
    for (auto &e_ptr : pl->exprs) {
        Node* e = e_ptr.get();
        if (auto s = dynamic_cast<StringNode*>(e)) {
            string lbl = intern_string(s->value);
            os << "\tleaq " << lbl << "(%rip), %rdi\n";
            os << "\txor %rax,%rax\n\tcall printf@PLT\n";
        } else {
            bool is_real = expr_is_real(e);
            emit_expr(e, os, is_real);
            if (is_real) {
                os << "\tleaq fmt_double_print(%rip), %rdi\n";
                os << "\tmov $1, %rax\n\tcall printf@PLT\n";   // ✅ 修正
            } else {
                os << "\tleaq fmt_int_print(%rip), %rdi\n";
                os << "\tmovq %rax, %rsi\n";
                os << "\txor %rax,%rax\n\tcall printf@PLT\n";
            }
        }
    }
}

// ===== 语句生成 =====
static void emit_stmt(Node* stmt, ostream &ofs) {
    if (!stmt) return;
    if (auto prog = dynamic_cast<Program*>(stmt)) { for (auto &s : prog->stmts) emit_stmt(s.get(), ofs); return; }

    if (auto as = dynamic_cast<AssignStmt*>(stmt)) {
        if (!as->expr) return;
        if (auto in = dynamic_cast<InputNode*>(as->expr.get())) {
            string lbl = intern_string(in->prompt);
            string vt = get_symbol_type(as->name);
            bool tgt_real = (vt == "real" || vt == "double" || vt == "float");
            if (tgt_real) { emit_input_real(lbl, ofs); ofs << "\tmovsd %xmm0, " << as->name << "(%rip)\n"; }
            else { emit_input_int(lbl, ofs); ofs << "\tmovq %rax, " << as->name << "(%rip)\n"; }
        } else {
            string vt = get_symbol_type(as->name);
            bool expect_real = (vt == "real" || vt == "double" || vt == "float");
            emit_expr(as->expr.get(), ofs, expect_real);
            if (expect_real) ofs << "\tmovsd %xmm0, " << as->name << "(%rip)\n";
            else ofs << "\tmovq %rax, " << as->name << "(%rip)\n";
        }
        return;
    }

    if (auto pl = dynamic_cast<PrintStmtList*>(stmt)) { emit_print_list(pl, ofs); return; }

    if (auto ps = dynamic_cast<PrintStmt*>(stmt)) {
        bool is_real = expr_is_real(ps->expr.get());
        emit_expr(ps->expr.get(), ofs, is_real);
        if (is_real)
            ofs << "\tleaq fmt_double_print(%rip), %rdi\n\tmov $1, %rax\n\tcall printf@PLT\n";  // ✅ 修正
        else
            ofs << "\tleaq fmt_int_print(%rip), %rdi\n\tmovq %rax,%rsi\n\txor %rax,%rax\n\tcall printf@PLT\n";
        return;
    }
}

// ===== 顶层接口 =====
bool generate_asm(Program* root, const string &out_filename) {
    if (!root) return false;
    ofstream ofs(out_filename);
    if (!ofs) return false;

    ro_strings.clear(); ro_reals.clear(); str_counter = real_counter = input_cleanup_counter = 0;
    for (auto &stmt : root->stmts) collect_rodata_stmt(stmt.get());
    auto idents = collect_idents();

    emit_prologue(ofs, idents);
    for (auto &stmt : root->stmts) emit_stmt(stmt.get(), ofs);
    emit_epilogue(ofs);
    ofs.close();
    return true;
}

