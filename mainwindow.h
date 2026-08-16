#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    Settings settings() const { return m_settings; }

private slots:
    void openFile();
    void closeTab();
    void showSettings();

private:
    void addTab(const QString &filePath);
    void updateStatusBar();

    Ui::MainWindow *ui;
    Settings m_settings;
};

#endif // MAINWINDOW_H
