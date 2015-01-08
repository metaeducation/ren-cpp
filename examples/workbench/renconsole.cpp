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

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

#include "renconsole.h"
#include "mainwindow.h"
#include "fakestdio.h"
#include "watchlist.h"
#include "rensyntaxer.h"



///
/// WORKER OBJECT FOR HANDLING EVALUATIONS
///

//
// We push this item to the worker thread and let it do the actual evaluation
// while we keep monitoring the GUI for an interrupt
//
// http://doc.qt.io/qt-5/qthread.html#details
//

class Worker : public QObject
{
    Q_OBJECT

public slots:
    void doWork(QString const & input) {
        ren::Value result;
        bool success = false;

        ren::Value start = ren::runtime("stats/timer");

        try {
            result = ren::runtime(input.toUtf8().constData());
            success = true;
        }
        catch (ren::evaluation_error const & e) {
            result = e.error();
        }
        catch (ren::exit_command const & e) {
            qApp->exit(e.code());
        }
        catch (ren::evaluation_cancelled const & e) {
            // Let returning none for the error mean cancellation
            result = ren::none;
        }

        // Console hook needs to run here for low latency for things like
        // timers, otherwise a UI event could be holding it up.

        ren::Value delta = ren::runtime("stats/timer -", start);

        emit resultReady(success, result, delta);
    }

signals:
    void resultReady(
        bool success,
        ren::Value const & result,
        ren::Value const & delta
    );
};



///
/// CONSOLE CONSTRUCTION
///

//
// The console subclasses the ReplPad, adding in the constraint that you
// can't interact with the console in a modifying way if an evaluation is
// underway.  It provides the virtual evaluate function...which doesn't
// run synchronously but posts a work item to an evaluation thread (this
// keeps the GUI thread responsive and allows a way of reading requests
// to cancel from the user)
//

RenConsole::RenConsole (QWidget * parent) :
    ReplPad (parent),
    fakeOut (new FakeStdout (*this)),
    evaluating (false)
{    
    // We want to be able to append text from threads besides the GUI thread.
    // It is a synchronous operation for a worker, but still goes through the
    // emit process.

    connect(
        this, &RenConsole::needTextAppend,
        this, &RenConsole::onAppendText,
        Qt::BlockingQueuedConnection
    );


    // Set up the Evaluator so it's wired up for signals and slots
    // and on another thread from the GUI.  This technique is taken directly
    // from the Qt5 example, and even uses the same naming:
    //
    //   http://doc.qt.io/qt-5/qthread.html#details

    Worker * worker = new Worker;
    worker->moveToThread(&workerThread);
    connect(
        &workerThread, &QThread::finished,
        worker, &QObject::deleteLater,
        Qt::DirectConnection
    );
    connect(
        this, &RenConsole::operate,
        worker, &Worker::doWork,
        Qt::QueuedConnection
    );
    connect(
        worker, &Worker::resultReady,
        this, &RenConsole::handleResults,
        // technically it can't handle more results...but better perhaps
        // to crash than to block if some new invariant is introduced and
        // make it look like it's working.
        Qt::QueuedConnection
    );
    workerThread.start();


    // Tell the runtime to use the fake stdout instead of the ordinary one
    // (otherwise the output would be going to the terminal that launched
    // the GUI app, not a window inside that app).

    ren::Engine::runFinder().setOutputStream(*fakeOut);


    // We call reset synchronously here, but we want to be able to call it
    // asynchronously if we're doing something like an onDocumentChanged
    // handler to avoid infinite recursion

    connect(
        this, &RenConsole::requestConsoleReset,
        this, &RenConsole::onConsoleReset,
        Qt::QueuedConnection
    );


    // Initialize text formats used.  What makes this difficult is "zoom in"
    // and "zoom out", so if you get too creative with the font settings then
    // CtrlPlus and CtrlMinus won't do anything useful.  See issue:
    //
    //     https://github.com/metaeducation/ren-garden/issues/7

    // Make the input just a shade lighter black than the output.  (It's also
    // not a fixed width font, so between those two differences you should be
    // able to see what's what.)

    inputFormat.setForeground(QColor {0x20, 0x20, 0x20});

    promptFormat.setForeground(Qt::darkGray);
    promptFormat.setFontWeight(QFont::Bold);

    hintFormat.setForeground(Qt::darkGray);
    hintFormat.setFontItalic(true);

    errorFormat.setForeground(Qt::darkRed);

    // See what works well enough on platforms to demo in terms of common
    // monospace fonts (or if monospace is even what we want to differentiate
    // the output...)
    //
    // http://stackoverflow.com/a/1835938/211160

    outputFormat.setFontFamily("Courier");
    outputFormat.setFontWeight(QFont::Bold);

    auto consoleFunction = ren::makeFunction(
        "{CONSOLE dialect for customizing Ren Workbench commands}"
        ":arg [word! path! block! paren! integer!]"
        "    {word to watch or other legal parameter, see documentation)}",

        REN_STD_FUNCTION,

        [this](ren::Value const & arg) -> ren::Value
        {
            if (arg.isBlock()) {
                ren::runtime("do make error! {Block form of dialect soon...}");
            }

            return consoleDialect(arg);
        }
    );

    ren::runtime("console: quote", consoleFunction);

    // Load the incubator routines that are not written in C++ from the
    // resource file

    QFile file(":/scripts/incubator.reb");
    file.open(QIODevice::ReadOnly);

    QByteArray dump = file.readAll();
    ren::runtime(dump.data());

    // Print the banner and the first prompt.  Any time we're going to do
    // a write to the console, we need to do so while the modifyMutex is
    // locked.

    QMutexLocker locker {&modifyMutex};
    printBanner();
    appendNewPrompt();
}



