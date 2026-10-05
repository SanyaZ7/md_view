cmake_minimum_required(VERSION 3.5)

# ============================================================
# Always export compile_commands.json
# (must be set before project() / target creation)
# ============================================================

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(md_view VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)


# ============================================================
# Markdown lexer library
# ============================================================

set(MD_LEXER_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/md_lexer_v2"
)

set(MD_LEXER_LIBRARY
    "${MD_LEXER_DIR}/libmd_lexer_v2.so"
)

if(NOT EXISTS "${MD_LEXER_LIBRARY}")
    message(FATAL_ERROR
        "Library libmd_lexer_v2.so was not found:\n"
        "  ${MD_LEXER_LIBRARY}\n"
        "Build md_lexer_v2 first."
    )
endif()

add_library(md_lexer_v2 SHARED IMPORTED)

set_target_properties(md_lexer_v2 PROPERTIES
    IMPORTED_LOCATION
        "${MD_LEXER_LIBRARY}"

    INTERFACE_INCLUDE_DIRECTORIES
        "${MD_LEXER_DIR}"
)


# ============================================================
# TeX/math AST library
# ============================================================

set(TEX_AST_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Tex_AST_v2"
)

set(TEX_AST_LIBRARY
    "${TEX_AST_DIR}/libTex_AST_v2.so"
)

if(NOT EXISTS "${TEX_AST_LIBRARY}")
    message(FATAL_ERROR
        "Library libTex_AST_v2.so was not found:\n"
        "  ${TEX_AST_LIBRARY}\n"
        "Build Tex_AST_v2 first."
    )
endif()

add_library(Tex_AST_v2 SHARED IMPORTED)

set_target_properties(Tex_AST_v2 PROPERTIES
    IMPORTED_LOCATION
        "${TEX_AST_LIBRARY}"

    INTERFACE_INCLUDE_DIRECTORIES
        "${TEX_AST_DIR}"
)


# ============================================================
# Application sources
# ============================================================

set(PROJECT_SOURCES
    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    editorwidget.cpp
    editorwidget.h

    MarkdownGraphicsView.cpp
    MarkdownGraphicsView.h

    MarkdownHtmlRenderer.cpp
    MarkdownHtmlRenderer.h

    MarkdownMathObjects.cpp
    MarkdownMathObjects.h

    MarkdownCodeBlocks.cpp
    MarkdownCodeBlocks.h

    MarkdownSourceNavigation.cpp

    CodeBlockEditor.cpp
    CodeBlockEditor.h

    settings.cpp
    settings.h

    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui

    # Markdown lexer public headers
    md_lexer_v2/MarkdownNode.h
    md_lexer_v2/Lexers.h
    md_lexer_v2/MarkdownParser.h
    md_lexer_v2/CommonUtils.h

    # TeX/math AST public headers
    Tex_AST_v2/expression_types.h
    Tex_AST_v2/math_lexer.h
    Tex_AST_v2/math_parser.h
    Tex_AST_v2/math_renderer.h
)


# ============================================================
# Executable
# ============================================================

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(md_view
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(md_view SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(md_view
            ${PROJECT_SOURCES}
        )
    endif()
endif()


# ============================================================
# Include directories
# ============================================================

target_include_directories(md_view
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${MD_LEXER_DIR}"
        "${TEX_AST_DIR}"
)


# ============================================================
# Libraries
# ============================================================

target_link_libraries(md_view
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        md_lexer_v2
        Tex_AST_v2
)


# ============================================================
# Runtime library search paths
# ============================================================

set_target_properties(md_view PROPERTIES
    BUILD_RPATH
        "${MD_LEXER_DIR};${TEX_AST_DIR}"

    INSTALL_RPATH
        "$ORIGIN/../md_lexer_v2;$ORIGIN/../Tex_AST_v2"

    MACOSX_BUNDLE_GUI_IDENTIFIER
        my.example.com

    MACOSX_BUNDLE_BUNDLE_VERSION
        ${PROJECT_VERSION}

    MACOSX_BUNDLE_SHORT_VERSION_STRING
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)


# ============================================================
# Qt finalization
# ============================================================

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(md_view)
endif()


# ============================================================
# Regenerate compile_commands.json on every build
#
# Works for any CMAKE_BUILD_TYPE (Debug / Release / RelWithDebInfo
# / MinSizeRel / empty) and any generator (Make / Ninja / ...).
#
# The target has no OUTPUT, so CMake treats it as always out-of-date
# and re-runs it on every `make` / `ninja` / `cmake --build`.
# ============================================================

add_custom_target(update_compile_commands ALL
    COMMAND ${CMAKE_COMMAND}
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        -S "${CMAKE_SOURCE_DIR}"
        -B "${CMAKE_BINARY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_BINARY_DIR}/compile_commands.json"
        "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Regenerating compile_commands.json in project root"
    VERBATIM
)

# Запускать цель только после сборки md_view,
# чтобы не было гонки за CMakeCache.txt с текущим прогоном сборки.
add_dependencies(update_compile_commands md_view)
