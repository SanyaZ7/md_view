#include "settingsdialog.h"
#include "./ui_settingsdialog.h"

SettingsDialog::SettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    loadFromSettings(settings);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

Settings SettingsDialog::settings() const
{
    Settings s;
    saveToSettings(s);
    return s;
}

void SettingsDialog::loadFromSettings(const Settings &s)
{
    ui->chkLineNumbers->setChecked(s.lineNumbers);
    ui->chkWordWrap->setChecked(s.wordWrap);
}

void SettingsDialog::saveToSettings(Settings &s) const
{
    s.lineNumbers = ui->chkLineNumbers->isChecked();
    s.wordWrap = ui->chkWordWrap->isChecked();
}
