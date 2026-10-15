#ifndef HIGHLIGHT_H
#define HIGHLIGHT_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

/*
 * Подсветка кода для языков, для которых в проект собраны
 * грамматики tree-sitter (C, C++, Bash, ...).
 *
 * Библиотека не зависит от Qt и не требует tree-sitter на этапе
 * сборки: при отсутствии tree-sitter собирается та же самая
 * libhighlight.so, но с заглушкой (available() == false,
 * tokenize() возвращает пустой список).
 */
namespace highlight
{

enum class Language
{
    None,
    C,
    Cpp,
    Bash,
    CMake,
    Css,
    Glsl,
    Html,
    JavaScript,
    Json,
    Make,
    Rust,
    Zig,
    Xml,
    Dtd
};

/*
 * Язык по расширению файла (без точки):
 *   "c"                                  -> C
 *   "h", "hpp", "cpp", "cxx", "cc", ...  -> Cpp
 *   "sh", "bash", "zsh"                  -> Bash
 *   "cmake"                              -> CMake
 *   "css"                                -> Css
 *   "glsl", "vert", "frag", ...          -> Glsl
 *   "html", "htm"                        -> Html
 *   "js", "mjs", "cjs", "jsx"            -> JavaScript
 *   "json"                               -> Json
 *   "mk", "mak", "make"                  -> Make
 *   "rs"                                 -> Rust
 *   "zig"                                -> Zig
 *   "xml", "svg", ...                    -> Xml
 *   "dtd"                                -> Dtd
 *   всё остальное                        -> None
 */
Language language_from_extension(const std::string& extension);

/*
 * Язык по имени файла (путь или базовое имя, регистр не важен).
 *
 * Нужен для файлов, у которых расширение не определяет язык:
 *   "Makefile", "GNUmakefile", "makefile.am"  -> Make
 *   "CMakeLists.txt"                          -> CMake
 * Для остальных имён поведение совпадает с language_from_extension().
 */
Language language_from_file_name(const std::string& file_name);

/*
 * Язык по info string блока кода в Markdown (```bash, ```cpp, ...).
 * Берётся первое слово, регистр не важен.
 */
Language language_from_name(const std::string& name);

struct Token
{
    std::size_t start = 0;
    std::size_t length = 0;

    /*
     * Семантический тип токена: имя capture из highlights.scm
     * ("keyword", "string", "number", "comment", "function",
     * "type", "variable", "constant", "operator", ...).
     * Цвет по типу задаёт ColorScheme.
     */
    std::string type;
};

class Highlighter
{
public:
    virtual ~Highlighter() = default;

    /*
     * Разбивает код на непересекающиеся подсвеченные фрагменты.
     * Промежутки без подсветки в результат не попадают.
     */
    virtual std::vector<Token> tokenize(const std::string& code) = 0;

    /*
     * true, если работает реальная реализация (tree-sitter),
     * false для заглушки.
     */
    virtual bool available() const = 0;

    /*
     * Всегда возвращает валидный объект. Если для языка нет
     * подсветки или tree-sitter недоступен — возвращает заглушку.
     */
    static std::unique_ptr<Highlighter> create(Language language);

    /* Доступен ли tree-sitter в данной сборке библиотеки. */
    static bool tree_sitter_available();
};

} // namespace highlight

#endif // HIGHLIGHT_H
