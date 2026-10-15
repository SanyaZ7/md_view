#include "backend.h"

#include <tree_sitter/api.h>

#include <algorithm>
#include <cstdint>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

/*
 * Тексты highlights.scm вкомпилированы в библиотеку (см. Makefile,
 * правило для obj/queries_data.h), поэтому .scm-файлы не нужны
 * в рантайме.
 */
#include "queries_data.h"

/*
 * Грамматики линкуются в libhighlight.so статически.
 */
extern "C" const TSLanguage* tree_sitter_c(void);
extern "C" const TSLanguage* tree_sitter_cpp(void);
extern "C" const TSLanguage* tree_sitter_bash(void);
extern "C" const TSLanguage* tree_sitter_cmake(void);
extern "C" const TSLanguage* tree_sitter_css(void);
extern "C" const TSLanguage* tree_sitter_glsl(void);
extern "C" const TSLanguage* tree_sitter_html(void);
extern "C" const TSLanguage* tree_sitter_javascript(void);
extern "C" const TSLanguage* tree_sitter_json(void);
extern "C" const TSLanguage* tree_sitter_make(void);
extern "C" const TSLanguage* tree_sitter_rust(void);
extern "C" const TSLanguage* tree_sitter_zig(void);
extern "C" const TSLanguage* tree_sitter_xml(void);
extern "C" const TSLanguage* tree_sitter_dtd(void);

