//
// renconsole.cpp
// This file is part of Ren Garden
// Copyright (C) 2015 MetÆducation
//
// Ren Garden is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Ren Garden is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Ren Garden.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://ren-garden.metaeducation.com for more information on this project
//


#include <QtWidgets>

#include "mainwindow.h"
#include "renconsole.h"
#include "watchlist.h"

#include "rencpp/ren.hpp"

bool forcingQuit = false;

MainWindow::MainWindow() :
    opacity (initialOpacity),
    fading (false),
    fadeTimer (nullptr)
{
    // Registration of ren::Value to allow them to be proxied across threads as
    // parameters to Qt signals and slots.  See notes here on the importance
    // of being consistent about using namespaces, as it is string based:
    //
    //     http://stackoverflow.com/questions/22044737/
    //
    // Note that only values that are *default constructible* may be registered
    // in this way, which rules out all the other RenCpp types.  If you want

    qRegisterMetaType<ren::Value>("ren::Value");


    console = new RenConsole;
    setCentralWidget(console);
    console->show();

    dockWatch = new QDockWidget(tr("watch"), this);
    dockWatch->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );

    watchList = new WatchList;
    dockWatch->setWidget(watchList);

    connect(
        console, &RenConsole::finishedEvaluation,
        watchList, &WatchList::updateAllWatchers,
        Qt::DirectConnection
    );

    connect(
        console, &RenConsole::reportStatus,
        statusBar(), [this](QString const & message) {
            // Slot wants a "timeout" (0 for "until next message")
            statusBar()->showMessage(message, 0);
        },
        Qt::DirectConnection
    );

    connect(
        &console->currentRepl(), &ReplPad::fadeOutToQuit,
        this, &MainWindow::onFadeOutToQuit,
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

    connect(
        watchList, &WatchList::reportStatus,
        statusBar(), [this](QString const & message) {
            // Slot wants a "timeout" (0 for "until next message")
            statusBar()->showMessage(message, 0);
        },
        Qt::DirectConnection
    );

    addDockWidget(Qt::RightDockWidgetArea, dockWatch);
    dockWatch->hide();

    createActions();
    createMenus();
    createStatusBar();

    updateMenus();

    connect(
        &console->currentRepl(), &ReplPad::copyAvailable,
        cutAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    connect(
        &console->currentRepl(), &ReplPad::copyAvailable,
        copyAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    readSettings();

    setWindowTitle(tr(u8"Ren [人] Garden"));
    setUnifiedTitleAndToolBarOnMac(true);
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


void MainWindow::cut()
{
    console->currentRepl().cutSafely();
}

void MainWindow::copy()
{
    console->currentRepl().copy();
}

void MainWindow::paste()
{
    console->currentRepl().pasteSafely();
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
            "Copyright © 2015 MetÆducation, GPL License\n\n"
            "Rebol, Red, and Qt are governed by the terms of their licenses."
        )
    );
}


void MainWindow::updateMenus()
{
    bool hasSelection = console->currentRepl().textCursor().hasSelection();
    cutAct->setEnabled(hasSelection);
    copyAct->setEnabled(hasSelection);
}


void MainWindow::createActions()
{
    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcuts(QKeySequence::Quit);
    exitAct->setStatusTip(tr("Exit the application"));
    connect(
        exitAct, &QAction::triggered,
        qApp, &QApplication::quit,
        Qt::DirectConnection
    );

    cutAct = new QAction(QIcon(":/images/cut.png"), tr("Cu&t"), this);
    cutAct->setShortcuts(QKeySequence::Cut);
    cutAct->setStatusTip(tr("Cut the current selection's contents to the "
                            "clipboard"));
    connect(
        cutAct, &QAction::triggered,
        this, &MainWindow::cut,
        Qt::DirectConnection
    );

    copyAct = new QAction(QIcon(":/images/copy.png"), tr("&Copy"), this);
    copyAct->setShortcuts(QKeySequence::Copy);
    copyAct->setStatusTip(tr("Copy the current selection's contents to the "
                             "clipboard"));
    connect(
        copyAct, &QAction::triggered,
        this, &MainWindow::copy,
        Qt::DirectConnection);

    pasteAct = new QAction(QIcon(":/images/paste.png"), tr("&Paste"), this);
    pasteAct->setShortcuts(QKeySequence::Paste);
    pasteAct->setStatusTip(tr("Paste the clipboard's contents into the current "
                              "selection"));
    connect(
        pasteAct, &QAction::triggered,
        this, &MainWindow::paste,
        Qt::DirectConnection
    );

    newTabAct = new QAction(tr("New &Tab"), this);
    newTabAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
    newTabAct->setStatusTip(tr("Create a new tab (not multithreaded!)"));
    connect(
        newTabAct, &QAction::triggered,
        [this]() { console->createNewTab(); }
    );

    nextTabAct = new QAction(tr("&Next Tab"), this);
    nextTabAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Tab));
    nextTabAct->setStatusTip(tr("Select the next tab"));
    connect(
        nextTabAct, &QAction::triggered,
        [this]() {
            if (console->currentIndex() == console->count() - 1) {
                // message about no next tab?
                return;
            }

            console->setCurrentIndex(console->currentIndex() + 1);
        }
    );

    previousTabAct = new QAction(tr("&Previous Tab"), this);
    previousTabAct->setShortcut(
        QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Tab)
    );
    previousTabAct->setStatusTip(tr("Select the previous tab"));
    connect(
        previousTabAct, &QAction::triggered,
        [this]() {
            if (console->currentIndex() == 0) {
                // message about no previous tab?
                return;
            }

            console->setCurrentIndex(console->currentIndex() - 1);
        }
    );

    aboutAct = new QAction(tr("&About"), this);
    aboutAct->setStatusTip(tr("Show the application's About box"));
    connect(
        aboutAct, &QAction::triggered,
        this, &MainWindow::about,
        Qt::DirectConnection
    );
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

    windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(newTabAct);
    windowMenu->addAction(nextTabAct);
    windowMenu->addAction(previousTabAct);

    menuBar()->addSeparator();

    helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(aboutAct);
}


