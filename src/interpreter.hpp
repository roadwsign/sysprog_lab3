#pragma once

#include "ast.hpp"
#include <string>

class Interpreter {
public:
    std::string run(const AstNode* program);
};
