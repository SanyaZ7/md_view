#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include <string>
#include <cstddef>

// Общие вспомогательные функции для парсинга текста
std::string remove_common_indent(const std::string& text, std::size_t indent);
bool is_blank_line(const std::string& line);
bool is_space_without_newline(char value);
bool is_line_break(char value);
std::size_t find_line_end(const std::string& text, std::size_t start);
std::size_t find_next_line_start(const std::string& text, std::size_t line_end);
std::size_t count_leading_spaces(const std::string& line);

#endif // COMMON_UTILS_H

