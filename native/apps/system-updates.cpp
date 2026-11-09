// SPDX-License-Identifier: GPL-3.0-or-later
#include "system-controls.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QUuid>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>
void addUpdateControls(QVBoxLayout* layout, QLabel* status)
{
    auto* parent = layout->parentWidget();
    auto* list = new QListWidget(parent);
    layout->addWidget(list);
    auto* check = new QPushButton(QObject::tr("Check Windows Update"), parent);
    layout->addWidget(check);
    auto* install = new QPushButton(QObject::tr("Download and install selected updates…"), parent);
    install->setEnabled(false);
    layout->addWidget(install);
    auto* restart = new QPushButton(QObject::tr("Restart Windows…"), parent);
    layout->addWidget(restart);
    auto* refresh = new QTimer(parent);
    QObject::connect(refresh, &QTimer::timeout, parent,
        [=] { check->setEnabled(!parent->window()->property("updatesRunning").toBool()); });
    refresh->start(500);
    QObject::connect(check, &QPushButton::clicked, parent, [=] {
        if (parent->window()->property("updatesRunning").toBool())
            return;
        install->setEnabled(false);
        list->clear();
        runSettingsScript(status, check, QStringLiteral(R"PS(
$s=New-Object -ComObject Microsoft.Update.Session
$r=$s.CreateUpdateSearcher().Search("IsInstalled=0 and IsHidden=0 and Type='Software'")
ConvertTo-Json -Depth 5 -Compress -InputObject @($r.Updates | ForEach-Object { [pscustomobject]@{Title=$_.Title;ID=$_.Identity.UpdateID;Revision=$_.Identity.RevisionNumber;Eula=$_.EulaText} })
)PS"),
            [=](QByteArray bytes) {
                QJsonParseError error;
                const auto document = QJsonDocument::fromJson(bytes, &error);
                if (error.error != QJsonParseError::NoError || !document.isArray()) {
                    status->setText(QObject::tr("Could not read update results."));
                    return;
                }
                for (const auto& value : document.array()) {
                    const auto update = value.toObject();
                    if (QUuid(update["ID"].toString()).isNull())
                        continue;
                    auto* item = new QListWidgetItem(update["Title"].toString(), list);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Unchecked);
                    item->setData(Qt::UserRole, update);
                }
                install->setEnabled(list->count() > 0);
                status->setText(QObject::tr("%1 available updates. Select the updates to install.").arg(list->count()));
            });
    });
    QObject::connect(install, &QPushButton::clicked, parent, [=] {
        if (parent->window()->property("updatesRunning").toBool())
            return;
        QStringList wanted;
        QString licenses;
        for (int i = 0; i < list->count(); ++i) {
            const auto* item = list->item(i);
            if (item->checkState() != Qt::Checked)
                continue;
            const auto update = item->data(Qt::UserRole).toJsonObject();
            const QUuid id(update["ID"].toString());
            int revision = update["Revision"].toInt(-1);
            if (id.isNull() || revision < 0)
                continue;
            const auto hash
                = QCryptographicHash::hash(update["Eula"].toString().toUtf8(), QCryptographicHash::Sha256).toHex();
            wanted << QStringLiteral("'%1:%2'='%3'")
                          .arg(id.toString(QUuid::WithoutBraces))
                          .arg(revision)
                          .arg(QString::fromLatin1(hash));
            licenses += update["Title"].toString() + "\n\n" + update["Eula"].toString() + "\n\n";
        }
        if (wanted.isEmpty()) {
            status->setText(QObject::tr("Select at least one update."));
            return;
        }
        QDialog review(parent);
        review.setWindowTitle(QObject::tr("Review update licenses"));
        review.resize(720, 520);
        auto* contents = new QVBoxLayout(&review);
        auto* explanation = new QLabel(QObject::tr("Installing these updates changes Windows. Review the licenses "
                                                   "below. This application will not restart Windows automatically."),
            &review);
        explanation->setWordWrap(true);
        contents->addWidget(explanation);
        auto* text = new QPlainTextEdit(licenses, &review);
        text->setReadOnly(true);
        contents->addWidget(text);
        auto* buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::Cancel, &review);
        buttons->button(QDialogButtonBox::Yes)->setText(QObject::tr("Accept licenses and install"));
        contents->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &review, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &review, &QDialog::reject);
        if (review.exec() != QDialog::Accepted)
            return;
        runSettingsScript(
            status, install,
            QStringLiteral(R"PS(
$wanted=@{%1}
$s=New-Object -ComObject Microsoft.Update.Session
$r=$s.CreateUpdateSearcher().Search("IsInstalled=0 and IsHidden=0 and Type='Software'")
$selected=New-Object -ComObject Microsoft.Update.UpdateColl
$sha=[Security.Cryptography.SHA256]::Create()
foreach($u in $r.Updates) {
 $key=$u.Identity.UpdateID+':'+$u.Identity.RevisionNumber
 if($wanted.ContainsKey($key)) {
  $hash=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes([string]$u.EulaText)))).Replace('-','').ToLowerInvariant()
  if($hash -ne $wanted[$key]) {throw 'Update license changed. Refresh and review again.'}
  if(-not $u.EulaAccepted){$u.AcceptEula()}
  [void]$selected.Add($u)
 }
}
$sha.Dispose()
if($selected.Count -ne $wanted.Count){throw 'The selected updates changed. Refresh before installing.'}
$d=$s.CreateUpdateDownloader();$d.Updates=$selected;$download=$d.Download()
foreach($u in $selected){if(-not $u.IsDownloaded){throw ('Download failed: '+$u.Title)}}
$i=$s.CreateUpdateInstaller();$i.Updates=$selected;$i.ForceQuiet=$true;$i.AllowSourcePrompts=$false
$result=$i.Install()
ConvertTo-Json -Compress -InputObject ([pscustomobject]@{Result=[int]$result.ResultCode;Reboot=[bool]$result.RebootRequired})
)PS")
                .arg(wanted.join(';')),
            [=](QByteArray bytes) {
                const auto result = QJsonDocument::fromJson(bytes).object();
                status->setText(
                    QObject::tr("Update result: %1. Restart required: %2. Refresh to check remaining updates.")
                        .arg(result["Result"].toInt())
                        .arg(result["Reboot"].toBool() ? QObject::tr("Yes") : QObject::tr("No")));
            },
            true);
    });
    QObject::connect(restart, &QPushButton::clicked, parent, [=] {
        if (parent->window()->property("updatesRunning").toBool())
            return;
        if (QMessageBox::question(parent, QObject::tr("Restart Windows"),
                QObject::tr("Save your work before restarting Windows. Restart now?"))
            == QMessageBox::Yes)
            QProcess::startDetached(qEnvironmentVariable("SystemRoot") + "/System32/shutdown.exe", { "/r", "/t", "0" });
    });
    status->setText(
        QObject::tr("Updates are checked and installed only when requested. Administrator permission may be required. "
                    "Result codes: 2 = succeeded; 3 = some updates failed; 4 = failed; 5 = aborted."));
}
