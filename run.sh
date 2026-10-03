#!/usr/bin/env bash
#
# run.sh — обёртка для запуска md_view из корня проекта.
#
# Использование:
#   ./run.sh                       # сборка Release_min_size (по умолчанию)
#   ./run.sh --build Debug         # другая сборка
#   ./run.sh --list-builds         # показать доступные сборки
#   ./run.sh --force-software      # принудительно программный рендер
#   ./run.sh --no-check            # пропустить проверку пакетов
#   ./run.sh -- --file README.md   # передать аргументы в md_view
#

set -euo pipefail

# ════════════════════════════════════════════════════════════════════
#  НАСТРОЙКИ — правьте здесь
# ════════════════════════════════════════════════════════════════════

# Корень проекта (каталог, где лежат build/ и md_lexer_v2/).
# Пусто = взять каталог, где лежит сам run.sh.
PROJECT_ROOT="${PROJECT_ROOT:-}"

# Тип сборки по умолчанию. Доступные варианты см. в build/.
# Можно переопределить:  BUILD_TYPE=Debug ./run.sh
#                   или:  ./run.sh --build Debug
BUILD_TYPE="${BUILD_TYPE:-Release_min_size}"

# Имя бинарника внутри build/<BUILD_TYPE>/
BINARY_NAME="${BINARY_NAME:-md_view}"

# Каталог с библиотеками (относительно корня проекта).
# Можно переопределить:  LEXER_DIR=/opt/lib ./run.sh
LEXER_DIR="${LEXER_DIR:-md_lexer_v2}"

# ════════════════════════════════════════════════════════════════════
#  Конец настроек — дальше лучше ничего не трогать
# ════════════════════════════════════════════════════════════════════

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
PROJECT_ROOT="${PROJECT_ROOT:-$SCRIPT_DIR}"

# ---------- Разбор аргументов ----------
FORCE_SOFTWARE=0
SKIP_CHECK=0
LIST_BUILDS=0
PASSTHRU=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)          BUILD_TYPE="$2"; shift 2 ;;
        --build=*)        BUILD_TYPE="${1#*=}"; shift ;;
        --list-builds)    LIST_BUILDS=1; shift ;;
        --force-software) FORCE_SOFTWARE=1; shift ;;
        --no-check)       SKIP_CHECK=1; shift ;;
        --)               shift; PASSTHRU+=("$@"); break ;;
        *)                PASSTHRU+=("$1"); shift ;;
    esac
done

# ---------- Пути (зависят от BUILD_TYPE) ----------
BUILD_DIR="$PROJECT_ROOT/build/$BUILD_TYPE"
BINARY="$BUILD_DIR/$BINARY_NAME"
LEXER_PATH="$PROJECT_ROOT/$LEXER_DIR"

# ---------- Цветной вывод ----------
if [[ -t 1 ]]; then
    C_RED=$'\033[31m'; C_GRN=$'\033[32m'; C_YLW=$'\033[33m'
    C_BLU=$'\033[34m'; C_RST=$'\033[0m'
else
    C_RED=; C_GRN=; C_YLW=; C_BLU=; C_RST=
fi
info()  { printf '%s[run]%s %s\n' "$C_BLU" "$C_RST" "$*"; }
ok()    { printf '%s[ok ]%s %s\n' "$C_GRN" "$C_RST" "$*"; }
warn()  { printf '%s[warn]%s %s\n' "$C_YLW" "$C_RST" "$*" >&2; }
die()   { printf '%s[fail]%s %s\n' "$C_RED" "$C_RST" "$*" >&2; exit 1; }

# ---------- Список доступных сборок ----------
list_builds() {
    local build_root="$PROJECT_ROOT/build"
    if [[ ! -d "$build_root" ]]; then
        die "каталог сборок не найден: $build_root"
    fi
    printf '%sДоступные сборки в %s:%s\n' "$C_BLU" "$build_root" "$C_RST"
    local found=0
    while IFS= read -r -d '' dir; do
        local name; name="$(basename "$dir")"
        local bin="$dir/$BINARY_NAME"
        if [[ -x "$bin" ]]; then
            printf '  %s✔%s %-24s  (%s)\n' "$C_GRN" "$C_RST" "$name" "$bin"
            found=1
        else
            printf '  %s·%s %-24s  (бинарник отсутствует)\n' "$C_YLW" "$C_RST" "$name"
        fi
    done < <(find "$build_root" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)
    (( found )) || warn "ни в одной сборке не найден $BINARY_NAME"
}

# ---------- Проверка пакетов ----------
REQUIRED_PKGS=(
    "libgl1-mesa-dri|/usr/lib/x86_64-linux-gnu/dri/swrast_dri.so"
    "mesa-utils|glxinfo"
    "fonts-noto-color-emoji|/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf"
    "fonts-noto-cjk|/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
    "qt6-base-dev|/usr/lib/x86_64-linux-gnu/libQt6Core.so.6"
    "libqt6widgets6|/usr/lib/x86_64-linux-gnu/libQt6Widgets.so.6"
)

