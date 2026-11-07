#include "ast.hpp"

#include <sstream>

AstNode::AstNode(std::string nodeKind, std::string nodeValue)
    : kind(std::move(nodeKind)), value(std::move(nodeValue)) {}

AstNode::~AstNode() {
    for (AstNode* child : children) {
        delete child;
    }
}

AstNode* makeNode(const std::string& kind, const std::string& value) {
    return new AstNode(kind, value);
}

void addChild(AstNode* parent, AstNode* child) {
    if (parent && child) {
        parent->children.push_back(child);
    }
}

std::string jsonEscape(const std::string& text) {
    std::ostringstream out;
    for (unsigned char ch : text) {
        switch (ch) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\b': out << "\\b"; break;
            case '\f': out << "\\f"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (ch < 0x20) {
                    const char* hex = "0123456789abcdef";
                    out << "\\u00" << hex[ch >> 4] << hex[ch & 0x0f];
                } else {
                    out << static_cast<char>(ch);
                }
        }
    }
    return out.str();
}

static void writeNodeJson(std::ostringstream& out, const AstNode* node, int level) {
    if (!node) {
        out << "null";
        return;
    }

    const std::string pad(static_cast<std::size_t>(level), ' ');
    const std::string childPad(static_cast<std::size_t>(level + 2), ' ');

    out << "{\n";
    out << childPad << "\"kind\": \"" << jsonEscape(node->kind) << "\",\n";
    out << childPad << "\"value\": \"" << jsonEscape(node->value) << "\",\n";
    out << childPad << "\"children\": [";

    if (!node->children.empty()) {
        out << '\n';
        for (std::size_t i = 0; i < node->children.size(); ++i) {
            out << std::string(static_cast<std::size_t>(level + 4), ' ');
            writeNodeJson(out, node->children[i], level + 4);
            if (i + 1 < node->children.size()) out << ',';
            out << '\n';
        }
        out << childPad;
    }

    out << "]\n" << pad << '}';
}

std::string astToJson(const AstNode* node, int indent) {
    std::ostringstream out;
    writeNodeJson(out, node, indent);
    return out.str();
}
