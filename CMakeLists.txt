cmake_minimum_required(VERSION 3.5)

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

set(PROJECT_SOURCES
    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    editorwidget.cpp
    editorwidget.h

    MarkdownGraphicsView.cpp
    MarkdownGraphicsView.h

    settings.cpp
    settings.h

    settingsdialog.cpp
    settingsdialog.h
    settingsdialog.ui

    md_lexer_v2/MarkdownNode.h
    md_lexer_v2/Lexers.h
    md_lexer_v2/MarkdownParser.h
    md_lexer_v2/CommonUtils.h
)

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

target_link_libraries(md_view
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        md_lexer_v2
)

target_include_directories(md_view
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${MD_LEXER_DIR}"
)

set_target_properties(md_view PROPERTIES
    BUILD_RPATH "${MD_LEXER_DIR}"
    INSTALL_RPATH "$ORIGIN/../md_lexer_v2"
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING
        ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(md_view)
endif()