void MainWindow::createStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}


void MainWindow::readSettings()
{
    QSettings settings("Metaeducation", "Ren Garden");
    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    int zoom = settings.value("zoom", 0).toInt();

    move(pos);
    resize(size);
    console->currentRepl().setZoom(zoom);
}


void MainWindow::writeSettings()
{
    QSettings settings("Metaeducation", "Ren Garden");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.setValue("zoom", console->currentRepl().getZoom());
}


void MainWindow::switchLayoutDirection()
{
    if (layoutDirection() == Qt::LeftToRight)
        qApp->setLayoutDirection(Qt::RightToLeft);
    else
        qApp->setLayoutDirection(Qt::LeftToRight);
}


//
// Removing the Q from the runtime for QUIT led to a thought about more
// innovative ways to leave the console, and this was a thought inspired
// by the power-off button "fade out" from ChromeOS.
//
// If you hold down escape, this begins fading the window, and then if you
// release it, it starts fading back in.  Hold it down long enough without
// releasing then the fade gets to a point where the app closes.
//
// As with the exit menu item, this can't be used to kill a hung GUI thread
// (it wouldn't even run).  But as with that menu item this will force quit
// a hung worker thread if the evaluator is not responding to cancel for
// some reason.
//
void MainWindow::onFadeOutToQuit(bool escaping)
{
    if (not escaping) {
        // Timer should have been started by the original request to escape,
        // we leave it running but let it count the opacity up.  (There is
        // probably a usability design thing for making this all nonlinear.)
        // We might think of asserting the timer, but key events can be
        // finicky and you might get a key up with no key down.

        fading = false;
        return;
    }

    fading = true;

    // If the timer is not null, we should only have to change the delta,
    // otherwise we need to create and connect it up.  (We use a lambda
    // function because this little piece of functionality is nicely tied
    // up all in this one slot handler).

    if (not fadeTimer) {
        fadeTimer = new QTimer {this};
        connect(
            fadeTimer, &QTimer::timeout,
            [this]() -> void {
                opacity = opacity + (fading ? -deltaOpacity : deltaOpacity);

                if (opacity <= quittingOpacity) {
                    fadeTimer->stop();
                    forcingQuit = true;
                    qApp->quit();
                }
                else if (opacity >= initialOpacity) {
                    opacity = initialOpacity;
                    setWindowOpacity(1.0);
                    fadeTimer->stop();
                    fadeTimer->deleteLater();
                    fadeTimer = nullptr;
                }

                // Qt tolerates setting window opacities to "more than 1.0"
                // and treats it as 1.0.  But why propagate nonsense any
                // further than you must?

                if (opacity <= 1.0)
                    setWindowOpacity(opacity);
            }
        );

        // Now that the timer has been connected, it's safe to start it

        fadeTimer->setInterval(msecInterval);
        fadeTimer->start();
    }
}
