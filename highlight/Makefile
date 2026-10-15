CXX = g++

# ----------------------------------------------------------------------
# Build configuration
# ----------------------------------------------------------------------

JOBS ?= 8

BEAR ?= bear
COMPILE_COMMANDS ?= compile_commands.json

# ----------------------------------------------------------------------
# Compiler flags
# ----------------------------------------------------------------------

CXXFLAGS = \
	-Wall \
	-Wextra \
	-Wno-unused-parameter \
	-std=c++17 \
	-O2 \
	-fdata-sections \
	-ffunction-sections \
	-fPIC \
	-flto

# obj/ содержит сгенерированный queries_data.h
CPPFLAGS = -Iobj

LDFLAGS = \
	-Wl,--gc-sections

LIBS = \
	-pthread

AR := ar
ARFLAGS := rcs


# rpath: '$$ORIGIN' — четверной $ компенсирует немедленное раскрытие
# в := и повторное раскрытие в рецепте; линкер получит буквально
# -Wl,-rpath,'$ORIGIN'
RPATH_FLAG := -Wl,-rpath,'$$$$ORIGIN'

# ----------------------------------------------------------------------
# tree-sitter discovery
#
# Порядок:
#   1. распаковка архива, если исходников нет, а архив есть;
#   2. локальные копии в ../tree-sitter/tree-sitter* (ядро и грамматики);
#   3. системный tree-sitter (pkg-config);
#   4. ничего из перечисленного — сборка с заглушкой.
#
# Файл реализации backend'а выбирается здесь, а не препроцессором:
# ровно один из TreeSitterBackend.cpp / NoopBackend.cpp попадает в сборку.
# При любом исходе libhighlight.so создаётся с одним и тем же API.
# ----------------------------------------------------------------------

PROJECT_ROOT := $(abspath $(CURDIR)/..)

# Все исходники tree-sitter лежат в общей папке tree-sitter/.
TS_ROOT := $(PROJECT_ROOT)/tree-sitter

TS_CORE_DIR   := $(TS_ROOT)/tree-sitter
TS_C_DIR      := $(TS_ROOT)/tree-sitter-c
TS_CPP_DIR    := $(TS_ROOT)/tree-sitter-cpp
TS_BASH_DIR   := $(TS_ROOT)/tree-sitter-bash
TS_CMAKE_DIR  := $(TS_ROOT)/tree-sitter-cmake
TS_MAKE_DIR   := $(TS_ROOT)/tree-sitter-make
TS_CSS_DIR    := $(TS_ROOT)/tree-sitter-css
TS_GLSL_DIR   := $(TS_ROOT)/tree-sitter-glsl
TS_HTML_DIR   := $(TS_ROOT)/tree-sitter-html
TS_JS_DIR     := $(TS_ROOT)/tree-sitter-js
TS_JSON_DIR   := $(TS_ROOT)/tree-sitter-json
TS_RUST_DIR   := $(TS_ROOT)/tree-sitter-rust
TS_ZIG_DIR    := $(TS_ROOT)/tree-sitter-zig

# tree-sitter-xml — сдвоенный: отдельные грамматики xml и dtd.
TS_XML_DIR    := $(TS_ROOT)/tree-sitter-xml/xml
TS_DTD_DIR    := $(TS_ROOT)/tree-sitter-xml/dtd
TS_XML_Q_DIR  := $(TS_ROOT)/tree-sitter-xml/queries

TS_ARCHIVE_DIR ?= $(PROJECT_ROOT)

TS_ARCHIVE_ZIP := $(firstword \
	$(wildcard $(TS_ARCHIVE_DIR)/tree-sitter.zip) \
	$(wildcard $(TS_ARCHIVE_DIR)/tree-sitter*.zip))

TS_ARCHIVE_TGZ := $(firstword \
	$(wildcard $(TS_ARCHIVE_DIR)/tree-sitter*.tar.gz) \
	$(wildcard $(TS_ARCHIVE_DIR)/tree-sitter*.tgz))

