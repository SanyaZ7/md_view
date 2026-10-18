#include "SearchPanel.h"
#include "ui_SearchPanel.h"

#include <QColor>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QSet>
#include <QTextDocument>
#include <QTextEdit>
#include <QToolButton>

namespace {

constexpr int MatchIndexRole = Qt::UserRole;
constexpr int MaxResults = 64;

int meaningfulCharacterCount(const QString &text)
{
    int count = 0;
    for (const QChar character : text) {
        if (!character.isSpace()) {
            ++count;
        }
    }
    return count;
}

// Уровни поиска идут от строгого к самому мягкому, каждый следующий
// включает правила предыдущих.
constexpr int LevelExact = 0;
constexpr int LevelCaseFolded = 1;
constexpr int LevelWhitespace = 2;
constexpr int LevelLookalike = 3;
constexpr int LevelDash = 4;
constexpr int LevelAnyDash = 5;
constexpr int LevelCount = 6;

bool isTire(QChar character)
{
    // Длинное (—) и короткое (–) тире.
    const ushort value = character.unicode();
    return value == 0x2013 || value == 0x2014;
}

bool isHyphen(QChar character)
{
    // Обычный дефис и типографские дефисы.
    const ushort value = character.unicode();
    return value == 0x002D || value == 0x2010 || value == 0x2011;
}

bool isDashSpace(QChar character)
{
    return isTire(character) || isHyphen(character);
}

QChar normalizedCharacter(QChar character, int level)
{
    const QChar folded = level >= LevelCaseFolded
        ? character.toCaseFolded() : character;

    // Уровень 5 приравнивает к пробелу тире, уровень 6 — ещё и дефис.
    // Схлопывание в общую пробельную группу делает только уровень 6.
    if (level >= LevelAnyDash && isDashSpace(folded)) {
        return QLatin1Char(' ');
    }
    if (level >= LevelDash && isTire(folded)) {
        return QLatin1Char(' ');
    }
    if (level < LevelLookalike) {
        return folded;
    }

    switch (folded.unicode()) {
    case 0x0430: return QLatin1Char('a'); // а
    case 0x0432: return QLatin1Char('b'); // в
    case 0x0435: return QLatin1Char('e'); // е
    case 0x0451: return QLatin1Char('e'); // ё
    case 0x043A: return QLatin1Char('k'); // к
    case 0x043C: return QLatin1Char('m'); // м
    case 0x043D: return QLatin1Char('h'); // н
    case 0x043E: return QLatin1Char('o'); // о
    case 0x0440: return QLatin1Char('p'); // р
    case 0x0441: return QLatin1Char('c'); // с
    case 0x0442: return QLatin1Char('t'); // т
    case 0x0443: return QLatin1Char('y'); // у
    case 0x0445: return QLatin1Char('x'); // х
    case 0x0456: return QLatin1Char('i'); // і
    case 0x0458: return QLatin1Char('j'); // ј
    case 0x0455: return QLatin1Char('s'); // ѕ
    case 0x04CF: return QLatin1Char('l'); // ӏ
    default: return folded;
    }
}

struct NormalizedText {
    QString text;
    QVector<int> starts;
    QVector<int> ends;
};

bool isCollapsibleSpace(QChar character, int level)
{
    // Уровни 3–5 схлопывают только пробельные символы. На уровне 6 в ту же
    // группу входят тире и дефисы.
    return character.isSpace() ||
           (level >= LevelAnyDash && isDashSpace(character));
}

NormalizedText normalize(const QString &source, int level, bool trimEdges = false)
{
    NormalizedText result;
    const bool collapseSpaces = level >= LevelWhitespace;
    int i = 0;
    int limit = source.size();
    // Ведущие и замыкающие пробелы необязательны: слово, скопированное
    // с пробелом на конце, должно находиться и без него. На старших уровнях
    // крайние тире и дефисы ведут себя так же. Запрос, состоящий только
    // из таких символов, не обрезаем, иначе искать будет нечего.
    if (trimEdges && collapseSpaces) {
        while (i < limit && isCollapsibleSpace(source.at(i), level)) {
            ++i;
        }
        while (limit > i && isCollapsibleSpace(source.at(limit - 1), level)) {
            --limit;
        }
        if (i >= limit) {
            i = 0;
            limit = source.size();
        }
    }
    while (i < limit) {
        const QChar current = source.at(i);
        if (collapseSpaces && isCollapsibleSpace(current, level)) {
            // Группа пробельных символов (а на старших уровнях — ещё и тире
            // с дефисами) схлопывается в один условный пробел.
            const int runStart = i;
            while (i < limit && isCollapsibleSpace(source.at(i), level)) {
                ++i;
            }
            result.text.append(QLatin1Char(' '));
            result.starts.append(runStart);
            result.ends.append(i);
            continue;
        }

        const int start = i;
        int end = i + 1;
        if (current == QLatin1Char('\r') && i + 1 < limit &&
            source.at(i + 1) == QLatin1Char('\n')) {
            end = i + 2;
            ++i;
        }
        result.text.append(normalizedCharacter(current, level));
        result.starts.append(start);
        result.ends.append(end);
        ++i;
    }
    return result;
}

QString matchPreview(const QString &text, int start, int end)
{
    const int lineStart = qMax(text.lastIndexOf(QLatin1Char('\n'), start),
                               text.lastIndexOf(QLatin1Char('\r'), start)) + 1;
    int lineEnd = text.indexOf(QLatin1Char('\n'), end);
    const int crEnd = text.indexOf(QLatin1Char('\r'), end);
    if (lineEnd < 0 || (crEnd >= 0 && crEnd < lineEnd)) {
        lineEnd = crEnd;
    }
    if (lineEnd < 0) {
        lineEnd = text.size();
    }
    QString preview = text.mid(lineStart, lineEnd - lineStart).simplified();
    if (preview.size() > 100) {
        preview = preview.left(97) + QStringLiteral("…");
    }
    return preview;
}

} // namespace

