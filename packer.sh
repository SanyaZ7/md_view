#!/usr/bin/env bash
# packer.sh — сборка переносимого дистрибутива md_view
#
# Дистрибутив: бинарник + собственные библиотеки проекта.
# Qt6 и системные библиотеки берутся из системы пользователя.
#
# Использование:
#   ./packer.sh          собрать dist/ и md_view-*.tar.gz
#   ./packer.sh clean    удалить dist/ и архивы
#
# Переменные:
#   BUILD_TYPE   тип сборки (по умолчанию Release_min_size)

set -euo pipefail

# ════════════════════════════════════════════════════════════════════════
# Настройки
# ════════════════════════════════════════════════════════════════════════
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_TYPE="${BUILD_TYPE:-Release_min_size}"
BUILD_DIR="$PROJECT_ROOT/build/$BUILD_TYPE"
DIST_DIR="$PROJECT_ROOT/dist"
BINARY="$BUILD_DIR/md_view"

# Собственные библиотеки: <каталог>:<префикс>
# tree-sitter влинкован статически в libhighlight.so
PROJECT_LIBS=(
    "md_lexer_v2:libmd_lexer_v2"
    "Tex_AST_v2:libTex_AST_v2"
    "highlight:libhighlight"
)

# ════════════════════════════════════════════════════════════════════════
# Вывод
# ════════════════════════════════════════════════════════════════════════
if [[ -t 1 ]]; then
    C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'; C_BLU=$'\033[34m'; C_RST=$'\033[0m'
else
    C_RED=; C_GRN=; C_YLW=; C_BLU=; C_RST=
fi
info() { printf '%s[pack]%s %s\n' "$C_BLU" "$C_RST" "$*"; }
ok()   { printf '%s[ ok ]%s %s\n' "$C_GRN" "$C_RST" "$*"; }
warn() { printf '%s[warn]%s %s\n' "$C_YLW" "$C_RST" "$*" >&2; }
die()  { printf '%s[fail]%s %s\n' "$C_RED" "$C_RST" "$*" >&2; exit 1; }

# ════════════════════════════════════════════════════════════════════════
# Проверки
# ════════════════════════════════════════════════════════════════════════
check_inputs() {
    [[ -x "$BINARY" ]] || die "бинарник не найден: $BINARY
Соберите: cmake --build '$BUILD_DIR' -j"
    ok "бинарник: $BINARY"

    local entry dir prefix
    for entry in "${PROJECT_LIBS[@]}"; do
        dir="${entry%%:*}"; prefix="${entry##*:}"
        find "$PROJECT_ROOT/$dir" -maxdepth 1 -name "${prefix}.so*" -print -quit | grep -q . \
            || die "не найдена библиотека: $dir/${prefix}.so"
    done
    ok "библиотеки проекта на месте"
}

# ════════════════════════════════════════════════════════════════════════
# Копирование
# ════════════════════════════════════════════════════════════════════════
copy_project_libs() {
    info "копирую собственные библиотеки..."
    local entry dir prefix
    for entry in "${PROJECT_LIBS[@]}"; do
        dir="${entry%%:*}"; prefix="${entry##*:}"
        mkdir -p "$DIST_DIR/$dir"
        # -L: разыменовываем симлинки, чтобы не тащить битые ссылки
        cp -L "$PROJECT_ROOT/$dir/${prefix}.so"* "$DIST_DIR/$dir/"
        ok "  $dir/"
    done
}

# ════════════════════════════════════════════════════════════════════════
# Пути поиска библиотек
# ════════════════════════════════════════════════════════════════════════
patch_rpath() {
    local rpath='$ORIGIN/md_lexer_v2:$ORIGIN/Tex_AST_v2:$ORIGIN/highlight'

    if command -v patchelf &>/dev/null; then
        patchelf --set-rpath "$rpath" "$DIST_DIR/md_view"
        ok "rpath выставлен через patchelf"
        return
    fi

    # Без patchelf: обёртка выставляет LD_LIBRARY_PATH
    warn "patchelf не найден — создаю launch.sh"
    cat > "$DIST_DIR/launch.sh" <<'EOF'
#!/usr/bin/env bash
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/md_lexer_v2:$SCRIPT_DIR/Tex_AST_v2:$SCRIPT_DIR/highlight${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$SCRIPT_DIR/md_view" "$@"
EOF
    chmod +x "$DIST_DIR/launch.sh"
    ok "launch.sh создан"
}

# ════════════════════════════════════════════════════════════════════════
# README и архив
# ════════════════════════════════════════════════════════════════════════
create_readme() {
    {
        echo "md_view"
        echo "======"
        echo
        echo "Сборка: $(date '+%Y-%m-%d %H:%M')"
        echo
        echo "Запуск:"
        echo "  ./md_view [файл.md]"
        [[ -f "$DIST_DIR/launch.sh" ]] && echo "  либо: ./launch.sh [файл.md]"
        echo
        echo "Требования:"
        echo "  - Linux x86_64, X11 или Wayland"
        echo "  - Qt6 в системе"
    } > "$DIST_DIR/README.txt"
}

make_archive() {
    local name="md_view-${BUILD_TYPE}-$(date +%Y%m%d).tar.gz"
    info "создаю архив..."
    tar -C "$PROJECT_ROOT" -czf "$PROJECT_ROOT/$name" dist
    ok "архив: $name ($(du -h "$PROJECT_ROOT/$name" | cut -f1))"
}

# ════════════════════════════════════════════════════════════════════════
# Очистка
# ════════════════════════════════════════════════════════════════════════
do_clean() {
    rm -rf "$DIST_DIR" "$PROJECT_ROOT"/md_view-*.tar.gz
    ok "dist/ и архивы удалены"
}

# ════════════════════════════════════════════════════════════════════════
# Главный сценарий
# ════════════════════════════════════════════════════════════════════════
do_pack() {
    check_inputs

    rm -rf "$DIST_DIR"
    mkdir -p "$DIST_DIR"

    cp -L "$BINARY" "$DIST_DIR/"
    ok "бинарник скопирован"

    copy_project_libs
    patch_rpath
    create_readme
    make_archive

    echo
    ok "готово: $DIST_DIR/ ($(du -sh "$DIST_DIR" | cut -f1))"
}

case "${1:-}" in
    clean)  do_clean ;;
    ""|pack) do_pack ;;
    *)      die "неизвестный аргумент: $1 (используйте clean или без аргументов)" ;;
esac

