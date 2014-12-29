#include <QtWidgets>

#include "mainwindow.h"
#include "renconsole.h"
#include "watchlist.h"

#include "rencpp/ren.hpp"

MainWindow::MainWindow()
{
    qRegisterMetaType<ren::Value>("ren::Value");

    console = new RenConsole;
    setCentralWidget(console);

    dockWatch = new QDockWidget(tr("watch"), this);
    dockWatch->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );

    watchList = new WatchList;
    dockWatch->setWidget(watchList);

    connect(
        console, &RenConsole::finishedEvaluation,
        watchList, &WatchList::updateWatches,
        Qt::DirectConnection
    );

    // REVIEW: is there a better way of having a command say it wants to
    // hide the dock the watchList is in?

    connect(
        watchList, &WatchList::showDockRequested,
        dockWatch, &QDockWidget::show,
        Qt::QueuedConnection
    );

    connect(
        watchList, &WatchList::hideDockRequested,
        dockWatch, &QDockWidget::hide,
        Qt::QueuedConnection
    );

    addDockWidget(Qt::RightDockWidgetArea, dockWatch);
    dockWatch->hide();

    createActions();
    createMenus();
    createStatusBar();

    updateMenus();

    connect(
        console, &RenConsole::copyAvailable,
        cutAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    connect(
        console, &RenConsole::copyAvailable,
        copyAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    readSettings();

    setWindowTitle(tr("Ren [人] Garden"));
    setUnifiedTitleAndToolBarOnMac(true);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


void MainWindow::cut()
{
    console->cut();
}

void MainWindow::copy()
{
    console->copy();
}

void MainWindow::paste()
{
    console->paste();
}


void MainWindow::about()
{
    QMessageBox::about(
        this,
        tr("About Ren [人] Garden"),
        tr(
            "The <b>Ren [人] Garden</b> workbench integrates Rebol or Red"
            " evaluators into a Qt-based environment, by utilizing the Rencpp"
            " binding.\n\n"
            "Copyright © 2014 HostileFork.com, GPL License\n\n"
            "Rebol, Red, and Qt are governed by the terms of their licenses."
        )
    );
}


void MainWindow::updateMenus()
{
    bool hasSelection = console->textCursor().hasSelection();
    cutAct->setEnabled(hasSelection);
    copyAct->setEnabled(hasSelection);
}


void MainWindow::createActions()
{
    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, &QAction::triggered, qApp, &QApplication::quit, Qt::DirectConnection);

    cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(cutAct, &QAction::triggered, this, &MainWindow::cut, Qt::DirectConnection);

    copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(copyAct, &QAction::triggered, this, &MainWindow::copy, Qt::DirectConnection);

    pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(pasteAct, &QAction::triggered, this, &MainWindow::paste, Qt::DirectConnection);

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::about, Qt::DirectConnection);

    aboutQtAct = new QAction(tr("About &Qt"), this);
    aboutQtAct->setStatusTip(tr("Show the Qt library's About box"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt, Qt::DirectConnection);
}


void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *action = fileMenu->addAction(tr("Switch layout direction"));
    connect(action, &QAction::triggered, this, &MainWindow::switchLayoutDirection, Qt::DirectConnection);
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
    helpMenu->addAction(aboutQtAct);
}


void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}


void MainWindow::readSettings()
{
    QSettings settings("HostileFork", "Ren Garden");
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    move(pos);
    resize(size);
}


void MainWindow::writeSettings()
{
    QSettings settings("HostileFork", "Ren Garden");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
}


void MainWindow::switchLayoutDirection()
{
    if (layoutDirection() == Qt::LeftToRight)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);
}