//
// This prints out a banner message to identify the Ren Workbench and the
// technologies used.  Does one need to fritter away time on splashy
// graphics?  Well, it's like what Penn and Teller say about smoking:
//
//     "Don't do it.
//        (...unless you want to look cool.)"
//
// Assuming source is in UTF8 and so Ren symbol and Copyright symbol are
// acceptable to use.  You can insert either plain text or a subset of HTML
// into the QTextEdit, and for a list of the supported HTML subset see here:
//
//    http://doc.qt.io/qt-5/richtext-html-subset.html
//

void RenConsole::printBanner() {

    QTextCursor cursor {document()};

    cursor.insertImage(QImage (":/images/red-logo.png"));

    cursor.insertImage(QImage (":/images/rebol-logo.png"));

    QTextCharFormat headerFormat;
    headerFormat.setFont(
        QFont("Helvetica", defaultFont.pointSize() * 1.5)
    );

    cursor.insertText("\n", headerFormat);

    // Show off/test RenCpp by picking off the letters of the title one at a
    // time, (w/proper Unicode) via C++ iterator interface underneath a
    // range-based for, then use the coercion of ren::Character to QChar
    // to build a new string for the opening title

    QString heading;
    for (ren::Character ch : ren::String{"{<h1>Ren [人] Garden</h1>}"})
        heading += ch;

    cursor.insertHtml(heading);

    // Use a font we set the size explicitly for so this text intentionally
    // does not participate in zoom in and zoom out
    //
    // https://github.com/metaeducation/ren-garden/issues/7

    QTextCharFormat subheadingFormat;
    subheadingFormat.setFont(
        QFont(
            "Helvetica",
            defaultFont.pointSize()
        )
    );
    subheadingFormat.setForeground(Qt::darkGray);

    cursor.insertText("\n", subheadingFormat);

    std::vector<char const *> components = {
        "<i><b>Red</b> is © 2015 Nenad Rakocevic, BSD License</i>",

        "<i><b>Rebol</b> is © 2015 REBOL Technologies, Apache 2 License</i>",

        "<i><b>Ren</b> is a project by Humanistic Data Initiative</i>",

        "<i><b>RenCpp</b></b> is © 2015 HostileFork.com, Boost License</i>",

        "<i><b>Qt</b> is © 2015 Digia Plc, LGPL 2.1 or GPL 3 License</i>",

        "",

        "<i><b>Ren Garden</b> is © 2015 MetÆducation, GPL 3 License</i>"
    };

    for (auto & credit : components) {
        cursor.insertHtml(credit);
        cursor.insertText("\n");
    }

    // Center all that stuff

    int endHeaderPos = cursor.position();

    cursor.setPosition(0, QTextCursor::KeepAnchor);
    QTextBlockFormat leftFormat = cursor.blockFormat();
    QTextBlockFormat centeredFormat = leftFormat;
    centeredFormat.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    cursor.setBlockFormat(centeredFormat);

    cursor.setPosition(endHeaderPos);
    cursor.setBlockFormat(leftFormat);

    appendText("\n");
}