# Распаковка выполняется при разборе Makefile.
# Если tree-sitter уже распакован — повторной распаковки нет.
TS_UNPACK := $(shell \
	if [ -f "$(TS_CORE_DIR)/lib/include/tree_sitter/api.h" ]; then \
		:; \
	elif [ -n "$(TS_ARCHIVE_ZIP)" ]; then \
		echo "highlight: распаковка $(TS_ARCHIVE_ZIP)" >&2; \
		if ! unzip -oq "$(TS_ARCHIVE_ZIP)" -d "$(PROJECT_ROOT)"; then \
			echo "warning: не удалось распаковать $(TS_ARCHIVE_ZIP)" >&2; \
		fi; \
	elif [ -n "$(TS_ARCHIVE_TGZ)" ]; then \
		echo "highlight: распаковка $(TS_ARCHIVE_TGZ)" >&2; \
		if ! tar -xzf "$(TS_ARCHIVE_TGZ)" -C "$(PROJECT_ROOT)"; then \
			echo "warning: не удалось распаковать $(TS_ARCHIVE_TGZ)" >&2; \
		fi; \
	else \
		echo "warning: tree-sitter не распакован: архив не найден в $(TS_ARCHIVE_DIR)" >&2; \
	fi)

# Локальные исходники ядра и грамматик: заголовки, статические библиотеки,
# запросы подсветки. Все три условия обязательны.
#
# Имя статической библиотеки zig — libtree-sitter-md_view.a (артефакт
# сборки грамматики; переименование — отдельная задача).
TS_LOCAL_CORE_OK := $(wildcard $(TS_CORE_DIR)/lib/include/tree_sitter/api.h)
TS_LOCAL_LIBS_OK := $(wildcard $(TS_CORE_DIR)/libtree-sitter.a) \
                    $(wildcard $(TS_C_DIR)/libtree-sitter-c.a) \
                    $(wildcard $(TS_CPP_DIR)/libtree-sitter-cpp.a) \
                    $(wildcard $(TS_BASH_DIR)/libtree-sitter-bash.a) \
                    $(wildcard $(TS_CMAKE_DIR)/libtree-sitter-cmake.a) \
                    $(wildcard $(TS_MAKE_DIR)/libtree-sitter-make.a) \
                    $(wildcard $(TS_CSS_DIR)/libtree-sitter-css.a) \
                    $(wildcard $(TS_GLSL_DIR)/libtree-sitter-glsl.a) \
                    $(wildcard $(TS_HTML_DIR)/libtree-sitter-html.a) \
                    $(wildcard $(TS_JS_DIR)/libtree-sitter-javascript.a) \
                    $(wildcard $(TS_JSON_DIR)/libtree-sitter-json.a) \
                    $(wildcard $(TS_RUST_DIR)/libtree-sitter-rust.a) \
                    $(wildcard $(TS_ZIG_DIR)/libtree-sitter-md_view.a) \
                    $(wildcard $(TS_XML_DIR)/libtree-sitter-xml.a) \
                    $(wildcard $(TS_DTD_DIR)/libtree-sitter-dtd.a)
TS_LOCAL_QUERIES_OK := $(wildcard $(TS_C_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_CPP_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_BASH_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_CMAKE_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_MAKE_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_CSS_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_GLSL_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_HTML_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_JS_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_JSON_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_RUST_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_ZIG_DIR)/queries/highlights.scm) \
                       $(wildcard $(TS_XML_Q_DIR)/xml/highlights.scm) \
                       $(wildcard $(TS_XML_Q_DIR)/dtd/highlights.scm)

# Системная установка: pkg-config, затем заголовки + библиотека.
PKG_CONFIG ?= pkg-config
TS_PC_CFLAGS := $(shell $(PKG_CONFIG) --cflags tree-sitter 2>/dev/null)
TS_PC_LIBS   := $(shell $(PKG_CONFIG) --libs tree-sitter 2>/dev/null)
TS_PC_OK     := $(shell $(PKG_CONFIG) --exists tree-sitter 2>/dev/null && echo yes)

