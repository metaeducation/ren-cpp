//
// mainwindow.cpp
// This file is part of Ren Garden
// Copyright (C) 2015-2018 MetÆducation
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
#include "valueexplorer.h"

#include "rencpp/ren.hpp"

bool forcingQuit = false;

MainWindow::MainWindow() :
    opacity (initialOpacity),
    fading (false),
    fadeTimer (nullptr)
{
    setWindowTitle(tr("Ren Garden"));
    setUnifiedTitleAndToolBarOnMac(true);

    // Registration of ren::AnyValue to allow them to be proxied across threads as
    // parameters to Qt signals and slots.  See notes here on the importance
    // of being consistent about using namespaces, as it is string based:
    //
    //     http://stackoverflow.com/questions/22044737/
    //
    // Note that only values that are *default constructible* may be registered
    // in this way, which rules out all the other RenCpp types, unless they
    // are embedded in an optional.  There is no way to automatically
    // instantiate a generic template e.g. Q_DECLARE_METATYPE_TEMPLATE_1ARG,
    // as that is an internal macro:
    //
    //     https://bugreports.qt.io/browse/QTBUG-35848

    qRegisterMetaType<ren::AnyValue>("ren::AnyValue");

    qRegisterMetaType<ren::optional<ren::AnyValue>>("ren::optional<ren::AnyValue>");
    // !!! register ren::optional<> for all value types, or add on-demand?

    // Rebol's design is not multithreaded, and features have not been vetted
    // to work in a multithreaded environment...even if both are taking turns
    // calling the evaluator from the top-level.  While that works fine for
    // general expressions, if something calls out and makes a network
    // call on one thread--the other may not be able to pick it up.
    //
    // For this reason, even though currently some Ren Garden features will
    // make calls from the GUI...we want to be sure that initialization
    // is done on the thread we will ultimately use as a worker.

    worker = new EvaluatorWorker;
    worker->moveToThread(&workerThread);
    connect(
        &workerThread, &QThread::finished,
        worker, &QObject::deleteLater,
        Qt::DirectConnection
    );
    connect(
        this, &MainWindow::initializeEvaluator,
        worker, &EvaluatorWorker::initialize,
        Qt::QueuedConnection
    );
    connect(
        worker, &EvaluatorWorker::initializeDone,
        this, &MainWindow::finishInitializing,
        Qt::QueuedConnection
    );
    connect(
        worker, &EvaluatorWorker::caughtNonRebolException,
        this, &MainWindow::cppExceptionNotice,
        Qt::QueuedConnection
    );
    workerThread.start();

    // Don't run any code that might use Ren/C++ values until we've given the
    // evaluator thread dibs on potentially thread-sensitive initialization

    emit initializeEvaluator();

    // Note: MainWindow is still invisible at this point
}


