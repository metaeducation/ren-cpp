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

#include <array>
#include <vector>

#include <QtWidgets>

#include "rencpp/ren.hpp"

using namespace ren;

#include "renconsole.h"
#include "mainwindow.h"
#include "fakestdio.h"
#include "watchlist.h"
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
    void doWork(
        ren::Value const & dialectValue,
        ren::Value const & contextValue,
        QString const & input,
        bool meta
    ) {
        // See notes on MainWindow about qRegisterMetaType about why dialect
        // and context are passed as ren::Value instead of ren::Function and
        // ren::Context (also why it needs the ren:: prefix for slots)

        Function dialect = static_cast<Function>(dialectValue);
        Context context = static_cast<Context>(contextValue);

        Value result;
        bool success = false;

        try {
            // We *always* generate a block to pass to the dialect.  This
            // is Ren Garden, not "arbitrary shell"... so if you want to
            // pass an arbitrary string you must type it in as {49+3hfa} in
            // a properly loadable string.

            auto loaded = context.create<Block>(input.toUtf8().constData());

            if (meta) {
                if (not runtime("find words-of quote", dialect, "/meta"))
                    throw Error {"current dialect has no /meta refinement"};

                result = context(Path {dialect, "meta"}, loaded);
            }
            else
                result = context(dialect, loaded);

            success = true;
        }
        catch (evaluation_error const & e) {
            result = e.error();
        }
        catch (exit_command const & e) {
            qApp->exit(e.code());
        }
        catch (evaluation_cancelled const & e) {
            // Let returning none for the error mean cancellation
            result = none;
        }
        catch (...) {
            // some other C++ error was thrown (shouldn't be possible)
            assert(false);
            throw;
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

                    if (runtime("find words-of quote", arg, "/meta"))
                        runtime(Path {arg, "meta"}, LitWord {"banner"});

                    auto it = tabinfo.find(&repl());
                    it->second.dialect = static_cast<Function>(arg);

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
                    repl().appendImage(static_cast<Image>(arg), true);
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

                if (blk[1].isEqualTo<Word>("tab")) {
                    if (blk[2].isTag()) {
                        auto it = tabinfo.find(&repl());
                        it->second.label = static_cast<Tag>(blk[2]);
                        return unset;
                    }
                }

                throw Error {"Unknown dialect options"};
            }

            throw Error {"Unknown dialect options."};
        }
    );

    // Create the function that will be added to the environment to be bound
    // to the word WATCH.  If you give it a word or path it will be quoted,
    // but if you give it a paren it will be an expression to evaluate.  If
    // you give it a block it will be interpreted in the "watch dialect".
    // Feature under development.  :-)

    // We face a problem here that Ren is not running on the GUI thread.
    // That's because we want to be able to keep the GUI responsive while
    // running.  But we have to manage our changes on the data structures
    // by posting messages.

    // Because we're quoting it's hard to get a logic, so the reserved
    // words for on, off, true, false, yes, and no are recognized explicitly
    // Any logic value could be used with parens however, and if those words
    // have been reassigned to something else the parens could work for that
    // as well.

    auto watchFunction = Function::construct(
        "{WATCH dialect for monitoring and un-monitoring in the Ren Workbench}"
        ":arg [word! path! block! paren! integer! tag!]"
        "    {word to watch or other legal parameter, see documentation)}"
        "/result {watch the result of the evaluation (not the expression)}",

        REN_STD_FUNCTION,

        [this](Value const & arg, Value const & useResult) -> Value {

            auto it = tabinfo.find(&repl());
            WatchList * watchList = it->second.watchList;

            if (not arg.isBlock())
                return watchList->watchDialect(
                    arg, not useResult, std::experimental::nullopt
                );

            std::experimental::optional<Tag> nextLabel;

            Block aggregate {};

            bool nextRecalculates = true;

            for (Value item : static_cast<Block>(arg)) {
                if (item.isTag()) {
                    nextLabel = static_cast<Tag>(item);
                }
                else if (item.isRefinement()) {
                    if (item.isEqualTo<Refinement>("result")) {
                        nextRecalculates = false;
                    }
                    else
                        throw Error {
                            "only /result refinement is supported in blocks"
                        };
                }
                else {
                    // REVIEW: exception handling if they watch something
                    // with no value in the block form?  e.g. watch [x y]
                    // and x gets added as a watch where both undefined,
                    // but y doesn't?

                    Value watchResult
                        = watchList->watchDialect(
                            item, nextRecalculates, nextLabel
                        );
                    nextRecalculates = true;
                    nextLabel = std::experimental::nullopt;
                    if (not watchResult.isUnset())
                        runtime("append", aggregate, watchResult);
                }
            }
            return aggregate;
        }
    );


    runtime(
        "console: quote", consoleFunction,
        "shell: quote", shell.getShellDialectFunction(),
        "watch: quote", watchFunction,

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
            "%for-range-dialect.reb",
            "%object-context.reb"
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
        [this](int) {
            auto it = tabinfo.find(&repl());
            emit switchWatchList(it->second.watchList);
        }
    );


    connect(
        this, &QTabWidget::tabCloseRequested,
        [this](int index) {
            tryCloseTab(index);
        }
    );

    createNewTab();
}



