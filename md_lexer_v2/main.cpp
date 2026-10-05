#include "MarkdownParser.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
// Вывод дерева
// ============================================================

static std::string node_type_name(
    NodeType type
) {
    switch (type) {
        case NodeType::Text:
            return "TEXT";

        case NodeType::Heading:
            return "HEADING";

        case NodeType::Bold:
            return "BOLD";

        case NodeType::Italic:
            return "ITALIC";

        case NodeType::Strikethrough:
            return "STRIKE";

        case NodeType::InlineCode:
            return "CODE";

        case NodeType::CodeBlock:
            return "CODE_BLOCK";

        case NodeType::Link:
            return "LINK";

        case NodeType::Image:
            return "IMAGE";

        case NodeType::List:
            return "LIST";

        case NodeType::ListItem:
            return "LIST_ITEM";

        case NodeType::Quote:
            return "QUOTE";

        case NodeType::HorizontalRule:
            return "HR";

        case NodeType::Table:
            return "TABLE";

        case NodeType::TableRow:
            return "ROW";

        case NodeType::TableCell:
            return "CELL";
    }

    return "UNKNOWN";
}

static void print_node(
    const MarkdownNode& node,
    int indent
) {
    std::string prefix(
        static_cast<std::size_t>(indent) * 2,
        ' '
    );

    std::cout << prefix
              << "["
              << node_type_name(node.type);

    if (node.level > 0) {
        std::cout << " level=" << node.level;
    }

    if (node.type == NodeType::List &&
        node.ordered) {
        std::cout << " ordered";
    }

    if (node.type == NodeType::TableRow &&
        node.is_header) {
        std::cout << " header";
    }

    if (!node.content.empty()) {
        std::cout << " \""
                  << node.content
                  << "\"";
    }

    if (!node.url.empty()) {
        std::cout << " url="
                  << node.url;
    }

    if (node.type == NodeType::TableCell) {
        if (node.alignment == Alignment::Center) {
            std::cout << " align=center";
        } else if (node.alignment == Alignment::Right) {
            std::cout << " align=right";
        } else {
            std::cout << " align=left";
        }
    }

    std::cout << "]";

    if (node.children.empty()) {
        std::cout << '\n';
        return;
    }

    std::cout << '\n';

    for (std::size_t i = 0;
         i < node.children.size();
         ++i) {
        print_node(
            node.children[i],
            indent + 1
        );
    }
}

static void print_node(
    const MarkdownNode& node
) {
    print_node(node, 0);
}

// ============================================================
// Нормализация переводов строк
// ============================================================

static std::string normalize_line_breaks(
    const std::string& raw
) {
    std::string buffer;
    buffer.reserve(raw.size());

    for (std::size_t i = 0;
         i < raw.length();
         ++i) {
        if (raw[i] == '\r') {
            if (i + 1 < raw.length() &&
                raw[i + 1] == '\n') {
                buffer += '\n';
                ++i;
            } else {
                buffer += '\n';
            }
        } else {
            buffer += raw[i];
        }
    }

    return buffer;
}

// ============================================================
// Main
// ============================================================

int main(
    int argc,
    char* argv[]
) {
    if (argc < 2) {
        std::cerr
            << "Usage: "
            << argv[0]
            << " <file.md>"
            << std::endl;

        return 1;
    }

    std::ifstream file(argv[1]);

    if (!file.is_open()) {
        std::cerr
            << "Error: Could not open file "
            << argv[1]
            << std::endl;

        return 1;
    }

    std::stringstream stream;
    stream << file.rdbuf();

    std::string raw = stream.str();
    file.close();

    std::string buffer =
        normalize_line_breaks(raw);

    MarkdownParser parser;

    std::vector<MarkdownNode> document =
        parser.parse(buffer);

#if !MD_DEV_DEBUG
    std::cout
        << "=== Markdown Parse Tree ==="
        << std::endl;

    for (std::size_t i = 0;
         i < document.size();
         ++i) {
        print_node(document[i]);
        std::cout << std::endl;
    }
#else
    std::cerr
        << "[DEV] Main tree output is disabled. "
        << "Build with -DMD_DEV_DEBUG=0 to enable it."
        << std::endl;
#endif

    return 0;
}

