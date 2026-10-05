## Project Overview

**md_view** is a Qt/C++ desktop application for viewing and editing Markdown files with:

- Syntax-highlighted source editor with line numbers
- Rendered Markdown preview using `QGraphicsTextItem`
- LaTeX math formula rendering, inline `$...$` and block `$$...$$`
- Scrollable code blocks with copy buttons
- Synchronized scrolling between source editor and preview

---

## Top-Level Application Files

| File | Purpose |
|------|---------|
| `main.cpp` | Application entry point. Creates `QApplication` and `MainWindow`. |
| `mainwindow.h` / `mainwindow.cpp` | Main window: tab management, open/save/close files, settings dialog, and status bar. |
| `editorwidget.h` / `editorwidget.cpp` | Composite widget containing `CodeEditor` (source) and `MarkdownGraphicsView` (preview) in a `QStackedWidget`. Handles render-mode switching, scroll synchronization, and reparsing on text changes. |
| `MarkdownGraphicsView.h` / `MarkdownGraphicsView.cpp` | Preview widget. Coordinates rendering, owns the `QGraphicsScene` and `QGraphicsTextItem`, updates scene geometry, handles resize and context-menu events, and activates embedded code editors. |
| `MarkdownHtmlRenderer.h` / `MarkdownHtmlRenderer.cpp` | Converts `MarkdownNode` trees to HTML. Handles inline and block elements, lists, tables, code-block marker generation, and source-line candidate normalization. |
| `MarkdownMathObjects.h` / `MarkdownMathObjects.cpp` | Implements Qt inline text objects for rendered fractions, square roots, and hats. Registers handlers with `QTextDocument` and replaces math marker sequences with inline objects. |
| `MarkdownCodeBlocks.h` / `MarkdownCodeBlocks.cpp` | Implements code-block support for `MarkdownGraphicsView`: copy buttons, scrollable code editors, placeholder block heights, and code-widget height calculation. |
| `CodeBlockEditor.h` / `CodeBlockEditor.cpp` | `QPlainTextEdit` used for long code blocks. Handles activation, focus, and routing wheel events between the code editor and preview. |
| `MarkdownSourceNavigation.cpp` | Implements `MarkdownGraphicsView` source-line anchor construction, scrolling to a source line, and finding the source line at the current preview scroll position. The methods are declared in `MarkdownGraphicsView.h`. |
| `settings.h` / `settings.cpp` | `Settings` struct (line numbers, word wrap, code-block height limit). Loads and saves settings via `QSettings` INI file. |
| `settingsdialog.h` / `settingsdialog.cpp` | Modal dialog for editing settings. |

### Rendering flow

1. `EditorWidget` parses source Markdown using `MarkdownParser`.
2. It passes the resulting `MarkdownNode` tree to `MarkdownGraphicsView::setDocument()`.
3. `MarkdownGraphicsView` asks `MarkdownUtils::blocks_to_html()` to produce HTML and sets it on a `QGraphicsTextItem`.
4. `MarkdownMathObjects` installs Qt inline objects for math marker sequences in the text document.
5. The code-block support creates copy buttons and, when configured, scrollable code-editor overlays.
6. `MarkdownSourceNavigation.cpp` maps preview positions to source lines and back.

### Notes for changes

- Keep `MarkdownGraphicsView` as the coordinator for the preview. Put Markdown-to-HTML conversion in `MarkdownHtmlRenderer`, math inline-object implementation in `MarkdownMathObjects`, code-block behavior in `MarkdownCodeBlocks`, and source-navigation method definitions in `MarkdownSourceNavigation.cpp`.
- `MarkdownCodeBlocks.cpp` and `MarkdownSourceNavigation.cpp` define methods declared in `MarkdownGraphicsView.h`; they are separate implementation files, not separate manager classes.
- `MarkdownMathObjects.h` contains the `Q_OBJECT` declarations for the math object classes. Let Qt `AUTOMOC` process the header. Do not add a manual `#include "*.moc"` for these classes.
- When adding or moving application source files, update the `PROJECT_SOURCES` list in `CMakeLists.txt`. Do not edit generated files under `build/`; they are recreated by CMake and Qt's build steps.

---

## `md_lexer_v2/` — Markdown Parser Library

A standalone C++ library with no Qt dependency that parses Markdown into a tree of `MarkdownNode` structs.

