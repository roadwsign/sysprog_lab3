%option noyywrap nounput noinput nodefault
%x COMMENT

%{
#include "parser.tab.hpp"

#include <string>

extern std::string g_lex_error;

static int lexerLine = 1;
static int lexerColumn = 1;

#define YY_USER_ACTION                                                        \
    do {                                                                      \
        yylloc.first_line = lexerLine;                                        \
        yylloc.first_column = lexerColumn;                                    \
        for (int i = 0; i < yyleng; ++i) {                                    \
            if (yytext[i] == '\n') {                                        \
                ++lexerLine;                                                  \
                lexerColumn = 1;                                              \
            } else {                                                          \
                ++lexerColumn;                                                \
            }                                                                 \
        }                                                                     \
        yylloc.last_line = lexerLine;                                         \
        yylloc.last_column = lexerColumn > 1 ? lexerColumn - 1 : 1;           \
    } while (false);

static void setLexicalError(const char* text) {
    g_lex_error = "Lexical error at line " + std::to_string(yylloc.first_line) +
                  ", column " + std::to_string(yylloc.first_column) +
                  ": unsupported token '" + std::string(text, static_cast<std::size_t>(yyleng)) + "'";
}
%}

DIGIT       [0-9]
IDENT       [a-zA-Z_][a-zA-Z0-9_]*
NUMBER      ({DIGIT}+([.]{DIGIT}+)?|[.]{DIGIT}+)

%%

^[ \t]*#[ \t]*include[ \t]*<iostream>[ \t]*  { /* This include is accepted and does not affect the AST. */ }
[ \t\r]+                                      { /* horizontal whitespace */ }
\n                                               { /* newline */ }
"//"[^\n]*                                  { /* line comment */ }
"/*"                                         { BEGIN(COMMENT); }
<COMMENT>"*/"                                { BEGIN(INITIAL); }
<COMMENT>\n                                  { /* keep line/column tracking */ }
<COMMENT>.                                   { /* comment contents */ }
<COMMENT><<EOF>>                             {
                                                  g_lex_error = "Lexical error at line " +
                                                      std::to_string(lexerLine) +
                                                      ": unterminated block comment";
                                                  BEGIN(INITIAL);
                                                  return 0;
                                               }

"std::cout"                                  { return T_COUT; }
"std::endl"                                  { return T_ENDL; }

"int"                                        { return T_INT; }
"double"                                     { return T_DOUBLE; }
"bool"                                       { return T_BOOL; }
"true"                                       { return T_TRUE; }
"false"                                      { return T_FALSE; }
"if"                                         { return T_IF; }
"else"                                       { return T_ELSE; }
"while"                                      { return T_WHILE; }
"print"                                      { return T_PRINT; }
"return"                                     { return T_RETURN; }
"main"                                       { return T_MAIN; }

"<="                                         { return T_LE; }
">="                                         { return T_GE; }
"=="                                         { return T_EQ; }
"!="                                         { return T_NE; }
"&&"                                         { return T_AND; }
"||"                                         { return T_OR; }
"<<"                                         { return T_SHL; }

{NUMBER}                                     {
                                                  yylval.text = new std::string(yytext, static_cast<std::size_t>(yyleng));
                                                  return NUMBER;
                                               }
{IDENT}                                      {
                                                  yylval.text = new std::string(yytext, static_cast<std::size_t>(yyleng));
                                                  return IDENTIFIER;
                                               }

[(){};,+*/%=<>!-]                            { return static_cast<unsigned char>(yytext[0]); }

.                                            {
                                                  setLexicalError(yytext);
                                                  return T_INVALID;
                                               }

<<EOF>>                                      {
                                                  yylloc.first_line = lexerLine;
                                                  yylloc.last_line = lexerLine;
                                                  yylloc.first_column = lexerColumn;
                                                  yylloc.last_column = lexerColumn;
                                                  return 0;
                                               }

%%
