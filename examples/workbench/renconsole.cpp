//
// Using the current maybe-not-long-term idea (that isn't too terrible), the
// console is just a Qt rich text editor.  It has a special rule that it
// remembers the position in the document where the last command ended, and
// although it will let you select before that point it won't let you modify
// the text--you can only edit in the lines of the current input prompt
// position.  So we have to intercept keypresses and decide whether to let
// them fall through to the text editor's handling.
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

RenConsole::RenConsole (MainWindow * parent) :
    shouldFollow (true),
    parent (parent),
    fakeOut (new FakeStdout (*this)),
    evaluating (false),
    hasUndo (false)
{
    // Set up our safety hook to make sure modifications to the underlying
    // QTextDocument only happen when we explicitly asked for them.

    connect(
        this, &RenConsole::textChanged,
        this, &RenConsole::onTextChanged,
        Qt::DirectConnection
    );


    // It may be possible to do some special processing on advanced MIME
    // types, such as if you pasted a jpeg it could be turned into some
    // kind of image!
    //
    // http://www.qtcentre.org/threads/11735-QTextEdit-paste-rich-text-as-plain-text-only
    //
    // For now we accept the convenience of the default stripping out of
    // rich text information on user pasted text.

    setAcceptRichText(false);


    // We want to be able to append text from threads besides the GUI thread.
    // It is a synchronous operation for a worker, but still goes through the
    // emit process.
    connect(
        this, &RenConsole::needTextAppend,
        this, &RenConsole::onAppendText,
        Qt::BlockingQueuedConnection
    );

    // A pet peeve of mine is when you're trying to do something with a scroll
    // bar and select, and the view jumps out from under you.  If you touch
    // the scroll bars or do any navigation of your own after an evaluation
    // starts...then if you want to go to the end you'll ask for it (by hitting
    // a key and trying to put input in)

    connect(
        verticalScrollBar(), &QScrollBar::valueChanged,
        [this] (int value) {
            if (value == verticalScrollBar()->maximum())
                followLatestOutput();
            else
                dontFollowLatestOutput();
        }
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


    // "Magic Undo" requires us to know when we've crossed the lines from
    // what's available in the document queue to undoing content from the
    // console to edit old data.  We get this via signals, and save in a
    // variable.

    connect(
        this, &QTextEdit::undoAvailable,
        [this] (bool b) {
            hasUndo = b;
        }
    );

    // Print the banner and a prompt.

    QMutexLocker locker {&modifyMutex};
    printBanner();
    endCursor().insertText("\n");
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
}


//
// RenConsole::onConsoleReset()
//

void RenConsole::onConsoleReset() {
    QMutexLocker locker {&modifyMutex};

    clear();
    appendNewPrompt();
}



///
/// RICH-TEXT CONSOLE BEHAVIOR
///


//
// RenConsole::onTextChanged()
//
// Because it's important to know what people did to cause this likely to
// be annoying problem, display an explanatory message before resetting.
//

void RenConsole::onTextChanged() {
    if (modifyMutex.tryLock()) {
        modifyMutex.unlock();

        QMessageBox::information(
            this,
            "Unexpected Modification",
            "Though we give the appearance of the QTextEdit being editable,"
            " it actually can only be edited under the precise moments we"
            " allow.  But putting it in 'read only' mode drops the insertion"
            " cursor.  We try to trap every way of inserting content to"
            " control it, but you found a way around it.  If you remember"
            " what you just did, report it to the bug tracker!"
        );

        emit requestConsoleReset();
    }
}


//
// RenConsole::endCursor()
//
// Helper to get a cursor located at the tail of the QTextDocument underlying
// the console.  (Use textCursor() to get the actual caret location).
//

QTextCursor RenConsole::endCursor() const {
    QTextCursor result {document()};

    result.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    return result;
}



void RenConsole::dontFollowLatestOutput() {
    shouldFollow = false;
}


void RenConsole::followLatestOutput() {
    setTextCursor(endCursor());
    shouldFollow = true;
}


void RenConsole::mousePressEvent(QMouseEvent * event) {
    dontFollowLatestOutput();
    QTextEdit::mousePressEvent(event);
}


void RenConsole::appendText(
    QString const & text,
    QTextCharFormat const & format
) {
    if (RenConsole::thread() == QThread::currentThread()) {
        // blocking connection would cause deadlock.
        onAppendText(text, format);
    }
    else {
        // we need to block in order to properly check for write mutex
        // authority (otherwise we could just queue and run...)
        emit needTextAppend(text, format);
    }
}


void RenConsole::onAppendText(
    QString const & text,
    QTextCharFormat const & format
) {
    QTextCursor cursor = endCursor();

    cursor.insertText(text, format);

    if (shouldFollow) {
        setTextCursor(cursor);

        // For reasons not quite understood, you don't always wind up at the
        // bottom of the document when a scroll happens.  :-/

        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}



//
// RenConsole::appendPrompt()
//
// Append a prompt and remember the cursor's offset into the QTextDocument,
// which we'll use later to find the beginning of the user's input.
//

void RenConsole::appendNewPrompt() {
    QTextCharFormat promptFormat;
    promptFormat.setForeground(Qt::darkGray);
    promptFormat.setFontWeight(QFont::Bold);

    appendText(">>", promptFormat);

    // Text insertions continue using whatever format was set up previously,
    // So clear the format as of the space insertion.

    appendText(" ", QTextCharFormat {});

    // You always have to ask for multi-line mode with shift-enter.  Maybe
    // someone will prefer the opposite?  Not likely.

    history.emplace_back(endCursor().position());

    // We can't allow people to undo their edits past the last prompts,
    // but we do have a "magic Undo" stack that can undo unwanted commands

    document()->clearUndoRedoStacks();
}



//
// RenConsole::getCurrentInput()
//
// Get content from the last prompt all the way to the end.  We do this by
// storing the index of the end of the last prompt text.
//

QString RenConsole::getCurrentInput() const {
    QTextCursor cursor {document()};

    cursor.setPosition(history.back().inputPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    // The misleadingly named QTextCursor::​selectedText() gives you back
    // text with Unicode U+2029 paragraph separator characters instead of a
    // newline \n character, for reasons known only to Qt.

    return cursor.selection().toPlainText();
}


void RenConsole::clearCurrentInput() {
    QTextCursor cursor {document()};

    cursor.setPosition(history.back().inputPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    cursor.removeSelectedText();
    setTextCursor(endCursor());
}


void RenConsole::containInputSelection() {
    QTextCursor cursor {document()};
    cursor.setPosition(
        std::max(textCursor().anchor(), history.back().inputPos)
    );
    cursor.setPosition(
        std::max(textCursor().position(), history.back().inputPos),
        QTextCursor::KeepAnchor
    );
    setTextCursor(cursor);
}



//
// RenConsole::keyPressEvent()
//
// Main keyboard hook for handling input.  Note that input can come from
// other sources--such as the clipboard, and so that has to be intercepted
// as well (to scrub non-plaintext formatting off of information coming from
// web browsers, etc.
//

void RenConsole::keyPressEvent(QKeyEvent * event) {

    int const key = event->key();
    bool shifted = event->modifiers() & Qt::ShiftModifier;
    bool ctrled = event->modifiers() & Qt::ControlModifier;

    QString temp = event->text();

    bool hasRealText = not event->text().isEmpty();
    for (QChar ch : event->text()) {
        if (not ch.isPrint() or (ch == '\t')) {
            // Note that isPrint() includes whitespace, but not \r
            // we try to be sure by handling Enter and Return as if they
            // were not printable.  I have a key on my keyboard labeled
            // "enter RETURN" and it produces "\r", so it's important to
            // handle it by keycode vs. just by the whitespace produced.

            // As Rebol and Red are somewhat "religiously" driven languages,
            // I think that tabs being invisible complexity in source is
            // against that religion.  So the console treats tabs as
            // non-printables, and will trap any attempt to insert the
            // literal character (while substituting with 4 spaces)

            hasRealText = false;
            break;
        }
    }


    // If something has no printable representation, we usually assume
    // getting it in a key event isn't asking us to mutate the document.
    // Thus we can just call the default QTextEdithandling for the navigation
    // or accelerator handling that is needed.
    //
    // There are some exceptions, so we form it as a while loop to make it
    // easier to style with breaks.

    while (not hasRealText) {

        if (
            (key == Qt::Key_Return)
            or (key == Qt::Key_Enter)
            or (key == Qt::Key_Backspace)
            or (key == Qt::Key_Delete)
            or (key == Qt::Key_Tab)
        ) {
            // Though not true for all programs at all times, in the
            // console's case all of these are operations asking to modify
            // the state of the console.  So fall through.
            break;
        }


        if (
            event->matches(QKeySequence::Undo)
            or event->matches(QKeySequence::Redo)
        ) {
            // Undo and Redo are requests for modification to the console.
            // Attempts to hook the QTextEdit's undo and redo behavior with
            // QAction commands were not successful; doing it in this
            // routine seems the only way to override it.

            break;
        }


        if ((key == Qt::Key_Up) or (key == Qt::Key_Down))
            if (not history.back().multiLineMode) {
                // Multi-line mode uses ordinary cursor navigation in the
                // console.  But single line mode up and down are most
                // probably going to be used to implement a "paging through
                // commands history", as traditional in consoles and shells.
                // That would modify the content of the console, hence
                // fall through.
                break;
            }


        // That should be all the modifying operations.  But if we turn out
        // to be wrong and the QTextEdit default handler does modify the
        // document, we'll trap it with an error and then clear console.

        QTextEdit::keyPressEvent(event);
        return;
    }


    // No modifying operations while you are running in the console.  You
    // can only request to cancel.  For issues tied to canceling semantics
    // in RenCpp, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/19

    if (evaluating) {
        if (key == Qt::Key_Escape) {
            ren::runtime.cancel();

            // Message of successful cancellation is given in the console
            // by the handler when the evaluation thread finishes.
            return;
        }

        parent->statusBar()->showMessage("Evaluation in progress, can't edit");
        followLatestOutput();
        return;
    }


    // Whatever we do from here should update the status bar, even to clear
    // it.  We put a test message on to make sure all paths clear.

    parent->statusBar()->showMessage("Unlabeled status path - report!");


    // Temporarily allow writing to the console during the rest of this
    // routine.  Any return or throw will result in the release of the lock
    // by the QMutexLocker's destructor.

    QMutexLocker locker {&modifyMutex};


    // If they have made a selection and have intent to modify, we must
    // contain that selection within the boundaries of the editable area.

    containInputSelection();


    // Command history browsing via up and down arrows currently unwritten.
    // Will need some refactoring to flip the input between single and multi
    // line modes.  Might present some visual oddity swapping really long
    // program segments with really short ones.

    if ((key == Qt::Key_Up) or (key == Qt::Key_Down)) {
        assert(not history.back().multiLineMode);
        parent->statusBar()->showMessage("UP/DOWN history nav not finished.");
        return;
    }


    // Escape is given several meanings depending on the context.  If you
    // are evaluating, it will cancel.  If you are not evaluating, it will
    // clear any input you've given (an undoable action).  If you're not
    // evaluating and it has cleared your input, it will bump you to the
    // outermost shell.

    if ((key == Qt::Key_Escape)) {
        if (not getCurrentInput().isEmpty()) {
            clearCurrentInput();
            return;
        }

        // Nested dialect shells not implemented... YET!

        parent->statusBar()->showMessage(
            "Cannot escape further - you are in the root console dialect."
        );
        return;
    }


    // Testing of an initial magicUndo concept, which will backtrack the work
    // log and take you to where you were from previous evaluations.

    if (event->matches(QKeySequence::Undo)) {
        if (hasUndo) {
            undo();
            return;
        }

        if (history.size() > 1) {
            history.pop_back();

            // Clear from the previous record's endPos to the current end
            // of the document, then position the cursor at the eval pos

            QTextCursor cursor = endCursor();
            cursor.setPosition(history.back().endPos, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            cursor.setPosition(history.back().evalCursorPos);
            setTextCursor(cursor);

            // Keep it from trying to record that "edit to undo" as an
            // undoable action, which causes madness.

            document()->clearUndoRedoStacks();
            return;
        }

        // It's sort of pleasing to be able to go all the way back to zero,
        // so clear the input even though it's a bit of a forgery...

        if (not getCurrentInput().isEmpty()) {
            clearCurrentInput();
            document()->clearUndoRedoStacks();
            return;
        }

        parent->statusBar()->showMessage("Nothing available for undo.");
        return;
    }


    // Behavior of Enter/Return depends on the line mode you are in, due to
    // this fantastic suggestion.  :-)
    //
    //    https://github.com/hostilefork/ren-garden/issues/4
    //
    // Ctrl-Enter always evaluates.  But Enter evaluates when you are in
    // single-line mode.  Shift-Enter switches you into multi-line mode where
    // Enter doesn't evaluate, but Shift-Enter starts acting like ordinary
    // Enter once you're in it.  This reduces accidents.

    if ((key == Qt::Key_Enter) or (key == Qt::Key_Return)) {

        if (
            (not history.back().multiLineMode) and (shifted and (not ctrled))
        ) {
            // Switch from single to multi-line mode

            QString input = getCurrentInput();
            clearCurrentInput();

            QTextCharFormat hintFormat;
            hintFormat.setForeground(Qt::darkGray);
            hintFormat.setFontItalic(true);
            appendText("[ctrl-enter to evaluate]", hintFormat);

            appendText("\n");
            history.back().inputPos = endCursor().position();

            appendText(input);

            history.back().multiLineMode = true;
            return;
        }


        // The user may have entered or pasted an arbitrary amount of
        // whitespace, so the cursor may have no data after it.  Find
        // the last good position (which may be equal to the cursor
        // position if there's no extra whitespace)

        int lastGoodPosition = [&]() {
            QTextCursor cursor = textCursor();

            setTextCursor(endCursor());
            if (not find(QRegExp("[^\\s]"), QTextDocument::FindBackward)) {
                throw std::runtime_error("No non-whitespace in console.");
            }

            int result = textCursor().position();
            setTextCursor(cursor);
            return result;
        }();

        int extraneousNewlines = [&]() {
            if (textCursor().position() <= lastGoodPosition)
                return 0;

            if (not history.back().multiLineMode)
                return 0;

            QTextCursor cursor {document()};
            cursor.setPosition(lastGoodPosition);
            cursor.setPosition(
                endCursor().position(), QTextCursor::KeepAnchor
            );

            return cursor.selection().toPlainText().count("\n");
        }();

        if (
            ctrled or (not history.back().multiLineMode)
            or (extraneousNewlines > 0)
        ) {
            // Perform an evaluation.  But first, clean up all the whitespace
            // at the tail of the input (if it happens after our cursor
            // position.)

            history.back().evalCursorPos = textCursor().position();

            QTextCursor cursor = endCursor();
            cursor.setPosition(
                std::max(
                    textCursor().position(),
                    history.back().evalCursorPos
                ),
                QTextCursor::KeepAnchor
            );
            cursor.removeSelectedText();

            history.back().endPos = cursor.position();

            appendText("\n");

            QString input = getCurrentInput();
            if (input.isEmpty()) {
                appendNewPrompt();
            } else {
                evaluating = true;
                operate(input); // queues request to thread, returns

                parent->statusBar()->showMessage(
                    "Evaluation thread running..."
                );
            }

            followLatestOutput();
            return;
        }


        if (textCursor().anchor() == textCursor().position()) {
            // Try to do some kind of "auto-indent" on enter, by finding
            // out the current line's indentation level, and inserting
            // that many spaces after the newline we insert.

            QTextCursor cursor = textCursor();

            if (not find(QRegExp("^"), QTextDocument::FindBackward)) {
                assert(false);
                return;
            }
            int startLinePos = textCursor().position();
            int endSpacesPos = startLinePos + 1;

            if (find(QRegExp("[^\\s]"))) {
                if (textCursor().position() < cursor.position())
                    endSpacesPos = textCursor().position();
            }

            cursor.insertText("\n");
            cursor.insertText(
                QString(endSpacesPos - startLinePos - 1, QChar::Space)
            );
            setTextCursor(cursor);
            return;
        }

        // Fallthrough for now...but what about having a range selection
        // and hitting enter?  What happens to your indent?
    }


    QString tabString {4, QChar::Space};

    if (textCursor().anchor() != textCursor().position()) {
        // Range selections, we replace them with the event text unless it
        // is a tab, where we entab or detab the content based on shift.

        if (key == Qt::Key_Tab) {
            // "You selected a large range of text and pressed tab?  And you
            // didn't want me to erase it all?  Hmmm...I don't know what this
            // entabbing and detabbing is you mention, but with that I'd have
            // to hit BACKSPACE and tab.  Twice as many keystrokes, for that
            // operation.  That I perform *all* the time"  ;-P

            QString contents = textCursor().selection().toPlainText();
            if (shifted) {
                QString regex {"^"};
                regex += tabString;
                contents.replace(QRegExp(regex), "");
            } else
                contents.replace(QRegExp("^"), tabString);
            textCursor().removeSelectedText();
            textCursor().insertText(contents);
            return;
        }

        // Trust the default method to do the right editing, and not do
        // anything outside the boundaries of the selection

        textCursor().removeSelectedText();

        if ((key != Qt::Key_Backspace) && (key != Qt::Key_Delete))
            textCursor().insertText(event->text());

        return;
    }


    // Selection is collapsed, so just an insertion point.

    if (key == Qt::Key_Tab) {
        textCursor().insertText(tabString);
        return;
    }


    if (key == Qt::Key_Backspace) {
        if (textCursor().position() <= history.back().inputPos) {
            parent->statusBar()->showMessage(
                "Can't backspace beginning of input."
            );
            return;
        }

        // Note: we'd like to outdent tabs
        QTextCursor cursor = textCursor();
        cursor.setPosition(cursor.position() - 4, QTextCursor::KeepAnchor);

        if (cursor.selection().toPlainText() == tabString) {
            cursor.removeSelectedText();
            setTextCursor(cursor);
            return;
        }

        cursor = textCursor();
        cursor.setPosition(cursor.position() - 1, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        setTextCursor(cursor);
        return;
    }

    if (not hasRealText) {
        QMessageBox::information(
            this,
            "Attempt to Insert Non-Printables in Console",
            "Somehow or another, you've managed to enter something or paste"
            " something that wanted to put non-printable (and non-whitespace)"
            " characters into the console.  If you can remember what you did"
            " and do it again, please report it to the bug tracker.  But"
            " to be on the safe side, we're ignoring that insert attempt."
        );
        return;
    }

    textCursor().insertText(event->text());
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

    QTextCursor cursor = endCursor();

    if (not success) {
        QTextCharFormat errorFormat;
        errorFormat.setForeground(Qt::darkRed);

        // The value does not represent the result of the operation; it
        // represents the error that was raised while running it, or if
        // that error is unset then assume a cancellation

        if (result.isUnset()) {
            cursor.insertText("[Escape]\n", errorFormat);
        }
        else {
            // Formed errors have a newline on the end implicitly
            cursor.insertText(static_cast<QString>(result), errorFormat);
        }
    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        QTextCharFormat resultPromptFormat;
        resultPromptFormat.setForeground(Qt::darkGray);
        cursor.insertText("==", resultPromptFormat);

        cursor.insertText(" ", QTextCharFormat {});

        cursor.insertText(static_cast<QString>(result));
        cursor.insertText("\n");
    }

    // I like this newline always, even though Rebol's console only puts in
    // the newline if you get a non-unset evaluation...

    cursor.insertText("\n");

    appendNewPrompt();

    if (delta) {
        parent->statusBar()->showMessage(
            QString("Command completed in ") + static_cast<QString>(delta)
        );
    }

    parent->watchList->updateWatches();

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
