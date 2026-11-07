#include "interpreter.hpp"

#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <variant>
#include <vector>

namespace {
using Value = std::variant<long long, double, bool>;

struct Variable {
    std::string type;
    std::optional<Value> value;
};

class Runtime {
public:
    std::string run(const AstNode* program) {
        scopes_.emplace_back();
        execute(program);
        return output_.str();
    }

private:
    std::vector<std::unordered_map<std::string, Variable>> scopes_;
    std::ostringstream output_;
    bool returned_ = false;

    static const AstNode* child(const AstNode* node, std::size_t index) {
        if (!node || index >= node->children.size()) return nullptr;
        return node->children[index];
    }

    static bool isInteger(const Value& value) {
        return std::holds_alternative<long long>(value);
    }

    static double asNumber(const Value& value) {
        if (const auto* integer = std::get_if<long long>(&value)) return static_cast<double>(*integer);
        if (const auto* real = std::get_if<double>(&value)) return *real;
        return std::get<bool>(value) ? 1.0 : 0.0;
    }

    static bool asBool(const Value& value) {
        if (const auto* boolean = std::get_if<bool>(&value)) return *boolean;
        return asNumber(value) != 0.0;
    }

    static std::string format(const Value& value) {
        if (const auto* integer = std::get_if<long long>(&value)) return std::to_string(*integer);
        if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "1" : "0";
        std::ostringstream out;
        out << std::setprecision(15) << std::get<double>(value);
        return out.str();
    }

    static Value convertTo(const std::string& type, const Value& value) {
        if (type == "int") return static_cast<long long>(asNumber(value));
        if (type == "double") return asNumber(value);
        if (type == "bool") return asBool(value);
        throw std::runtime_error("unknown data type '" + type + "'");
    }

    Variable& findVariable(const std::string& name) {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(name);
            if (found != scope->end()) return found->second;
        }
        throw std::runtime_error("variable '" + name + "' was not declared");
    }

    Value getVariable(const std::string& name) {
        Variable& variable = findVariable(name);
        if (!variable.value.has_value()) {
            throw std::runtime_error("variable '" + name + "' was used before initialization");
        }
        return *variable.value;
    }

    Value evaluate(const AstNode* node) {
        if (!node) throw std::runtime_error("invalid expression in AST");

        if (node->kind == "Number") {
            try {
                if (node->value.find('.') == std::string::npos) {
                    return static_cast<long long>(std::stoll(node->value));
                }
                return std::stod(node->value);
            } catch (...) {
                throw std::runtime_error("invalid numeric literal '" + node->value + "'");
            }
        }

        if (node->kind == "Boolean") return node->value == "true";
        if (node->kind == "Identifier") return getVariable(node->value);

        if (node->kind == "Unary") {
            const Value operand = evaluate(child(node, 0));
            if (node->value == "!") return !asBool(operand);
            if (node->value == "+") return operand;
            if (node->value == "-") {
                if (isInteger(operand)) return -std::get<long long>(operand);
                return -asNumber(operand);
            }
            throw std::runtime_error("unsupported unary operator '" + node->value + "'");
        }

        if (node->kind == "Binary") {
            const Value left = evaluate(child(node, 0));
            const std::string& op = node->value;

            if (op == "&&" && !asBool(left)) return false;
            if (op == "||" && asBool(left)) return true;

            const Value right = evaluate(child(node, 1));
            const bool bothIntegers = isInteger(left) && isInteger(right);
            const double a = asNumber(left);
            const double b = asNumber(right);

            if (op == "+") return bothIntegers ? Value(std::get<long long>(left) + std::get<long long>(right)) : Value(a + b);
            if (op == "-") return bothIntegers ? Value(std::get<long long>(left) - std::get<long long>(right)) : Value(a - b);
            if (op == "*") return bothIntegers ? Value(std::get<long long>(left) * std::get<long long>(right)) : Value(a * b);
            if (op == "/") {
                if (b == 0.0) throw std::runtime_error("division by zero");
                if (bothIntegers) return std::get<long long>(left) / std::get<long long>(right);
                return a / b;
            }
            if (op == "%") {
                if (!bothIntegers) throw std::runtime_error("operator '%' requires integer operands");
                if (std::get<long long>(right) == 0) throw std::runtime_error("remainder by zero");
                return std::get<long long>(left) % std::get<long long>(right);
            }
            if (op == "==") return a == b;
            if (op == "!=") return a != b;
            if (op == "<") return a < b;
            if (op == "<=") return a <= b;
            if (op == ">") return a > b;
            if (op == ">=") return a >= b;
            if (op == "&&") return asBool(left) && asBool(right);
            if (op == "||") return asBool(left) || asBool(right);

            throw std::runtime_error("unsupported binary operator '" + op + "'");
        }

        throw std::runtime_error("node '" + node->kind + "' cannot be used as an expression");
    }

    void execute(const AstNode* node) {
        if (!node || returned_) return;

        if (node->kind == "Program" || node->kind == "Statements") {
            for (const AstNode* statement : node->children) {
                execute(statement);
                if (returned_) break;
            }
            return;
        }

        if (node->kind == "MainFunction") {
            execute(child(node, 0));
            return;
        }

        if (node->kind == "Block") {
            scopes_.emplace_back();
            try {
                execute(child(node, 0));
            } catch (...) {
                scopes_.pop_back();
                throw;
            }
            scopes_.pop_back();
            return;
        }

        if (node->kind == "EmptyStatement") return;

        if (node->kind == "Declaration") {
            const AstNode* identifier = child(node, 0);
            if (!identifier) throw std::runtime_error("invalid declaration node");
            auto& currentScope = scopes_.back();
            if (currentScope.find(identifier->value) != currentScope.end()) {
                throw std::runtime_error("variable '" + identifier->value + "' is already declared in this scope");
            }

            Variable variable;
            variable.type = node->value;
            if (node->children.size() > 1) {
                variable.value = convertTo(variable.type, evaluate(child(node, 1)));
            }
            currentScope.emplace(identifier->value, std::move(variable));
            return;
        }

        if (node->kind == "Assignment") {
            Variable& variable = findVariable(node->value);
            variable.value = convertTo(variable.type, evaluate(child(node, 0)));
            return;
        }

        if (node->kind == "Print" || node->kind == "PrintNoNewline") {
            output_ << format(evaluate(child(node, 0)));
            if (node->kind == "Print") output_ << '\n';
            return;
        }

        if (node->kind == "If") {
            if (asBool(evaluate(child(node, 0)))) {
                execute(child(node, 1));
            } else if (node->children.size() > 2) {
                execute(child(node, 2));
            }
            return;
        }

        if (node->kind == "While") {
            std::size_t iterations = 0;
            while (asBool(evaluate(child(node, 0)))) {
                if (++iterations > 100000) {
                    throw std::runtime_error("loop stopped: more than 100000 iterations");
                }
                execute(child(node, 1));
                if (returned_) break;
            }
            return;
        }

        if (node->kind == "Return") {
            if (!node->children.empty()) (void)evaluate(child(node, 0));
            returned_ = true;
            return;
        }

        throw std::runtime_error("unsupported statement node '" + node->kind + "'");
    }

};
} // namespace

std::string Interpreter::run(const AstNode* program) {
    Runtime runtime;
    return runtime.run(program);
}
