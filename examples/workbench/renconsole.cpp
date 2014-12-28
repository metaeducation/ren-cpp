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
    shouldTrack (true),
    parent (parent),
    fakeOut (new FakeStdout (*this)),
    evaluating (false)
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
        [this] (int value) -> void {
            if (value == verticalScrollBar()->maximum())
                startTracking();
            else
                stopTracking();
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



void RenConsole::stopTracking() {
    shouldTrack = false;
}


void RenConsole::startTracking() {
    setTextCursor(endCursor());
    shouldTrack = true;
}


void RenConsole::mousePressEvent(QMouseEvent * event) {
    stopTracking();
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

    if (shouldTrack) {
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

    inputPos = endCursor().position();

    // We can't allow people to undo their edits past the last prompts.
    // (Though it would be kind of interesting, in a Time-Traveling-Debugger
    // kind of way...if you undid your editing in the Workbench and if it
    // went back to the previous eval the side effects rolled back...)
    //
    // http://debug.elm-lang.org/

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

    cursor.setPosition(inputPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    // The misleadingly named QTextCursor::​selectedText() gives you back
    // text with Unicode U+2029 paragraph separator characters instead of a
    // newline \n character, for reasons known only to Qt.

    return cursor.selection().toPlainText();
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
    Qt::KeyboardModifiers const modifiers = event->modifiers();


    // In the special case of a Ctrl-C or Escape, we want to interrupt
    // any evaluations in progress.  For cancel semantics, see:
    //
    //     https://github.com/hostilefork/rencpp/issues/19
    //
    // If no evaluations are in progress, then we want to "reset" the
    // console so that if we are hooking to a different command interpreter
    // than the default, we return to the default.

    if (
        ((key == Qt::Key_C) and (modifiers & Qt::ControlModifier))
        or (key == Qt::Key_Escape)
    ) {
        ren::runtime.cancel();
        return;
    }


    // Enter (or "Return", sometimes a different key, but we give the same
    // meaning) acts differently depending on whether you've pressed
    // shift or not.  If you press shift, it enters a newline.  If you don't
    // press shift, it evaluates.
    //
    // REVIEW: A feature like in Rebol2 where unclosed braces are detecte
    // and it refuses to evaluate may be nice.

    if (
        ((key == Qt::Key_Enter) or (key == Qt::Key_Return))
        and not (modifiers & Qt::ShiftModifier)
    ) {
        // A request to evaluate always starts tracking and jumps to the end,
        // whether we can do anything or not.

        startTracking();

        // Don't start nested evaluations!
        if (evaluating)
            return;

        QMutexLocker locker {&modifyMutex};

        appendText("\n");

        QString input = getCurrentInput();
        if (input.isEmpty()) {
            appendNewPrompt();
        } else {
            evaluating = true;
            operate(input); // queues request to thread, returns
        }

        return;
    }


    // Here we handle any operations that are attempts by the user to edit
    // the document.  Some of these will be rejected if they cannot be
    // adjusted to be meaningful only in the user editing area.

    if (
        (key == Qt::Key_Backspace) or (key == Qt::Key_Delete)
        or (not event->text().isEmpty())
    ) {
        // Jump to the end of input if evaluating
        if (evaluating) {
            startTracking();
            return;
        }

        QString temp = event->text();
        QTextCursor cursor = textCursor();

        // Whatever we wind up doing here should clear the status bar message,
        // if nothing else to put a message up for why we didn't do anything
        // with their input (TBD)

        parent->statusBar()->clearMessage();

        // If they hit backspace, we might need to jump the caret to the end of
        // the input and

        bool backspace = event->key() == Qt::Key_Backspace;

        auto blockedEdit = [&]() -> bool {
            return (cursor.anchor() < (backspace ? inputPos + 1 : inputPos))
            or (cursor.position() < (backspace ? inputPos + 1 : inputPos));
        };

        // Now, we'll only take the keypress if the start and end are in the
        // prompt editing area... if not, we clear selection and jump them
        // to the end.

        if (blockedEdit()) {
            startTracking();
        }

        // If we jumped the input to the end and it's still not okay, they're
        // at a blank prompt and trying to backspace...

        if (blockedEdit()) {
            assert(backspace);
            return;
        }

        // Temporarily enable writing and call the default keypress handler
        // for the text edit (trusting it won't do anything unpredictable!)
        // To be perfectly safe, the editing operation should be handled
        // here in code (in case "Backspace" got mapped to something that
        // will go destroy parts we aren't expecting)

        QMutexLocker locker {&modifyMutex};
        QTextEdit::keyPressEvent(event);

        return;
    }


    // Fall through to the default handler; any modification attempts will be
    // caught by the onDocumentChanged hook and lead to resetting the console

    QTextEdit::keyPressEvent(event);
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
            cursor.insertText(
                static_cast<std::string>(result).c_str(),
                errorFormat
            );
        }
    }
    else if (not result.isUnset()) {
        // If we evaluated and it wasn't unset, print an eval result ==

        QTextCharFormat resultPromptFormat;
        resultPromptFormat.setForeground(Qt::darkGray);
        cursor.insertText("==", resultPromptFormat);

        cursor.insertText(" ", QTextCharFormat {});

        cursor.insertText(static_cast<std::string>(result).c_str());
        cursor.insertText("\n");
    }

    // I like this newline always, even though Rebol's console only puts in
    // the newline if you get a non-unset evaluation...

    cursor.insertText("\n");

    appendNewPrompt();

    if (delta) {
        parent->statusBar()->showMessage(
            QString("Command completed in ")
            + static_cast<std::string>(delta).c_str()
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
