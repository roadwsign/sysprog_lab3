#pragma once

#include "ast.hpp"

#include <string>

class PythonGenerator {
public:
    std::string generate(const AstNode* program);
};