void RenConsole::printPrompt() {
    pushFormat(promptFormat);
    appendText(">>");

    pushFormat(inputFormat);
    appendText(" ");
}


void RenConsole::printMultilinePrompt() {
    pushFormat(hintFormat);
    appendText("[ctrl-enter to evaluate]");

    pushFormat(inputFormat);
    appendText("\n");
}


///
/// RICH-TEXT CONSOLE BEHAVIOR
///


void RenConsole::onAppendText(QString const & text) {
    ReplPad::appendText(text);
}

void RenConsole::appendText(QString const & text) {
    if (RenConsole::thread() == QThread::currentThread()) {
        // blocking connection would cause deadlock.
        ReplPad::appendText(text);
    }
    else {
        // we need to block in order to properly check for write mutex
        // authority (otherwise we could just queue and run...)
        emit needTextAppend(text);
    }
}




bool RenConsole::isReadyToModify(QKeyEvent * event) {

    // No modifying operations while you are running in the console.  You
    // can only request to cancel.  For issues tied to canceling semantics
    // in RenCpp, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/19

    if (evaluating) {
        if (event->key() == Qt::Key_Escape) {
            ren::runtime.cancel();

            // Message of successful cancellation is given in the console
            // by the handler when the evaluation thread finishes.
            return false;
        }

        emit reportStatus("Evaluation in progress, can't edit");
        followLatestOutput();
        return false;
    }

    return true;
}


void RenConsole::evaluate(QString const & input) {
    evaluating = true;

    pushFormat(outputFormat);

    emit operate(input);
}


///
/// EVALUATION RESULT HANDLER
///

//
// When the evaluator has finished running, we want to print out the
// results and maybe a new prompt.
//

void RenConsole::handleResults(
    bool success,
    ren::Value const & result,
    ren::Value const & delta
) {
    assert(evaluating);

    // At the moment, RenCpp actually flushes on every write.  This isn't
    // such a bad idea for stdout, and works well for the console, but may
    // change later...so keep the flush here as a note.

    ren::Engine::runFinder().getOutputStream().flush();

    QMutexLocker locker {&modifyMutex};

    if (not success) {
        pushFormat(errorFormat);

        // The value does not represent the result of the operation; it
        // represents the error that was raised while running it (or if
        // that error is unset then assume a cancellation).  Formed errors
        // have an implicit newline on the end implicitly

        if (result)
            appendText(static_cast<QString>(result));
        else
            appendText("[Escape]\n");

    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        pushFormat(promptFormat);
        appendText("==");

        pushFormat(inputFormat);
        appendText(" ");

        // Technically this should run through a hook that is "guaranteed"
        // not to hang or crash; we cannot escape out of this call.  So
        // just like to_string works, this should too.

        QString molded = static_cast<QString>(
            ren::runtime("mold/all", result)
        );
        appendText(molded);
        appendText("\n");
    }

    // I like this newline always, even though Rebol's console only puts in
    // the newline if you get a non-unset evaluation...

    appendText("\n");

    appendNewPrompt();

    if (delta) {
        emit reportStatus(
            QString("Command completed in ") + static_cast<QString>(delta)
        );
    }

    emit finishedEvaluation();

    evaluating = false;
}



ren::Value RenConsole::consoleDialect(ren::Value const &) {
    ren::runtime("do make error! {Coming soon to a cross-platform near you}");
    UNREACHABLE_CODE();
}


///
/// DESTRUCTOR
///

//
// If we try to destroy a RenConsole, there may be a worker thread still
// running that we wait for to quit.
//

RenConsole::~RenConsole() {
    ren::runtime.cancel();
    workerThread.quit();
    if (not workerThread.wait(1000)) {
        // How to print to console about quitting
        QMessageBox::information(
            nullptr,
            "Ren Garden Terminated Abnormally",
            "A cancel request was sent to the evaluator but the thread it was"
            " running didn't exit in a timely manner.  This should not happen,"
            " so if you can remember what you were doing or reproduce it then"
            " please report it on the issue tracker!"
        );
        exit(1337); // REVIEW: What exit codes will Ren Garden use?
    }
}


// This bit is necessary because we're defining a Q_OBJECT class in a .cpp
// file instead of a header file (Worker)

#include "renconsole.moc"