| File | Purpose |
|------|---------|
| `MarkdownNode.h` | Core data structures: `NodeType` enum, `Alignment` enum, `MarkdownNode` struct, and `LexerResult` struct. |
| `CommonUtils.h` / `CommonUtils.cpp` | Low-level text utilities: line-end finding, blank-line detection, leading-space counting, and common-indent removal. |
| `Lexers.h` / `Lexers.cpp` | Inline and block lexer implementations. Inline support includes code, image, link, bold, italic, strikethrough, and inline math. Block support includes headings, fenced code, quotes, horizontal rules, lists, tables, and math blocks. Each lexer exposes `try_consume(buffer, position) → LexerResult`. |
| `MarkdownParser.h` / `MarkdownParser.cpp` | Orchestrates lexers. `parse()` splits text into blocks, `parse_blocks()` dispatches to block lexers, and `parse_inline()` dispatches to inline lexers. Also handles nested lists, table cells, and fenced-Markdown recursion. |
| `main.cpp` | CLI test harness: reads a `.md` file and prints the parsed tree to stdout. |

### When to open which file

- **Adding a Markdown element** → `Lexers.h` / `Lexers.cpp` (add a lexer), `MarkdownParser.cpp` (register it in `parse_inline` or `parse_blocks`), and `MarkdownNode.h` (add a `NodeType` if needed).
- **Fixing parsing logic** → `MarkdownParser.cpp`.
- **Fixing detection of a specific element** → the corresponding lexer in `Lexers.cpp`.
- **Understanding the AST shape** → `MarkdownNode.h`.

---

## `Tex_AST_v2/` — LaTeX Math Formula Parser & Renderer

A standalone library with no Qt dependency that lexes, parses, and renders LaTeX math formulas to HTML.

| File | Purpose |
|------|---------|
| `expression_types.h` | AST node definitions: `TokenKind` enum, `Token` struct, and AST node classes such as `Number`, `Identifier`, `GreekSymbol`, `CommandNode`, `CommandWithArgument`, `UnaryOperation`, `BinaryOperation`, `Power`, and `Subscript`. |
| `math_lexer.h` / `math_lexer.cpp` | Lexes LaTeX source into a token stream. Handles math-mode delimiters (`$`, `$$`, `\(`, `\[`), numbers, identifiers, Greek letters, and LaTeX commands. |
| `math_parser.h` / `math_parser.cpp` | Recursive-descent parser. Builds an AST from tokens. Supports relations, addition, multiplication, unary and postfix operations, `\frac`, `\hat`, `\mathbf`, `\sqrt`, `\mathcal`, parentheses, brackets, and absolute-value bars. |
| `math_renderer.h` / `math_renderer.cpp` | Renders the AST to HTML. `render_inline_formula()` handles inline formulas and `render_block_formula()` handles block formulas. The output may include marker sequences consumed by the Qt preview's math-object layer. |
| `main.cpp` | CLI test harness for reading a `.tex` file and printing the AST. |

### When to open which file

- **Adding a LaTeX command** → `math_lexer.cpp` (tokenize), `math_parser.cpp` (parse into AST), and `math_renderer.cpp` (render).
- **Fixing formula parsing** → `math_parser.cpp`.
- **Fixing HTML formula rendering or generated markers** → `math_renderer.cpp`.
- **Adding an AST node type** → `expression_types.h`.

---

## Integration Points

```text
editorwidget.cpp
  └─ calls MarkdownParser::parse()                [md_lexer_v2]
  └─ passes AST to MarkdownGraphicsView::setDocument()

MarkdownGraphicsView.cpp
  ├─ calls MarkdownUtils::blocks_to_html()        [MarkdownHtmlRenderer]
  ├─ registers and installs math inline objects   [MarkdownMathObjects]
  ├─ creates code-block controls                   [MarkdownCodeBlocks]
  └─ manages QGraphicsTextItem, scene, and layout

MarkdownHtmlRenderer.cpp
  ├─ converts MarkdownNode trees to HTML
  ├─ calls math_ast::render_inline_formula()       [Tex_AST_v2]
  └─ calls math_ast::render_block_formula()        [Tex_AST_v2]

MarkdownMathObjects.cpp
  └─ turns math marker sequences in QTextDocument into Qt inline objects

MarkdownCodeBlocks.cpp
  └─ creates copy buttons and scrollable code-block overlays

MarkdownSourceNavigation.cpp
  └─ maps source lines to preview positions and preview scroll to source lines

