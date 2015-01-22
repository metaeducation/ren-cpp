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

#include <vector>

#include <QtWidgets>

#include "rencpp/ren.hpp"

using namespace ren;

#include "renconsole.h"
#include "mainwindow.h"
#include "fakestdio.h"
#include "watchlist.h"
#include "rensyntaxer.h"
#include "renpackage.h"

extern bool forcingQuit;


///
/// WORKER OBJECT FOR HANDLING REN EVALUATIONS
///

//
// We push this item to the worker thread and let it do the actual evaluation
// while we keep monitoring the GUI for an interrupt
//
// http://doc.qt.io/qt-5/qthread.html#details
//
// Ultimately it should be the case that the GUI never calls an "open coded"
// arbitrary evaluation of user code in the runtime.  Short things
// might be okay if you are *certain* the evaluator is not currently running.
//

class EvaluatorWorker : public QObject
{
    Q_OBJECT

public slots:
    // See notes on MainWindow about qRegisterMetaType about why dialect is
    // a ren::Value instead of ren::Function and needs the ren:: prefix
    void doWork(
        ren::Value const & dialect,
        QString const & input,
        bool meta
    ) {
        Value result;
        bool success = false;

        try {
            // We *always* generate a block to pass to the dialect.  This
            // is Ren Garden, not "arbitrary shell"... so if you want to
            // pass an arbitrary string you must type it in as {49+3hfa} in
            // a properly loadable string.

            QString buffer {"{["};
            buffer += input;
            buffer += "]}";

            Value loaded
                = runtime("load", buffer.toUtf8().constData());

            if (meta) {
                if (not runtime("find words-of quote", dialect, "/meta")) {
                    throw Error {"current dialect has no /meta refinement"};
                }

                result = runtime(Path {dialect, "meta"}, loaded);
            }
            else {
                result = dialect.apply(loaded);
            }

            success = true;
        }
        catch (evaluation_error const & e) {
            result = e.error();
            assert(result);
        }
        catch (exit_command const & e) {
            qApp->exit(e.code());
        }
        catch (evaluation_cancelled const & e) {
            // Let returning none for the error mean cancellation
            result = none;
        }

        emit resultReady(success, result);
    }

signals:
    void resultReady(
        bool success,
        ren::Value const & result // namespace ren:: needed for signal!
    );
};



///
/// CONSOLE CONSTRUCTION
///

//
// The console doesn't inherit from ReplPad, it *contains* it.  This
// is part of decoupling the dependency, so that QTextEdit features do not
// "creep in" implicitly...making it easier to substitute another control
// (such as a QWebView) for the UI.
//

