#pragma once

#include "ast.hpp"

#include <string>

extern AstNode* g_root;
extern std::string g_parse_error;
extern std::string g_lex_error;

struct yy_buffer_state;
using YY_BUFFER_STATE = yy_buffer_state*;

int yyparse(void);
YY_BUFFER_STATE yy_scan_bytes(const char* bytes, int length);
void yy_delete_buffer(YY_BUFFER_STATE buffer);
