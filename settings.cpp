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

    s.limitCodeBlockHeight = ini.value("view/limitCodeBlockHeight", false).toBool();
    s.codeBlockMaxLines    = qMax(8, ini.value("view/codeBlockMaxLines", 20).toInt());

    s.dropFilesAsPaths   = ini.value("view/dropFilesAsPaths", false).toBool();
    s.tabStopWidth       = qBound(1, ini.value("view/tabStopWidth", 4).toInt(), 16);
    return s;
}

void saveSettings(const Settings &s)
{
    QSettings ini(settingsFilePath(), QSettings::IniFormat);

    ini.setValue("view/lineNumbers", s.lineNumbers);
    ini.setValue("view/wordWrap",    s.wordWrap);

    ini.setValue("view/limitCodeBlockHeight", s.limitCodeBlockHeight);
    ini.setValue("view/codeBlockMaxLines",    qMax(8, s.codeBlockMaxLines));

    ini.setValue("view/dropFilesAsPaths", s.dropFilesAsPaths);
    ini.setValue("view/tabStopWidth", qBound(1, s.tabStopWidth, 16));
    ini.sync();
}
