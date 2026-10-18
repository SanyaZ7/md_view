#include "mainwindow.h"

#include <QApplication>
#include <QFileInfo>
#include <QUrl>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    // Обрабатываем аргументы командной строки (запуск из файлового менеджера)
    QStringList args = QCoreApplication::arguments();
    if (args.size() > 1) {
        QString filePath = args.at(1);
        // Обработка file:// URL
        if (filePath.startsWith("file://")) {
            filePath = QUrl(filePath).toLocalFile();
        }
        w.openFileInRenderMode(filePath);
    }

    w.show();
    return a.exec();
}
