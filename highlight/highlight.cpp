#include "highlight.h"

#include "backend.h"

#include <algorithm>
#include <cctype>

namespace highlight
{
namespace
{

class NoopHighlighter : public Highlighter
{
public:
    std::vector<Token> tokenize(const std::string& /*code*/) override
    {
        return {};
    }

    bool available() const override
    {
        return false;
    }
};

std::string normalized(const std::string& value)
{
    std::string result = value;

    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char symbol) {
            return static_cast<char>(std::tolower(symbol));
        });

    return result;
}

std::string first_word(const std::string& value)
{
    const std::string lowered = normalized(value);

    const std::size_t begin = lowered.find_first_not_of(" \t");

    if (begin == std::string::npos) {
        return {};
    }

    const std::size_t end = lowered.find_first_of(" \t", begin);

    return lowered.substr(begin, end - begin);
}

/*
 * Базовое имя пути: после последнего '/'.
 */
std::string base_name(const std::string& value)
{
    const std::size_t slash = value.find_last_of('/');

    if (slash == std::string::npos) {
        return value;
    }

    return value.substr(slash + 1);
}

} // namespace


Language language_from_extension(const std::string& extension)
{
    const std::string lowered = normalized(extension);

    if (lowered == "c") {
        return Language::C;
    }

    if (lowered == "h" ||
        lowered == "hpp" ||
        lowered == "hxx" ||
        lowered == "cpp" ||
        lowered == "cxx" ||
        lowered == "cc") {
        return Language::Cpp;
    }

    if (lowered == "sh" ||
        lowered == "bash" ||
        lowered == "zsh") {
        return Language::Bash;
    }

    if (lowered == "cmake") {
        return Language::CMake;
    }

    if (lowered == "css") {
        return Language::Css;
    }

    if (lowered == "glsl" ||
        lowered == "vert" ||
        lowered == "frag" ||
        lowered == "geom" ||
        lowered == "tesc" ||
        lowered == "tese" ||
        lowered == "comp") {
        return Language::Glsl;
    }

    if (lowered == "html" || lowered == "htm") {
        return Language::Html;
    }

    if (lowered == "js" ||
        lowered == "mjs" ||
        lowered == "cjs" ||
        lowered == "jsx") {
        return Language::JavaScript;
    }

    if (lowered == "json") {
        return Language::Json;
    }

    if (lowered == "mk" ||
        lowered == "mak" ||
        lowered == "make") {
        return Language::Make;
    }

    if (lowered == "rs") {
        return Language::Rust;
    }

    if (lowered == "zig") {
        return Language::Zig;
    }

    if (lowered == "xml" ||
        lowered == "svg" ||
        lowered == "xsd" ||
        lowered == "xsl" ||
        lowered == "xslt") {
        return Language::Xml;
    }

    if (lowered == "dtd") {
        return Language::Dtd;
    }

    return Language::None;
}

Language language_from_file_name(const std::string& file_name)
{
    const std::string name = normalized(base_name(file_name));

    /*
     * У make-файлов нет расширения, у CMakeLists.txt расширение
     * не показательно, поэтому имена проверяются до расширения.
     */
    if (name == "makefile" ||
        name == "gnumakefile" ||
        name == "bsdmakefile" ||
        name == "makefile.am" ||
        name == "makefile.in") {
        return Language::Make;
    }

    if (name == "cmakelists.txt" || name == "cmakecache.txt") {
        return Language::CMake;
    }

    const std::size_t dot = name.find_last_of('.');

    if (dot == std::string::npos) {
        return Language::None;
    }

    return language_from_extension(name.substr(dot + 1));
}

Language language_from_name(const std::string& name)
{
    const std::string word = first_word(name);

    if (word == "c") {
        return Language::C;
    }

    if (word == "cpp" ||
        word == "c++" ||
        word == "cxx" ||
        word == "hpp" ||
        word == "cc") {
        return Language::Cpp;
    }

    if (word == "sh" ||
        word == "bash" ||
        word == "shell" ||
        word == "zsh") {
        return Language::Bash;
    }

    if (word == "cmake") {
        return Language::CMake;
    }

    if (word == "css") {
        return Language::Css;
    }

    if (word == "glsl") {
        return Language::Glsl;
    }

    if (word == "html" || word == "htm") {
        return Language::Html;
    }

    if (word == "js" ||
        word == "javascript" ||
        word == "jsx" ||
        word == "mjs" ||
        word == "cjs") {
        return Language::JavaScript;
    }

    if (word == "json") {
        return Language::Json;
    }

    if (word == "make" ||
        word == "makefile" ||
        word == "gnumakefile" ||
        word == "mk") {
        return Language::Make;
    }

    if (word == "rust" || word == "rs") {
        return Language::Rust;
    }

    if (word == "zig") {
        return Language::Zig;
    }

    if (word == "xml" || word == "svg") {
        return Language::Xml;
    }

    if (word == "dtd") {
        return Language::Dtd;
    }

    return Language::None;
}

std::unique_ptr<Highlighter> Highlighter::create(Language language)
{
    if (language == Language::Html) {
        return detail::make_embedded_html_highlighter();
    }

    if (language != Language::None) {
        std::unique_ptr<Highlighter> highlighter =
            detail::make_tree_sitter_highlighter(language);

        if (highlighter) {
            return highlighter;
        }
    }

    return std::make_unique<NoopHighlighter>();
}

bool Highlighter::tree_sitter_available()
{
    return detail::tree_sitter_backend_available();
}

} // namespace highlight
