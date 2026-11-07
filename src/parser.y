%code requires {
#include "ast.hpp"
#include <string>
}

%{
#include "ast.hpp"

#include <sstream>
#include <string>

AstNode* g_root = nullptr;
std::string g_parse_error;
std::string g_lex_error;

int yylex(void);
void yyerror(const char* message);
%}

%define parse.error detailed
%locations

%union {
    AstNode* node;
    std::string* text;
}

%token T_INT T_DOUBLE T_BOOL T_TRUE T_FALSE
%token T_IF T_ELSE T_WHILE T_PRINT T_RETURN T_MAIN
%token T_COUT T_ENDL
%token T_LE T_GE T_EQ T_NE T_AND T_OR T_SHL
%token T_INVALID
%token <text> IDENTIFIER NUMBER

%type <node> program main_function statement_list statement block
%type <node> declaration assignment print_statement output_statement
%type <node> if_statement while_statement return_statement
%type <node> expression unary primary
%type <text> type

%destructor { if ($$ != g_root) delete $$; } <node>
%destructor { delete $$; } <text>

%left T_OR
%left T_AND
%left T_EQ T_NE
%left '<' T_LE '>' T_GE
%left '+' '-'
%left '*' '/' '%'
%right UNOT UMINUS UPLUS
%nonassoc LOWER_THAN_ELSE
%nonassoc T_ELSE

%start program

%%

program
    : main_function
      {
          $$ = makeNode("Program");
          addChild($$, $1);
          g_root = $$;
      }
    | statement_list
      {
          $$ = makeNode("Program");
          addChild($$, $1);
          g_root = $$;
      }
    ;

main_function
    : T_INT T_MAIN '(' ')' block
      {
          $$ = makeNode("MainFunction", "int main()");
          addChild($$, $5);
      }
    ;

statement_list
    : %empty
      {
          $$ = makeNode("Statements");
      }
    | statement_list statement
      {
          addChild($1, $2);
          $$ = $1;
      }
    ;

statement
    : declaration       { $$ = $1; }
    | assignment        { $$ = $1; }
    | print_statement   { $$ = $1; }
    | output_statement  { $$ = $1; }
    | if_statement      { $$ = $1; }
    | while_statement   { $$ = $1; }
    | return_statement  { $$ = $1; }
    | block             { $$ = $1; }
    | ';'               { $$ = makeNode("EmptyStatement"); }
    ;

block
    : '{' statement_list '}'
      {
          $$ = makeNode("Block");
          addChild($$, $2);
      }
    ;

type
    : T_INT    { $$ = new std::string("int"); }
    | T_DOUBLE { $$ = new std::string("double"); }
    | T_BOOL   { $$ = new std::string("bool"); }
    ;

declaration
    : type IDENTIFIER ';'
      {
          $$ = makeNode("Declaration", *$1);
          addChild($$, makeNode("Identifier", *$2));
          delete $1;
          delete $2;
      }
    | type IDENTIFIER '=' expression ';'
      {
          $$ = makeNode("Declaration", *$1);
          addChild($$, makeNode("Identifier", *$2));
          addChild($$, $4);
          delete $1;
          delete $2;
      }
    ;

assignment
    : IDENTIFIER '=' expression ';'
      {
          $$ = makeNode("Assignment", *$1);
          addChild($$, $3);
          delete $1;
      }
    ;

print_statement
    : T_PRINT '(' expression ')' ';'
      {
          $$ = makeNode("Print");
          addChild($$, $3);
      }
    ;

/* A small, useful part of iostream syntax is supported as well. */
output_statement
    : T_COUT T_SHL expression T_SHL T_ENDL ';'
      {
          $$ = makeNode("Print");
          addChild($$, $3);
      }
    | T_COUT T_SHL expression ';'
      {
          $$ = makeNode("PrintNoNewline");
          addChild($$, $3);
      }
    ;

if_statement
    : T_IF '(' expression ')' statement %prec LOWER_THAN_ELSE
      {
          $$ = makeNode("If");
          addChild($$, $3);
          addChild($$, $5);
      }
    | T_IF '(' expression ')' statement T_ELSE statement
      {
          $$ = makeNode("If");
          addChild($$, $3);
          addChild($$, $5);
          addChild($$, $7);
      }
    ;

while_statement
    : T_WHILE '(' expression ')' statement
      {
          $$ = makeNode("While");
          addChild($$, $3);
          addChild($$, $5);
      }
    ;

return_statement
    : T_RETURN ';'
      {
          $$ = makeNode("Return");
      }
    | T_RETURN expression ';'
      {
          $$ = makeNode("Return");
          addChild($$, $2);
      }
    ;

expression
    : expression T_OR expression
      {
          $$ = makeNode("Binary", "||");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression T_AND expression
      {
          $$ = makeNode("Binary", "&&");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression T_EQ expression
      {
          $$ = makeNode("Binary", "==");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression T_NE expression
      {
          $$ = makeNode("Binary", "!=");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '<' expression
      {
          $$ = makeNode("Binary", "<");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression T_LE expression
      {
          $$ = makeNode("Binary", "<=");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '>' expression
      {
          $$ = makeNode("Binary", ">");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression T_GE expression
      {
          $$ = makeNode("Binary", ">=");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '+' expression
      {
          $$ = makeNode("Binary", "+");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '-' expression
      {
          $$ = makeNode("Binary", "-");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '*' expression
      {
          $$ = makeNode("Binary", "*");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '/' expression
      {
          $$ = makeNode("Binary", "/");
          addChild($$, $1);
          addChild($$, $3);
      }
    | expression '%' expression
      {
          $$ = makeNode("Binary", "%");
          addChild($$, $1);
          addChild($$, $3);
      }
    | unary { $$ = $1; }
    ;

unary
    : '!' unary %prec UNOT
      {
          $$ = makeNode("Unary", "!");
          addChild($$, $2);
      }
    | '-' unary %prec UMINUS
      {
          $$ = makeNode("Unary", "-");
          addChild($$, $2);
      }
    | '+' unary %prec UPLUS
      {
          $$ = makeNode("Unary", "+");
          addChild($$, $2);
      }
    | primary { $$ = $1; }
    ;

primary
    : NUMBER
      {
          $$ = makeNode("Number", *$1);
          delete $1;
      }
    | T_TRUE  { $$ = makeNode("Boolean", "true"); }
    | T_FALSE { $$ = makeNode("Boolean", "false"); }
    | IDENTIFIER
      {
          $$ = makeNode("Identifier", *$1);
          delete $1;
      }
    | T_INVALID { $$ = makeNode("InvalidToken"); }
    | '(' expression ')' { $$ = $2; }
    ;

%%

void yyerror(const char* message) {
    std::ostringstream output;
    output << "Syntax error at line " << yylloc.first_line
           << ", column " << yylloc.first_column << ": " << message;
    g_parse_error = output.str();
}