void RenConsole::createNewTab() {

    if (evaluatingRepl) {
        emit reportStatus(tr("Can't spawn tab during evaluation."));
        return;
    }

    auto pad = new ReplPad {*this, *this, this};

    // should be able to .copy() user but it's not working.  Trying to see
    // why not...

    /*std::experimental::optional<Context> context;
    if (count() == 0)
        context = Context {};
    else {
        auto it = tabinfo.find(&replFromIndex(0));
        context = it->second.context.copy(false);
    }*/

    auto emplacement = tabinfo.emplace(std::make_pair(pad,
        TabInfo {
            static_cast<Function>(consoleFunction),
            (count() > 0)
                ? std::experimental::nullopt
                : std::experimental::optional<Tag>{Tag {"&Main"}},
            Context::lookup("USER").copy(),
            new WatchList (nullptr)
        }
    ));

    addTab(pad, "(unnamed)");

    connect(
        pad, &ReplPad::reportStatus,
        this, &RenConsole::reportStatus,
        Qt::DirectConnection
    );

    connect(
        this, &RenConsole::finishedEvaluation,
        (emplacement.first)->second.watchList, &WatchList::updateAllWatchers,
        Qt::DirectConnection
    );

    connect(
        (emplacement.first)->second.watchList, &WatchList::showDockRequested,
        this, &RenConsole::showDockRequested,
        Qt::DirectConnection
    );

    connect(
        (emplacement.first)->second.watchList, &WatchList::hideDockRequested,
        this, &RenConsole::hideDockRequested,
        Qt::DirectConnection
    );

    connect(
        (emplacement.first)->second.watchList, &WatchList::reportStatus,
        this, &RenConsole::reportStatus,
        Qt::DirectConnection
    );

    setCurrentWidget(pad);

    evaluate("console :console", false);
}


void RenConsole::tryCloseTab(int index) {
    if (count() == 1) {
        emit reportStatus(tr("Can't close last tab"));
        return;
    }

    ReplPad * pad = &replFromIndex(index);

    if (evaluatingRepl == pad) {
        emit reportStatus(tr("Evaluation in progress, can't close tab"));
        return;
    }

    auto it = tabinfo.find(pad);

    removeTab(index);
    delete pad;
    delete it->second.watchList;
    tabinfo.erase(it);

    repl().setFocus();
}