SearchPanel::SearchPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SearchPanel)
{
    ui->setupUi(this);
    connect(ui->searchInput, &QPlainTextEdit::textChanged,
            this, &SearchPanel::updateMatches);
    connect(ui->nextButton, &QToolButton::clicked,
            this, &SearchPanel::selectNext);
    connect(ui->previousButton, &QToolButton::clicked,
            this, &SearchPanel::selectPrevious);
    connect(ui->resultsList, &QListWidget::currentRowChanged,
            this, &SearchPanel::selectResult);
}

SearchPanel::~SearchPanel()
{
    delete ui;
}

void SearchPanel::setEditor(QPlainTextEdit *editor)
{
    if (m_editor == editor) {
        return;
    }
    if (m_editor) {
        disconnect(m_editor->document(), nullptr, this, nullptr);
        m_editor->setExtraSelections({});
    }
    m_editor = editor;
    if (m_editor) {
        connect(m_editor->document(), &QTextDocument::contentsChanged,
                this, &SearchPanel::updateMatches);
    }
    refreshMatches(false);
}

void SearchPanel::focusInput()
{
    ui->searchInput->setFocus();
    ui->searchInput->selectAll();
}

void SearchPanel::updateMatches()
{
    refreshMatches(true);
}

void SearchPanel::refreshMatches(bool preserveSelection)
{
    const int oldStart = preserveSelection && m_currentMatch >= 0 &&
                         m_currentMatch < m_matches.size()
        ? m_matches.at(m_currentMatch).cursor.selectionStart() : -1;
    m_matches.clear();
    m_currentMatch = -1;
    ui->resultsList->clear();

    const QString query = ui->searchInput->toPlainText();
    const QString source = m_editor ? m_editor->toPlainText() : QString();
    int exactCount = 0;
    bool truncated = false;
    // Пустой запрос или запрос из одних пробелов не ищем.
    const int maxResults = meaningfulCharacterCount(query) < 3 ? MaxResults : -1;
    if (meaningfulCharacterCount(query) > 0 && !source.isEmpty()) {
        // Начиная с уровня эквивалентных пробелов крайние пробелы запроса
        // необязательны: слово, скопированное с пробелом на конце, должно
        // находиться и без него.
        const NormalizedText queries[LevelCount] = {
            normalize(query, LevelExact),
            normalize(query, LevelCaseFolded),
            normalize(query, LevelWhitespace, true),
            normalize(query, LevelLookalike, true),
            normalize(query, LevelDash, true),
            normalize(query, LevelAnyDash, true)
        };
        const QString levelNames[LevelCount] = {
            QStringLiteral("Точные совпадения"),
            QStringLiteral("Без учёта регистра"),
            QStringLiteral("Эквивалентные пробелы"),
            QStringLiteral("Похожие кириллические и латинские буквы"),
            QStringLiteral("Пробелы и длинное/короткое тире"),
            QStringLiteral("Группы пробелов и тире")
        };

        // Поиск ведётся на самом мягком уровне: он включает правила всех
        // предыдущих и потому находит все совпадения. Дальше каждое найденное
        // место относится к самому строгому уровню, которому оно удовлетворяет,
        // чтобы группа 6 не перетягивала к себе совпадения из групп 3–5.
        const int searchLevel = LevelCount - 1;
        const QString needle = queries[searchLevel].text;
        const NormalizedText document = normalize(source, searchLevel);

        QVector<QPair<int, int>> ranges;
        QSet<QPair<int, int>> seenRanges;
        if (!needle.isEmpty()) {
            int position = 0;
            while ((position = document.text.indexOf(needle, position)) >= 0) {
                const int start = document.starts.at(position);
                const int end = document.ends.at(position + needle.size() - 1);
                ++position;
                const QPair<int, int> range = qMakePair(start, end);
                if (seenRanges.contains(range)) {
                    continue;
                }
                if (maxResults >= 0 && ranges.size() >= maxResults) {
                    truncated = true;
                    break;
                }
                seenRanges.insert(range);
                ranges.append(range);
            }
        }

        QVector<QVector<QPair<int, int>>> byLevel(LevelCount);
        for (const QPair<int, int> &range : ranges) {
            const QString fragment = source.mid(range.first, range.second - range.first);
            // Совпадение попадает в самый строгий уровень, которому оно
            // удовлетворяет; более мягкие уровни его уже не перехватывают.
            int matchLevel = searchLevel;
            for (int level = 0; level < LevelCount; ++level) {
                const bool trim = level >= LevelWhitespace;
                if (normalize(fragment, level, trim).text == queries[level].text) {
                    matchLevel = level;
                    break;
                }
            }
            byLevel[matchLevel].append(range);
        }

        for (int level = 0; level < LevelCount; ++level) {
            if (byLevel[level].isEmpty()) {
                continue;
            }
            QListWidgetItem *header = new QListWidgetItem(levelNames[level]);
            header->setFlags(Qt::NoItemFlags);
            ui->resultsList->addItem(header);

            for (const QPair<int, int> &range : byLevel[level]) {
                QTextCursor cursor(m_editor->document());
                cursor.setPosition(range.first);
                cursor.setPosition(range.second, QTextCursor::KeepAnchor);
                m_matches.append({cursor, level});

                QListWidgetItem *item = new QListWidgetItem(
                    matchPreview(source, range.first, range.second));
                item->setData(MatchIndexRole, m_matches.size() - 1);
                ui->resultsList->addItem(item);

                if (level == LevelExact) {
                    ++exactCount;
                }
            }
        }
    }

    if (!m_matches.isEmpty()) {
        m_currentMatch = 0;
        if (oldStart >= 0) {
            for (int i = 0; i < m_matches.size(); ++i) {
                if (m_matches.at(i).cursor.selectionStart() >= oldStart) {
                    m_currentMatch = i;
                    break;
                }
            }
        }
    }

    ui->matchCount->setText(QStringLiteral("Всего: %1 (точных: %2, примерных: %3)%4")
        .arg(m_matches.size()).arg(exactCount).arg(m_matches.size() - exactCount)
        .arg(truncated ? QStringLiteral(", показаны первые %1").arg(MaxResults)
                       : QString()));
    if (!m_matches.isEmpty()) {
        for (int row = 0; row < ui->resultsList->count(); ++row) {
            if (ui->resultsList->item(row)->data(MatchIndexRole).toInt() == m_currentMatch &&
                ui->resultsList->item(row)->data(MatchIndexRole).isValid()) {
                ui->resultsList->setCurrentRow(row);
                break;
            }
        }
    }
    updateSelection();
}