namespace highlight
{
namespace detail
{
namespace
{

/*
 * Запрос из вкомпилированных данных queries_data.h по имени.
 */
std::string embedded_query(
    const unsigned char* data,
    unsigned int length)
{
    return std::string(
        reinterpret_cast<const char*>(data),
        length);
}

std::string c_query_text()
{
    return embedded_query(highlight_c_query, highlight_c_query_len);
}

/*
 * В C++-запросе первая строка — "; inherits: c". Ядро tree-sitter
 * наследование не разворачивает, поэтому C-запрос просто
 * дописывается перед C++-запросом: набор паттернов объединяется,
 * а строка-комментарий игнорируется парсером запросов.
 */
std::string cpp_query_text()
{
    return c_query_text()
        + "\n"
        + embedded_query(highlight_cpp_query, highlight_cpp_query_len);
}

std::string bash_query_text()
{
    return embedded_query(highlight_bash_query, highlight_bash_query_len);
}

std::string cmake_query_text()
{
    return embedded_query(highlight_cmake_query, highlight_cmake_query_len);
}

std::string css_query_text()
{
    return embedded_query(highlight_css_query, highlight_css_query_len);
}

/*
 * В GLSL-запросе есть строка "; inherits: c", поэтому C-запрос
 * дописывается перед ним так же, как для C++.
 */
std::string glsl_query_text()
{
    return c_query_text()
        + "\n"
        + embedded_query(highlight_glsl_query, highlight_glsl_query_len);
}

std::string html_query_text()
{
    return embedded_query(highlight_html_query, highlight_html_query_len);
}

std::string javascript_query_text()
{
    return embedded_query(
        highlight_javascript_query,
        highlight_javascript_query_len);
}

std::string json_query_text()
{
    return embedded_query(highlight_json_query, highlight_json_query_len);
}

std::string make_query_text()
{
    return embedded_query(highlight_make_query, highlight_make_query_len);
}

std::string rust_query_text()
{
    return embedded_query(highlight_rust_query, highlight_rust_query_len);
}

std::string zig_query_text()
{
    return embedded_query(highlight_zig_query, highlight_zig_query_len);
}

std::string xml_query_text()
{
    return embedded_query(highlight_xml_query, highlight_xml_query_len);
}

std::string dtd_query_text()
{
    return embedded_query(highlight_dtd_query, highlight_dtd_query_len);
}

struct Capture
{
    std::uint32_t start = 0;
    std::uint32_t end = 0;
    std::string type;
    std::uint32_t pattern_index = 0;
};

/*
 * Служебные capture не участвуют в подсветке:
 *   - имя, начинающееся с '_', нужно только предикату (#eq?/#match?);
 *   - @spell/@nospell — проверка орфографии;
 *   - @none — явный отказ от подсветки;
 *   - @conceal — скрытие текста;
 *   - @clean — вспомогательный capture в старых запросах (make).
 */
bool is_highlight_capture(const std::string& name)
{
    if (name.empty() || name.front() == '_') {
        return false;
    }

    return name != "spell" &&
           name != "nospell" &&
           name != "none" &&
           name != "conceal" &&
           name != "clean";
}

/*
 * Преобразование Lua-паттерна (#lua-match?) в ECMAScript-regexp.
 * Поддерживаются классы символов %a %c %d %g %l %p %s %u %w %x и их
 * заглавные (отрицающие) варианты; прочие "%X" трактуются как
 * экранированные литералы. Остальные символы переносятся как есть:
 * якоря ^/$ и классы [...] в обоих синтаксисах совпадают.
 */
std::string lua_pattern_to_regex(const std::string& pattern)
{
    std::string result;
    result.reserve(pattern.size() * 2);

    bool in_character_class = false;

    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const char symbol = pattern[index];

        if (symbol == '%' && in_character_class &&
            index + 1 < pattern.size()) {
            const char escaped = pattern[++index];

            if (escaped == ']' || escaped == '-' || escaped == '^' ||
                escaped == '[' || escaped == '%') {
                result += '\\';
            }

            result += escaped;
            continue;
        }

        if (symbol == '[') {
            in_character_class = true;
            result += symbol;
            continue;
        }

        if (symbol == ']') {
            in_character_class = false;
            result += symbol;
            continue;
        }

        if (symbol != '%' || index + 1 >= pattern.size()) {
            result += symbol;
            continue;
        }

        const auto character_class = [in_character_class](
                                         const char* contents) {
            return in_character_class
                ? std::string(contents)
                : std::string("[") + contents + "]";
        };

        switch (pattern[++index]) {
            case 'a': result += character_class("A-Za-z"); break;
            case 'A': result += character_class("^A-Za-z"); break;
            case 'c': result += character_class("\\x00-\\x1f\\x7f"); break;
            case 'd': result += character_class("0-9"); break;
            case 'D': result += character_class("^0-9"); break;
            case 'g': result += character_class("\\x21-\\x7e"); break;
            case 'G': result += character_class("^\\x21-\\x7e"); break;
            case 'l': result += character_class("a-z"); break;
            case 'L': result += character_class("^a-z"); break;
            case 'p': result += character_class("!-/:-@\\[-`{-~"); break;
            case 'P': result += character_class("^!-/:-@\\[-`{-~"); break;
            case 's': result += character_class(" \\t\\r\\n\\f\\v"); break;
            case 'S': result += character_class("^ \\t\\r\\n\\f\\v"); break;
            case 'u': result += character_class("A-Z"); break;
            case 'U': result += character_class("^A-Z"); break;
            case 'w': result += character_class("A-Za-z0-9_"); break;
            case 'W': result += character_class("^A-Za-z0-9_"); break;
            case 'x': result += character_class("A-Fa-f0-9"); break;
            case 'X': result += character_class("^A-Fa-f0-9"); break;
            case 'z': result += "\\x00"; break;
            default:
                /* "%" перед символом — экранированный литерал. */
                result += pattern[index];
                break;
        }
    }

    return result;
}

/*
 * Разбор предикатов запроса в набор «capture_id -> список
 * предикатов». Поддерживаются #match?, #lua-match?, #eq? и
 * #any-of? вместе с их отрицательными формами (not-...).
 *
 * #is?/#is-not? (проверки locals) не поддерживаются: локальные
 * переменные не отслеживаются, поэтому такие предикаты просто не
 * ограничивают подсветку.
 *
 * Шаги предиката образуют группу, завершающуюся
 * TSQueryPredicateStepTypeDone: имя предиката (String), затем
 * аргументы — Capture и/или String.
 */
struct Predicate
{
    enum class Kind
    {
        Regex,
        Equals,
        AnyOf
    };

    Kind kind = Kind::Regex;
    bool negate = false;

