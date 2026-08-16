#include "settings.h"

#include <QSettings>
#include <QCoreApplication>
#include <QDir>

// Файл лежит рядом с исполняемым файлом (папка проекта / build)
QString settingsFilePath()
{
    QString dir = QCoreApplication::applicationDirPath();
    return QDir(dir).filePath("settings.ini");
}

Settings loadSettings()
{
    QSettings ini(settingsFilePath(), QSettings::IniFormat);

    Settings s;
    s.lineNumbers = ini.value("view/lineNumbers", false).toBool();
    s.wordWrap    = ini.value("view/wordWrap",    false).toBool();
    return s;
}

void saveSettings(const Settings &s)
{
    QSettings ini(settingsFilePath(), QSettings::IniFormat);

    ini.setValue("view/lineNumbers", s.lineNumbers);
    ini.setValue("view/wordWrap",    s.wordWrap);
    ini.sync();
}
