#include "settingsdialog.h"
#include "./ui_settingsdialog.h"

SettingsDialog::SettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    loadFromSettings(settings);

    // Поле количества строк доступно только при включённом ограничении.
    connect(ui->chkLimitCodeBlockHeight, &QCheckBox::toggled,
            this, &SettingsDialog::updateCodeBlockLinesVisibility);
    updateCodeBlockLinesVisibility();
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

    ui->chkLimitCodeBlockHeight->setChecked(s.limitCodeBlockHeight);
    ui->spinCodeBlockMaxLines->setValue(qMax(8, s.codeBlockMaxLines));
}

void SettingsDialog::saveToSettings(Settings &s) const
{
    s.lineNumbers = ui->chkLineNumbers->isChecked();
    s.wordWrap    = ui->chkWordWrap->isChecked();

    s.limitCodeBlockHeight = ui->chkLimitCodeBlockHeight->isChecked();
    s.codeBlockMaxLines    = qMax(8, ui->spinCodeBlockMaxLines->value());
}

void SettingsDialog::updateCodeBlockLinesVisibility()
{
    const bool enabled = ui->chkLimitCodeBlockHeight->isChecked();

    ui->labelCodeBlockMaxLines->setVisible(enabled);
    ui->spinCodeBlockMaxLines->setVisible(enabled);
}
