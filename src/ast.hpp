#pragma once

#include <string>
#include <vector>
#include <ostream>

struct AstNode {
    std::string kind;
    std::string value;
    std::vector<AstNode*> children;

    AstNode(std::string nodeKind, std::string nodeValue = {});
    ~AstNode();

    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

AstNode* makeNode(const std::string& kind, const std::string& value = {});
void addChild(AstNode* parent, AstNode* child);
std::string astToJson(const AstNode* node, int indent = 0);
std::string jsonEscape(const std::string& text);