    std::regex regex;
    std::string value;
    std::vector<std::string> values;
};

using CaptureFilters =
    std::unordered_map<std::uint32_t, std::vector<Predicate>>;

void collect_predicates(
    const TSQuery* query,
    std::uint32_t pattern_index,
    CaptureFilters& filters)
{
    std::uint32_t step_count = 0;

    const TSQueryPredicateStep* steps =
        ts_query_predicates_for_pattern(
            query,
            pattern_index,
            &step_count);

    if (!steps) {
        return;
    }

    const auto string_value =
        [query](std::uint32_t value_id) {
            std::uint32_t length = 0;

            const char* value = ts_query_string_value_for_id(
                query,
                value_id,
                &length);

            return value
                ? std::string(value, length)
                : std::string();
        };

    std::uint32_t index = 0;

    while (index < step_count) {
        /* Группа предиката начинается со строки — его имени. */
        if (steps[index].type != TSQueryPredicateStepTypeString) {
            while (index < step_count &&
                   steps[index].type != TSQueryPredicateStepTypeDone) {
                ++index;
            }

            ++index;
            continue;
        }

        std::string predicate = string_value(steps[index].value_id);

        ++index;

        std::uint32_t capture_id = 0;
        bool has_capture = false;
        std::vector<std::string> arguments;

        while (index < step_count &&
               steps[index].type != TSQueryPredicateStepTypeDone) {
            if (steps[index].type == TSQueryPredicateStepTypeCapture) {
                capture_id = steps[index].value_id;
                has_capture = true;
            } else {
                arguments.push_back(string_value(steps[index].value_id));
            }

            ++index;
        }

        ++index;

        if (predicate.empty() || predicate.front() == '#') {
            predicate.erase(0, 1);
        }

        Predicate parsed;

        if (predicate.rfind("not-", 0) == 0) {
            parsed.negate = true;
            predicate.erase(0, 4);
        }

        const bool is_lua_match = predicate == "lua-match?";
        const bool is_match = predicate == "match?" || is_lua_match;
        const bool is_eq = predicate == "eq?";
        const bool is_any_of = predicate == "any-of?";

        if (!has_capture ||
            arguments.empty() ||
            (!is_match && !is_eq && !is_any_of)) {
            continue;
        }

        if (is_match) {
            parsed.kind = Predicate::Kind::Regex;

            try {
                parsed.regex = std::regex(
                    is_lua_match
                        ? lua_pattern_to_regex(arguments.front())
                        : arguments.front());
            } catch (const std::regex_error&) {
                /* Некорректное выражение — предикат не применяем. */
                continue;
            }
        } else if (is_eq) {
            parsed.kind = Predicate::Kind::Equals;
            parsed.value = arguments.front();
        } else {
            parsed.kind = Predicate::Kind::AnyOf;
            parsed.values = arguments;
        }

        filters[capture_id].push_back(std::move(parsed));
    }
}

bool predicate_holds(const Predicate& predicate, const std::string& text)
{
    bool result = false;

    switch (predicate.kind) {
        case Predicate::Kind::Regex:
            result = std::regex_search(text, predicate.regex);
            break;

        case Predicate::Kind::Equals:
            result = text == predicate.value;
            break;

        case Predicate::Kind::AnyOf:
            result =
                std::find(
                    predicate.values.begin(),
                    predicate.values.end(),
                    text) != predicate.values.end();
            break;
    }

    return predicate.negate ? !result : result;
}

class TreeSitterHighlighter : public Highlighter
{
public:
    TreeSitterHighlighter(
        const TSLanguage* language,
        std::string query_source)
        : parser_(ts_parser_new())
        , cursor_(ts_query_cursor_new())
    {
        if (!parser_ || !cursor_) {
            return;
        }

        if (!ts_parser_set_language(parser_, language)) {
            return;
        }

        std::uint32_t error_offset = 0;
        TSQueryError error_type = TSQueryErrorNone;

        query_ = ts_query_new(
            language,
            query_source.c_str(),
            static_cast<std::uint32_t>(query_source.size()),
            &error_offset,
            &error_type);

        if (!query_) {
            return;
        }

        ready_ = true;
    }

    ~TreeSitterHighlighter() override
    {
        if (query_) {
            ts_query_delete(query_);
        }

        if (cursor_) {
            ts_query_cursor_delete(cursor_);
        }

        if (parser_) {
            ts_parser_delete(parser_);
        }
    }

    bool available() const override
    {
        return ready_;
    }

