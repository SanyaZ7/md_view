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
| `CodeHighlighter.h` / `CodeHighlighter.cpp` | `QSyntaxHighlighter` for supported languages, backed by `libhighlight`. Tokenizes the whole document with tree-sitter and maps UTF-8 byte offsets to `QString` positions, including UTF-16 surrogate pairs. Explicitly applies `Noto Color Emoji` to emoji code points when available. Used by `CodeEditor` (by file extension) and `CodeBlockEditor` (by fence info string). |
| `FontUtils.h` | Shared code-font construction. Keeps the monospace editor font and configures Qt 6.8+ fallback families (`Noto Color Emoji`, `Noto Emoji`, `Symbola`, `DejaVu Sans`) while remaining source-compatible with older Qt versions. |
| `HighlightHtml.h` / `HighlightHtml.cpp` | Renders highlighted code as `<span>` HTML for the preview's `<pre>` blocks, using `highlight::ColorScheme`. |
| `MarkdownSourceNavigation.cpp` | Implements `MarkdownGraphicsView` source-line anchor construction, scrolling to a source line, and finding the source line at the current preview scroll position. The methods are declared in `MarkdownGraphicsView.h`. |
| `settings.h` / `settings.cpp` | `Settings` struct (line numbers, word wrap, code-block height limit). Loads and saves settings via `QSettings` INI file. |
| `settingsdialog.h` / `settingsdialog.cpp` | Modal dialog for editing settings. |
| `SearchPanel.h` / `SearchPanel.cpp` / `SearchPanel.ui` | Find panel shown in a `QDockWidget` (toggled by `Ctrl+F` from `MainWindow`). Multi-level fuzzy search over the source `QPlainTextEdit`: exact, case-folded, whitespace-equivalent, lookalike Cyrillic↔Latin, dash-equivalent, and full dash-as-space normalization. Results are grouped by the strictest matching level in a `QListWidget` and highlighted in the editor via `QTextEdit::ExtraSelection`. `setEditor()` rebinds the panel to the active tab's editor. |

### Rendering flow

1. `EditorWidget` parses source Markdown using `MarkdownParser`.
2. It passes the resulting `MarkdownNode` tree to `MarkdownGraphicsView::setDocument()`.
3. `MarkdownGraphicsView` asks `MarkdownUtils::blocks_to_html()` to produce HTML and sets it on a `QGraphicsTextItem`. Code blocks with a supported language are highlighted into `<span>`s by `HighlightHtml`.
4. `MarkdownMathObjects` installs Qt inline objects for math marker sequences in the text document.
5. The code-block support creates copy buttons and, when configured, scrollable code-editor overlays (highlighted by `CodeHighlighter`).
6. `MarkdownSourceNavigation.cpp` maps preview positions to source lines and back.

### Notes for changes

- Keep `MarkdownGraphicsView` as the coordinator for the preview. Put Markdown-to-HTML conversion in `MarkdownHtmlRenderer`, math inline-object implementation in `MarkdownMathObjects`, code-block behavior in `MarkdownCodeBlocks`, and source-navigation method definitions in `MarkdownSourceNavigation.cpp`.
- `MarkdownCodeBlocks.cpp` and `MarkdownSourceNavigation.cpp` define methods declared in `MarkdownGraphicsView.h`; they are separate implementation files, not separate manager classes.
- `MarkdownMathObjects.h` contains the `Q_OBJECT` declarations for the math object classes. Let Qt `AUTOMOC` process the header. Do not add a manual `#include "*.moc"` for these classes.
- When adding or moving application source files, update the `PROJECT_SOURCES` list in `CMakeLists.txt`. Do not edit generated files under `build/`; they are recreated by CMake and Qt's build steps.
- `md_lexer_v2/MarkdownNode.h` carries `language` (the fence info string, e.g. `cpp`). `MarkdownHtmlRenderer` forwards it in `CodeBlockInfo` so both the `<pre>` HTML and `CodeBlockEditor` can pick a grammar.

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

## `highlight/` — Syntax Highlighting Library

A standalone library with no Qt dependency that tokenizes source code into typed spans using tree-sitter. Built by its own `Makefile` (like `md_lexer_v2`). Integrated grammars: C, C++, Bash, CMake, Make, CSS, GLSL, HTML, JavaScript, JSON, Rust, Zig, XML, and DTD.

`libhighlight.so` is always required by the application, but tree-sitter itself is optional. The Makefile picks the backend at build time:

1. unpacks a `third_party/tree-sitter*.zip|tar.gz` archive if the sources are missing;
2. uses local `../tree-sitter/tree-sitter*` (headers + `.a` + `queries/highlights.scm`);
3. falls back to system tree-sitter via `pkg-config`;
4. otherwise builds `NoopBackend.cpp` — the same API, `available() == false`, empty token lists.

All tree-sitter sources (core and grammars) live under the `tree-sitter/` folder; the core is at `tree-sitter/tree-sitter`, grammars at `tree-sitter/tree-sitter-<lang>`. The XML grammar is doubled: `tree-sitter-xml/xml` (symbol `tree_sitter_xml`) and `tree-sitter-xml/dtd` (symbol `tree_sitter_dtd`), with queries under `tree-sitter-xml/queries/{xml,dtd}/highlights.scm`. The Zig static library is named `libtree-sitter-md_view.a` (a build artifact to be renamed later).

