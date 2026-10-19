#include "EmbeddedHtmlHighlighter.h"

#include "backend.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace highlight
{
namespace detail
{
namespace
{

struct Region
{
    std::size_t start = 0;
    std::size_t end = 0;
    Language language = Language::None;
};

bool is_name_char(char value)
{
    return std::isalnum(static_cast<unsigned char>(value)) ||
           value == '_' || value == '-' || value == ':';
}

std::string lower_ascii(std::string value)
{
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char symbol) {
            return static_cast<char>(std::tolower(symbol));
        });
    return value;
}

std::vector<Region> embedded_regions(const std::string& code)
{
    std::vector<Region> regions;
    std::size_t position = 0;

    while (position < code.size()) {
        const std::size_t open = code.find('<', position);
        if (open == std::string::npos) {
            break;
        }

        std::size_t name_begin = open + 1;
        if (name_begin < code.size() && code[name_begin] == '/') {
            ++name_begin;
        }

        const std::size_t name_end = [&]() {
            std::size_t end = name_begin;
            while (end < code.size() && is_name_char(code[end])) {
                ++end;
            }
            return end;
        }();

        if (name_end == name_begin) {
            position = open + 1;
            continue;
        }

        const std::string tag = lower_ascii(
            code.substr(name_begin, name_end - name_begin));

        Language language = Language::None;
        if (tag == "style") {
            language = Language::Css;
        } else if (tag == "script") {
            language = Language::JavaScript;
        } else {
            position = name_end;
            continue;
        }

        const std::size_t tag_end = code.find('>', name_end);
        if (tag_end == std::string::npos) {
            break;
        }

        const std::string closing = "</" + tag;
        const std::size_t content_start = tag_end + 1;
        std::size_t close = code.find(closing, content_start);

        while (close != std::string::npos) {
            const std::size_t after_name = close + closing.size();
            if (after_name >= code.size() ||
                !is_name_char(code[after_name])) {
                break;
            }
            close = code.find(closing, after_name);
        }

        if (close == std::string::npos) {
            break;
        }

        regions.push_back({content_start, close, language});
        position = close + closing.size();
    }

    return regions;
}

class EmbeddedHtmlHighlighter final : public Highlighter
{
public:
    EmbeddedHtmlHighlighter()
        : html_(make_tree_sitter_highlighter(Language::Html))
        , css_(make_tree_sitter_highlighter(Language::Css))
        , javascript_(make_tree_sitter_highlighter(Language::JavaScript))
    {
    }

    std::vector<Token> tokenize(const std::string& code) override
    {
        if (!html_) {
            return {};
        }

        const std::vector<Region> regions = embedded_regions(code);
        std::vector<Token> result;

        /*
         * HTML-highlights may contain one large token for the text node
         * inside <style> or <script>. Such a token must not hide the
         * tokens produced by the nested language highlighter.
         */
        for (const Token& token : html_->tokenize(code)) {
            const std::size_t token_end = token.start + token.length;
            const bool overlaps_embedded = std::any_of(
                regions.begin(), regions.end(),
                [&token, token_end](const Region& region) {
                    return token.start < region.end &&
                           token_end > region.start;
                });

            if (!overlaps_embedded) {
                result.push_back(token);
            }
        }

        for (const Region& region : regions) {
            Highlighter* nested = region.language == Language::Css
                ? css_.get()
                : javascript_.get();

            if (!nested) {
                continue;
            }

            const std::string content = code.substr(
                region.start, region.end - region.start);

            for (Token token : nested->tokenize(content)) {
                token.start += region.start;
                result.push_back(std::move(token));
            }
        }

        std::sort(
            result.begin(), result.end(),
            [](const Token& left, const Token& right) {
                if (left.start != right.start) {
                    return left.start < right.start;
                }
                return left.length > right.length;
            });

        std::vector<Token> non_overlapping;
        std::size_t consumed_until = 0;
        for (const Token& token : result) {
            if (token.start < consumed_until) {
                continue;
            }
            non_overlapping.push_back(token);
            consumed_until = token.start + token.length;
        }

        return non_overlapping;
    }

    bool available() const override
    {
        return html_ && html_->available();
    }

private:
    std::unique_ptr<Highlighter> html_;
    std::unique_ptr<Highlighter> css_;
    std::unique_ptr<Highlighter> javascript_;
};

} // namespace

std::unique_ptr<Highlighter> make_embedded_html_highlighter()
{
    return std::make_unique<EmbeddedHtmlHighlighter>();
}

} // namespace detail
} // namespace highlight