void SearchPanel::updateSelection()
{
    if (!m_editor) {
        return;
    }
    QList<QTextEdit::ExtraSelection> selections;
    for (int i = 0; i < m_matches.size(); ++i) {
        QTextEdit::ExtraSelection selection;
        selection.cursor = m_matches.at(i).cursor;
        selection.format.setBackground(i == m_currentMatch
            ? QColor(255, 170, 50) : QColor(255, 240, 130));
        selections.append(selection);
    }
    m_editor->setExtraSelections(selections);
    if (m_currentMatch >= 0 && m_currentMatch < m_matches.size()) {
        m_editor->setTextCursor(m_matches.at(m_currentMatch).cursor);
        m_editor->ensureCursorVisible();
    }
}

void SearchPanel::selectNext()
{
    if (m_matches.isEmpty()) return;
    m_currentMatch = (m_currentMatch + 1) % m_matches.size();
    for (int row = 0; row < ui->resultsList->count(); ++row) {
        if (ui->resultsList->item(row)->data(MatchIndexRole).toInt() == m_currentMatch &&
            ui->resultsList->item(row)->data(MatchIndexRole).isValid()) {
            ui->resultsList->setCurrentRow(row);
            break;
        }
    }
    updateSelection();
}

void SearchPanel::selectPrevious()
{
    if (m_matches.isEmpty()) return;
    m_currentMatch = (m_currentMatch - 1 + m_matches.size()) % m_matches.size();
    for (int row = 0; row < ui->resultsList->count(); ++row) {
        if (ui->resultsList->item(row)->data(MatchIndexRole).toInt() == m_currentMatch &&
            ui->resultsList->item(row)->data(MatchIndexRole).isValid()) {
            ui->resultsList->setCurrentRow(row);
            break;
        }
    }
    updateSelection();
}

void SearchPanel::selectResult(int row)
{
    if (row < 0 || row >= ui->resultsList->count()) return;
    const QVariant matchIndex = ui->resultsList->item(row)->data(MatchIndexRole);
    if (!matchIndex.isValid()) return;
    m_currentMatch = matchIndex.toInt();
    updateSelection();
}

