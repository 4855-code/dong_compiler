#ifndef NODE_H
#define NODE_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <algorithm>
#include "symbol.h"

// 全局符号表在 main.cpp 中定义；在这里声明为 extern 供 node 使用
extern std::vector<Symbol> sym_table;

struct Node { 
    virtual ~Node()=default; 
    virtual void dump(int indent=0) const = 0;

    // ===== 新增树状打印 =====
    virtual std::string node_name() const = 0;
    virtual std::vector<Node*> get_children() const { return {}; }

    void print_prefix(const std::string &prefix, bool isLast) const {
        std::cout << prefix;
        std::cout << (isLast ? "└── " : "├── ");
    }

    // 声明：具体实现放在文件末尾，带 root / L / R 标记
    void dump_tree(const std::string &prefix = "", bool isLast = true) const;
};

using NodePtr = std::shared_ptr<Node>;

static void indent_print(int n){ for(int i=0;i<n;i++) std::cout<<"  "; }

struct Expr : Node {};

// ======= 表达式类型 =======
struct Integer : Expr {
    long val;
    Integer(long v):val(v){}
    void dump(int indent=0) const override { indent_print(indent); std::cout<<"Integer("<<val<<")\n"; }
    std::string node_name() const override { return "Integer(" + std::to_string(val) + ")"; }
};

struct Real : Expr {
    double val;
    Real(double v):val(v){}
    void dump(int indent=0) const override { indent_print(indent); std::cout<<"Real("<<val<<")\n"; }
    std::string node_name() const override { return "Real(" + std::to_string(val) + ")"; }
};

struct Var : Expr {
    std::string name;
    std::string type;
    Var(const std::string &n): name(n), type("unknown") {
        auto it = std::find_if(sym_table.begin(), sym_table.end(),
            [&](const Symbol &s){ return s.name==n; });
        if(it != sym_table.end()) type = it->value_type;
    }
    void dump(int indent=0) const override { indent_print(indent); std::cout << "Var(" << name << ":" << type << ")\n"; }
    std::string node_name() const override { return "Var(" + name + ":" + type + ")"; }
};

struct Binary : Expr {
    std::string op;
    NodePtr left,right;
    Binary(const std::string &o, NodePtr l, NodePtr r):op(o),left(l),right(r){}
    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout<<"Binary("<<op<<")\n";
        if(left) left->dump(indent+1);
        if(right) right->dump(indent+1);
    }
    std::string node_name() const override { return "Binary(" + op + ")"; }
    std::vector<Node*> get_children() const override { return { left.get(), right.get() }; }
};

// ======= StringNode =======
struct StringNode : Expr {
    std::string value;
    StringNode(const std::string &v) : value(v) {}
    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout << "StringNode(" << value << ")\n";
    }
    std::string node_name() const override { return "StringNode(" + value + ")"; }
};

// ======= 语句类型 =======
struct Stmt : Node {};

struct ExprStmt : Stmt {
    NodePtr expr;
    ExprStmt(NodePtr e):expr(e){}
    void dump(int indent=0) const override { indent_print(indent); std::cout<<"ExprStmt\n"; if(expr) expr->dump(indent+1);}
    std::string node_name() const override { return "ExprStmt"; }
    std::vector<Node*> get_children() const override { return { expr.get() }; }
};

// ======= AssignStmt：在树上伪装成 Binary(=)，有左右子节点 =======
struct AssignStmt : Stmt {
    std::string name;  // 变量名，给符号表 / TAC / 汇编用
    NodePtr lhs;       // 左子节点：Var(name)
    NodePtr expr;      // 右子节点：原来的表达式

