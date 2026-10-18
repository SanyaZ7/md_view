#include "mainwindow.h"
#include "settingsdialog.h"
#include "editorwidget.h"       // новый заголовок
#include "highlight.h"
#include "ui_mainwindow.h"

#include <QTabWidget>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QMessageBox>
#include <QByteArray>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QTextDocument>
#include <QTextCursor>

namespace {

/*
 * Категория файла по его имени для обработки перетаскивания.
 * Расширения, которые распознаёт подсветка (tree-sitter), считаются
 * текстовыми; .md — рендеринг; .txt — текст; всё остальное — неизвестное.
 */
enum class DropKind {
    Markdown,
    PlainText,
    Unknown
};

DropKind classifyDroppedFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QString suffix = info.suffix().toLower();

    if (suffix == "md" || suffix == "markdown") {
        return DropKind::Markdown;
    }

    if (suffix == "txt") {
        return DropKind::PlainText;
    }

    // Файлы, для которых есть подсветка (Makefile, CMakeLists.txt и т.д.).
    if (highlight::language_from_file_name(
            info.fileName().toStdString()) != highlight::Language::None) {
        return DropKind::PlainText;
    }

    return DropKind::Unknown;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->tabWidget->setTabsClosable(true);
    ui->tabWidget->setMovable(true);
    m_settings = loadSettings();

    setAcceptDrops(true);

    connect(ui->tabWidget, &QTabWidget::tabCloseRequested,
            this, &MainWindow::closeTab);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::updateStatusBar);
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, &MainWindow::updateUndoRedoActions);

    connect(ui->actionUndo, &QAction::triggered,
            this, &MainWindow::undoCurrentTab);
    connect(ui->actionRedo, &QAction::triggered,
            this, &MainWindow::redoCurrentTab);

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

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasUrls()) {
        return;
    }

    for (const QUrl &url : event->mimeData()->urls()) {
        QString filePath = url.toLocalFile();
        if (filePath.isEmpty()) {
            continue;
        }

        DropKind kind = classifyDroppedFile(filePath);

        if (kind == DropKind::Unknown) {
            auto answer = QMessageBox::question(
                this,
                "Неизвестный файл",
                QString(
                    "Не удаётся определить тип файла «%1».\n"
                    "Всё равно открыть?"
                ).arg(QFileInfo(filePath).fileName()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
            );
            if (answer != QMessageBox::Yes) {
                continue;
            }
            addTab(filePath, false);
        } else {
            addTab(filePath, kind == DropKind::Markdown);
        }
    }

    event->acceptProposedAction();
}

int MainWindow::findTabForFile(const QString &filePath) const
{
    const QString canonical =
        QFileInfo(filePath).canonicalFilePath();

    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        EditorWidget *editor =
            qobject_cast<EditorWidget*>(
                ui->tabWidget->widget(i));

        if (editor &&
            QFileInfo(editor->filePath())
                .canonicalFilePath() == canonical) {
            return i;
        }
    }

    return -1;
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

void MainWindow::openFileInRenderMode(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return;
    }

    // Файл уже открыт — просто переключаемся на него в режиме рендеринга.
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        EditorWidget *widget = qobject_cast<EditorWidget*>(ui->tabWidget->widget(i));
        if (widget && widget->filePath() == filePath) {
            ui->tabWidget->setCurrentIndex(i);
            widget->setRenderMode(true);
            return;
        }
    }

    addTab(filePath, true);
}

void MainWindow::addTab(const QString &filePath, bool renderMode)
{
    /*
     * Если файл уже открыт — переключаемся на вкладку.
     * При этом синхронизируем режим: если файл был открыт в raw,
     * а запросили render — переключаем.
     */
    int existing = findTabForFile(filePath);
    if (existing >= 0) {
        EditorWidget *editor =
            qobject_cast<EditorWidget*>(
                ui->tabWidget->widget(existing));
        if (editor) {
            if (renderMode && !editor->renderMode()) {
                editor->setRenderMode(true);
            }
            ui->tabWidget->setCurrentIndex(existing);
            return;
        }
    }

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

    if (renderMode) {
        editor->setRenderMode(true);
    }

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
    connect(editor->editor()->document(), &QTextDocument::undoAvailable,
            this, &MainWindow::updateUndoRedoActions);
    connect(editor->editor()->document(), &QTextDocument::redoAvailable,
            this, &MainWindow::updateUndoRedoActions);

    updateStatusBar();
    updateUndoRedoActions();
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
    updateUndoRedoActions();
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
    updateUndoRedoActions();
}

void MainWindow::undoCurrentTab()
{
    EditorWidget *editor = qobject_cast<EditorWidget *>(
        ui->tabWidget->currentWidget());
    if (editor) {
        editor->editor()->undo();
    }
}

void MainWindow::redoCurrentTab()
{
    EditorWidget *editor = qobject_cast<EditorWidget *>(
        ui->tabWidget->currentWidget());
    if (editor) {
        editor->editor()->redo();
    }
}

void MainWindow::updateUndoRedoActions()
{
    EditorWidget *editor = qobject_cast<EditorWidget *>(
        ui->tabWidget->currentWidget());
    const QTextDocument *document = editor ? editor->editor()->document() : nullptr;
    const int undoSteps = document ? document->availableUndoSteps() : 0;
    const int redoSteps = document ? document->availableRedoSteps() : 0;

    ui->actionUndo->setText(QString::fromUtf8("↶ %1").arg(undoSteps));
    ui->actionRedo->setText(QString::fromUtf8("↷ %1").arg(redoSteps));
    ui->actionUndo->setEnabled(undoSteps > 0);
    ui->actionRedo->setEnabled(redoSteps > 0);
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



