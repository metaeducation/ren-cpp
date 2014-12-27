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
/// CONSOLE CONSTRUCTOR
///

//
// Right now the console constructor is really mostly about setting up a
// long graphical banner.  Because spending time on things of that sort is
// like what Penn and Teller say about smoking:
//
//     "Don't do it.
//        (...unless you want to look cool.)"
//

RenConsole::RenConsole(MainWindow * parent) :
    parent (parent),
    prompt (">>"),
    fakeOut (new FakeStdout (*this))
{
    // First we set up the Evaluator so it's wired up for signals and slots
    // and on another thread from the GUI

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

    // Now do the banner using images packed into the resource file

    QTextCursor cursor = textCursor();

    cursor.insertImage(QImage (":/images/red-logo.png"));

    cursor.insertImage(QImage (":/images/rebol-logo.png"));

    cursor.insertText("\n");

    QTextCharFormat headerFormat;
    headerFormat.setFont(QFont("Helvetica", 24, QFont::Bold));
    cursor.insertText("Ren [人] Workbench", headerFormat);

    QTextCharFormat subheadingFormat;
    subheadingFormat.setForeground(Qt::darkGray);
    cursor.insertText("\n", subheadingFormat);

    std::vector<char const *> components = {
        "<i><b>Red</b> is © 2014 Nenad Rakocevic, BSD License</i>",

        "<i><b>Rebol</b> is © 2014 REBOL Technologies, Apache 2 License</i>",

        "<i><b>Rencpp</b></b> is © 2014 HostileFork.com, Boost License</i>",

        "<i><b>Qt</b> is © 2014 Digia Plc, LGPL 2.1 or GPL 3 License</i>",

        "",

        "<i><b>Ren Workbench</b> is © 2014 HostileFork.com, GPL 3 License</i>"
    };

    // A quick shout-out to C++11's "range-based for"

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

    // Now let's put out a prompt

    promptFormat.setForeground(Qt::darkGray);
    promptFormat.setFontWeight(QFont::Bold);

    cursor.insertText("\n");
    cursor.insertText(prompt, promptFormat);
    cursor.insertText(" ", QTextCharFormat ());

    // move the cursor to the end of the text, and remember its position

    inputPos = cursor.position();

    // Tell the runtime to use the fake stdout instead of the ordinary one

    rebol::runtime.setOutputStream(*fakeOut);
}



///
/// KEYPRESS EVENT HANDLING
///

//
// Using the current maybe-not-long-term idea (that isn't too terrible), the
// console is just a Qt rich text editor.  It has a special rule that it
// remembers the position in the document where the last command ended, and
// although it will let you select before that point it won't let you modify
// the text--you can only edit in the lines of the current input prompt
// position.  So we have to intercept keypresses and decide whether to let
// them fall through to the text editor's handling.
//

void RenConsole::keyPressEvent(QKeyEvent * event) {

    bool ctrlc = (event->key() == Qt::Key_C)
        and (event->modifiers() & Qt::ControlModifier);

    if (ctrlc or (event->key() == Qt::Key_Escape)) {
        // In the special case of a Ctrl-C or Escape, we want to interrupt
        // any evaluations in progress.  For cancel semantics, see:
        //
        //     https://github.com/hostilefork/rencpp/issues/19

        ren::runtime.cancel();
        return;
    }

    bool enter = (event->key() == Qt::Key_Enter)
        or (event->key() == Qt::Key_Return);

    QTextCursor cursor = textCursor();

    // If no shift modifier, then enter means evaluate

    if (enter and not (event->modifiers() & Qt::ShiftModifier)) {

        // Get content from the last prompt all the way to the end

        cursor.setPosition(inputPos, QTextCursor::MoveAnchor);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

        QString input = cursor.selection().toPlainText();

        // With the input captured, collapse the range selection to the end
        // of the document, and throw in a newline before the code runs
        // and starts printing output

        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        cursor.insertText("\n");

        if (not input.isEmpty()) {
            // Queue work on another thread
            operate(input);
            return;
        }

        // If we reacted to the enter at all, we still need another prompt.
        // This behavior is up for debate, we could just as well have a beep
        // sound and status warning or something.

        handleResults(true, ren::Value (), ren::none);
        return;
    }

    // If the key wasn't a textual input, ignore and pass through.

    if (event->text().isEmpty()) {
        QTextEdit::keyPressEvent(event);
        return;
    }

    // Most edits are additive, but if they're backspacing we need to be
    // at least one ahead of the ordinary input limit.

    bool backspace = event->key() == Qt::Key_Backspace;

    auto blockedEdit = [&]() -> bool {
        return (cursor.anchor() < (backspace ? inputPos + 1 : inputPos))
        or (cursor.position() < (backspace ? inputPos + 1 : inputPos));
    };

    // Now, we'll only take the keypress if the start and end are in the
    // prompt editing area... if not, we clear selection and jump them
    // to the end.

    if (blockedEdit()) {
        cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
        setTextCursor(cursor);
    }

    // If we jumped the input to the end and it's still not okay, they're at
    // a blank prompt and trying to backspace...

    if (blockedEdit()) {
        assert(backspace);
        return;
    }

    // Now handle the edit as valid, and clear the status bar message

    parent->statusBar()->clearMessage();

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
    // The output to the console seems to flush very aggressively, but with
    // Rencpp it does buffer by default, so be sure to flush the output
    // before printing any eval results or errors

    ren::runtime.getOutputStream().flush();

    QTextCursor cursor = textCursor();
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);

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

        cursor.insertText("==", promptFormat);
        cursor.insertText(" ", QTextCharFormat ());
        cursor.insertText(static_cast<std::string>(result).c_str());
        cursor.insertText("\n");
    }

    // I like this newline always, even though Rebol's console only puts in
    // the newline if you get a non-unset evaluation...

    cursor.insertText("\n");

    // Print another prompt

    cursor.insertText(prompt, promptFormat);
    cursor.insertText(" ", QTextCharFormat ());

    // Move the physical cursor position

    setTextCursor(cursor);

    inputPos = cursor.position();

    if (delta) {
        parent->statusBar()->showMessage(
            QString("Command completed in ")
            + static_cast<std::string>(delta).c_str()
        );
    }

    parent->watchList->updateWatches();
}



///
/// DESTRUCTOR
///

RenConsole::~RenConsole() {
    ren::runtime.cancel();
    workerThread.quit();
    workerThread.wait();
}

#include "renconsole.moc"
