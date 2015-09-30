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
#include "renpackage.h"
#include "evaluator.h"

extern bool forcingQuit;


//
// CONSOLE CONSTRUCTION
//

//
// The console doesn't inherit from ReplPad, it *contains* it.  This
// is part of decoupling the dependency, so that QTextEdit features do not
// "creep in" implicitly...making it easier to substitute another control
// (such as a QWebView) for the UI.
//

RenConsole::RenConsole (EvaluatorWorker * worker, QWidget * parent) :
    QTabWidget (parent),
    helpersContext (),
    userContext (static_cast<Context>(runtime("system/contexts/user"))),
    libContext (static_cast<Context>(runtime("system/contexts/lib"))),
    shell (),
    bannerPrinted (false),
    evaluatingRepl (nullptr),
    target (none),
    proposalsContext (), // we will copy it from userContext when ready...
    useProposals (true)
{
    // Set up the Evaluator so it's wired up for signals and slots
    // and on another thread from the GUI.  This technique is taken directly
    // from the Qt5 example, and even uses the same naming:
    //
    //   http://doc.qt.io/qt-5/qthread.html#details

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

    // Define the console dialect.  Debugging in lambdas is not so good in
    // GDB right now, as it won't show locals if the lambda is in a
    // constructor like this.  Research better debug methods.

    consoleFunction = Function::construct(
        "{Default CONSOLE dialect for executing commands in Ren Garden}"
        "arg [block! any-function! string! word! image! object!]"
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

                    getTabInfo(repl()).dialect = static_cast<Function>(arg);

                    return unset;
                }

                // Passing in an object means it will use that object as the
                // context for this tab.

                if (arg.isContext()) {
                    getTabInfo(repl()).context = static_cast<Context>(arg);
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

                    auto triple = static_cast<Block>((*helpersContext)(
                        "console-buffer-helper", blk[2]
                    ));

                    // Helpers speak Rebol conventions by design, so we have
                    // to subtract from the 1-based index for C++ indexing

                    pendingBuffer = static_cast<String>(triple[1]);
                    pendingPosition = static_cast<Integer>(triple[2]) - 1;
                    pendingAnchor = static_cast<Integer>(triple[3]) - 1;

                    return unset;
                }

                if (blk[1].isEqualTo<Word>("tab")) {
                    if (blk[2].isTag()) {
                        getTabInfo(repl()).label = static_cast<Tag>(blk[2]);
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

            WatchList & watchList = *getTabInfo(repl()).watchList;

            if (not arg.isBlock())
                return watchList.watchDialect(arg, not useResult, nullopt);

            Block aggregate {};

            optional<Tag> nextLabel;
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
                    // !!! exception handling if they watch something
                    // with no value in the block form?  e.g. watch [x y]
                    // and x gets added as a watch where both undefined,
                    // but y doesn't?

                    Value watchResult
                        = watchList.watchDialect(
                            item, nextRecalculates, nextLabel
                        );
                    nextRecalculates = true;
                    nextLabel = nullopt;
                    if (not watchResult.isUnset())
                        runtime("append", aggregate, watchResult);
                }
            }
            return aggregate;
        }
    );

    // This is a placeholder for a test of a question of "what good might Ren
    // data be without an evaluator, and to force dealing with the question.
    // The package lists themselves are candidates for the question; to load
    // and process them as data without binding them into a context or
    // assigning them to variables.

    rendataPackage = QSharedPointer<RenPackage>::create(
        // resource file prefix
        ":/rendata/",

        // URL prefix...
        "https://raw.githubusercontent.com/hostilefork/rencpp"
        "/develop/examples/workbench/rendata",

        Block {
            "%copyright.ren"
        },

        nullopt // do not load into any context, just data
    );



    // Incubator routines for addressing as many design questions as
    // possible without modifying the Rebol code itself.  The COMBINE
    // assignment to JOIN is done here internally vs. in the proposal itself,
    // as a practical issue to co-evolve COMBINE-the-proposal even with
    // those who defender of the current JOIN.

    proposalsContext = userContext.copy(false);

    // !!! On MinGW 4.9.1 under Windows, there is an issue where if you pass
    // the result from dereferencing a std::optional<Context> through perfect
    // forwarding (as in the constructor below) it will attempt to std::move
    // the variable.  The "RenPackage" was an experiment in the first place,
    // and after looking into this enough to believe it to be more likely
    // a compiler bug than an issue in Ren/C++ class design a workaround is
    // the best choice for the moment.  So we force to a reference to prevent
    // the choice of && from *proposalsContext;
    Context const & proposalsRef = *proposalsContext;

    proposalsPackage = QSharedPointer<RenPackage>::create(
        // resource file prefix
        ":/scripts/rebol-proposals/",

        // URL prefix
        "https://raw.githubusercontent.com/hostilefork/rencpp"
        "/develop/examples/workbench/scripts/rebol-proposals/",

        Block {
            "%comparison-operators.reb",
            "%combine.reb",
            "%while-until.reb",
            "%print-only-with.reb",
            "%charset-generators.reb",
            "%exit-end-quit.reb",
            "%remold-reform-repend.reb",
            "%to-string-spelling.reb",
            "%find-min-max.reb",
            "%ls-cd-dt-short-names.reb",
            "%loop-dialect.reb",
            "%object-context.reb",
            "%help-dialect.reb"
        },

        proposalsRef
    );

    (*proposalsContext)(
        "concat: :join", // I don't care what you call it, it's bad
        "join: :combine" // Righful owner of the fitting name...
    );

    // go ahead and take the good print, even in "vanilla" Rebol, and also
    // include COMBINE

    userContext(
        "print: quote", (*proposalsContext)(":print"),
        "combine: quote", (*proposalsContext)(":combine")
    );


    // The beginnings of a test...
    /* proposalsPackage->downloadLocally(); */


    // Load Rebol code from the helpers module.  See the description of the
    // motivations and explations in:
    //
    //     /scripts/helpers/README.md
    //
    // Because the helpers rely upon the implementation in proposals, they
    // have to be bound against the functions in proposals.  The lack of
    // an ability to have a hierarchy besides the hardcoded LIB means we
    // have to load into a copy of proposals.

    helpersContext = proposalsContext->copy();

    // !!! See notes above on MinGW 4.9.1 workaround
    Context const & helpersRef = *helpersContext;

    helpersPackage = QSharedPointer<RenPackage>::create(
        // resource file prefix
        ":/scripts/helpers/",

        // URL prefix...should we assume updating the helpers could break Ren
        // Garden and not offer to update?
        "https://raw.githubusercontent.com/hostilefork/rencpp"
        "/develop/examples/workbench/scripts/helpers",

        Block {
            "%shell.reb",
            "%edit-buffer.reb",
            "%autocomplete.reb"
        },

        helpersRef
    );

    // The shell relies on the helpers, so we couldn't initialize it until
    // this point...

    shell.reset(new RenShell(*helpersContext));


    // make it possible to get at the proposals context from both user
    // and proposals, and also install the console extensions in both

    for (auto context : std::vector<Context>{*proposalsContext, userContext}) {
        context(
            "append system/contexts", Block {"proposals:", *proposalsContext},

            "console: quote", consoleFunction,
            "shell: quote", shell->getShellDialectFunction(),
            "watch: quote", watchFunction,

            // A bit too easy to overwrite them, e.g. `console: :shell`
            "protect 'console",
            "protect 'shell"
        );
    }

    // With everything set up, it's time to add our Repl(s)

    setTabsClosable(true); // close button per tab
    setTabBarAutoHide(true); // hide if < 2 tabs

    // http://stackoverflow.com/q/15585793/211160
    setStyleSheet("QTabWidget::pane { border: 0; }");

    connect(
        this, &QTabWidget::currentChanged,
        [this](int) {
            emit switchWatchList(getTabInfo(repl()).watchList);
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

    Context context = useProposals
        ? proposalsContext->copy()
        : userContext.copy();

    auto emplacement = tabinfos.emplace(std::make_pair(pad,
        TabInfo {
            static_cast<Function>(consoleFunction),
            (count() > 0) ? nullopt : optional<Tag>{Tag {"&Main"}},
            context,
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

    evaluate(*pad, "console :console", false);
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

    auto it = tabinfos.find(pad);

    removeTab(index);
    delete pad;
    delete it->second.watchList;
    tabinfos.erase(it);

    repl().setFocus();
}


void RenConsole::updateTabLabels() {
    for (int index = 0; index < count(); index++) {
        auto & label = getTabInfo(replFromIndex(index)).label;

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

    // Here is an investigation into using .REN files in a C++ program, via
    // a resource file which presumes no runtime, and uses RenCpp in a
    // sort of "JSON parser" way.  Very simple test to get that question
    // started--get the Ren Garden copyright notice.  The file says:
    //
    //      Ren [
    //         ...
    //      ]
    //
    //      copyright: {...}
    //
    // But since it's not a Rebol header, when Rebol LOADs it we get the
    // header and the data as is.  We have to skip through the structure
    // ourselves to get what we want.

    auto copyrightData = static_cast<Block>(
        rendataPackage->getData(Filename {"copyright.ren"})
    );

    assert(copyrightData[1].isEqualTo<Word>("Ren"));
    assert(copyrightData[2].isBlock());
    assert(copyrightData[3].isEqualTo<SetWord>("copyright"));

    repl().appendHtml(static_cast<String>(copyrightData[4]), true);

    repl().appendText("\n", true);

    repl().pushFormat(repl().outputFormat);
    repl().appendText("\n");
}



//
// REPLPAD HOOKS
//

//
// The ReplPad is language-and-evaluator agnostic.  It offers an interface
// that we implement, giving RenConsole the specific behaviors for evaluation
// from RenCpp.
//

bool RenConsole::isReadyToModify(ReplPad & pad, bool escaping) {

    // No modifying operations while you are running in the console.  You
    // can only request to cancel.  For issues tied to canceling semantics
    // in RenCpp, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/19

    if (evaluatingRepl) {
        if (escaping) {
            if (evaluatingRepl == &pad)
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


void RenConsole::evaluate(ReplPad & pad, QString const & input, bool meta) {
    Engine::runFinder().setOutputStream(pad.fakeOut);
    Engine::runFinder().setInputStream(pad.fakeIn);

    auto tabinfo = getTabInfo(pad);

    // Allow the output text to be a different format

    pad.pushFormat(pad.outputFormat);

    evaluatingRepl = &pad;

    emit operate(tabinfo.dialect, tabinfo.context, input, meta);
}


void RenConsole::escape(ReplPad & pad) {
    if (evaluatingRepl) {

        // For issues tied to canceling semantics in RenCpp, see:
        //
        //     https://github.com/hostilefork/rencpp/issues/19

        if (evaluatingRepl == &pad)
            runtime.cancel();
        else
            emit reportStatus("Current tab is not evaluating to be canceled.");
        return;
    }

    if (not getTabInfo(pad).dialect.isEqualTo(consoleFunction)) {

        // give banner opportunity or other dialect switch code, which would
        // not be able to run if we just said dialect = consoleFunction

        getTabInfo(pad).context(consoleFunction, "quote", consoleFunction);

        pad.appendText("\n\n");
        pad.appendNewPrompt();
    }
}


QString RenConsole::getPromptString(ReplPad & pad) {
    // If a function has a /meta refinement, we will ask it via the
    // meta protocol what it wants its prompt to be.

    QString customPrompt;

    auto dialect = getTabInfo(pad).dialect;

    if (runtime("find words-of quote", dialect, "/meta"))
        customPrompt = to_QString(runtime(
            Path {dialect, "meta"}, LitWord {"prompt"}
        ));
    else
        customPrompt = "?";

    return customPrompt;
}



//
// EVALUATION RESULT HANDLER
//

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



//
// SYNTAX-SENSITIVE HOOKS
//

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
    QString const & text,
    int index,
    bool backward
) {
    // Currently we use the knowledge that all contexts are inheriting from
    // LIB, so we search this tab's context and then fall back upon LIB
    // for other finding.  DocKimbel has suggested that Red's contexts
    // may not have any complex hierarchy, so more understanding is needed

    Block contexts {getTabInfo(repl()).context, libContext};

    optional<Block> completion;

    try {
        completion = static_cast<Block>((*helpersContext)(
            backward
                ? "autocomplete-helper/backward"
                : "autocomplete-helper",
            contexts,
            text,
            index + 1 // Rebol conventions, make cursor position 1-base
        ));

        // We send even if it's unset, to clear the panel.  Is that a
        // good idea or not?

        emit exploreValue((*proposalsContext)(":help"), (*completion)[3]);

        return std::pair<QString, int> {
            static_cast<String>((*completion)[1]),
            static_cast<Integer>((*completion)[2]) - 1 // C++ conventions
        };
    }
    catch (std::exception const & e) {
        // Some error during the helper... tell user so (but don't crash)
        auto msg = e.what();
        QMessageBox::information(
            NULL,
            "autocomplete-helper internal error",
            msg
        );
    }
    catch (...) {
        assert(false); // some non-std::exception??  :-/
        throw;
    }

    return std::make_pair(text, index);
}



//
// DESTRUCTOR
//

//
// If we try to destroy a RenConsole, there may be a worker thread still
// running that we wait for to quit.
//

RenConsole::~RenConsole() {
    runtime.cancel();
}


void RenConsole::setUseProposals(bool useProposals) {
    this->useProposals = useProposals;

    std::string command {"console system/contexts/"};
    command += useProposals ? "proposals" : "user";
    getTabInfo(repl()).context(command.c_str());
}