# Запросы подсветки вкомпилируются в библиотеку (см. obj/queries_data.h).
TS_C_QUERY    := $(TS_C_DIR)/queries/highlights.scm
TS_CPP_QUERY  := $(TS_CPP_DIR)/queries/highlights.scm
TS_BASH_QUERY := $(TS_BASH_DIR)/queries/highlights.scm
TS_CMAKE_QUERY := $(TS_CMAKE_DIR)/queries/highlights.scm
TS_MAKE_QUERY  := $(TS_MAKE_DIR)/queries/highlights.scm
TS_CSS_QUERY  := $(TS_CSS_DIR)/queries/highlights.scm
TS_GLSL_QUERY := $(TS_GLSL_DIR)/queries/highlights.scm
TS_HTML_QUERY := $(TS_HTML_DIR)/queries/highlights.scm
TS_JS_QUERY   := $(TS_JS_DIR)/queries/highlights.scm
TS_JSON_QUERY := $(TS_JSON_DIR)/queries/highlights.scm
TS_RUST_QUERY := $(TS_RUST_DIR)/queries/highlights.scm
TS_ZIG_QUERY  := $(TS_ZIG_DIR)/queries/highlights.scm
TS_XML_QUERY  := $(TS_XML_Q_DIR)/xml/highlights.scm
TS_DTD_QUERY  := $(TS_XML_Q_DIR)/dtd/highlights.scm

TS_USE_SYSTEM :=
TS_USE_LOCAL  :=

ifeq ($(strip $(TS_LOCAL_CORE_OK)),)
else ifeq ($(strip $(TS_LOCAL_LIBS_OK)),)
else ifeq ($(strip $(TS_LOCAL_QUERIES_OK)),)
else
  TS_USE_LOCAL := yes
endif

ifeq ($(TS_USE_LOCAL),yes)
  TS_BACKEND := obj/TreeSitterBackend.o
  CPPFLAGS   += -I$(TS_CORE_DIR)/lib/include
  TS_LINK    := \
	-Wl,--whole-archive \
	$(TS_CORE_DIR)/libtree-sitter.a \
	$(TS_C_DIR)/libtree-sitter-c.a \
	$(TS_CPP_DIR)/libtree-sitter-cpp.a \
	$(TS_BASH_DIR)/libtree-sitter-bash.a \
	$(TS_CMAKE_DIR)/libtree-sitter-cmake.a \
	$(TS_MAKE_DIR)/libtree-sitter-make.a \
	$(TS_CSS_DIR)/libtree-sitter-css.a \
	$(TS_GLSL_DIR)/libtree-sitter-glsl.a \
	$(TS_HTML_DIR)/libtree-sitter-html.a \
	$(TS_JS_DIR)/libtree-sitter-javascript.a \
	$(TS_JSON_DIR)/libtree-sitter-json.a \
	$(TS_RUST_DIR)/libtree-sitter-rust.a \
	$(TS_ZIG_DIR)/libtree-sitter-md_view.a \
	$(TS_XML_DIR)/libtree-sitter-xml.a \
	$(TS_DTD_DIR)/libtree-sitter-dtd.a \
	-Wl,--no-whole-archive
else ifeq ($(TS_PC_OK),yes)
  TS_USE_SYSTEM := yes
  TS_BACKEND := obj/TreeSitterBackend.o
  CPPFLAGS   += $(TS_PC_CFLAGS)
  TS_LINK    := $(TS_PC_LIBS)
else
  TS_BACKEND := obj/NoopBackend.o
  TS_LINK    :=
endif

# ----------------------------------------------------------------------
# Source files
# ----------------------------------------------------------------------

EXCLUDED_SRCS ?= navigation_invalid.cpp

# Все .cpp в текущей директории, кроме исключённых и backend-файлов
# (backend выбирается выше).
SRCS := $(filter-out \
	$(EXCLUDED_SRCS) TreeSitterBackend.cpp NoopBackend.cpp, \
	$(wildcard *.cpp))

SRCS := $(foreach f,$(SRCS),$(if $(wildcard $(f)),$(f)))

