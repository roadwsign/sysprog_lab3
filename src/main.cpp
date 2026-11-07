#include "ast.hpp"
#include "python_generator.hpp"
#include "interpreter.hpp"
#include "parser_api.hpp"

#include <iostream>
#include <iterator>
#include <limits>
#include <string>

static void writeFailure(const std::string& message, const AstNode* ast = nullptr,
                         const std::string& generatedCode = "") {
    std::cout << "{\"ok\":false,\"error\":\"" << jsonEscape(message) << "\"";
    if (ast) {
        std::cout << ",\"ast\":" << astToJson(ast);
        std::cout << ",\"generatedCode\":\"" << jsonEscape(generatedCode) << "\"";
    }
    std::cout << '}';
}

int main() {
    const std::string source((std::istreambuf_iterator<char>(std::cin)),
                             std::istreambuf_iterator<char>());

    if (source.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        writeFailure("Input is too large for the lexer.");
        return 0;
    }

    g_root = nullptr;
    g_parse_error.clear();
    g_lex_error.clear();

    YY_BUFFER_STATE inputBuffer = yy_scan_bytes(source.data(), static_cast<int>(source.size()));
    const int parseResult = yyparse();
    yy_delete_buffer(inputBuffer);

    if (!g_lex_error.empty()) {
        writeFailure(g_lex_error);
        delete g_root;
        g_root = nullptr;
        return 0;
    }

    if (parseResult != 0 || !g_root) {
        writeFailure(g_parse_error.empty() ? "Syntax error: could not parse the input." : g_parse_error);
        delete g_root;
        g_root = nullptr;
        return 0;
    }

    try {
        PythonGenerator generator;
        const std::string generatedCode = generator.generate(g_root);

        Interpreter interpreter;
        const std::string output = interpreter.run(g_root);

        std::cout << "{\"ok\":true,\"output\":\"" << jsonEscape(output)
                  << "\",\"generatedCode\":\"" << jsonEscape(generatedCode)
                  << "\",\"ast\":" << astToJson(g_root) << '}';
        delete g_root;
        g_root = nullptr;
        return 0;
    } catch (const std::exception& error) {
        std::string generatedCode;
        try {
            generatedCode = PythonGenerator().generate(g_root);
        } catch (...) {
        }
        writeFailure(std::string("Runtime error: ") + error.what(), g_root, generatedCode);
        delete g_root;
        g_root = nullptr;
        return 0;
    } catch (...) {
        writeFailure("Unknown internal error", g_root);
        delete g_root;
        g_root = nullptr;
        return 0;
    }
}
