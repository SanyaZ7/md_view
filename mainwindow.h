#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "settings.h"

class QDragEnterEvent;
class QDropEvent;
class QCloseEvent;
class QDockWidget;
class QAction;

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

    // Открывает файл сразу в режиме рендеринга (для запуска с аргументом-файлом).
    void openFileInRenderMode(const QString &filePath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    void openFile();
    void closeTab(int index = -1);
    void showSettings();
    void saveCurrentTab();
    void saveAllTabs();
    void onEditorTextChanged();
    void undoCurrentTab();
    void redoCurrentTab();
    void updateUndoRedoActions();
    void toggleSearchPanel();

private:
    void addTab(const QString &filePath, bool renderMode = false);
    // Индекс вкладки с этим файлом или -1, если не открыт.
    int findTabForFile(const QString &filePath) const;
    void updateStatusBar();
    void applySettingsToAllTabs();
    void updateTabTitle(int index, bool modified);
    bool saveEditor(EditorWidget *editor);

    Ui::MainWindow *ui;
    Settings m_settings;
    QDockWidget *m_searchDock = nullptr;
    QAction *m_toggleSearchAction = nullptr;
};

#endif // MAINWINDOW_H