RenConsole::RenConsole (QWidget * parent) :
    QTabWidget (parent),
    bannerPrinted (false),
    evaluatingRepl (nullptr),
    dialect (none),
    target (none)
{
    // Set up the Evaluator so it's wired up for signals and slots
    // and on another thread from the GUI.  This technique is taken directly
    // from the Qt5 example, and even uses the same naming:
    //
    //   http://doc.qt.io/qt-5/qthread.html#details

    EvaluatorWorker * worker = new EvaluatorWorker;
    worker->moveToThread(&workerThread);
    connect(
        &workerThread, &QThread::finished,
        worker, &QObject::deleteLater,
        Qt::DirectConnection
    );
    connect(
        this, &RenConsole::operate,
        worker, &EvaluatorWorker::doWork,
        Qt::QueuedConnection
    );
    connect(
        worker, &EvaluatorWorker::resultReady,
        this, &RenConsole::handleResults,
        // technically it can't handle more results...but better perhaps
        // to crash than to block if some new invariant is introduced and
        // make it look like it's working.
        Qt::QueuedConnection
    );
    workerThread.start();


    // Define the console dialect.  Debugging in lambdas is not so good in
    // GDB right now, as it won't show locals if the lambda is in a
    // constructor like this.  Research better debug methods.

    consoleFunction = Function::construct(
        "{Default CONSOLE dialect for executing commands in Ren Garden}"
        "arg [block! any-function! string! word! image!]"
        "    {block to execute or other instruction (see documentation)}"
        "/meta {Interpret in 'meta mode' for controlling the dialect}",

        REN_STD_FUNCTION,

        [this](Value const & arg, Value const & meta) -> Value
        {
            if (not meta) {
                // the case that the unmodified CONSOLE make *some* exceptions
                // and throw in some kind of behavior unknown to DO, like
                // a more clever version of HELP that breaks the arity rules
                // entirely.  But there's something to be said for consistency.

                if (arg.isBlock())
                    return runtime("do", arg);

                // Passing in a function is the way of swapping in a new
                // dialect to be used in the console.  It has the opportunity
                // to print out a banner, because it is called with the /meta
                // refinement and a parameter of 'banner.

                if (arg.isFunction()) {
                    Value wordsOf = runtime("words-of quote", arg);

                    Block blk = static_cast<Block>(wordsOf);

                    if (
                        blk.isEmpty()
                        or not (
                            blk[1].isWord()
                            or blk[1].isLitWord()
                            or blk[1].isGetWord()
                        )
                        or ((blk.length() > 1) and not blk[2].isRefinement())
                    ) {
                        throw Error {
                            "Console dialects must be single arity"
                            " (/meta switch for control)}"
                        };
                    }

                    // should we check if arg.isEqualTo(dialect) and do
                    // something special in that case?

                    dialect = arg;
                    if (runtime("find words-of quote", dialect, "/meta"))
                        runtime(Path {dialect, "meta"}, LitWord {"banner"});

                    return unset;
                }

                // Passing a string and having it set the status bar is not
                // any permanent guarantee of a feature, but it's useful for
                // demonstration purposes so let's do that for now.

                if (arg.isString()) {
                    emit reportStatus(to_QString(arg));
                    return unset;
                }

                // Displaying images is not something stdin/stdout is suited
                // for, so at the moment if you want an image displayed on
                // the console you have to use CONSOLE

                if (arg.isImage()) {
                    currentRepl().appendImage(static_cast<Image>(arg), true);
                    return unset;
                }

                throw Error {"More CONSOLE features soon!"};
            }

            if (arg.isWord()) {
                if (arg.isEqualTo<Word>("prompt"))
                    return String {""}; // doesn't add before >> prompt

                if (arg.isEqualTo<Word>("banner")) {
                    if (not bannerPrinted) {
                        printBanner();
                        bannerPrinted = true;
                        return none;
                    }
                }

                // Meta protocol may query you for lit-word features you
                // do not support, so don't raise an error if you don't
                // know what it is... just return none.

                return none;
            }

            if (arg.isBlock()) {
                auto blk = static_cast<Block>(arg);

                if (blk[1].isEqualTo<Word>("target")) {
                    target = blk[2].apply();
                    return unset;
                }

                if (blk[1].isEqualTo<Word>("buffer")) {
                    // The console buffer helper examines the parameter
                    // to "buffer" and tells us what the string is, the
                    // location of the anchor point for the desired selection
                    // and the location of the end point

                    auto triple = static_cast<Block>(runtime(
                        "ren-garden/console-buffer-helper", blk[2]
                    ));

                    pendingBuffer = static_cast<String>(triple[1]);
                    pendingPosition = static_cast<Integer>(triple[2]);
                    pendingAnchor = static_cast<Integer>(triple[3]);

                    return unset;
                }

                throw Error {"Unknown dialect options"};
            }

            throw Error {"Unknown dialect options."};
        }
    );

    runtime(
        "console: quote", consoleFunction,
        "shell: quote", shell.getShellDialectFunction(),

        // A bit too easy to overwrite them, e.g. `console: :shell`
        "protect 'console protect 'shell"
    );


    // Load the the ren-garden console helpers for syntax highlight
    // or other such features.

    helpers = QSharedPointer<RenPackage>::create(
        // resource file prefix
        ":/scripts/",

        // URL prefix...should we assume updating the helpers could break Ren
        // Garden and not offer to update?
        "https://raw.githubusercontent.com/hostilefork/rencpp"
        "/develop/examples/workbench/scripts/",

        Block {
            "%ren-garden.reb"
        }
    );


    // Incubator routines for addressing as many design questions as
    // possible without modifying the Rebol code itself

    proposals = QSharedPointer<RenPackage>::create(
        // resource file prefix
        ":/scripts/rebol-proposals/",

        // URL prefix
        "https://raw.githubusercontent.com/hostilefork/rencpp"
        "/develop/examples/workbench/scripts/rebol-proposals/",

        Block {
            "%combine.reb",
            "%while-until.reb",
            "%print-only-with.reb",
            "%charset-generators.reb",
            "%exit-end-quit.reb",
            "%question-marks.reb",
            "%remold-reform-repend.reb",
            "%to-string-spelling.reb",
            "%find-min-max.reb",
            "%ls-cd-dt-short-names.reb",
            "%for-range-dialect.reb"
        }
    );

    // The beginnings of a test...
    /* proposals->downloadLocally(); */


    // With everything set up, it's time to add our Repl(s)

    setTabsClosable(true); // close button per tab
    setTabBarAutoHide(true); // hide if < 2 tabs

    // http://stackoverflow.com/q/15585793/211160
    setStyleSheet("QTabWidget::pane { border: 0; }");

    connect(
        this, &QTabWidget::currentChanged,
        [this]() {
            // Anything to do when the tab changes?
        }
    );


    connect(
        this, &QTabWidget::tabCloseRequested,
        [this](int index) {
            if (&replFromIndex(index) == evaluatingRepl) {
                // REVIEW: try canceling here with a posted close, and then
                // handle unresponsive cancellations etc.

                emit reportStatus("Can't close evaluator, cancel first.");
                return;
            }

            removeTab(index);
        }
    );

    auto repl = new ReplPad {*this, this};
    addTab(repl, "Main");

    connect(
        repl, &ReplPad::reportStatus,
        this, &RenConsole::reportStatus,
        Qt::AutoConnection // worker thread may report status also
    );


    // Now make console the default dialect we use to interpret commands
    // Switching dialects offers the ability to run (dialect)/meta 'banner,
    // which may print.

    dialect = consoleFunction;
    evaluate("console :console", false);
}