void MainWindow::finishInitializing() {
    console = new RenConsole (worker);
    setCentralWidget(console);
    console->show();

    dockValueExplorer = new QDockWidget(tr("explorer"), this);

    auto explorer = new ValueExplorer (dockValueExplorer);
    dockValueExplorer->setWidget(explorer);

    dockWatch = new QDockWidget(tr("watch"), this);
    dockWatch->setAllowedAreas(
        Qt::LeftDockWidgetArea| Qt::RightDockWidgetArea
        | Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea
    );

    connect(
        console, &RenConsole::switchWatchList,
        [this](WatchList * watchList) {
            dockWatch->setWidget(watchList);
        }
    );

    connect(
        console, &RenConsole::showDockRequested,
        this, &MainWindow::onShowDockRequested,
        Qt::QueuedConnection
    );

    connect(
        console, &RenConsole::hideDockRequested,
        this, &MainWindow::onHideDockRequested,
        Qt::QueuedConnection
    );

    connect(
        console, &RenConsole::reportStatus,
        statusBar(), [this](QString const & message) {
            // Slot wants a "timeout" (0 for "until next message")
            statusBar()->showMessage(message, 0);
        },
        Qt::QueuedConnection // May be sent from worker thread
    );

    connect(
        &console->repl(), &ReplPad::fadeOutToQuit,
        this, &MainWindow::onFadeOutToQuit,
        Qt::DirectConnection
    );

    connect(
        console, &RenConsole::exploreValue,
        explorer, &ValueExplorer::setValue,
        Qt::QueuedConnection
    );

    addDockWidget(Qt::RightDockWidgetArea, dockWatch);
    dockWatch->hide();

    addDockWidget(Qt::TopDockWidgetArea, dockValueExplorer);
    dockValueExplorer->hide();

    createActions();
    createMenus();
    createStatusBar();

    updateMenus();

    connect(
        &console->repl(), &ReplPad::copyAvailable,
        cutAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    connect(
        &console->repl(), &ReplPad::copyAvailable,
        copyAct, &QAction::setEnabled,
        Qt::DirectConnection
    );

    connect(
        dockValueExplorer, &QDockWidget::visibilityChanged,
        valueExplorerAct, &QAction::setChecked,
        Qt::DirectConnection
    );

    connect(
        dockWatch, &QDockWidget::visibilityChanged,
        watchListAct, &QAction::setChecked,
        Qt::DirectConnection
    );

    readSettings();

    show();
}


void MainWindow::closeEvent(QCloseEvent *event)
{
    writeSettings();
    event->accept();
}


void MainWindow::cut()
{
    console->repl().cutSafely();
}

void MainWindow::copy()
{
    console->repl().copy();
}

void MainWindow::paste()
{
    console->repl().pasteSafely();
}


void MainWindow::about()
{
    QMessageBox::about(
        this,
        tr("About Ren Garden"),
        tr(
            "The <b>Ren Garden</b> workbench integrates Rebol language"
            " evaluators into a Qt-based environment, by utilizing the RenCpp"
            " binding.<br><br>"
            "Copyright © 2015-2018 MetÆducation, GPL License<br><br>"
            "Rebol and Qt are governed by the terms of their licenses."
        )
    );
}


void MainWindow::updateMenus()
{
    bool hasSelection = console->repl().textCursor().hasSelection();
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

    proposalsAct = new QAction(tr("Use &Proposals "), this);
    proposalsAct->setStatusTip(tr("Enable or disable experimental language "
                                  "proposals curated by @HostileFork."));
    proposalsAct->setCheckable(true);
    proposalsAct->setChecked(console->getUseProposals());
    connect(
        proposalsAct, &QAction::triggered,
        [this](bool checked) {
            console->setUseProposals(checked);
        }
    );

    newTabAct = new QAction(tr("New &Tab"), this);
    newTabAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_T));
    newTabAct->setStatusTip(tr("Create a new tab (not multithreaded!)"));
    connect(
        newTabAct, &QAction::triggered,
        [this]() { console->createNewTab(); }
    );

    nextTabAct = new QAction(tr("&Next Tab"), this);
    nextTabAct->setShortcuts(QList<QKeySequence> {
        QKeySequence(Qt::CTRL + Qt::Key_Tab),
        QKeySequence(Qt::CTRL + Qt::Key_PageDown)
    });
    nextTabAct->setStatusTip(tr("Select the next tab"));
    connect(
        nextTabAct, &QAction::triggered,
        [this]() {
            if (console->currentIndex() == console->count() - 1) {
                console->setCurrentIndex(0);
                return;
            }

            console->setCurrentIndex(console->currentIndex() + 1);
        }
    );

    previousTabAct = new QAction(tr("&Previous Tab"), this);
    previousTabAct->setShortcuts(QList<QKeySequence> {
        QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_Tab),
        QKeySequence(Qt::CTRL + Qt::Key_PageUp)
    });
    previousTabAct->setStatusTip(tr("Select the previous tab"));
    connect(
        previousTabAct, &QAction::triggered,
        [this]() {
            if (console->currentIndex() == 0) {
                console->setCurrentIndex(console->count() - 1);
                return;
            }

            console->setCurrentIndex(console->currentIndex() - 1);
        }
    );

    closeTabAct = new QAction(tr("&Close Tab"), this);
    closeTabAct->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_W));
    closeTabAct->setStatusTip(tr("Close current tab"));
    connect(
        closeTabAct, &QAction::triggered,
        [this]() {
            console->tryCloseTab(console->currentIndex());
        }
    );

    watchListAct = new QAction(tr("&Watch List"), this);
    watchListAct->setCheckable(true);
    connect(
        watchListAct, &QAction::triggered,
        [this](bool checked) {
            dockWatch->setVisible(checked);
        }
    );

    valueExplorerAct = new QAction(tr("Value &Explorer"), this);
    valueExplorerAct->setCheckable(true);
    connect(
        valueExplorerAct, &QAction::triggered,
        [this](bool checked) {
            dockValueExplorer->setVisible(checked);
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
    connect(
        action, &QAction::triggered,
        this, &MainWindow::switchLayoutDirection,
        Qt::DirectConnection
    );
    fileMenu->addAction(exitAct);

    editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(cutAct);
    editMenu->addAction(copyAct);
    editMenu->addAction(pasteAct);

    windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(newTabAct);
    windowMenu->addAction(nextTabAct);
    windowMenu->addAction(previousTabAct);
    windowMenu->addAction(closeTabAct);
    windowMenu->addSeparator();
    windowMenu->addAction(watchListAct);
    windowMenu->addAction(valueExplorerAct);

    languageMenu = menuBar()->addMenu(tr("&Language"));
    languageMenu->addAction(proposalsAct);

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

    // Qt has a habit of opening teeny windows by default.  If a previous
    // session did not save a size and location, then use 70% of the available
    // desktop space.
    //
    // https://stackoverflow.com/a/26742685/211160
    //
    if (!settings.contains("pos") || !settings.contains("size"))
        resize(QDesktopWidget().availableGeometry(this).size() * 0.7);

    QPoint pos = settings.value("pos", QPoint(200, 200)).toPoint();
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    int zoom = settings.value("zoom", 0).toInt();
    bool useProposals = settings.value("useProposals", true).toBool();

    move(pos);
    resize(size);
    console->repl().setZoom(zoom);
    console->setUseProposals(useProposals);
}


void MainWindow::writeSettings()
{
    QSettings settings("Metaeducation", "Ren Garden");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.setValue("zoom", console->repl().getZoom());
    settings.setValue("useProposals", console->getUseProposals());
}


void MainWindow::switchLayoutDirection()
{
    if (layoutDirection() == Qt::LeftToRight) {
        qApp->setLayoutDirection(Qt::RightToLeft);

        QTextOption rightToLeft;
        rightToLeft.setTextDirection(Qt::RightToLeft);

        console->repl().document()->setDefaultTextOption(rightToLeft);
    }
    else {
        qApp->setLayoutDirection(Qt::LeftToRight);

        QTextOption leftToRight;
        leftToRight.setTextDirection(Qt::LeftToRight);

        console->repl().document()->setDefaultTextOption(leftToRight);
    }
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
    if (!escaping) {
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

    if (!fadeTimer) {
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


void MainWindow::onShowDockRequested(WatchList * watchList) {
    dockWatch->setWidget(watchList);
    dockWatch->show();
}


void MainWindow::onHideDockRequested(WatchList * watchList) {
    dockWatch->setWidget(watchList);
    dockWatch->hide();
}


void MainWindow::cppExceptionNotice(char const * what) {
    if (what) {
        QMessageBox::information(
            nullptr,
            what,
            "A C++ std::exception was thrown during evaluation.  That"
            " means that somewhere in the chain a function was"
            " called that was implemented as a C++ extension that"
            " threw it.  We're gracefully catching it and not crashing,"
            " BUT please report the issue to the bug tracker.  (Unless"
            " you're extending Ren Garden and it's your bug, in which"
            " case...fix it yourself!  :-P)"
        );
    }
    else {
        QMessageBox::information(
            nullptr,
            "Mystery C++ datatype thrown",
            "A C++ exception was thrown during evaluation, which was *not*"
            " derived from std::exception.  This is considered poor"
            " practice...you're not supposed to write things like"
            " `throw 10;`.  Because it doesn't have a what() method we"
            " can't tell you much about what went wrong.  We're gracefully"
            " catching it and not crashing...but please report this!"
        );
    }
}


MainWindow::~MainWindow() {
    workerThread.quit();
    if (!workerThread.wait(1000) && !forcingQuit) {
        // How to print to console about quitting
        QMessageBox::information(
            nullptr,
            "Ren Garden Terminated Abnormally",
            "A cancel request was sent to the evaluator but the thread it was"
            " running didn't exit in a timely manner.  This should not happen,"
            " so if you can remember what you were doing or reproduce it then"
            " please report it on the issue tracker!"
        );
        exit(1337); // !!! What exit codes will Ren Garden use?
    }
}
