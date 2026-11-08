// SPDX-License-Identifier: GPL-3.0-or-later
#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QTableWidget>
#include <QToolButton>
#include <QElapsedTimer>
#include <QThread>
#include <QTextStream>
#include <QFontDatabase>
int main(int argc,char **argv)
{
    qputenv("QT_STYLE_OVERRIDE","plasmawindows");
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    const int font=QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR")+"/Fonts/segoeui.ttf");
    if(font>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).value(0),10));
    QMainWindow window;window.setWindowTitle("Breeze window integration");window.resize(640,400);
    auto *menu=window.menuBar()->addMenu("File");menu->addAction("New Window");menu->addAction("Close");
    auto *table=new QTableWidget(3,3);table->setHorizontalHeaderLabels({"Process","CPU","Memory"});table->setItem(0,0,new QTableWidgetItem("Konsole"));table->setItem(1,0,new QTableWidgetItem("Dolphin"));table->setItem(2,0,new QTableWidgetItem("Process Monitor"));window.setCentralWidget(table);window.show();
    auto pump=[&] {QElapsedTimer timer;timer.start();while(timer.elapsed()<150){app.processEvents();QThread::msleep(5);}};pump();
    QWidget *bar=window.findChild<QWidget*>("PlasmaBreezeTitleBar");
    bool ok=bar && window.windowFlags().testFlag(Qt::FramelessWindowHint) && window.contentsMargins().top()>=32 && window.menuBar()->actions().size()==1;
    if(bar) {
        const auto buttons=bar->findChildren<QToolButton*>();ok &= buttons.size()==3;
        window.grab().save("breeze-window-preview.png");
        window.showFullScreen();pump();ok &= bar->isHidden() && window.contentsMargins().top()==0;
        window.showNormal();pump();ok &= !bar->isHidden() && window.contentsMargins().top()>=32;
        if(buttons.size()==3){buttons[1]->click();pump();ok &= window.isMaximized();buttons[1]->click();pump();ok &= !window.isMaximized();buttons[0]->click();pump();ok &= window.isMinimized();window.showNormal();pump();buttons[2]->click();pump();ok &= !window.isVisible();}
    }
    QTextStream(stdout)<<(ok?"PASS":"FAIL")<<": Breeze chrome, content margins, preserved menu, maximize/restore/minimize/close\n";
    return ok?0:1;
}