void RenConsole::updateTabLabels() {
    for (int index = 0; index < count(); index++) {
        auto it = tabinfo.find(&replFromIndex(index));
        auto label = it->second.label;

        // Use a number (starting at 1) followed by the label (if there is one)

        QString text {to_string(index + 1).c_str()};
        text += ". ";
        if (label)
            text += (*label).spellingOf<QString>();

        setTabText(index, text);
    }
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

    repl().appendImage(QImage (":/images/banner-logo.png"), true);

    QTextCharFormat headerFormat;
    headerFormat.setFont(
        QFont("Helvetica", repl().defaultFont.pointSize() * 1.5)
    );

    // Use a font we set the size explicitly for so this text intentionally
    // does not participate in zoom in and zoom out
    //
    // https://github.com/metaeducation/ren-garden/issues/7

    QTextCharFormat subheadingFormat;
    subheadingFormat.setFont(
        QFont(
            "Helvetica",
            repl().defaultFont.pointSize()
        )
    );
    subheadingFormat.setForeground(Qt::darkGray);

    repl().pushFormat(subheadingFormat);

    std::vector<char const *> components = {
        "<i><b>Red</b> is © 2015 Nenad Rakocevic, BSD License</i>",

        "<i><b>Rebol</b> is © 2015 REBOL Technologies, Apache 2 License</i>",

        "<i><b>Ren</b> is a project by Humanistic Data Initiative</i>",

        "<i><b>RenCpp</b></b> is © 2015 HostileFork.com, Boost License</i>",

        "<i><b>Qt</b> is © 2015 Digia Plc, LGPL 2.1 or GPL 3 License</i>",

    };

    for (auto & credit : components) {
        repl().appendHtml(credit, true);
        repl().appendText("\n", true);
    }

    repl().appendText("\n", true);

    // For the sake of demonstration, get the Ren Garden copyright value
    // out of a context set up in the resource file in ren-garden.reb

    repl().appendHtml(
        static_cast<String>(runtime("ren-garden/copyright")),
        true
    );

    repl().appendText("\n", true);

    repl().pushFormat(repl().outputFormat);
    repl().appendText("\n");
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
            if (evaluatingRepl == &repl())
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


void RenConsole::evaluate(
    QString const & input,
    bool meta
) {
    static int count = 0;

    evaluatingRepl = &repl();

    Engine::runFinder().setOutputStream(evaluatingRepl->fakeOut);
    Engine::runFinder().setInputStream(evaluatingRepl->fakeIn);

    auto it = tabinfo.find(evaluatingRepl);

    // Allow the output text to be a different format

    evaluatingRepl->pushFormat(evaluatingRepl->outputFormat);

    emit operate(
        it->second.dialect,
        it->second.context,
        input,
        meta
    );

    count++;
}


void RenConsole::escape() {
    if (evaluatingRepl) {

        // For issues tied to canceling semantics in RenCpp, see:
        //
        //     https://github.com/hostilefork/rencpp/issues/19

        if (evaluatingRepl == &repl())
            runtime.cancel();
        else
            emit reportStatus("Current tab is not evaluating to be canceled.");
        return;
    }

    auto it = tabinfo.find(&repl());
    if (not it->second.dialect.isEqualTo(consoleFunction)) {

        // give banner opportunity or other dialect switch code, which would
        // not be able to run if we just said dialect = consoleFunction

        runtime(consoleFunction, "quote", consoleFunction);

        repl().appendText("\n\n");
        repl().appendNewPrompt();
    }
}


QString RenConsole::getPromptString() {
    // If a function has a /meta refinement, we will ask it via the
    // meta protocol what it wants its prompt to be.

    QString customPrompt;

    auto it = tabinfo.find(&repl());
    if (runtime("find words-of quote", it->second.dialect, "/meta"))
        customPrompt = to_QString(runtime(
            Path {it->second.dialect, "meta"}, LitWord {"prompt"}
        ));
    else
        customPrompt = "?";

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

    // For help with debugging context issues (temporary)

    /*auto it = tabinfo.find(evaluatingRepl);
    evaluatingRepl->appendText(to_QString(it->second.context));
    evaluatingRepl->appendText("\n");*/

    if (not success) {
        pendingBuffer.clear();

        evaluatingRepl->pushFormat(repl().errorFormat);

        // The value does not represent the result of the operation; it
        // represents the error that was raised while running it (or if
        // that error is unset then assume a cancellation).  Formed errors
        // have an implicit newline on the end implicitly

        if (result)
            evaluatingRepl->appendText(to_QString(result));
        else
            evaluatingRepl->appendText("[Escape]\n");
    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        evaluatingRepl->pushFormat(repl().promptFormatNormal);
        evaluatingRepl->appendText("==");

        evaluatingRepl->pushFormat(repl().inputFormatNormal);
        evaluatingRepl->appendText(" ");

        // Technically this should run through a hook that is "guaranteed"
        // not to hang or crash; we cannot escape out of this call.  So
        // just like to_string works, this should too.

        if (result.isFunction()) {
            evaluatingRepl->appendText("#[function! (");
            evaluatingRepl->appendText(
                to_QString(runtime("words-of quote", result))
            );
            evaluatingRepl->appendText(") [...]]");
        }
        else {
            evaluatingRepl->appendText(
                to_QString(runtime("mold/all quote", result))
            );
        }

        evaluatingRepl->appendText("\n");
    }

    // Rebol's console only puts in the newline if you get a non-unset
    // evaluation... but here we put one in all cases to space out the prompts

    evaluatingRepl->appendText("\n");

    evaluatingRepl->appendNewPrompt();

    if (not pendingBuffer.isEmpty()) {
        evaluatingRepl->setBuffer(
            pendingBuffer, pendingPosition, pendingAnchor
        );
        pendingBuffer.clear();
    }


    // TBD: divide status bar into "command succeeded" and "error" as well
    // as parts controllable by the console automator

    /*emit reportStatus(QString("..."));*/

    updateTabLabels();

    emit finishedEvaluation();

    evaluatingRepl = nullptr;
}



///
/// SYNTAX-SENSITIVE HOOKS
///

//
// Ideally this would be done with separately sandboxed "Engines", a feature
// supported by RenCpp's scaffolding but not currently by the Rebol or Red
// runtimes.  But without sandboxed engines it can still be possible; Rebol
// TASK! has some thread local storage that can do transcode on multiple
// threads independently.
//

std::pair<int, int> RenConsole::rangeForWholeToken(
    QString buffer, int offset
) const {
    if (buffer.isEmpty())
        return std::make_pair(0, 0);


    // Took a shot at using PARSE instead of the RegEx.  This code snippet
    // does not currently work, however.  Two reasons:
    //
    //     >> parse "abb" [copy x thru ["a" any "b"] (print x)]
    //     "a"
    //     == false
    //
    // That should return "abb".  (In Red it does.)  Also, it complains
    // about the `negate ws` charset being used in the position it is
    // as a "parse error"

#ifdef REBOL_PARSE_FOR_AUTOCOMPLETE
    auto blk = static_cast<Block>(runtime(
        "wrap", Block {
            "ws: whitespace/ascii "
            "sym: negate ws " // why not negate ws?
            "do", Block {
                "parse", String {leading}, "[ "
                    "(pos: 0) "
                    "any [ws (++ pos)] "
                    "some [ "
                        "copy stem thru [ "
                            "sym (start: pos) "
                            "any [sym (++ pos)] "
                        "] "
                    "] "
                    "(finish: pos) "
                "] "
             },
            "probe reduce [start finish] "
        }
    ));

    assert(blk[1].isInteger());
    assert(blk[2].isInteger());

    int start = static_cast<Integer>(blk[1]);
    int finish = static_cast<Integer>(blk[2]);
#else
    int start = buffer.lastIndexOf(QRegExp {"[\\s][^\\s]"}, offset);
    if (start == -1)
        start = 0;
    else
        start++;
    int finish = buffer.indexOf(QRegExp {"[^\\s][\\s]"}, offset);
    if (finish == -1)
        finish = buffer.length();
#endif

    return std::make_pair(start, finish);
}


std::pair<QString, int> RenConsole::autoComplete(
    QString const & text, int index, bool backwards
) const {
    // TBD: Reverse iterators to run this process backwards
    // http://stackoverflow.com/questions/8542591/
    if (backwards)
        throw std::runtime_error("Backwards autoComplete traversal not written yet.");

    // Currently we use the knowledge that all contexts are inheriting from
    // LIB, so we search this tab's context and then fall back upon LIB
    // for other finding.  But we need better lookup than that (e.g.
    // contextual, noticing if it's a path and doing some sort of eval
    // to see if we're picking outside of an object).  Really this should
    // be calling through the runtime for the whole thing, but starting
    // it as C++ to define the interface.

    auto it = tabinfo.find(&repl());

    std::array<Context, 2> contexts = {{
        it->second.context,
        Context::lookup("LIB")
    }};

    QString stem = text.left(index);
    QString firstCandidate;
    QString candidate;
    bool takeNext = false;

    for (
        auto itContext = contexts.begin();
        itContext != contexts.end();
        itContext++
    ) {
        Context & context = *itContext;

        Block words = static_cast<Block>(runtime("words-of", context));
        for (auto value : words) {
            Word word = static_cast<Word>(value);
            QString spelling = word.spellingOf<QString>();
            if (
                (spelling.indexOf(stem) == 0)
                and (not runtime("unset?", LitPath {context, word}))
            ) {
                if (takeNext) {
                    // If we saw the exact word we were looking for, only
                    // take it if it doesn't exist in a prior ('higher
                    // priority') context

                    bool outranked = false;
                    for (
                        auto itPriority = contexts.begin();
                        itPriority != itContext;
                        it++
                    ) {
                        auto path = (*itPriority).create<LitPath>(
                            *itPriority, spelling
                        );

                        if (runtime("not unset?", path)) {
                            outranked = true;
                            break;
                        }
                    }

                    if (not outranked)
                        return std::make_pair(spelling, index);
                }

                if (firstCandidate.isEmpty())
                    firstCandidate = spelling;

                if (spelling == text)
                    takeNext = true;
                else
                    candidate = spelling;
            }
        }
    }

    if (takeNext and (not firstCandidate.isEmpty()))
        return std::make_pair(firstCandidate, index);

    if (not candidate.isEmpty())
        return std::make_pair(candidate, index);

    return std::make_pair(text, index);
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
