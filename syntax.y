%code requires {
#include <vector>
#include <string>
#include "node.h"
#include "symbol.h"

std::string get_symbol_type(const std::string &name);
void add_symbol(const char* name, const std::string &type, const std::string &value_type);
}

%{
#include <cstdio>
#include <cstdlib>
#include <string>
#include "node.h"
#include "symbol.h"

// Bison 访问全局 AST
extern Program *g_program;
extern int yylex();
extern char* yytext;
void yyerror(const char *s);

// 保存当前声明类型
std::string current_decl_type;
%}

%union {
    long ival;
    double fval;
    Node* node;
    Program* prog;
    char* sval;
}

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> IDENT
%token <sval> STRING
%token INT REAL PRINT FANG INPUT

%type <node> stmt expr decl_list print_list
%type <prog> stmt_list block program_list program

%right '='
%left '+' '-'
%left '*' '/'

%%

// ------------------- 顶层 -------------------
program:
      program_list {
          g_program = new Program();
          for (auto n : $1->stmts) g_program->stmts.push_back(n);
          delete $1;
      }
    ;

program_list:
      /* empty */ { $$ = new Program(); }
    | program_list block {
          $$ = $1;
          if ($2) $$->stmts.push_back(NodePtr($2));
      }
    ;

block:
      FANG '{' stmt_list '}' {
          Program* p = new Program();
          for (auto n : $3->stmts) p->stmts.push_back(NodePtr(n));
          delete $3;
          $$ = p;
      }
    ;

// ------------------- 语句 -------------------
stmt_list:
      /* empty */ { $$ = new Program(); }
    | stmt_list stmt {
          if($2) $1->stmts.push_back(NodePtr($2));
          $$ = $1;
      }
    ;

stmt:
      INT { current_decl_type = "int"; } decl_list ';' { $$ = $3; }
    | REAL { current_decl_type = "real"; } decl_list ';' { $$ = $3; }

    | IDENT '=' expr ';' {
          std::string rhs_type = "int";

          if (dynamic_cast<Real*>($3)) rhs_type = "real";
          else if (auto v = dynamic_cast<Var*>($3)) {
              std::string vt = get_symbol_type(v->name);
              if (vt == "real") rhs_type = "real";
          }
          else if (auto b = dynamic_cast<Binary*>($3)) {
              if (dynamic_cast<Real*>(b->left.get()) || dynamic_cast<Real*>(b->right.get())) rhs_type = "real";
              if (auto lv = dynamic_cast<Var*>(b->left.get()))
                  if (get_symbol_type(lv->name) == "real") rhs_type = "real";
              if (auto rv = dynamic_cast<Var*>(b->right.get()))
                  if (get_symbol_type(rv->name) == "real") rhs_type = "real";
          }
          else if (dynamic_cast<InputNode*>($3)) rhs_type = "real";

          // ⚡ 修正版：如果符号已存在但 value_type 为空，则更新类型；
          // 对于首次在赋值中出现的标识符，记为 ident + rhs_type
          std::string old_type = get_symbol_type($1);
          if (old_type == "")
              add_symbol($1, "ident", rhs_type);

          $$ = new AssignStmt(std::string($1), NodePtr($3));
          free($1);
      }

    | PRINT '(' print_list ')' ';' { $$ = $3; }
    | expr ';' { $$ = new ExprStmt(NodePtr($1)); }
    ;

decl_list:
      IDENT '=' expr {
          add_symbol($1, "ident", current_decl_type);

          Program* p = new Program();
          p->stmts.push_back(NodePtr(new AssignStmt($1, NodePtr($3))));
          free($1);
          $$ = p;
      }
    | IDENT {
          add_symbol($1, "ident", current_decl_type);

          Program* p = new Program();
          p->stmts.push_back(NodePtr(new AssignStmt($1, nullptr)));
          free($1);
          $$ = p;
      }
    | decl_list ',' IDENT '=' expr {
          Program* p = static_cast<Program*>($1);
          add_symbol($3, "ident", current_decl_type);
          p->stmts.push_back(NodePtr(new AssignStmt($3, NodePtr($5))));
          free($3);
          $$ = p;
      }
    | decl_list ',' IDENT {
          Program* p = static_cast<Program*>($1);
          add_symbol($3, "ident", current_decl_type);
          p->stmts.push_back(NodePtr(new AssignStmt($3, nullptr)));
          free($3);
          $$ = p;
      }
    ;

expr:
      INTEGER { $$ = new Integer($1); }
    | FLOAT   { $$ = new Real($1); }
    | IDENT   { $$ = new Var(std::string($1)); free($1); }
    | INPUT '(' STRING ')' { $$ = new InputNode(std::string($3)); free($3); }
    | expr '+' expr { $$ = new Binary("+", NodePtr($1), NodePtr($3)); }
    | expr '-' expr { $$ = new Binary("-", NodePtr($1), NodePtr($3)); }
    | expr '*' expr { $$ = new Binary("*", NodePtr($1), NodePtr($3)); }
    | expr '/' expr { $$ = new Binary("/", NodePtr($1), NodePtr($3)); }
    | '(' expr ')'  { $$ = $2; }
    ;

print_list:
      expr {
          std::vector<NodePtr> v; v.push_back(NodePtr($1));
          $$ = new PrintStmtList(v);
      }
    | STRING {
          std::vector<NodePtr> v; v.push_back(NodePtr(new StringNode($1)));
          $$ = new PrintStmtList(v);
      }
    | print_list ',' expr {
          auto list = dynamic_cast<PrintStmtList*>($1);
          list->exprs.push_back(NodePtr($3)); $$ = list;
      }
    | print_list ',' STRING {
          auto list = dynamic_cast<PrintStmtList*>($1);
          list->exprs.push_back(NodePtr(new StringNode($3))); $$ = list;
      }
    ;

%%

void yyerror(const char *s) {
    fprintf(stderr, "Syntax error: %s (near token '%s')\n", s, yytext);
}

