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
	-std=c++2a \
	-O2 \
	-fdata-sections \
	-ffunction-sections \
	-fPIC \
	-flto

CPPFLAGS := -I/usr/include/boost

LDFLAGS = \
	-Wl,--gc-sections

LIBS = \
	-pthread

AR := ar
ARFLAGS := rcs

# rpath: '$$ORIGIN' — двойной $ защищает от съедания make/eval,
# линкер получит буквально -Wl,-rpath,'$ORIGIN'
RPATH_FLAG := -Wl,-rpath,'$$$$ORIGIN'

# ----------------------------------------------------------------------
# Source files
# ----------------------------------------------------------------------

EXCLUDED_SRCS ?= navigation_invalid.cpp

# Все .cpp в текущей директории, кроме исключённых
SRCS := $(filter-out \
	$(EXCLUDED_SRCS), \
	$(wildcard *.cpp))

# Оставляем только реально существующие файлы (защита от «фантомных» имён)
SRCS := $(foreach f,$(SRCS),$(if $(wildcard $(f)),$(f)))

MAIN_PATTERN := ^[[:space:]]*(int|signed|auto)[[:space:]]+main[[:space:]]*\(

# Поиск main: проверяем существование файла и глушим stderr grep,
# чтобы отсутствующий файл не ломал разбор.
MAIN_SRCS := $(shell \
	for f in $(SRCS); do \
		if [ -f "$$f" ] && grep -qE '$(MAIN_PATTERN)' "$$f" 2>/dev/null; then \
			echo "$$f"; \
		fi; \
	done \
)

# LIB_SRCS — всё, что НЕ содержит main (идёт в библиотеку)
# MAIN_SRCS — файлы с main (идут в отдельные приложения)
LIB_SRCS := $(filter-out $(MAIN_SRCS),$(SRCS))

LIB_OBJS  := $(patsubst %.cpp,obj/%.o,$(LIB_SRCS))
MAIN_OBJS := $(patsubst %.cpp,obj/%.o,$(MAIN_SRCS))
ALL_OBJS  := $(LIB_OBJS) $(MAIN_OBJS)

# ----------------------------------------------------------------------
# Target names
# ----------------------------------------------------------------------

# Имя проекта = имя каталога с Makefile.
# Например: каталог md_lexer_v2 -> libmd_lexer_v2.a/.so, md_lexer_v2, md_lexer_v2_test2
PROJECT_NAME := $(notdir $(CURDIR))

STATIC_LIB := lib$(PROJECT_NAME).a
SHARED_LIB := lib$(PROJECT_NAME).so

# Имя исполняемого файла для конкретного src с main():
#   main.cpp  -> $(PROJECT_NAME)
#   test1.cpp -> $(PROJECT_NAME)_test1
define EXE_NAME_FOR
$(if $(filter main.cpp,$(1)),$(PROJECT_NAME),$(PROJECT_NAME)_$(basename $(1)))
endef

MAIN_EXES := $(foreach src,$(MAIN_SRCS),$(call EXE_NAME_FOR,$(src)))

# ----------------------------------------------------------------------
# Main build
# ----------------------------------------------------------------------

.PHONY: all clean obj compile-db-extras header-commands syntax-only-commands bear print-vars

all: obj $(STATIC_LIB) $(SHARED_LIB) $(MAIN_EXES)
	@echo "Проверка наличия Makefile в поддиректориях..."
	@for dir in */ ; do \
		if [ "$$dir" != "obj/" ] && \
		   [ -f "$$dir/Makefile" ]; then \
			echo "Найден Makefile в директории: $$dir"; \
			$(MAKE) -C "$${dir%*/}" -j$(JOBS); \
		elif [ "$$dir" != "obj/" ]; then \
			echo "В директории $$dir нет файла Makefile"; \
		fi; \
	done

obj:
	@mkdir -p obj

# Компиляция объектов
obj/%.o: %.cpp | obj
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

# Статическая библиотека
$(STATIC_LIB): $(LIB_OBJS)
	@echo "Создание статической библиотеки: $@"
	$(AR) $(ARFLAGS) $@ $^

# Динамическая библиотека
$(SHARED_LIB): $(LIB_OBJS)
	@echo "Создание динамической библиотеки: $@"
	$(CXX) $(CXXFLAGS) -shared \
		-Wl,-soname,$@ \
		-o $@ \
		$^ \
		$(LDFLAGS) $(LIBS)

# Правило для каждого исполняемого файла, содержащего main().
# $$@ и $$(2) внутри define защищают от преждевременного раскрытия eval.
# Имя цели $(1) вычисляется ДО eval (в $(call)).
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
	@echo "CURDIR       = $(CURDIR)"
	@echo "PROJECT_NAME = $(PROJECT_NAME)"
	@echo "SRCS         = $(SRCS)"
	@echo "MAIN_SRCS    = $(MAIN_SRCS)"
	@echo "LIB_SRCS     = $(LIB_SRCS)"
	@echo "MAIN_EXES    = $(MAIN_EXES)"
	@echo "STATIC_LIB   = $(STATIC_LIB)"
	@echo "SHARED_LIB   = $(SHARED_LIB)"

# ----------------------------------------------------------------------
# Extra compilation database entries (Bear support)
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

# ----------------------------------------------------------------------
# Compilation database
# ----------------------------------------------------------------------

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

	@echo "Очистка поддиректорий..."
	@for dir in */ ; do \
		if [ "$$dir" != "obj/" ] && \
		   [ -f "$$dir/Makefile" ]; then \
			echo "Выполняется очистка в директории: $$dir"; \
			$(MAKE) -C "$${dir%*/}" clean; \
		elif [ "$$dir" != "obj/" ]; then \
			echo "В директории $$dir нет файла Makefile для очистки"; \
		fi; \
	done

# ----------------------------------------------------------------------
# Extra variables for Bear
# ----------------------------------------------------------------------

COMPILE_DATABASE_HEADERS := $(sort \
	$(wildcard *.h) \
	$(wildcard *.hh) \
	$(wildcard *.hpp) \
	$(wildcard *.hxx))

COMPILE_DATABASE_SYNTAX_SRCS := $(EXCLUDED_SRCS)
