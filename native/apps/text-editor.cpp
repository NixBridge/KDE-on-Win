// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QLineEdit>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QCloseEvent>
#include <QTextStream>
#include <QTimer>
#include <QTemporaryDir>
#include <QFontDatabase>
#include <vector>
#include <memory>
#include <KIconTheme>
#include <KIconLoader>
#include "text-codec.h"

class Editor final : public QMainWindow
{
public:
    Editor()
    {
        resize(900, 650); edit = new QPlainTextEdit(this); setCentralWidget(edit);
        auto *file = menuBar()->addMenu(tr("&File"));
        file->addAction(tr("&New"), QKeySequence::New, this, [this] { if (confirmSave()) { edit->clear(); path.clear(); format = {}; edit->document()->setModified(false); updateTitle(); } });
        file->addAction(tr("&Open…"), QKeySequence::Open, this, [this] { const QString name = QFileDialog::getOpenFileName(this, tr("Open Text File")); if (!name.isEmpty()) open(name); });
        file->addAction(tr("&Save"), QKeySequence::Save, this, [this] { save(false); });
        file->addAction(tr("Save &As…"), QKeySequence::SaveAs, this, [this] { save(true); });
        file->addSeparator(); file->addAction(tr("&Quit"), QKeySequence::Quit, this, &QWidget::close);
        auto *menu = menuBar()->addMenu(tr("&Edit"));
        menu->addAction(tr("Undo"), QKeySequence::Undo, edit, &QPlainTextEdit::undo);
        menu->addAction(tr("Redo"), QKeySequence::Redo, edit, &QPlainTextEdit::redo);
        menu->addSeparator();
        menu->addAction(tr("Cut"), QKeySequence::Cut, edit, &QPlainTextEdit::cut);
        menu->addAction(tr("Copy"), QKeySequence::Copy, edit, &QPlainTextEdit::copy);
        menu->addAction(tr("Paste"), QKeySequence::Paste, edit, &QPlainTextEdit::paste);
        menu->addAction(tr("Select All"), QKeySequence::SelectAll, edit, &QPlainTextEdit::selectAll);
        menu->addAction(tr("Find…"), QKeySequence::Find, this, [this] {
            bool accepted = false; const QString query = QInputDialog::getText(this, tr("Find"), tr("Text:"), QLineEdit::Normal, lastQuery, &accepted);
            if (!accepted || query.isEmpty()) return; lastQuery = query;
            if (!edit->find(query)) { auto cursor = edit->textCursor(); cursor.movePosition(QTextCursor::Start); edit->setTextCursor(cursor); edit->find(query); }
        });
        connect(edit->document(), &QTextDocument::modificationChanged, this, [this] { updateTitle(); });
        connect(edit, &QPlainTextEdit::cursorPositionChanged, this, [this] { const auto cursor = edit->textCursor(); statusBar()->showMessage(tr("Line %1, Column %2").arg(cursor.blockNumber()+1).arg(cursor.positionInBlock()+1)); });
        updateTitle();
    }
    bool open(const QString &name)
    {
        if (!confirmSave()) return false;
        QFile file(name);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64*1024*1024) { QMessageBox::warning(this, tr("Cannot Open File"), tr("Cannot read this file, or it exceeds 64 MiB.")); return false; }
        QString text; TextFormat detected;
        if (!decodeText(file.readAll(), text, detected)) { QMessageBox::warning(this, tr("Unsupported Text Encoding"), tr("Open UTF-8 or BOM-marked UTF-16 text. Binary and invalid text files are not changed.")); return false; }
        edit->setPlainText(text); format = detected; path = QFileInfo(name).absoluteFilePath(); edit->document()->setModified(false); updateTitle(); return true;
    }
protected:
    void closeEvent(QCloseEvent *event) override { confirmSave() ? event->accept() : event->ignore(); }
