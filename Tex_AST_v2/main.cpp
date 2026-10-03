#include <iostream>
#include <fstream>
#include <sstream>
#include "math_lexer.h"
#include "math_parser.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <file.tex>\n";
        return 1;
    }

    std::ifstream file(argv[1]);
    if (!file) {
        std::cerr << "Не удалось открыть файл: " << argv[1] << '\n';
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    // Первый проход: лексический анализ
    math_ast::Lexer lexer(source);
    std::vector<math_ast::Token> tokens = lexer.run();

    if (!lexer.get_errors().empty()) {
        for (const std::string& error : lexer.get_errors()) {
            std::cerr << error << '\n';
        }
        return 1;
    }

    // Второй проход: синтаксический анализ и построение AST
    math_ast::Parser parser(tokens);
    std::vector<math_ast::AstNodePtr> formulas = parser.parse_document();

    if (!parser.get_errors().empty()) {
        for (const std::string& error : parser.get_errors()) {
            std::cerr << error << '\n';
        }
        return 1;
    }

    std::cout << "Формулы корректны\n";
    for (size_t i = 0; i < formulas.size(); ++i) {
        std::cout << "AST формулы " << i + 1 << ": ";
        formulas[i]->print(std::cout);
        std::cout << '\n';
    }

    return 0;
}