    std::vector<Token> tokenize(const std::string& code) override
    {
        std::vector<Token> result;

        if (!ready_) {
            return result;
        }

        TSTree* tree = ts_parser_parse_string(
            parser_,
            nullptr,
            code.c_str(),
            static_cast<std::uint32_t>(code.size()));

        if (!tree) {
            return result;
        }

        ts_query_cursor_exec(cursor_, query_, ts_tree_root_node(tree));

        std::vector<Capture> captures;

        TSQueryMatch match;

        while (ts_query_cursor_next_match(cursor_, &match)) {
            CaptureFilters filters;

            collect_predicates(query_, match.pattern_index, filters);

            for (std::uint32_t i = 0; i < match.capture_count; ++i) {
                const TSQueryCapture capture = match.captures[i];

                std::uint32_t name_length = 0;
                const char* name = ts_query_capture_name_for_id(
                    query_,
                    capture.index,
                    &name_length);

                if (!name) {
                    continue;
                }

                const std::string capture_name(name, name_length);

                if (!is_highlight_capture(capture_name)) {
                    continue;
                }

                const std::uint32_t start =
                    ts_node_start_byte(capture.node);

                const std::uint32_t end =
                    ts_node_end_byte(capture.node);

                if (end <= start || end > code.size()) {
                    continue;
                }

                const auto filter = filters.find(capture.index);

                if (filter != filters.end()) {
                    const std::string text =
                        code.substr(start, end - start);

                    const bool holds =
                        std::all_of(
                            filter->second.begin(),
                            filter->second.end(),
                            [&text](const Predicate& predicate) {
                                return predicate_holds(predicate, text);
                            });

                    if (!holds) {
                        continue;
                    }
                }

                captures.push_back({
                    start,
                    end,
                    capture_name,
                    match.pattern_index
                });
            }
        }

        ts_tree_delete(tree);

        if (captures.empty()) {
            return result;
        }

        /*
         * При перекрытии побеждает более длинный фрагмент, а при
         * равной длине — объявленный позже в запросе паттерн
         * (обычно более специфичный, например @function против
         * общего @variable из первого паттерна запроса).
         */
        std::sort(
            captures.begin(),
            captures.end(),
            [](const Capture& left, const Capture& right) {
                if (left.start != right.start) {
                    return left.start < right.start;
                }

                if (left.end != right.end) {
                    return left.end > right.end;
                }

                return left.pattern_index > right.pattern_index;
            });

        std::uint32_t consumed_until = 0;

        for (const Capture& capture : captures) {
            if (capture.start < consumed_until) {
                continue;
            }

            highlight::Token token;
            token.start = capture.start;
            token.length = capture.end - capture.start;
            token.type = capture.type;

            result.push_back(std::move(token));

            consumed_until = capture.end;
        }

        return result;
    }

private:
    TSParser* parser_ = nullptr;
    TSQuery* query_ = nullptr;
    TSQueryCursor* cursor_ = nullptr;
    bool ready_ = false;
};

} // namespace

std::unique_ptr<Highlighter> make_tree_sitter_highlighter(Language language)
{
    switch (language) {
        case Language::C:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_c(),
                c_query_text());

        case Language::Cpp:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_cpp(),
                cpp_query_text());

        case Language::Bash:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_bash(),
                bash_query_text());

        case Language::CMake:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_cmake(),
                cmake_query_text());

        case Language::Css:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_css(),
                css_query_text());

        case Language::Glsl:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_glsl(),
                glsl_query_text());

        case Language::Html:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_html(),
                html_query_text());

        case Language::JavaScript:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_javascript(),
                javascript_query_text());

        case Language::Json:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_json(),
                json_query_text());

        case Language::Make:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_make(),
                make_query_text());

        case Language::Rust:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_rust(),
                rust_query_text());

        case Language::Zig:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_zig(),
                zig_query_text());

        case Language::Xml:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_xml(),
                xml_query_text());

        case Language::Dtd:
            return std::make_unique<TreeSitterHighlighter>(
                tree_sitter_dtd(),
                dtd_query_text());

        case Language::None:
        default:
            return nullptr;
    }
}

bool tree_sitter_backend_available()
{
    return true;
}

} // namespace detail
} // namespace highlight
