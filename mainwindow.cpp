#include "mainwindow.h"
#include "settingsdialog.h"
#include "editorwidget.h"       // новый заголовок
//#include "./ui_mainwindow.h"
#include "/home/alex/workspace/md_view/build/Release_min_size/md_view_autogen/include/ui_mainwindow.h"

#include <QTabWidget>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QByteArray>
#include <QCloseEvent>

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
	connect(ui->actionSave, &QAction::triggered, this, &MainWindow::saveCurrentTab);
	connect(ui->actionSaveAll, &QAction::triggered, this, &MainWindow::saveAllTabs);
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

void MainWindow::addTab(const QString &filePath)
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

    const QString content =
        QString::fromUtf8(fileData);

    EditorWidget *editor =
        new EditorWidget(this);

    editor->setPlainText(content);
    editor->setFilePath(filePath);
    editor->applySettings(m_settings);

    // Загрузка исходного содержимого не является изменением.
    editor->editor()->document()->setModified(false);

    QFileInfo info(filePath);

    const int index =
        ui->tabWidget->addTab(
            editor,
            info.fileName()
        );

    ui->tabWidget->setCurrentIndex(index);

    connect(editor, &EditorWidget::textChangedFlag,
            this, &MainWindow::onEditorTextChanged);

    updateStatusBar();
}

void MainWindow::closeTab(int index)
{
    if (index < 0) {
        index = ui->tabWidget->currentIndex();
    }

    if (index < 0 || index >= ui->tabWidget->count()) {
        return;
    }

    QWidget *widget = ui->tabWidget->widget(index);
    EditorWidget *editor =
        qobject_cast<EditorWidget *>(widget);

    if (editor &&
        editor->editor()->document()->isModified()) {
        const QMessageBox::StandardButton result =
            QMessageBox::warning(
                this,
                "Несохранённые изменения",
                QString(
                    "Файл \"%1\" был изменён.\n"
                    "Сохранить изменения?"
                ).arg(
                    QFileInfo(editor->filePath()).fileName()
                ),
                QMessageBox::Save |
                QMessageBox::Discard |
                QMessageBox::Cancel,
                QMessageBox::Save
            );

        if (result == QMessageBox::Cancel) {
            return;
        }

        if (result == QMessageBox::Save &&
            !saveEditor(editor)) {
            return;
        }
    }

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

// Сохранение текущей вкладки
void MainWindow::saveCurrentTab()
{
    int index = ui->tabWidget->currentIndex();
    if (index < 0)
        return;

    EditorWidget *editor = qobject_cast<EditorWidget*>(ui->tabWidget->widget(index));
    if (editor) {
        saveEditor(editor);
    }
}

// Сохранение всех вкладок с несохранёнными изменениями
void MainWindow::saveAllTabs()
{
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        EditorWidget *editor = qobject_cast<EditorWidget*>(ui->tabWidget->widget(i));
        if (editor) {
            // Сохраняем только вкладки с несохранёнными изменениями
            QString title = ui->tabWidget->tabText(i);
            if (title.startsWith("*")) {
                saveEditor(editor);
            }
        }
    }
}

// Обработчик изменения текста - обновляет заголовок вкладки
void MainWindow::onEditorTextChanged()
{
    EditorWidget *editor =
        qobject_cast<EditorWidget *>(sender());

    if (!editor) {
        return;
    }

    const int index =
        ui->tabWidget->indexOf(editor);

    if (index < 0) {
        return;
    }

    updateTabTitle(
        index,
        editor->editor()->document()->isModified()
    );
}

// Метод для обновления заголовка вкладки
void MainWindow::updateTabTitle(int index, bool modified)
{
    if (index < 0 || index >= ui->tabWidget->count())
        return;
        
    QString title = ui->tabWidget->tabText(index);
    // Убираем существующую "*"
    if (title.startsWith("*")) {
        title = title.mid(1);
    }
    
    if (modified) {
        title = "*" + title;
    }
    
    ui->tabWidget->setTabText(index, title);
}

// Метод для сохранения содержимого редактора
bool MainWindow::saveEditor(EditorWidget *editor)
{
    if (!editor) {
        return false;
    }

    QString filePath = editor->filePath();

    if (filePath.isEmpty()) {
        filePath = QFileDialog::getSaveFileName(
            this,
            "Сохранить файл",
            QString(),
            "Все файлы (*)"
        );

        if (filePath.isEmpty()) {
            return false;
        }

        editor->setFilePath(filePath);

        QFileInfo info(filePath);
        const int index =
            ui->tabWidget->indexOf(editor);

        if (index >= 0) {
            ui->tabWidget->setTabText(
                index,
                info.fileName()
            );
        }
    }

    const QString content =
        editor->editor()->toPlainText();

    QFile file(filePath);

    if (!file.open(
            QIODevice::WriteOnly |
            QIODevice::Text)) {
        QMessageBox::warning(
            this,
            "Ошибка",
            QString(
                "Не удалось сохранить файл:\n%1"
            ).arg(file.errorString())
        );

        return false;
    }

    QTextStream out(&file);
    out << content;
    file.close();

    // Содержимое сохранено — документ больше не изменён.
    editor->editor()->document()->setModified(false);

    const int index =
        ui->tabWidget->indexOf(editor);

    if (index >= 0) {
        updateTabTitle(index, false);
    }

    updateStatusBar();

    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    while (ui->tabWidget->count() > 0) {
        const int index =
            ui->tabWidget->count() - 1;

        QWidget *widget =
            ui->tabWidget->widget(index);

        EditorWidget *editor =
            qobject_cast<EditorWidget *>(widget);

        if (editor &&
            editor->editor()->document()->isModified()) {
            const QMessageBox::StandardButton result =
                QMessageBox::warning(
                    this,
                    "Несохранённые изменения",
                    QString(
                        "Файл \"%1\" был изменён.\n"
                        "Сохранить изменения?"
                    ).arg(
                        QFileInfo(editor->filePath()).fileName()
                    ),
                    QMessageBox::Save |
                    QMessageBox::Discard |
                    QMessageBox::Cancel,
                    QMessageBox::Save
                );

            if (result == QMessageBox::Cancel) {
                event->ignore();
                return;
            }

            if (result == QMessageBox::Save &&
                !saveEditor(editor)) {
                event->ignore();
                return;
            }
        }

        ui->tabWidget->removeTab(index);
        delete widget;
    }

    event->accept();
}



