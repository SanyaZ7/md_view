#include "mainwindow.h"
#include "settingsdialog.h"
#include "./ui_mainwindow.h"

#include <QTabWidget>
#include <QTextEdit>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setMovable(true);
    m_settings = loadSettings();   // ← загрузить при старте
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

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        QTextEdit *editor = qobject_cast<QTextEdit*>(ui->tabWidget->widget(i));
        if (editor && editor->property("filePath").toString() == filePath) {
            ui->tabWidget->setCurrentIndex(i);
            return;
        }
    }

    addTab(filePath);
}

void MainWindow::addTab(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка",
            QString("Не удалось открыть файл:\n%1").arg(file.errorString()));
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    QTextEdit *editor = new QTextEdit(this);
    editor->setPlainText(content);
    editor->setProperty("filePath", filePath);

    QFileInfo info(filePath);
    int index = ui->tabWidget->addTab(editor, info.fileName());
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
        // TODO: применить настройки к открытым вкладкам
    }
}

void MainWindow::updateStatusBar()
{
    int index = ui->tabWidget->currentIndex();
    if (index < 0) {
        ui->statusbar->showMessage("Нет открытых файлов");
        return;
    }

    QTextEdit *editor = qobject_cast<QTextEdit*>(ui->tabWidget->widget(index));
    if (editor) {
        ui->statusbar->showMessage(editor->property("filePath").toString());
    }
}
