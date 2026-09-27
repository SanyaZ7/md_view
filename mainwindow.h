#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "settings.h"

class QCloseEvent;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class EditorWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    Settings settings() const
    {
        return m_settings;
    }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void openFile();
    void closeTab(int index = -1);
    void showSettings();
    void saveCurrentTab();
    void saveAllTabs();
    void onEditorTextChanged();

private:
    void addTab(const QString &filePath);
    void updateStatusBar();
    void applySettingsToAllTabs();
    void updateTabTitle(int index, bool modified);
    bool saveEditor(EditorWidget *editor);

    Ui::MainWindow *ui;
    Settings m_settings;
};

#endif // MAINWINDOW_H