void RenConsole::createNewTab() {

    auto repl = new ReplPad {*this, this};
    addTab(repl, "New Tab");

    connect(
        repl, &ReplPad::reportStatus,
        this, &RenConsole::reportStatus,
        Qt::DirectConnection
    );

    // Give repl a prompt, but we will let replOne get one through the
    // natural ending of the banner execution

    repl->appendNewPrompt();
    setCurrentWidget(repl);
}


void RenConsole::tryCloseTab() {
    if (count() == 1) {
        emit reportStatus(tr("Can't close last tab"));
        return;
    }

    if (evaluatingRepl == &currentRepl()) {
        emit reportStatus(tr("Evaluation in progress, can't close tab"));
        return;
    }

    ReplPad * repl = &currentRepl();
    removeTab(currentIndex());
    delete repl;

    currentRepl().setFocus();
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

    currentRepl().appendImage(QImage (":/images/banner-logo.png"), true);

    QTextCharFormat headerFormat;
    headerFormat.setFont(
        QFont("Helvetica", currentRepl().defaultFont.pointSize() * 1.5)
    );

    // Use a font we set the size explicitly for so this text intentionally
    // does not participate in zoom in and zoom out
    //
    // https://github.com/metaeducation/ren-garden/issues/7

    QTextCharFormat subheadingFormat;
    subheadingFormat.setFont(
        QFont(
            "Helvetica",
            currentRepl().defaultFont.pointSize()
        )
    );
    subheadingFormat.setForeground(Qt::darkGray);

    currentRepl().pushFormat(subheadingFormat);

    std::vector<char const *> components = {
        "<i><b>Red</b> is © 2015 Nenad Rakocevic, BSD License</i>",

        "<i><b>Rebol</b> is © 2015 REBOL Technologies, Apache 2 License</i>",

        "<i><b>Ren</b> is a project by Humanistic Data Initiative</i>",

        "<i><b>RenCpp</b></b> is © 2015 HostileFork.com, Boost License</i>",

        "<i><b>Qt</b> is © 2015 Digia Plc, LGPL 2.1 or GPL 3 License</i>",

    };

    for (auto & credit : components) {
        currentRepl().appendHtml(credit, true);
        currentRepl().appendText("\n", true);
    }

    currentRepl().appendText("\n", true);

    // For the sake of demonstration, get the Ren Garden copyright value
    // out of a context set up in the resource file in ren-garden.reb

    currentRepl().appendHtml(
        static_cast<String>(runtime("ren-garden/copyright")),
        true
    );

    currentRepl().appendText("\n", true);

    currentRepl().appendText("\n");
}



///
/// REPLPAD HOOKS
///

//
// The ReplPad is language-and-evaluator agnostic.  It offers an interface
// that we implement, giving RenConsole the specific behaviors for evaluation
// from RenCpp.
//

