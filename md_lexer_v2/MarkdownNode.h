#ifndef MARKDOWN_NODE_H
#define MARKDOWN_NODE_H

#include <cstddef>
#include <string>
#include <vector>

enum class NodeType {
    Text,
    Heading,
    Bold,
    Italic,
    Strikethrough,
    InlineCode,
    CodeBlock,
    FencedMarkdown,
    Link,
    Image,
    HardBreak,
    List,
    ListItem,
    Quote,
    HorizontalRule,
    Table,
    TableRow,
    TableCell,
    MathInline,
    MathBlock
};

enum class Alignment {
    Left,
    Center,
    Right
};

struct MarkdownNode {
    NodeType type = NodeType::Text;
    std::string content;
    int level = 0;
    std::string url;
    std::vector<MarkdownNode> children;
    Alignment alignment = Alignment::Left;
    bool ordered = false;
    bool is_header = false;

    /*
     * Номера строк исходного Markdown.
     *
     * Значения задаются парсером для блочных узлов.
     * Нумерация начинается с нуля.
     */
    int source_start_line = -1;
    int source_end_line = -1;

    static MarkdownNode text(const std::string& value) {
        MarkdownNode node;
        node.type = NodeType::Text;
        node.content = value;
        return node;
    }
};


struct LexerResult {
    std::size_t length = 0;
    MarkdownNode node;
    bool matched = false;
};

#endif // MARKDOWN_NODE_H

