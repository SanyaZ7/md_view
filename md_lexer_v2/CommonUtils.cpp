#include "CommonUtils.h"

std::string remove_common_indent(const std::string& text, std::size_t indent) {
    std::string result;
    result.reserve(text.length());

    std::size_t position = 0;

    while (position < text.length()) {
        std::size_t line_end = find_line_end(text, position);
        std::size_t remove_count = 0;

        while (remove_count < indent &&
               position + remove_count < line_end &&
               text[position + remove_count] == ' ') {
            ++remove_count;
        }

        result.append(
            text,
            position + remove_count,
            line_end - position - remove_count
        );

        if (line_end >= text.length()) {
            break;
        }

        result.push_back('\n');
        position = find_next_line_start(text, line_end);
    }

    return result;
}

bool is_blank_line(const std::string& line) {
    for (std::size_t i = 0; i < line.length(); ++i) {
        if (!is_space_without_newline(line[i])) {
            return false;
        }
    }
    return true;
}

bool is_space_without_newline(char value) {
    return value == ' ' || value == '\t';
}

bool is_line_break(char value) {
    return value == '\n' || value == '\r';
}

std::size_t find_line_end(const std::string& text, std::size_t start) {
    std::size_t newline = text.find('\n', start);

    if (newline == std::string::npos) {
        return text.length();
    }

    if (newline > start && text[newline - 1] == '\r') {
        return newline - 1;
    }

    return newline;
}

std::size_t find_next_line_start(const std::string& text, std::size_t line_end) {
    if (line_end >= text.length()) {
        return text.length();
    }

    if (text[line_end] == '\r' &&
        line_end + 1 < text.length() &&
        text[line_end + 1] == '\n') {
        return line_end + 2;
    }

    if (text[line_end] == '\n' || text[line_end] == '\r') {
        return line_end + 1;
    }

    return line_end;
}

std::size_t count_leading_spaces(const std::string& line) {
    std::size_t count = 0;
    while (count < line.length() && is_space_without_newline(line[count])) {
        ++count;
    }
    return count;
}