    // 兼容原来的构造：new AssignStmt($1, NodePtr($3))
    AssignStmt(const std::string &n, NodePtr e)
        : name(n),
          lhs(std::make_shared<Var>(n)),
          expr(e)
    {}

    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout << "Binary(=)\n";
        if (lhs)  lhs->dump(indent+1);
        if (expr) expr->dump(indent+1);
    }

    std::string node_name() const override {
        return "Binary(=)";
        // 若想带上变量名可以改成：
        // return "Binary(=," + name + ")";
    }

    std::vector<Node*> get_children() const override {
        std::vector<Node*> c;
        if (lhs)  c.push_back(lhs.get());   // [L]
        if (expr) c.push_back(expr.get());  // [R]
        return c;
    }
};

// print 单参数（兼容旧版本）
struct PrintStmt : Stmt {
    NodePtr expr;
    PrintStmt(NodePtr e):expr(e){}
    void dump(int indent=0) const override { indent_print(indent); std::cout<<"Print\n"; if(expr) expr->dump(indent+1);}
    std::string node_name() const override { return "Print"; }
    std::vector<Node*> get_children() const override { return { expr.get() }; }
};

// ======= InputNode：拆开，下面挂一个 StringNode 子节点 =======
struct InputNode : Expr {
    std::string prompt;   // 仍给 asm_generator 等用
    NodePtr prompt_node;  // AST 上的子节点

    InputNode(const std::string &p)
        : prompt(p),
          prompt_node(std::make_shared<StringNode>(p))
    {}

    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout << "Input\n";
        if (prompt_node) prompt_node->dump(indent + 1);
    }

    std::string node_name() const override {
        return "Input";
    }

    std::vector<Node*> get_children() const override {
        return { prompt_node.get() };
    }
};

// ======= Program 节点 =======
struct Program : Node {
    std::vector<NodePtr> stmts;
    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout<<"Program\n";
        for(auto &s:stmts) s->dump(indent+1);
    }
    std::string node_name() const override { return "Program"; }
    std::vector<Node*> get_children() const override {
        std::vector<Node*> c;
        for (auto &s : stmts) c.push_back(s.get());
        return c;
    }
};

// ======= PrintStmtList =======
struct PrintStmtList : Stmt {
    std::vector<NodePtr> exprs;
    PrintStmtList(const std::vector<NodePtr>& v) : exprs(v) {}
    void dump(int indent=0) const override {
        indent_print(indent);
        std::cout << "PrintStmtList\n";
        for(auto &e: exprs) e->dump(indent+1);
    }
    std::string node_name() const override { return "PrintStmtList"; }
    std::vector<Node*> get_children() const override {
        std::vector<Node*> c;
        for (auto &e : exprs) c.push_back(e.get());
        return c;
    }
};

// ========== 带 [root]/[L]/[R]/[child] 标记的树打印实现 ==========

// 递归辅助函数
static inline void dump_tree_impl(
    const Node* node,
    const std::string &prefix,
    bool isLast,
    const std::string &role   // "root" / "L" / "R" / "child" / ""
) {
    if (!node) return;

    // 打印前缀和树枝
    node->print_prefix(prefix, isLast);

    // 打印角色标记
    if (!role.empty()) {
        std::cout << "[" << role << "] ";
    }

    // 打印当前结点名字
    std::cout << node->node_name() << "\n";

    // 递归子结点
    std::vector<Node*> children = node->get_children();
    for (size_t i = 0; i < children.size(); ++i) {
        bool lastChild = (i == children.size() - 1);

        // 给子结点算一个角色
        std::string childRole;
        if (children.size() == 2) {
            // 恰好两个孩子：按左右子结点标
            childRole = (i == 0 ? "L" : "R");
        } else if (role == "root") {
            // 根节点的直接孩子：统一叫 child
            childRole = "child";
        } else {
            childRole = ""; // 再往下如果还想标，可以根据需要改这里
        }

        dump_tree_impl(
            children[i],
            prefix + (isLast ? "    " : "│   "),
            lastChild,
            childRole
        );
    }
}

// Node::dump_tree 对外入口
inline void Node::dump_tree(const std::string &prefix, bool isLast) const {
    // 外面一般直接 root->dump_tree()
    dump_tree_impl(this, prefix, isLast, prefix.empty() ? "root" : "");
}

#endif // NODE_H