private:
    bool confirmSave()
    {
        if (!edit->document()->isModified()) return true;
        const auto answer = QMessageBox::question(this, tr("Unsaved Changes"), tr("Save changes before continuing?"), QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel, QMessageBox::Save);
        return answer == QMessageBox::Discard || (answer == QMessageBox::Save && save(false));
    }
    bool save(bool choose)
    {
        const QString name = choose || path.isEmpty() ? QFileDialog::getSaveFileName(this, tr("Save Text File"), path) : path;
        if (name.isEmpty()) return false;
        QString error;
        if (!writeTextFile(name, edit->toPlainText(), format, error)) { QMessageBox::warning(this, tr("Cannot Save File"), error); return false; }
        path = QFileInfo(name).absoluteFilePath(); edit->document()->setModified(false); updateTitle(); return true;
    }
    void updateTitle() { setWindowTitle((path.isEmpty() ? tr("Untitled") : QFileInfo(path).fileName()) + (edit->document()->isModified() ? QStringLiteral(" *") : QString()) + tr(" — Text Editor")); }
    QPlainTextEdit *edit;
    QString path, lastQuery;
    TextFormat format;
};
int main(int argc, char **argv)
{
    qputenv("QT_STYLE_OVERRIDE", "plasmawindows"); KIconTheme::initTheme(); QApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) {
        const int id = QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+QStringLiteral("/Fonts/segoeui.ttf"));
        if (id >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(id).value(0), 10));
    }
    app.setDesktopFileName(QStringLiteral("org.nixbridge.texteditor"));
    app.setWindowIcon(QIcon(KIconLoader::global()->loadIcon(QStringLiteral("accessories-text-editor"), KIconLoader::Desktop, 48)));
    if (app.arguments().contains(QStringLiteral("--self-test"))) {
        bool ok = true;
        for (const auto encoding : {QStringConverter::Utf8, QStringConverter::Utf16LE, QStringConverter::Utf16BE}) {
            TextFormat format{encoding, true, true}, decoded; QString text;
            const auto bytes = encodeText(QStringLiteral("中文\nsecond line"), format);
            ok &= decodeText(bytes, text, decoded) && text == QStringLiteral("中文\nsecond line") && encodeText(text, decoded) == bytes;
        }
        QString text; TextFormat format; ok &= !decodeText(QByteArray("bad\xff", 4), text, format);
        ok &= !decodeText(QByteArray("\xe4\xb8", 2), text, format);
        QTemporaryDir directory; QString error;
        const QString path = directory.filePath(QStringLiteral("document.txt"));
        ok &= writeTextFile(path, QStringLiteral("original"), format, error) && writeTextFile(path, QStringLiteral("updated"), format, error);
        QFile saved(path); ok &= saved.open(QIODevice::ReadOnly) && saved.readAll() == QByteArray("updated");
        ok &= !writeTextFile(directory.filePath(QStringLiteral("missing/document.txt")), QStringLiteral("unwritten"), format, error);
        QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": UTF-8/UTF-16 BOM, Unicode and CRLF round trips; invalid input rejected; atomic save and write failure\n"; return ok ? 0 : 1;
    }
    std::vector<std::unique_ptr<Editor>> editors;
    auto addEditor = [&] { editors.push_back(std::make_unique<Editor>()); editors.back()->show(); return editors.back().get(); };
    if (app.arguments().contains(QStringLiteral("--ui-smoke-test"))) {
        auto *editor = addEditor();
        QTimer::singleShot(500, editor, [editor, &app] {
            auto *text = editor->findChild<QPlainTextEdit*>(); text->appendPlainText(QStringLiteral("test"));
            const bool ok = text->document()->isModified() && editor->windowTitle().contains(QLatin1Char('*')) && editor->menuBar()->actions().size() == 2 && editor->findChild<QWidget*>(QStringLiteral("PlasmaBreezeTitleBar"));
            editor->grab().save(QStringLiteral("text-editor-preview.png"));
            QTextStream(stdout) << (ok ? "PASS" : "FAIL") << ": editable document, modified state, menus and Breeze titlebar\n"; app.exit(ok ? 0 : 1);
        });
    } else if (app.arguments().size() > 1) {
        for (const QString &path : app.arguments().mid(1)) addEditor()->open(path);
    } else addEditor();
    return app.exec();
}
