#!/bin/bash

# Автоматическое определение пути к проекту (работает из любой директории)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$SCRIPT_DIR"

# Извлечение имени директории сборки (Release_min_size, Release или другое)
BUILD_TYPE_NAME=$(basename "$BUILD_DIR")

# Пути к файлам
BINARY_PATH="$BUILD_DIR/md_view"
DESKTOP_FILE="$HOME/.local/share/applications/mdview.desktop"
DESKTOP_DIR="$HOME/.local/share/applications"

# Проверка существования бинарника
if [ ! -f "$BINARY_PATH" ]; then
    echo "Ошибка: Бинарник не найден по пути $BINARY_PATH"
    exit 1
fi

# Создание .desktop файла
mkdir -p "$DESKTOP_DIR"

cat > "$DESKTOP_FILE" << EOF
[Desktop Entry]
Name=MDView
Exec=$BINARY_PATH %f
Icon=application-default-icon
Terminal=false
Type=Application
Categories=Utility;TextEditor;
MimeType=text/markdown;text/x-markdown;application/x-markdown;
NoDisplay=false
EOF

# Установка прав доступа
chmod +x "$DESKTOP_FILE"

# Обновление базы приложений
update-desktop-database "$DESKTOP_DIR" 2>/dev/null || true

# Установка привязки к файлам
xdg-mime default mdview.desktop text/markdown 2>/dev/null || true

echo "✓ Установка завершена"
echo "  Проект: $PROJECT_ROOT"
echo "  Сборка: $BUILD_TYPE_NAME"
echo "  Бинарник: $BINARY_PATH"

