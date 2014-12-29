//
// renconsole.cpp
// This file is part of Ren Garden
// Copyright (C) 2014 HostileFork.com
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
// See http://ren-garden.hostilefork.com/ for more information on this project
//


#include <QtWidgets>

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

#include "renconsole.h"
#include "mainwindow.h"
#include "fakestdio.h"
#include "watchlist.h"



///
/// WORKER OBJECT FOR HANDLING EVALUATIONS
///

//
// We push this item to the worker thread and let it do the actual evaluation
// while we keep monitoring the GUI for an interrupt
//
// http://doc.qt.io/qt-5/qthread.html#details
//

class Worker : public QThread
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
        catch (ren::evaluation_error & e) {
            result = e.error();
        }
        catch (ren::exit_command & e) {
            qApp->exit(e.code());
        }
        catch (ren::evaluation_cancelled & e) {
            // Let returning an unset for the error mean cancellation
        }

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
// Right now the console constructor is really mostly about setting up a
// long graphical banner.

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

    ren::runtime.setOutputStream(*fakeOut);


    // We call reset synchronously here, but we want to be able to call it
    // asynchronously if we're doing something like an onDocumentChanged
    // handler to avoid infinite recursion

    connect(
        this, &RenConsole::requestConsoleReset,
        this, &RenConsole::onConsoleReset,
        Qt::QueuedConnection
    );


    // Initialize text formats used - generalize anywhere specific styles
    // are hardcoded... someday :-)

    // Make the input just a shade lighter black than the output.  (It's also
    // not a fixed width font, so between those two differences you should be
    // able to see what's what.)

    inputFormat.setForeground(QColor {0x20, 0x20, 0x20});

    promptFormat.setForeground(Qt::darkGray);
    promptFormat.setFontWeight(QFont::Bold);

    hintFormat.setForeground(Qt::darkGray);
    hintFormat.setFontItalic(true);

    errorFormat.setForeground(Qt::darkRed);

    // http://stackoverflow.com/a/1835938/211160

    QFont outputFont {"Monospace"};
    outputFont.setStyleHint(QFont::TypeWriter);
    outputFont.setPointSize(currentFont().pointSize());
    outputFormat.setFont(outputFont);

    // Print the banner and a prompt.

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

    cursor.insertText("\n");

    QTextCharFormat headerFormat;
    headerFormat.setFont(QFont("Helvetica", 24, QFont::Bold));
    cursor.insertText("Ren [人] Garden", headerFormat);

    QTextCharFormat subheadingFormat;
    subheadingFormat.setForeground(Qt::darkGray);
    cursor.insertText("\n", subheadingFormat);

    std::vector<char const *> components = {
        "<i><b>Red</b> is © 2014 Nenad Rakocevic, BSD License</i>",

        "<i><b>Rebol</b> is © 2014 REBOL Technologies, Apache 2 License</i>",

        "<i><b>RenCpp</b></b> is © 2014 HostileFork.com, Boost License</i>",

        "<i><b>Qt</b> is © 2014 Digia Plc, LGPL 2.1 or GPL 3 License</i>",

        "",

        "<i><b>Ren Garden</b> is © 2014 HostileFork.com, GPL 3 License</i>"
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

    // Text insertions continue using whatever format was set up previously,
    // So clear the format as of the space insertion.

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



//
// RenConsole::keyPressEvent()
//
// Main keyboard hook for handling input.  Note that input can come from
// other sources--such as the clipboard, and so that has to be intercepted
// as well (to scrub non-plaintext formatting off of information coming from
// web browsers, etc.
//

void RenConsole::modifyingKeyPressEvent(QKeyEvent * event) {

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
            return;
        }

        emit commandStatus("Evaluation in progress, can't edit");
        followLatestOutput();
        return;
    }

    ReplPad::modifyingKeyPressEvent(event);
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

    // The output to the console seems to flush very aggressively, but with
    // Rencpp it does buffer by default, so be sure to flush the output
    // before printing any eval results or errors

    ren::runtime.getOutputStream().flush();

    QMutexLocker locker {&modifyMutex};

    if (not success) {
        pushFormat(errorFormat);

        // The value does not represent the result of the operation; it
        // represents the error that was raised while running it, or if
        // that error is unset then assume a cancellation.  Formed errors
        // have an implicit newline on the end implicitly

        if (result.isUnset())
            appendText("[Escape]\n");
        else
            appendText(static_cast<QString>(result));

    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        pushFormat(promptFormat);
        appendText("==");

        pushFormat(inputFormat);
        appendText(" ");

        appendText(static_cast<QString>(result));
        appendText("\n");
    }

    // I like this newline always, even though Rebol's console only puts in
    // the newline if you get a non-unset evaluation...

    appendText("\n");

    appendNewPrompt();

    if (delta) {
        emit commandStatus(
            QString("Command completed in ") + static_cast<QString>(delta)
        );
    }

    emit finishedEvaluation();

    evaluating = false;
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
    workerThread.wait();
}


// This bit is necessary because we're defining a Q_OBJECT class in a .cpp
// file instead of a header file (Worker)

#include "renconsole.moc"
