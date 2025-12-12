%{
#include "syntax.tab.h"   
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "symbol.h"        
void yyerror(const char *s);  
%}

DIGIT   [0-9]
ID      [a-zA-Z_][a-zA-Z0-9_]*
WS      [ \t\r\n]+

%%

"//".*           {  }
{WS}             {  }

"int"    { printf("[Keyword] int\n");  return INT; }      
"real"   { printf("[Keyword] real\n"); return REAL; }     
"print"  { printf("[Keyword] print\n");return PRINT; }   
"fang"   { printf("[Keyword] fang\n"); return FANG; }     
"input"  { printf("[Keyword] input\n"); return INPUT; }

\"[^\"]*\"  { 
    printf("[String] %s\n", yytext); 
    yylval.sval = strdup(yytext); 
    return STRING; 
}

{DIGIT}+"."{DIGIT}+  {
    printf("[Number] %s (real)\n", yytext);
    yylval.fval = atof(yytext);             
    add_symbol(yytext, "literal", "real");  
    return FLOAT;                           
}

{DIGIT}+ {
    printf("[Number] %s (int)\n", yytext);
    yylval.ival = atoi(yytext);
    add_symbol(yytext, "literal", "int");   
    return INTEGER;                         
}

{ID} {
    printf("[Identifier] %s\n", yytext);
    yylval.sval = strdup(yytext);           
    add_symbol(yytext, "ident");            
    return IDENT;                           
}

"="   { printf("[Operator] =\n"); return '='; }
"+"   { printf("[Operator] +\n"); return '+'; }
"-"   { printf("[Operator] -\n"); return '-'; }
"*"   { printf("[Operator] *\n"); return '*'; }
"/"   { printf("[Operator] /\n"); return '/'; }

";"   { printf("[Symbol] ;\n"); return ';'; }
"("   { printf("[Symbol] (\n"); return '('; }
")"   { printf("[Symbol] )\n"); return ')'; }
"{"   { printf("[Symbol] {\n"); return '{'; }
"}"   { printf("[Symbol] }\n"); return '}'; }
","   { printf("[Symbol] ,\n"); return ','; }

. {
    printf("[Unknown] %s\n", yytext);
}

%%

int yywrap(void) { 
    print_sym_table();  
    return 1;           
}

