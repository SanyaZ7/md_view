#ifndef SEARCHPANEL_H
#define SEARCHPANEL_H

#include <QWidget>
#include <QVector>
#include <QTextCursor>

class QPlainTextEdit;

namespace Ui {
class SearchPanel;
}

class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(QWidget *parent = nullptr);
    ~SearchPanel();

    void setEditor(QPlainTextEdit *editor);
    void focusInput();

private slots:
    void updateMatches();
    void selectNext();
    void selectPrevious();
    void selectResult(int row);

private:
    struct Match {
        QTextCursor cursor;
        int level;
    };

    void refreshMatches(bool preserveSelection);
    void updateSelection();

    Ui::SearchPanel *ui;
    QPlainTextEdit *m_editor = nullptr;
    QVector<Match> m_matches;
    int m_currentMatch = -1;
};

#endif // SEARCHPANEL_H