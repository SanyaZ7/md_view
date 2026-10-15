#include "highlight.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace
{

const char* language_name(highlight::Language language)
{
    switch (language) {
        case highlight::Language::C:
            return "C";

        case highlight::Language::Cpp:
            return "C++";

        case highlight::Language::Bash:
            return "Bash";

        case highlight::Language::CMake:
            return "CMake";

        case highlight::Language::Css:
            return "CSS";

        case highlight::Language::Glsl:
            return "GLSL";

        case highlight::Language::Html:
            return "HTML";

        case highlight::Language::JavaScript:
            return "JavaScript";

        case highlight::Language::Json:
            return "JSON";

        case highlight::Language::Make:
            return "Make";

        case highlight::Language::Rust:
            return "Rust";

        case highlight::Language::Zig:
            return "Zig";

        case highlight::Language::Xml:
            return "XML";

        case highlight::Language::Dtd:
            return "DTD";

        case highlight::Language::None:
        default:
            return "нет";
    }
}

void print_tokens(const std::string& path)
{
    std::ifstream file(path, std::ios::binary);

    if (!file) {
        std::cerr << "Не удалось открыть файл: " << path << "\n";
        return;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string code = buffer.str();

    const highlight::Language language =
        highlight::language_from_file_name(path);

    std::cout << "Файл: " << path
              << " | язык: " << language_name(language)
              << " | tree-sitter: "
              << (highlight::Highlighter::tree_sitter_available()
                      ? "да"
                      : "нет")
              << "\n";


    std::unique_ptr<highlight::Highlighter> highlighter =
        highlight::Highlighter::create(language);

    const std::vector<highlight::Token> tokens =
        highlighter->tokenize(code);

    std::cout << "Токенов: " << tokens.size() << "\n";

    for (const highlight::Token& token : tokens) {
        std::cout << "  " << token.start
                  << ".." << (token.start + token.length)
                  << "  " << token.type
                  << "  «" << code.substr(token.start, token.length)
                  << "»\n";
    }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "Использование: highlight <файл>\n";
        return 0;
    }

    for (int index = 1; index < argc; ++index) {
        print_tokens(argv[index]);
    }

    return 0;
}