check_packages() {
    local missing=()
    for entry in "${REQUIRED_PKGS[@]}"; do
        local pkg="${entry%%|*}"
        local probe="${entry##*|}"
        if [[ "$probe" == /* ]]; then
            [[ -e "$probe" ]] || missing+=("$pkg")
        else
            command -v "$probe" >/dev/null 2>&1 || missing+=("$pkg")
        fi
    done

    if (( ${#missing[@]} == 0 )); then
        ok "все необходимые пакеты установлены"
        return 0
    fi

    warn "не найдены пакеты: ${missing[*]}"
    if [[ -t 0 ]]; then
        read -r -p "Установить сейчас через apt? [y/N] " ans
        if [[ "$ans" =~ ^[Yy]$ ]]; then
            info "запускаю: apt-get install -y ${missing[*]}"
            if command -v sudo >/dev/null 2>&1 && [[ $EUID -ne 0 ]]; then
                sudo apt-get update
                sudo apt-get install -y "${missing[@]}"
            else
                apt-get update
                apt-get install -y "${missing[@]}"
            fi
            ok "пакеты установлены"
        else
            warn "продолжаю без установки — возможны ошибки"
        fi
    else
        warn "неинтерактивный режим — пропускаю установку"
    fi
}

# ---------- Проверка путей ----------
check_paths() {
    if [[ ! -d "$BUILD_DIR" ]]; then
        warn "каталог сборки не найден: $BUILD_DIR"
        list_builds
        die "выберите сборку через --build <тип>"
    fi
    [[ -x "$BINARY" ]] || {
        warn "бинарник не найден: $BINARY"
        list_builds
        die "соберите проект или выберите другую сборку"
    }
    ok "сборка:    $BUILD_TYPE"
    ok "бинарник:  $BINARY"

    [[ -d "$LEXER_PATH" ]] || die "каталог библиотек не найден: $LEXER_PATH"
    ok "библиотеки: $LEXER_PATH"
}

# ---------- Автодетект окружения ----------
detect_env() {
    local has_gpu=0
    if [[ -d /dev/dri ]] && compgen -G "/dev/dri/*" >/dev/null; then
        has_gpu=1
    fi

    local qpa=""
    if [[ -n "${WAYLAND_DISPLAY:-}" && -n "${XDG_RUNTIME_DIR:-}" ]]; then
        qpa="wayland"
    elif [[ -n "${DISPLAY:-}" ]]; then
        qpa="xcb"
    else
        warn "не найден ни WAYLAND_DISPLAY, ни DISPLAY — окно может не открыться"
    fi

    echo "gpu=$has_gpu qpa=$qpa"
}

# ---------- Сборка окружения ----------
build_env() {
    local has_gpu="$1" qpa="$2"

    ENV_VARS=()
    [[ -n "$qpa" ]] && ENV_VARS+=("QT_QPA_PLATFORM=$qpa")

    if (( FORCE_SOFTWARE )) || (( ! has_gpu )); then
        info "аппаратный GPU не найден (или --force-software) — включаю программный рендер"
        ENV_VARS+=(
            "QT_QUICK_BACKEND=software"
            "QSG_RHI_BACKEND=software"
            "QT_OPENGL=software"
            "LIBGL_ALWAYS_SOFTWARE=1"
        )
    else
        info "GPU обнаружен — использую аппаратный рендер"
    fi

    ENV_VARS+=("MESA_LOADER_DRIVER_OVERRIDE=llvmpipe")
    ENV_VARS+=("LD_LIBRARY_PATH=${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}$LEXER_PATH")

    if [[ "${QT_DEBUG_QPA:-0}" == "1" ]]; then
        ENV_VARS+=("QT_LOGGING_RULES=qt.qpa.*=true")
    fi
}

# ---------- main ----------
main() {
    if (( LIST_BUILDS )); then
        list_builds
        exit 0
    fi

    info "корень проекта: $PROJECT_ROOT"

    (( SKIP_CHECK )) || check_packages
    check_paths

    local detected; detected="$(detect_env)"
    local has_gpu="${detected#gpu=}"; has_gpu="${has_gpu%% *}"
    local qpa="${detected##*qpa=}"

    build_env "$has_gpu" "$qpa"

    info "переменные окружения:"
    for v in "${ENV_VARS[@]}"; do
        printf '       %s\n' "$v"
    done

    info "запуск: $BINARY ${PASSTHRU[*]:-}"
    echo

    exec env "${ENV_VARS[@]}" "$BINARY" "${PASSTHRU[@]}"
}

main "$@"