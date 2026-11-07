#include "python_generator.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
struct Symbol {
    std::string pythonName;
    std::string type;
};

class Generator {
public:
    std::string run(const AstNode* program) {
        scopes_.clear();
        nextNameId_ = 0;

        std::ostringstream out;
        writeRuntimeHelpers(out);
        out << "\n\ndef main():\n";

        scopes_.emplace_back();
        const AstNode* body = nullptr;
        if (program && !program->children.empty() &&
            program->children[0] && program->children[0]->kind == "MainFunction") {
            body = child(program->children[0], 0);
        } else if (program && !program->children.empty()) {
            body = program->children[0];
        }

        const std::size_t before = out.tellp();
        emitBlockContents(out, body, 1);
        if (static_cast<std::size_t>(out.tellp()) == before) {
            indent(out, 1);
            out << "pass\n";
        }
        scopes_.pop_back();

        out << "\n\nif __name__ == \"__main__\":\n";
        out << "    raise SystemExit(main())\n";
        return out.str();
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
    int nextNameId_ = 0;

    static const AstNode* child(const AstNode* node, std::size_t index) {
        if (!node || index >= node->children.size()) return nullptr;
        return node->children[index];
    }

    static void indent(std::ostringstream& out, int level) {
        for (int i = 0; i < level; ++i) out << "    ";
    }

    static void writeRuntimeHelpers(std::ostringstream& out) {
        out << "def _cpp_div(left, right):\n"
            << "    if right == 0:\n"
            << "        raise ZeroDivisionError(\"division by zero\")\n"
            << "    if isinstance(left, (int, bool)) and isinstance(right, (int, bool)):\n"
            << "        quotient = abs(int(left)) // abs(int(right))\n"
            << "        return -quotient if (left < 0) != (right < 0) else quotient\n"
            << "    return left / right\n\n"
            << "def _cpp_mod(left, right):\n"
            << "    if right == 0:\n"
            << "        raise ZeroDivisionError(\"remainder by zero\")\n"
            << "    if not (isinstance(left, (int, bool)) and isinstance(right, (int, bool))):\n"
            << "        raise TypeError(\"operator '%' requires integer operands\")\n"
            << "    return int(left) - _cpp_div(left, right) * int(right)\n\n"
            << "def _cpp_print(value, end=\"\\n\"):\n"
            << "    if isinstance(value, bool):\n"
            << "        text = \"1\" if value else \"0\"\n"
            << "    elif isinstance(value, float):\n"
            << "        text = format(value, \".15g\")\n"
            << "    else:\n"
            << "        text = str(value)\n"
            << "    print(text, end=end)\n";
    }

    static bool needsSafeName(const std::string& name) {
        static const std::vector<std::string> reserved = {
            "False", "None", "True", "and", "as", "assert", "async", "await",
            "break", "class", "continue", "def", "del", "elif", "else", "except",
            "finally", "for", "from", "global", "if", "import", "in", "is",
            "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
            "while", "with", "yield", "_cpp_div", "_cpp_mod", "_cpp_print", "main"
        };
        for (const std::string& word : reserved) {
            if (name == word) return true;
        }
        return false;
    }

    static std::string pythonSafeName(const std::string& name, int uniqueId) {
        if (needsSafeName(name)) return "_mini_cpp_" + name + "_" + std::to_string(uniqueId);
        return name;
    }

    bool outputNameExists(const std::string& candidate) const {
        for (const auto& scope : scopes_) {
            for (const auto& entry : scope) {
                if (entry.second.pythonName == candidate) return true;
            }
        }
        return false;
    }

    Symbol* findSymbol(const std::string& name) {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end()) return &found->second;
        }
        return nullptr;
    }

    std::string expressionCode(const AstNode* node) {
        if (!node) return "0";

        if (node->kind == "Number") return node->value;
        if (node->kind == "Boolean") return node->value == "true" ? "True" : "False";
        if (node->kind == "Identifier") {
            const Symbol* symbol = findSymbol(node->value);
            return symbol ? symbol->pythonName : pythonSafeName(node->value, 0);
        }
        if (node->kind == "Unary") {
            const std::string operand = expressionCode(child(node, 0));
            if (node->value == "!") return "(not bool(" + operand + "))";
            if (node->value == "+" || node->value == "-") {
                return "(" + node->value + operand + ")";
            }
            throw std::runtime_error("unsupported unary operator '" + node->value + "'");
        }
        if (node->kind == "Binary") {
            const std::string left = expressionCode(child(node, 0));
            const std::string right = expressionCode(child(node, 1));
            if (node->value == "/") return "_cpp_div(" + left + ", " + right + ")";
            if (node->value == "%") return "_cpp_mod(" + left + ", " + right + ")";
            if (node->value == "&&") return "(bool(" + left + ") and bool(" + right + "))";
            if (node->value == "||") return "(bool(" + left + ") or bool(" + right + "))";
            return "(" + left + " " + node->value + " " + right + ")";
        }

        throw std::runtime_error("cannot generate Python expression from AST node '" + node->kind + "'");
    }

    static std::string convertExpression(const std::string& type, const std::string& expression) {
        if (type == "int") return "int(" + expression + ")";
        if (type == "double") return "float(" + expression + ")";
        if (type == "bool") return "bool(" + expression + ")";
        return expression;
    }

    void emitBlockContents(std::ostringstream& out, const AstNode* blockOrStatements, int level) {
        if (!blockOrStatements) return;
        const AstNode* statements = blockOrStatements;
        if (statements->kind == "Block") statements = child(statements, 0);
        if (!statements) return;

        for (const AstNode* statement : statements->children) {
            emitStatement(out, statement, level);
        }
    }

    void emitSuite(std::ostringstream& out, const AstNode* body, int level) {
        scopes_.emplace_back();
        const std::size_t before = out.tellp();

        if (body && body->kind == "Block") {
            emitBlockContents(out, body, level);
        } else {
            emitStatement(out, body, level);
        }

        if (static_cast<std::size_t>(out.tellp()) == before) {
            indent(out, level);
            out << "pass\n";
        }
        scopes_.pop_back();
    }

    void emitStatement(std::ostringstream& out, const AstNode* node, int level) {
        if (!node) return;

        if (node->kind == "Statements") {
            emitBlockContents(out, node, level);
            return;
        }

        if (node->kind == "Block") {
            emitSuite(out, node, level);
            return;
        }

        if (node->kind == "Declaration") {
            const AstNode* identifier = child(node, 0);
            if (!identifier) throw std::runtime_error("declaration has no identifier");
            if (scopes_.empty()) scopes_.emplace_back();
            if (scopes_.back().find(identifier->value) != scopes_.back().end()) {
                throw std::runtime_error("variable '" + identifier->value + "' is already declared in this scope");
            }

            const std::string initialValue = node->children.size() > 1
                ? convertExpression(node->value, expressionCode(child(node, 1)))
                : std::string();

            const int uniqueId = nextNameId_++;
            std::string outputName = pythonSafeName(identifier->value, uniqueId);
            if (scopes_.size() > 1 || findSymbol(identifier->value)) {
                outputName += "__scope" + std::to_string(uniqueId);
            }
            while (outputNameExists(outputName)) {
                outputName += "_";
            }
            scopes_.back().emplace(identifier->value, Symbol{outputName, node->value});

            if (node->children.size() > 1) {
                indent(out, level);
                out << outputName << " = " << initialValue << '\n';
            }
            return;
        }

        if (node->kind == "Assignment") {
            Symbol* symbol = findSymbol(node->value);
            const std::string outputName = symbol ? symbol->pythonName : pythonSafeName(node->value, 0);
            const std::string value = expressionCode(child(node, 0));
            indent(out, level);
            out << outputName << " = "
                << (symbol ? convertExpression(symbol->type, value) : value) << '\n';
            return;
        }

        if (node->kind == "Print" || node->kind == "PrintNoNewline") {
            indent(out, level);
            out << "_cpp_print(" << expressionCode(child(node, 0));
            if (node->kind == "PrintNoNewline") out << ", end=\"\"";
            out << ")\n";
            return;
        }

        if (node->kind == "If") {
            indent(out, level);
            out << "if " << expressionCode(child(node, 0)) << ":\n";
            emitSuite(out, child(node, 1), level + 1);
            if (node->children.size() > 2) {
                indent(out, level);
                out << "else:\n";
                emitSuite(out, child(node, 2), level + 1);
            }
            return;
        }

        if (node->kind == "While") {
            indent(out, level);
            out << "while " << expressionCode(child(node, 0)) << ":\n";
            emitSuite(out, child(node, 1), level + 1);
            return;
        }

        if (node->kind == "Return") {
            indent(out, level);
            if (node->children.empty()) out << "return 0\n";
            else out << "return " << expressionCode(child(node, 0)) << '\n';
            return;
        }

        if (node->kind == "EmptyStatement") return;

        throw std::runtime_error("cannot generate Python statement from AST node '" + node->kind + "'");
    }
};
} // namespace

std::string PythonGenerator::generate(const AstNode* program) {
    return Generator().run(program);
}