MAIN_PATTERN := ^[[:space:]]*(int|signed|auto)[[:space:]]+main[[:space:]]*\(

MAIN_SRCS := $(shell \
	for f in $(SRCS); do \
		if [ -f "$$f" ] && grep -qE '$(MAIN_PATTERN)' "$$f" 2>/dev/null; then \
			echo "$$f"; \
		fi; \
	done \
)

LIB_SRCS := $(filter-out $(MAIN_SRCS),$(SRCS))

LIB_OBJS  := $(patsubst %.cpp,obj/%.o,$(LIB_SRCS)) $(TS_BACKEND)
MAIN_OBJS := $(patsubst %.cpp,obj/%.o,$(MAIN_SRCS))
ALL_OBJS  := $(LIB_OBJS) $(MAIN_OBJS)

# ----------------------------------------------------------------------
# Target names
# ----------------------------------------------------------------------

PROJECT_NAME := $(notdir $(CURDIR))

STATIC_LIB := lib$(PROJECT_NAME).a
SHARED_LIB := lib$(PROJECT_NAME).so

define EXE_NAME_FOR
$(if $(filter main.cpp,$(1)),$(PROJECT_NAME),$(PROJECT_NAME)_$(basename $(1)))
endef

MAIN_EXES := $(foreach src,$(MAIN_SRCS),$(call EXE_NAME_FOR,$(src)))

# ----------------------------------------------------------------------
# Main build
# ----------------------------------------------------------------------

.PHONY: all clean obj backend-info unpack-tree-sitter compile-db-extras \
        header-commands syntax-only-commands bear print-vars

all: obj backend-info $(STATIC_LIB) $(SHARED_LIB) $(MAIN_EXES)

obj:
	@mkdir -p obj

backend-info:
	@if [ "$(TS_USE_LOCAL)" = "yes" ]; then \
		echo "highlight: tree-sitter (local) -> подсветка включена"; \
	elif [ "$(TS_USE_SYSTEM)" = "yes" ]; then \
		echo "highlight: tree-sitter (system) -> подсветка включена"; \
	else \
		echo "highlight: tree-sitter не найден -> заглушка (подсветка отключена)"; \
	fi

unpack-tree-sitter:
	@if [ -n "$(TS_ARCHIVE_ZIP)" ]; then \
		unzip -oq "$(TS_ARCHIVE_ZIP)" -d "$(PROJECT_ROOT)"; \
		echo "Распаковано: $(TS_ARCHIVE_ZIP)"; \
	elif [ -n "$(TS_ARCHIVE_TGZ)" ]; then \
		tar -xzf "$(TS_ARCHIVE_TGZ)" -C "$(PROJECT_ROOT)"; \
		echo "Распаковано: $(TS_ARCHIVE_TGZ)"; \
	else \
		echo "Архив tree-sitter не найден в $(TS_ARCHIVE_DIR)"; \
	fi

# ----------------------------------------------------------------------
# tree-sitter data (queries) — вкомпилируется в библиотеку
# ----------------------------------------------------------------------

TS_QUERIES := \
	$(TS_C_QUERY) $(TS_CPP_QUERY) $(TS_BASH_QUERY) \
	$(TS_CMAKE_QUERY) $(TS_MAKE_QUERY) \
	$(TS_CSS_QUERY) $(TS_GLSL_QUERY) $(TS_HTML_QUERY) \
	$(TS_JS_QUERY) $(TS_JSON_QUERY) $(TS_RUST_QUERY) \
	$(TS_ZIG_QUERY) $(TS_XML_QUERY) $(TS_DTD_QUERY)

obj/queries_data.h: $(TS_QUERIES) | obj
	@echo "Генерация obj/queries_data.h из highlights.scm"
	@{ \
		echo "#ifndef HIGHLIGHT_QUERIES_DATA_H"; \
		echo "#define HIGHLIGHT_QUERIES_DATA_H"; \
		echo; \
		xxd -i -n highlight_c_query < "$(TS_C_QUERY)"; \
		xxd -i -n highlight_cpp_query < "$(TS_CPP_QUERY)"; \
		xxd -i -n highlight_bash_query < "$(TS_BASH_QUERY)"; \
		xxd -i -n highlight_cmake_query < "$(TS_CMAKE_QUERY)"; \
		xxd -i -n highlight_make_query < "$(TS_MAKE_QUERY)"; \
		xxd -i -n highlight_css_query < "$(TS_CSS_QUERY)"; \
		xxd -i -n highlight_glsl_query < "$(TS_GLSL_QUERY)"; \
		xxd -i -n highlight_html_query < "$(TS_HTML_QUERY)"; \
		xxd -i -n highlight_javascript_query < "$(TS_JS_QUERY)"; \
		xxd -i -n highlight_json_query < "$(TS_JSON_QUERY)"; \
		xxd -i -n highlight_rust_query < "$(TS_RUST_QUERY)"; \
		xxd -i -n highlight_zig_query < "$(TS_ZIG_QUERY)"; \
		xxd -i -n highlight_xml_query < "$(TS_XML_QUERY)"; \
		xxd -i -n highlight_dtd_query < "$(TS_DTD_QUERY)"; \
		echo; \
		echo "#endif"; \
	} > $@


# ----------------------------------------------------------------------
# Compilation
# ----------------------------------------------------------------------

obj/TreeSitterBackend.o: TreeSitterBackend.cpp obj/queries_data.h | obj
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

obj/NoopBackend.o: NoopBackend.cpp | obj
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

obj/%.o: %.cpp | obj
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# ----------------------------------------------------------------------
# Libraries
# ----------------------------------------------------------------------

$(STATIC_LIB): $(LIB_OBJS)
	@echo "Создание статической библиотеки: $@"
	$(AR) $(ARFLAGS) $@ $^

$(SHARED_LIB): $(LIB_OBJS)
	@echo "Создание динамической библиотеки: $@"
	$(CXX) $(CXXFLAGS) -shared \
		-Wl,-soname,$@ \
		-o $@ \
		$^ \
		$(TS_LINK) \
		$(LDFLAGS) $(LIBS)

define MAKE_EXE_RULE
$(1): $(2) $(SHARED_LIB)
	@echo "Сборка исполняемого файла: $$@"
	$(CXX) -o $$@ $(2) -L. -l$(PROJECT_NAME) $(RPATH_FLAG) $(LDFLAGS) $(LIBS)
endef

$(foreach src,$(MAIN_SRCS),\
  $(eval $(call MAKE_EXE_RULE,\
    $(call EXE_NAME_FOR,$(src)),\
    obj/$(basename $(src)).o\
  ))\
)

-include $(ALL_OBJS:.o=.d)

# ----------------------------------------------------------------------
# Diagnostics
# ----------------------------------------------------------------------

print-vars:
	@echo "CURDIR        = $(CURDIR)"
	@echo "TS_USE_LOCAL  = $(TS_USE_LOCAL)"
	@echo "TS_USE_SYSTEM = $(TS_USE_SYSTEM)"
	@echo "TS_BACKEND    = $(TS_BACKEND)"
	@echo "TS_LINK       = $(TS_LINK)"
	@echo "LIB_SRCS      = $(LIB_SRCS)"
	@echo "STATIC_LIB    = $(STATIC_LIB)"
	@echo "SHARED_LIB    = $(SHARED_LIB)"

# ----------------------------------------------------------------------
# Compilation database
# ----------------------------------------------------------------------

compile-db-extras: header-commands syntax-only-commands

header-commands:
	@for header in $(COMPILE_DATABASE_HEADERS); do \
		echo "Checking header for compilation database: $$header"; \
		$(CXX) $(CPPFLAGS) $(CXXFLAGS) -x c++ -fsyntax-only "$$header" || exit $$?; \
	done

syntax-only-commands:
	@for source in $(COMPILE_DATABASE_SYNTAX_SRCS); do \
		if [ -f "$$source" ]; then \
			echo "Checking syntax-only fixture: $$source"; \
			$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fsyntax-only "$$source" || true; \
		fi; \
	done

bear:
	@command -v $(BEAR) >/dev/null 2>&1 || { \
		echo "Error: $(BEAR) not found."; \
		echo "Install Bear and repeat the command."; \
		exit 1; \
	}
	@echo "Generating $(COMPILE_COMMANDS) using $(JOBS) parallel jobs..."
	$(BEAR) --output $(COMPILE_COMMANDS) -- \
		$(MAKE) -B -j$(JOBS) all compile-db-extras
	@echo "Compilation database generated: $(COMPILE_COMMANDS)"

# ----------------------------------------------------------------------
# Cleanup
# ----------------------------------------------------------------------

clean:
	rm -rf obj $(STATIC_LIB) $(SHARED_LIB) $(MAIN_EXES)

# ----------------------------------------------------------------------
# Extra variables for Bear
# ----------------------------------------------------------------------

COMPILE_DATABASE_HEADERS := $(sort \
	$(wildcard *.h) \
	$(wildcard *.hh) \
	$(wildcard *.hpp) \
	$(wildcard *.hxx))

COMPILE_DATABASE_SYNTAX_SRCS := $(EXCLUDED_SRCS)