Exactly one of `TreeSitterBackend.cpp` / `NoopBackend.cpp` is compiled, so there are no duplicate symbols and no `#ifdef` in application code. The integrated grammars are linked statically into `libhighlight.so`; `queries/highlights.scm` are embedded via `xxd` (`obj/queries_data.h`). The C++ and GLSL queries carry `; inherits: c` (tree-sitter does not expand `; inherits:`), so the C query is prepended to them at runtime.

Query predicates are evaluated in `TreeSitterBackend.cpp`: `#match?`, `#lua-match?`, `#eq?`, and `#any-of?`, including their `not-...` negated forms and multiple predicates per capture. `#is?`/`#is-not?` (locals) are not supported — locals are not tracked, so those predicates do not restrict highlighting.

| File | Purpose |
|------|---------|
| `highlight.h` | Public API: `Language`, `Token`, `Highlighter`. `language_from_extension()`, `language_from_file_name()` and `language_from_name()` map file extensions, file names (including `Makefile` and `CMakeLists.txt`) and fence info strings to `Language`. |
| `highlight.cpp` | Dispatcher: picks the tree-sitter backend or the no-op fallback; owns the extension/name mapping tables. HTML is routed to the composite embedded-language highlighter. |
| `backend.h` | Internal seam declaring `make_tree_sitter_highlighter()`, `make_embedded_html_highlighter()` and `tree_sitter_backend_available()`. |
| `EmbeddedHtmlHighlighter.h` / `EmbeddedHtmlHighlighter.cpp` | Composite highlighter for HTML documents. Uses the HTML grammar for markup and delegates the contents of `<style>` and `<script>` blocks to CSS and JavaScript highlighters, translating nested UTF-8 byte offsets back to document offsets and giving nested tokens priority over broad HTML text captures. |
| `TreeSitterBackend.cpp` | Real implementation for one tree-sitter grammar: declares one `tree_sitter_<lang>()` per grammar, concatenates queries as needed, parses with tree-sitter, runs the highlight queries, evaluates predicates, and resolves overlapping captures. |
| `NoopBackend.cpp` | Fallback implementation used when tree-sitter is unavailable. |
| `ColorScheme.h` / `ColorScheme.cpp` | Maps token types to `TokenStyle` (packed `0xRRGGBB`, bold/italic/underline). Shared by all languages; `style_for()` falls back from a composite type to its parent (`string.special.key → string.special → string`). `default_light()` is the current scheme; loading/editing it from the GUI is a separate task. |
| `main.cpp` | CLI test harness: prints tokens for a source file. |

### When to open which file

- **Changing how tokens are classified** → `TreeSitterBackend.cpp` (capture resolution) or the `queries/*.scm` files.
- **Adding a supported language** → `highlight.h` (`Language`), `highlight.cpp` (`language_from_*`), `TreeSitterBackend.cpp` (`extern "C"` grammar symbol, query text, `switch` case), and the `Makefile` (`TS_<LANG>_DIR`, `TS_LOCAL_LIBS_OK`, `TS_LOCAL_QUERIES_OK`, `TS_QUERY`, `TS_LINK`, `obj/queries_data.h`). CMakeLists.txt and Makefile-family files must be detected by `language_from_file_name()`, since their suffixes do not indicate the grammar.
- **Changing colors** → `ColorScheme.cpp` (single shared scheme for all languages).
- **Changing build/auto-unpack behavior** → `Makefile`.

### Notes for changes

- The application never links tree-sitter directly; it only includes `highlight/*.h` and links `libhighlight.so`. Keep `highlight.h` free of tree-sitter types.
- Byte offsets from tree-sitter are UTF-8; Qt positions are UTF-16. `CodeHighlighter` converts between them, including surrogate pairs, and explicitly applies the emoji font to emoji code points — do not use raw offsets on `QTextCursor`/`QSyntaxHighlighter`.
- `EmbeddedHtmlHighlighter` works entirely inside `libhighlight` and must not be moved into the Qt adapter: both preview HTML generation and the CLI highlighter use the same composite behavior.
- `highlight/` has no `Makefile` subdirectory recursion and is not listed in `PROJECT_SOURCES`; its `.cpp` files are collected by the local wildcard-based Makefile. Only headers used by the application need to be visible through `HIGHLIGHT_DIR`.

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
  ├─ calls math_ast::render_block_formula()        [Tex_AST_v2]
  └─ calls highlight_code_html()                   [highlight]

MarkdownMathObjects.cpp
  └─ turns math marker sequences in QTextDocument into Qt inline objects

MarkdownCodeBlocks.cpp
  └─ creates copy buttons and scrollable code-block overlays

CodeHighlighter.cpp
  └─ tokenizes with highlight::Highlighter         [highlight]
  └─ used by CodeEditor (source) and CodeBlockEditor (preview)

MarkdownSourceNavigation.cpp
  └─ maps source lines to preview positions and preview scroll to source lines