bool RenConsole::isReadyToModify(QKeyEvent * event) {

    // No modifying operations while you are running in the console.  You
    // can only request to cancel.  For issues tied to canceling semantics
    // in RenCpp, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/19

    if (evaluatingRepl) {
        if (event->key() == Qt::Key_Escape) {
            if (evaluatingRepl == &currentRepl())
                runtime.cancel();
            else
                emit reportStatus(
                    "Current tab is not running an evaluation to cancel."
                );

            // Message of successful cancellation is given in the console
            // by the handler when the evaluation thread finishes.
            return false;
        }

        emit reportStatus("Evaluation in progress, can't edit");
        return false;
    }

    return true;
}


void RenConsole::evaluate(QString const & input, bool meta) {
    static int count = 0;

    evaluatingRepl = &currentRepl();

    Engine::runFinder().setOutputStream(evaluatingRepl->fakeOut);
    Engine::runFinder().setInputStream(evaluatingRepl->fakeIn);

    emit operate(dialect, input, meta);

    count++;
}


void RenConsole::escape() {
    if (evaluatingRepl) {

        // For issues tied to canceling semantics in RenCpp, see:
        //
        //     https://github.com/hostilefork/rencpp/issues/19

        if (evaluatingRepl == &currentRepl())
            runtime.cancel();
        else
            emit reportStatus("Current tab is not evaluating to be canceled.");
        return;
    }

    if (not dialect.isEqualTo(consoleFunction)) {

        // give banner opportunity or other dialect switch code, which would
        // not be able to run if we just said dialect = consoleFunction

        runtime(consoleFunction, "quote", consoleFunction);

        currentRepl().appendText("\n\n");
        currentRepl().appendNewPrompt();
    }
}


QString RenConsole::getPromptString() {
    // If a function has a /meta refinement, we will ask it via the
    // meta protocol what it wants its prompt to be.

    QString customPrompt;

    if (dialect) {
        if (runtime("find words-of quote", dialect, "/meta"))
            customPrompt = to_QString(runtime(
                Path {dialect, "meta"}, LitWord {"prompt"}
            ));
        else
            customPrompt = "?";
    }

    return customPrompt;
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
    Value const & result
) {
    assert(evaluatingRepl);

    // At the moment, RenCpp actually flushes on every write.  This isn't
    // such a bad idea for stdout, and works well for the console, but may
    // change later...so keep the flush here as a note.

    Engine::runFinder().getOutputStream().flush();

    if (not success) {
        pendingBuffer.clear();

        currentRepl().pushFormat(currentRepl().errorFormat);

        // The value does not represent the result of the operation; it
        // represents the error that was raised while running it (or if
        // that error is unset then assume a cancellation).  Formed errors
        // have an implicit newline on the end implicitly

        if (result)
            currentRepl().appendText(to_QString(result));
        else
            currentRepl().appendText("[Escape]\n");
    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        currentRepl().pushFormat(currentRepl().promptFormatNormal);
        currentRepl().appendText("==");

        currentRepl().pushFormat(currentRepl().inputFormatNormal);
        currentRepl().appendText(" ");

        // Technically this should run through a hook that is "guaranteed"
        // not to hang or crash; we cannot escape out of this call.  So
        // just like to_string works, this should too.

        if (result.isFunction()) {
            currentRepl().appendText("#[function! (");
            currentRepl().appendText(to_QString(runtime("words-of quote", result)));
            currentRepl().appendText(") [...]]");
        }
        else {
            currentRepl().appendText(to_QString(runtime("mold/all quote", result)));
        }

        currentRepl().appendText("\n");
    }

    // Rebol's console only puts in the newline if you get a non-unset
    // evaluation... but here we put one in all cases to space out the prompts

    currentRepl().appendText("\n");

    currentRepl().appendNewPrompt();

    if (not pendingBuffer.isEmpty()) {
        currentRepl().setBuffer(pendingBuffer, pendingPosition, pendingAnchor);
        pendingBuffer.clear();
    }


    // TBD: divide status bar into "command succeeded" and "error" as well
    // as parts controllable by the console automator

    /*emit reportStatus(QString("..."));*/

    emit finishedEvaluation();

    evaluatingRepl = nullptr;
}


void RenConsole::focusInEvent(QFocusEvent *)
{
    currentWidget()->setFocus();
}


///
/// DESTRUCTOR
///

//
// If we try to destroy a RenConsole, there may be a worker thread still
// running that we wait for to quit.
//

RenConsole::~RenConsole() {
    runtime.cancel();
    workerThread.quit();
    if ((not workerThread.wait(1000)) and (not forcingQuit)) {
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
