#include "mainwindow.h"
#include "settingsdialog.h"
#include "editorwidget.h"       // новый заголовок
#include "./ui_mainwindow.h"

#include <QTabWidget>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QByteArray>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setMovable(true);
    m_settings = loadSettings();

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::closeTab);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::updateStatusBar);

    connect(ui->actionOpen, &QAction::triggered,
            this, &MainWindow::openFile);
    connect(ui->actionClose, &QAction::triggered,
            this, &MainWindow::closeTab);
    connect(ui->actionSettings, &QAction::triggered,
            this, &MainWindow::showSettings);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this, "Открыть файл", QString(), "Все файлы (*)");
    if (filePath.isEmpty())
        return;

    // Проверяем, не открыт ли уже файл
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        EditorWidget *widget = qobject_cast<EditorWidget*>(ui->tabWidget->widget(i));
        if (widget && widget->filePath() == filePath) {
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    addTab(filePath);
}

void MainWindow::addTab(
    const QString &filePath
)
{
    QFile file(filePath);

    if (!file.open(
            QIODevice::ReadOnly |
            QIODevice::Text)) {
        QMessageBox::warning(
            this,
            "Ошибка",
            QString(
                "Не удалось открыть файл:\n%1"
            ).arg(file.errorString())
        );

        return;
    }

    const QByteArray fileData =
        file.readAll();

    file.close();

    /*
     * Markdown-файл трактуется как UTF-8.
     * Это корректно обрабатывает кириллицу,
     * emoji и другие Unicode-символы.
     */
    const QString content =
        QString::fromUtf8(fileData);

    EditorWidget *editor =
        new EditorWidget(this);

    editor->setPlainText(content);
    editor->setFilePath(filePath);
    editor->applySettings(m_settings);

    QFileInfo info(filePath);

    const int index =
        ui->tabWidget->addTab(
            editor,
            info.fileName()
        );

    ui->tabWidget->setCurrentIndex(index);

    updateStatusBar();
}


void MainWindow::closeTab()
{
    int index = ui->tabWidget->currentIndex();
    if (index < 0)
        return;

    QWidget *widget = ui->tabWidget->widget(index);
    ui->tabWidget->removeTab(index);
    delete widget;

    updateStatusBar();
}

void MainWindow::showSettings()
{
    SettingsDialog dlg(m_settings, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_settings = dlg.settings();
        saveSettings(m_settings);
        applySettingsToAllTabs();   // применяем настройки немедленно
    }
}

void MainWindow::applySettingsToAllTabs()
{
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(ui->tabWidget->widget(i));
        if (editor) {
            editor->applySettings(m_settings);
        }
    }
}

void MainWindow::updateStatusBar()
{
    int index = ui->tabWidget->currentIndex();
    if (index < 0) {
        ui->statusbar->showMessage("Нет открытых файлов");
        return;
    }

    EditorWidget *editor = qobject_cast<EditorWidget*>(ui->tabWidget->widget(index));
    if (editor) {
        ui->statusbar->showMessage(editor->filePath());
    }
}

