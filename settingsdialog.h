#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "settings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsDialog; }
QT_END_NAMESPACE

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const Settings &settings, QWidget *parent = nullptr);
    ~SettingsDialog();

    Settings settings() const;

private:
    void loadFromSettings(const Settings &s);
    void saveToSettings(Settings &s) const;
    void updateCodeBlockLinesVisibility();

    Ui::SettingsDialog *ui;
};

#endif // SETTINGSDIALOG_H
