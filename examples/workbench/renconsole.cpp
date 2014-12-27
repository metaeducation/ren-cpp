#include <QtWidgets>

#include "renconsole.h"
#include "mainwindow.h"
#include "fakestdio.h"

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"

RenConsole::RenConsole(MainWindow * parent) :
    parent (parent),
    prompt (">>"),
    fakeOut (new FakeStdout(*this))
{
    QTextCursor cursor = textCursor();
    cursor.insertImage(QImage (":/images/rebol-logo.png"));

    QTextCharFormat headerFormat;
    headerFormat.setFont(QFont("Helvetica", 24, QFont::Bold));
    cursor.insertText("[Ren] Workbench", headerFormat);

    cursor.insertImage(QImage (":/images/red-logo.png"));

    QTextCharFormat subheadingFormat;
    subheadingFormat.setForeground(Qt::darkGray);
    cursor.insertText("\n", subheadingFormat);

    cursor.insertHtml(
        "<i><b>Red</b> is Copyright 2014 Nenad Rakocevic,"
        " BSD License</i>"
    );
    cursor.insertText("\n");

    cursor.insertHtml(
        "<i><b>Rebol</b> is Copyright 2014 REBOL Technologies,"
        " Apache 2 License</i>"
    );
    cursor.insertText("\n");

    cursor.insertHtml(
        "<i><b>Rencpp</b></b> binding is Copyright 2014 HostileFork.com,"
        " Boost Software License</i>"
    );
    cursor.insertText("\n");

    cursor.insertHtml(
        "<i><b>Qt</b> is Copyright 2014 Digia Plc and/or its subsidiary(-ies),"
        " LGPL 2.1 or GPL 3.0 License</i>"
    );
    cursor.insertText("\n");

    cursor.insertHtml(
        "<i><b>[Ren] Workbench</b> is Copyright 2014 HostileFork.com,"
        " GPL 3.0 License</i>"
    );
    cursor.insertText("\n");

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

    cursor.insertText("\n");

    promptFormat.setForeground(Qt::darkGray);
    promptFormat.setFontWeight(QFont::Bold);
    cursor.insertText(prompt, promptFormat);

    cursor.insertText(" ", QTextCharFormat ());

    // move the cursor to the end of the text, and remember it

    inputPos = cursor.position();

    // Tell the runtime to use the fake stdout instead of the ordinary one

    rebol::runtime.setOutputStream(*fakeOut);
}


void RenConsole::keyPressEvent(QKeyEvent * event) {
    bool enter = (event->key() == Qt::Key_Enter)
        or (event->key() == Qt::Key_Return);

    // Helper function to add text to the end

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

        ren::Value result;

        if (not input.isEmpty()) {
            ren::Value start = ren::runtime("stats/timer");

            try {
                result = ren::runtime(input.toUtf8().constData());

                ren::runtime.getOutputStream().flush();
            }
            catch (ren::evaluation_error & e) {
                ren::runtime.getOutputStream().flush();

                QTextCharFormat errorFormat;
                errorFormat.setForeground(Qt::darkRed);
                cursor.insertText(e.what(), errorFormat);
            }
            catch (ren::exit_command & e) {
                qApp->exit(e.code());
            }

            ren::Value delta = ren::runtime("stats/timer -", start);
            parent->statusBar()->showMessage(
                QString("Command completed in ")
                + static_cast<std::string>(delta).c_str()
            );
        }

        // If we evaluated and got a value we print an eval result ==

        if (not result.isUnset()) {
            cursor.insertText("==", promptFormat);
            cursor.insertText(" ", QTextCharFormat ());
            cursor.insertText(static_cast<std::string>(result).c_str());
            cursor.insertText("\n");
        }

        // I like the spacing always, even though that's not how Rebol did it

        cursor.insertText("\n");

        cursor.insertText(prompt, promptFormat);
        cursor.insertText(" ", QTextCharFormat ());

        // Move the physical cursor position

        setTextCursor(cursor);

        inputPos = cursor.position();
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
