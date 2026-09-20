#ifndef SETTINGS_H
#define SETTINGS_H

#include <QString>

struct Settings
{
    bool lineNumbers = false;
    bool wordWrap    = false;

    bool limitCodeBlockHeight = false;
    int  codeBlockMaxLines    = 20;
};

// Путь к файлу настроек (в папке проекта)
QString settingsFilePath();

// Загрузка / сохранение
Settings loadSettings();
void     saveSettings(const Settings &s);

#endif // SETTINGS_H
