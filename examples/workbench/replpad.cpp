//
// replpad.cpp
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

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <utility>

#include "replpad.h"



//
// HISTORY RECORDS
//

//
// Destined to be its own file...
//

//
// ReplPad::getCurrentInput()
//
// Get content from the last prompt all the way to the end.  We do this by
// storing the index of the end of the last prompt text.
//

QString ReplPad::HistoryEntry::getInput(ReplPad & pad) const {
    QTextCursor cursor {pad.document()};

    cursor.setPosition(inputPos, QTextCursor::MoveAnchor);
    if (endPos)
        cursor.setPosition(*endPos, QTextCursor::KeepAnchor);
    else
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    // The misleadingly named QTextCursor::​selectedText() gives you back
    // text with Unicode U+2029 paragraph separator characters instead of a
    // newline \n character, for reasons known only to Qt.

    return cursor.selection().toPlainText();
}

//
// CONSOLE CONSTRUCTION
//

//
// Right now the console constructor is really mostly about setting up a
// long graphical banner.

ReplPad::ReplPad (
    IReplPadHooks & hooks,
    IReplPadSyntaxer & syntaxer,
    QWidget * parent
) :
    QTextEdit (parent),
    hooks (hooks),
    syntaxer (syntaxer),
    fakeOut (*this),
    fakeIn (*this),
    defaultFont (currentFont()),
    zoomDelta (0),
    shouldFollow (true),
    isFormatPending (false),
    hasUndo (false),
    hasRedo (false),
    historyIndex (),
    selectionWasAutocomplete (false)
{
    // QTextEdit is a view into a QTextDocument; and it was not really in any
    // way designed to be used for what Ren Garden is doing with it.  One
    // example of how it was not designed to do this is that there are no
    // notifications for cut/copy/paste until *after* they have happened.
    // Such operations can destroy the known structure of the console (deleting
    // known points in the history).  We have to find ways to prevent these
    // unexpected changes--or undo them.  This safety hook catches any cases
    // where modifications were made without ReplPad making them.

    connect(
        this, &QTextEdit::textChanged,
        [this]() {
            if (not documentMutex.tryLock())
                return; // we are purposefully making the change!

            documentMutex.unlock();

            QMessageBox::information(
                this,
                "Unexpected Modification",
                "Though we give the appearance of the QTextEdit being"
                " editable, it actually can only be edited under the precise"
                " moments we allow.  But putting it in 'read only' mode drops"
                " the insertion cursor.  We try to trap every way of inserting"
                " content to control it, but you found a way around it.  If"
                " you remember what you just did, report to the bug tracker!"
            );

            // Can't edit the document from within the document edit handler
            emit salvageDocument();
        }
    );

    connect(
        this, &ReplPad::salvageDocument,
        [this]() {
            QMutexLocker lock {&documentMutex};

            if (hasUndo) {
                // trick to undo the last document modification, if we can!
                undo();
                return;
            }

            // No undo, we have to clear to avoid crashes based on possibly
            // now-invalid positions in the history buffer due to random edit

            clear();
            appendNewPrompt();
            dontFollowLatestOutput();
        }
    );


    // Should the selection change for a reason other than autocomplete, we
    // want to signal that a keypress should overwrite the content vs. add

    connect(
        this, &QTextEdit::selectionChanged,
        [this]() {
            if (not autocompleteMutex.tryLock()) {
                // autocomplete is doing the change!
                selectionWasAutocomplete = true;
                return;
            }

            autocompleteMutex.unlock();

            selectionWasAutocomplete = false;
        }
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


    // Being a C++ program, our interface for abstracted I/O is based on
    // iostreams, hence we have objects that are "fake" streams you can
    // read from and write to to interact with the console.  One signal
    // we need to process is when the fake input stream wants us to acquire
    // a line of input from the user.  The stream will synchronously block
    // until we notify it that the GUI has the line ready (hence the input
    // stream cannot be read from the GUI thread).

    connect(
        &fakeIn, &FakeStdin::requestInput,
        this, &ReplPad::onRequestInput,
        Qt::QueuedConnection
    );

    // We want to be able to append text from threads besides the GUI thread.
    // It is a synchronous operation for a worker, but still goes through the
    // emit process.

    connect(
        this, &ReplPad::needGuiThreadTextAppend,
        this, &ReplPad::appendText,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &ReplPad::needGuiThreadHtmlAppend,
        this, &ReplPad::appendHtml,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &ReplPad::needGuiThreadImageAppend,
        this, &ReplPad::appendImage,
        Qt::BlockingQueuedConnection
    );


    // It's very annoying when you're trying to do something with a scroll
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

    connect(
        this, &QTextEdit::redoAvailable,
        [this] (bool b) {
            hasRedo = b;
        }
    );


    // Initialize text formats used.  What makes this difficult is "zoom in"
    // and "zoom out", so if you get too creative with the font settings then
    // CtrlPlus and CtrlMinus won't do anything useful.  See issue:
    //
    //     https://github.com/metaeducation/ren-garden/issues/7

    // Make the input just a shade lighter black than the output.  (It's also
    // not a fixed width font, so between those two differences you should be
    // able to see what's what.)

    inputFormatNormal.setForeground(QColor {0x20, 0x20, 0x20});
    inputFormatMeta.setForeground(Qt::darkGreen);

    promptFormatNormal.setForeground(Qt::darkGray);
    promptFormatNormal.setFontWeight(QFont::Bold);

    promptFormatMeta.setForeground(Qt::darkMagenta);
    promptFormatMeta.setFontWeight(QFont::Bold);

    errorFormat.setForeground(Qt::darkRed);

    // See what works well enough on platforms to demo in terms of common
    // monospace fonts (or if monospace is even what we want to differentiate
    // the output...)
    //
    //     http://stackoverflow.com/a/1835938/211160

    outputFormat.setFontFamily("Courier");
    outputFormat.setFontWeight(QFont::Bold);

    leftFormat = textCursor().blockFormat();
    centeredFormat = leftFormat;
    centeredFormat.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
}



//
// BASIC CLIENT INTERFACE FOR ADDING MATERIAL TO THE CONSOLE
//

//
// This API is here to try and be a narrowing of the requirements the
// RenConsole has of the ReplPad.  By keeping abstractions like QTextCursor
// out of RenConsole, it makes it more feasible to swap out something that
// is not a QTextEdit as the widget.
//

void ReplPad::appendImage(QImage const & image, bool centered) {
    if (thread() != QThread::currentThread()) {
        // we need to block in order to properly check for write mutex
        // authority (otherwise we could just queue it and split...)
        // just calls this function again but from the Gui Thread

        emit needGuiThreadImageAppend(image, centered);
        return;
    }

    QMutexLocker lock {&documentMutex};

    QTextCursor cursor = endCursor();

    if (centered)
        cursor.setBlockFormat(centeredFormat);
    else
        cursor.setBlockFormat(leftFormat);

    cursor.insertImage(image);
    cursor.insertText("\n");
}


void ReplPad::appendText(QString const & text, bool centered) {
    if (thread() != QThread::currentThread()) {
        // we need to block in order to properly check for write mutex
        // authority (otherwise we could just queue it and split...)
        // just calls this function again but from the Gui Thread

        emit needGuiThreadTextAppend(text, centered);
        return;
    }

    QMutexLocker lock {&documentMutex};

    QTextCursor cursor = endCursor();

    if (centered)
        cursor.setBlockFormat(centeredFormat);
    else
        cursor.setBlockFormat(leftFormat);

    if (isFormatPending) {
        cursor.insertText(text, pendingFormat);
        pendingFormat = QTextCharFormat {};
        isFormatPending = false;
    }
    else
        cursor.insertText(text);

    if (shouldFollow) {
        setTextCursor(cursor);

        // For reasons not quite understood, you don't always wind up at the
        // bottom of the document when a scroll happens.  :-/

        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}


void ReplPad::appendHtml(QString const & html, bool centered) {
    if (thread() != QThread::currentThread()) {
        // we need to block in order to properly check for write mutex
        // authority (otherwise we could just queue it and split...)
        // just calls this function again but from the Gui Thread

        emit needGuiThreadHtmlAppend(html, centered);
        return;
    }

    QMutexLocker lock {&documentMutex};

    QTextCursor cursor = endCursor();

    if (centered)
        cursor.setBlockFormat(centeredFormat);
    else
        cursor.setBlockFormat(leftFormat);

    cursor.insertHtml(html);

    if (shouldFollow) {
        setTextCursor(cursor);

        // For reasons not quite understood, you don't always wind up at the
        // bottom of the document when a scroll happens.  :-/

        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}


void ReplPad::onRequestInput()
{
    document()->clearUndoRedoStacks();

    // temporary...let's just try returning something to show the method
    input = QString("Sample Input Response\n").toUtf8();

    QMutexLocker lock (&inputMutex);
    inputAvailable.wakeOne();
}




int ReplPad::getZoom() {
    return zoomDelta;
}


void ReplPad::setZoom(int delta) {
    // We're being asked to set the zoom a certain delta (positive or negative)
    // indiciating zoom-in and zoom-out calls assuming we are at zero.
    // But if we already have a zoom state, we have to compensate for that.

    delta += -zoomDelta;

    while (delta != 0) {
        if (delta > 0) {
            delta--;
            zoomIn();
            zoomDelta++;
        }
        else {
            delta++;
            zoomOut();
            zoomDelta--;
        }
    }
}

//
// RICH-TEXT CONSOLE BEHAVIOR
//

//
// ReplPad::endCursor()
//
// Helper to get a cursor located at the tail of the QTextDocument underlying
// the console.  (Use textCursor() to get the actual caret location).
//

QTextCursor ReplPad::endCursor() const {
    QTextCursor result {document()};

    result.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    return result;
}


void ReplPad::dontFollowLatestOutput() {
    shouldFollow = false;
}


void ReplPad::followLatestOutput() {
    setTextCursor(endCursor());
    shouldFollow = true;
}


void ReplPad::mousePressEvent(QMouseEvent * event) {

    if (event->buttons() == Qt::RightButton) {
        QTextCursor clickCursor = cursorForPosition(event->pos());
        if (textCursor().position() == textCursor().anchor())
            setTextCursor(clickCursor);
        else {
            int lo = std::min(textCursor().position(), textCursor().anchor());
            int hi = std::max(textCursor().position(), textCursor().anchor());
            if ((clickCursor.position() < lo) or (clickCursor.position() > hi))
                setTextCursor(clickCursor);
        }
    }

    QTextEdit::mousePressEvent(event);

    dontFollowLatestOutput();
}


void ReplPad::mouseDoubleClickEvent(QMouseEvent * event) {
    // There is no exposed "triple click" event in Qt.  The behavior you see
    // where entire lines are selected by QTextEdit if a third click happens
    // is custom implemented inside QTextEdit::mouseDoubleClickEvent.  A
    // diff on another widget implementing triple click here:
    //
    //     https://qt.gitorious.org/qt/qtdeclarative/merge_requests/6/diffs

    // We need to differentiate click behavior when they are in the source
    // code input part vs. the output part.  When in output, fall back on
    // normal QTextEdit behavior.

    HistoryEntry * entryInside = nullptr;

    for (auto it = history.rbegin(); it != history.rend(); it++) {
        if (textCursor().position() > it->inputPos) {
            int last = it->endPos ? *(it->endPos) : endCursor().position();

            if (textCursor().position() <= last)
                entryInside = &(*it);
            break;
        }
    }

    if (entryInside) {
        std::pair<int, int> range = syntaxer.rangeForWholeToken(
            entryInside->getInput(*this),
            textCursor().position() - entryInside->inputPos
        );

        QTextCursor cursor {document()};
        cursor.setPosition(range.first + entryInside->inputPos);
        cursor.setPosition(
            range.second + entryInside->inputPos,
            QTextCursor::KeepAnchor
        );
        setTextCursor(cursor);
    } else {
        QTextEdit::mouseDoubleClickEvent(event);
    }
}


void ReplPad::pushFormat(QTextCharFormat const & format) {
    isFormatPending = true;
    pendingFormat = format;
}



//
// ReplPad::appendPrompt()
//
// Append a prompt and remember the cursor's offset into the QTextDocument,
// which we'll use later to find the beginning of the user's input.
//

void ReplPad::appendNewPrompt() {

    // This initializes a new history entry, which also rewrites the
    // prompt -- capturing the position before and after the prompt text

    history.emplace_back(textCursor().position());
    rewritePrompt();

    // We can't allow people to undo their edits past the last prompts,
    // but we do have a "magic Undo" stack that can undo unwanted commands

    document()->clearUndoRedoStacks();
}



void ReplPad::rewritePrompt() {

    // In order to prevent corruption of the undo/redo history, we have to
    // do this in one atomic replacement.  If we did it one insertion at
    // a time, then it would let the user undo through that.  Unfortunately
    // something is wrong with an interaction of doing setTextCursor()
    // during an edit block. :-/  Seems to crash, sporadically (on linux)

    QMutexLocker lock {&documentMutex};

    bool saveShouldFollow = shouldFollow;
    shouldFollow = false;

    QTextCursor cursor {document()};
    cursor.beginEditBlock();

    HistoryEntry & entry = history.back();

    QString buffer = entry.getInput(*this);

    cursor.setPosition(history.back().promptPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    cursor.removeSelectedText();
    cursor = endCursor();

    QTextCharFormat promptFormat = entry.meta
        ? promptFormatMeta
        : promptFormatNormal;

    cursor.insertText(hooks.getPromptString(*this) + ">>", promptFormat);

    QTextCharFormat inputFormat = entry.meta
        ? inputFormatMeta
        : inputFormatNormal;

    if (entry.multiline) {
        QTextCharFormat hintFormat = promptFormat;
        hintFormat.setFontItalic(true);
        hintFormat.setFontWeight(QFont::Normal);

        cursor.insertText(
            " [ctrl-enter to evaluate]",
            hintFormat
        );

        cursor.insertText("\n", inputFormat);
    }
    else
        cursor.insertText(" ", inputFormat);

    entry.inputPos = cursor.position();

    cursor.insertText(buffer);

    // With the edit block closed, it's safe to set the text cursor, so update
    // the follow status and add an empty text string.

    cursor.endEditBlock();

    shouldFollow = saveShouldFollow;

    if (shouldFollow) {
        setTextCursor(cursor);

        // For reasons not quite understood, you don't always wind up at the
        // bottom of the document when a scroll happens.  :-/

        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
    }
}


void ReplPad::clearCurrentInput() {
    QMutexLocker lock {&documentMutex};

    QTextCursor cursor {document()};

    cursor.setPosition(history.back().inputPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    cursor.removeSelectedText();
    setTextCursor(endCursor());

    history.back().endPos = std::experimental::nullopt;
}


void ReplPad::setBuffer(QString const & text, int position, int anchor) {
    // Note: position and anchor are relative positions to the buffer string,
    // that range from 0 to text.length()

    clearCurrentInput();

    QTextCursor cursor {document()};

    cursor.setPosition(history.back().inputPos);

    int pos = cursor.position();

    appendText(text);

    cursor.setPosition(pos + anchor);
    cursor.setPosition(pos + position, QTextCursor::KeepAnchor);
    setTextCursor(cursor);
}


void ReplPad::containInputSelection() {
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
// ReplPad::keyPressEvent()
//
// Main keyboard hook for handling input.  Note that input can come from
// other sources--such as the clipboard, and so that has to be intercepted
// as well (to scrub non-plaintext formatting off of information coming from
// web browsers, etc.
//

void ReplPad::keyPressEvent(QKeyEvent * event) {

    int key = event->key();

    // Debugging alted or ctrled or shifted keys is made difficult if you set
    // a breakpoint and it tells you about hitting those keys themselves, so
    // since we do nothing in that case (yet) return quickly.

    if (
        (key == Qt::Key_Control)
        or (key == Qt::Key_Shift)
        or (key == Qt::Key_Alt)
    ) {
        QTextEdit::keyPressEvent(event);
        return;
    }

    // The "hold down escape to make window fade and quit" trick looks cool
    // but depends on a working implementation of SetWindowOpacity.  This
    // starts the timer that makes it fade (the timer is stopped on key up)

    if ((key == Qt::Key_Escape) and (not event->isAutoRepeat()))
        emit fadeOutToQuit(true);


    // Putting this list here for convenience.  In theory commands could take
    // into account whether you hit the 1 on a numeric kepad or on the top
    // row of the keyboard....

    bool shifted = event->modifiers() & Qt::ShiftModifier;

    // What ctrl means on the platforms is different:
    //
    //     http://stackoverflow.com/questions/16809139/

    bool ctrled = event->modifiers() & Qt::ControlModifier;
    bool alted = event->modifiers() & Qt::AltModifier;
    bool metaed = event->modifiers() & Qt::MetaModifier;
    bool keypadded = event->modifiers() & Qt::KeypadModifier;

    // "Groups and levels are two kinds of keyboard mode; in general, the
    // level determines whether the top or bottom symbol on a key is used,
    // and the group determines whether the left or right symbol is used.
    // On US keyboards, the shift key changes the keyboard level, and there
    // are no groups."

    bool groupswitched = event->modifiers() & Qt::GroupSwitchModifier;


    // Some systems use Key_Backtab instead of a shifted Tab.  We could
    // canonize to either, but to call attention to the anomaly we canonize
    // shifted tabs to Key_Backtab
    //
    //     http://www.qtcentre.org/threads/32646-shift-key

    if ((key == Qt::Key_Tab) and shifted) {
        key = Qt::Key_Backtab;
        shifted = false;
    }


    // Qt says some key events "have text" and hence correspond to an
    // intent to insert that QString, but it says that about lots of odd
    // control characters.  So we double check it for "hasRealText"
    //
    // Note that isPrint() includes whitespace, but not \r
    // we try to be sure by handling Enter and Return as if they
    // were not printable.  Apple keyboards have a key labeled both
    // "enter" and "return" that seem to produce "\r".  So it's
    // important to handle it by keycode vs. just by the whitespace
    // text produced produced.
    //
    // As Rebol and Red are somewhat "religiously" driven languages,
    // tabs being invisible complexity in source is against that
    // religion.  So the console treats tabs as non-printables, and
    // will trap any attempt to insert the literal character (while
    // substituting with 4 spaces)

    QString temp = event->text();

    bool hasRealText = not event->text().isEmpty();
    for (QChar ch : event->text()) {
        if (not ch.isPrint() or (ch == '\t')) {
            hasRealText = false;
            break;
        }
    }


    // Matching QKeySequence::ZoomIn seems to not work; the keysequence
    // Ctrl and = or Ctrl and Shift and = (to get a textual plus) isn't
    // showing up.  However, ZoomOut works?  That's on KDE and a small apple
    // keyboard, despite the key code being correct and control key hit.
    // Would need a debug build of Qt5 to know why.  In any case, this is
    // even better because we allow for Ctrl = to work which saves you on
    // the shifting if the + and - aren't on the same keys.

    if (
        event->matches(QKeySequence::ZoomIn)
        or (ctrled and ((key == Qt::Key_Plus) or (event->text() == "+")))
        or (ctrled and ((key == Qt::Key_Equal) or (event->text() == "=")))
    ) {
        zoomIn();
        zoomDelta += 1;
        return;
    }

    if (
        event->matches(QKeySequence::ZoomOut)
        or (ctrled and ((key == Qt::Key_Minus) or (event->text() == "-")))
    ) {
        zoomOut();
        zoomDelta -=1;
        return;
    }


    // If something has no printable representation, we usually assume
    // getting it in a key event isn't asking us to mutate the document.
    // Thus we can just call the default QTextEdithandling for the navigation
    // or accelerator handling that is needed.
    //
    // There are some exceptions, so we form it as a while loop to make it
    // easier to style with breaks.

    while (not hasRealText) {
        if ((key == Qt::Key_Up) or (key == Qt::Key_Down))
            if (
                ctrled or (
                    ((not history.back().multiline) or historyIndex)
                    and (textCursor().position() >= history.back().inputPos)
                    and (not shifted)
                )
            ) {
                // Ctrl-Up and Ctrl-Down always do history navigation.  But
                // if you don't use Ctrl then cursor navigation will act
                // normally *unless* you are either positioned in the edit
                // buffer in single line mode, or if you're in mid-paging
                // and are passing through a multi line mode entry.

                // Shift-Up can be used as a trick to get the cursor "unstuck"
                // from paging; it will make a selection to the previous line
                // but you can abandon that by pressing any other navigation
                // keys.  The cursor will "stick" again if you cursor into
                // the single line buffer.
                break;
            }

        historyIndex = std::experimental::nullopt;

        if (
            (key == Qt::Key_Return)
            or (key == Qt::Key_Enter)
            or (key == Qt::Key_Backspace)
            or (key == Qt::Key_Delete)
            or (key == Qt::Key_Tab)
            or (key == Qt::Key_Backtab)
            or (key == Qt::Key_Escape)
        ) {
            // Though not true for all programs at all times, in the
            // console's case all of these are operations asking to modify
            // the state of the console.  So fall through.
            break;
        }


        if ((key == Qt::Key_Space) and ctrled) {
            // Shifting into (or out of) meta mode, we edit the prompt
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


        // Cut, Paste, and Delete variants obviously modify, and we can just
        // pass them through to the QTextEdit with the write mutex locked here

        if (
            event->matches(QKeySequence::Cut)
            or event->matches(QKeySequence::Delete)
            or event->matches(QKeySequence::DeleteCompleteLine)
            or event->matches(QKeySequence::DeleteEndOfLine)
            or event->matches(QKeySequence::DeleteEndOfWord)
            or event->matches(QKeySequence::DeleteStartOfWord)
        ) {
            containInputSelection();

            QMutexLocker lock {&documentMutex};
            QTextEdit::keyPressEvent(event);
            return;
        }
        else if (event->matches(QKeySequence::Paste)) {
            pasteSafely();
            return;
        }


        // That should be all the modifying operations.  But if we turn out
        // to be wrong and the QTextEdit default handler does modify the
        // document, we'll trap it with an error and then clear console.

        QTextEdit::keyPressEvent(event);
        return;
    }


    // Give a hook opportunity to do something about this key, which is
    // asking to do some kind of modification when there may be an evaluation
    // running on another thread.

    if (not hooks.isReadyToModify(*this, key == Qt::Key_Escape)) {
        followLatestOutput();
        return;
    }


    // Whatever we do from here should update the status bar, even to clear
    // it.  Rather than simply clearing it, we should set up something more
    // formal to ensure all paths have some kind of success or failure report

    emit reportStatus("");


    // If they have made a selection and have intent to modify, we must
    // contain that selection within the boundaries of the editable area.

    containInputSelection();


    // Command history browsing via up and down arrows.  Presents some
    // visual oddity swapping really long program segments short ones

    if ((key == Qt::Key_Up) or (key == Qt::Key_Down)) {
        assert(history.size() != 0);

        if (not historyIndex)
            historyIndex = history.size() - 1;

        while (true) {
            if (
                (key == Qt::Key_Down)
                and (historyIndex == history.size() - 1)
            ) {
                emit reportStatus("Already at bottom of history.");
                return;
            }

            if (
                (key == Qt::Key_Up)
                and (historyIndex == static_cast<size_t>(0))
            ) {
                emit reportStatus("Already at top of history.");
                return;
            }

            historyIndex = *historyIndex + ((key == Qt::Key_Down) ? 1 : -1);

            // skip over empty lines

            if (not history[*historyIndex].getInput(*this).isEmpty())
                break;
        }

        clearCurrentInput();

        if (historyIndex == history.size() - 1) {
            // Because the edit history content is currently only reflected
            // in the document itself, we've lost what you were editing
            // when you started the cursor navigation.  Theoretically we
            // could save it and restore it, but for now we just leave an
            // empty non-multiline non-meta prompt.

            history.back().multiline = false;
            history.back().meta = false;
            rewritePrompt();
            historyIndex = std::experimental::nullopt;
        }
        else {
            HistoryEntry & oldEntry = history[*historyIndex];
            history.back().multiline = oldEntry.multiline;
            rewritePrompt();

            // We don't try and restore the prompt or meta state, because
            // that opens a can of worms about whether the execution state
            // can be rewound.  We just page through the text for now.

            setBuffer(
                oldEntry.getInput(*this),
                *(oldEntry.position),
                *(oldEntry.anchor)
            );
        }

        document()->clearUndoRedoStacks();
        return;
    }


    // Now that we know we're not doing paging, then all other operations will
    // forget where you were while cursoring through the history.

    historyIndex = std::experimental::nullopt;


    // Testing of an initial magicUndo concept, which will backtrack the work
    // log and take you to where you were from previous evaluations.

    if (event->matches(QKeySequence::Undo)) {
        if (hasUndo) {
            QMutexLocker lock {&documentMutex};
            undo();
            return;
        }

        // If there's input but no undo queue, clear the input first before
        // moving on to magic undo...

        if (not history.back().getInput(*this).isEmpty()) {
            clearCurrentInput();

            HistoryEntry & entry = history.back();

            entry.multiline = false;
            entry.meta = false;
            rewritePrompt();

            document()->clearUndoRedoStacks();
            return;
        }

        if (history.size() > 1) {
            QMutexLocker lock {&documentMutex};

            history.pop_back();

            HistoryEntry & entry = history.back();

            // Clear from the previous record's endPos to the current end
            // of the document

            QTextCursor cursor = endCursor();
            cursor.setPosition(*(entry.endPos), QTextCursor::KeepAnchor);
            cursor.removeSelectedText();

            // Restore the selection to what it was at the time the user had
            // performed an evaluation

            cursor.setPosition(*(entry.anchor) + entry.inputPos);
            cursor.setPosition(
                *(entry.position) + entry.inputPos,
                QTextCursor::KeepAnchor
            );
            setTextCursor(cursor);

            // The user can now edit and move positions around, so we need
            // to indicate that by clearing the endPos and position

            entry.endPos = std::experimental::nullopt;
            entry.position = std::experimental::nullopt;

            // Keep it from trying to record that "edit to undo" as an
            // undoable action, which causes madness.

            document()->clearUndoRedoStacks();
            return;
        }

        emit reportStatus("Nothing available for undo.");
        return;
    }


    // No magical redo as of yet...

    if (event->matches(QKeySequence::Redo)) {
        if (hasRedo) {
            QMutexLocker lock {&documentMutex};
            redo();
            return;
        }
        emit reportStatus("Nothing available for redo.");
        return;
    }


    // All other commands will assume we are only working with the current
    // history item, which lives at the tail of the history buffer

    HistoryEntry & entry = history.back();


    if (key == Qt::Key_Space) {
        if (ctrled) {
            entry.meta = not entry.meta;
            rewritePrompt();
            return;
        }
    }


    // Escape is given several meanings depending on the context.  If you
    // are evaluating, it will cancel.  If you are not evaluating, it will
    // clear any input you've given (an undoable action).  If you're not
    // evaluating and it has cleared your input, and you hit it twice enough
    // within the double click timer window, it will bump you out of the
    // current shell.

    if (key == Qt::Key_Escape) {
        if (not escapeTimer.hasExpired(
            qApp->styleHints()->mouseDoubleClickInterval()
        )) {
            hooks.escape(*this);
            return;
        }

        escapeTimer.start();

        // If there's any text, clear it and take us back.

        if (not entry.getInput(*this).isEmpty()) {
            clearCurrentInput();
            entry.multiline = false;
            rewritePrompt();
        }

        // If there's no text but we're in meta mode, get rid of it.

        if (entry.meta) {
            entry.meta = false;
            rewritePrompt();
        }
        return;
    }


    // Behavior of Enter/Return depends on the line mode you are in, due to
    // this fantastic suggestion.  :-)
    //
    //    https://github.com/metaeducation/ren-garden/issues/4
    //
    // Ctrl-Enter always evaluates.  But Enter evaluates when you are in
    // single-line mode.  Shift-Enter switches you into multi-line mode where
    // Enter doesn't evaluate, but Shift-Enter starts acting like ordinary
    // Enter once you're in it.  This reduces accidents.

    if ((key == Qt::Key_Enter) or (key == Qt::Key_Return)) {

        if ((not entry.multiline) and (shifted and (not ctrled))) {
            switchToMultiline();
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

            if (not entry.multiline)
                return 0;

            QTextCursor cursor {document()};
            cursor.setPosition(lastGoodPosition);
            cursor.setPosition(
                endCursor().position(), QTextCursor::KeepAnchor
            );

            return cursor.selection().toPlainText().count("\n");
        }();

        if (ctrled or (not entry.multiline) or (extraneousNewlines > 1)) {
            // Perform an evaluation.  But first, clean up all the whitespace
            // at the tail of the input (if it happens after our cursor
            // position.) 

            entry.position = textCursor().position() - entry.inputPos;

            entry.anchor = textCursor().anchor() - entry.inputPos;

            QTextCursor cursor = endCursor();
            cursor.setPosition(
                std::max(
                    textCursor().position(),
                    lastGoodPosition
                ),
                QTextCursor::KeepAnchor
            );

            if (true) {
                QMutexLocker lock {&documentMutex};
                cursor.removeSelectedText();
            }

            entry.endPos = cursor.position();

            appendText("\n");

            QString input = entry.getInput(*this);
            if (input.isEmpty()) {
                appendNewPrompt();
                followLatestOutput();
            }
            else {
                followLatestOutput();
                // implementation may (does) queue...
                hooks.evaluate(*this, input, entry.meta);
            }

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

            QMutexLocker lock {&documentMutex};
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
        // is a tab, where we entab or detab the content based on shift
        // IF it spans multiple lines (otherwise we handle it as an
        // autocomplete)

        QMutexLocker lock {&documentMutex};

        if ((key == Qt::Key_Tab) or (key == Qt::Key_Backtab)) {
            QString contents = textCursor().selection().toPlainText();

            if (contents.indexOf(QRegExp("[\\n]")) != -1) {
                // Tab with a multi-line selection should entab and detab,
                // but with spaces.

                if (key == Qt::Key_Backtab) {
                    QString regex {"^"};
                    regex += tabString;
                    contents.replace(QRegExp(regex), "");
                } else
                    contents.replace(QRegExp("^"), tabString);
                textCursor().removeSelectedText();
                textCursor().insertText(contents);
                return;
            }

            // If single-line contents selected, then collapse the selection
            // down to a point to use in autocomplete.

            QTextCursor cursor = textCursor();
            cursor.setPosition(
                std::min(cursor.position(), cursor.anchor())
            );
            setTextCursor(cursor);
        }
        else {
            // Just insert the text.  We overwrite what's in a ranged
            // selection unless the last selection was made by an autocomplete
            // (and they didn't explicitly say "delete")

            if ((key == Qt::Key_Backspace) or (key == Qt::Key_Delete)) {
                textCursor().removeSelectedText();
            }
            else {
                if (selectionWasAutocomplete) {
                    QTextCursor cursor = textCursor();
                    cursor.setPosition(cursor.position());
                    setTextCursor(cursor);
                }
                else
                    textCursor().removeSelectedText();

                textCursor().insertText(event->text());
            }

            return;
        }
    }

    // From here, selection is collapsed, so just an insertion point.

    assert(textCursor().anchor() == textCursor().position());

    if ((key == Qt::Key_Tab) or (key == Qt::Key_Backtab)) {
        // A tab will autocomplete unless it's in the beginning whitespace of
        // a line, in which case it inserts spaces.  For now we don't
        // distinguish tab from backtab, but backtab (a.k.a. shift-Tab)
        // should cycle backwards through the candidates for completion

        QMutexLocker lockAuto {&autocompleteMutex};
        QTextCursor cursor = textCursor();

        if (not find(QRegExp("^"), QTextDocument::FindBackward))
            assert(false); // we should be able to find a start of line!

        bool isPromptLine;
        int basis;

        if (textCursor().position() == entry.promptPos) {
            // We are on the prompt line (hence we have something besides
            // spaces to beginning of line, even if we haven't typed
            // anything).  This can be given a different behavior for the
            // whitespace case.
            basis = entry.inputPos;
            isPromptLine = true;
        }
        else {
            basis = textCursor().position();
            isPromptLine = false;
        }

        cursor.setPosition(basis, QTextCursor::KeepAnchor);

        QString leading = cursor.selection().toPlainText();
        cursor.setPosition(cursor.anchor());
        setTextCursor(cursor);

        // If just whitespace or nothing, handle it as inserting some
        // spaces for now.  (Could have special behavior if isPromptLine)

        if (leading.trimmed().isEmpty()) {
            QMutexLocker lockDoc {&documentMutex};
            textCursor().insertText(tabString);
            return;
        }

        // Ask the syntaxer to find the range of the current token.


        QString input = entry.getInput(*this);
        auto tokenRange = syntaxer.rangeForWholeToken(
            input, cursor.position() - entry.inputPos
        );

        QString incomplete = input.mid(
            tokenRange.first, tokenRange.second - tokenRange.first
        );

        auto completed = syntaxer.autoComplete(
            incomplete,
            cursor.position() - entry.inputPos - tokenRange.first,
            key == Qt::Key_Backtab
        );

        // Replace the token with the new token text from the completer

        QMutexLocker lock {&documentMutex};
        cursor.setPosition(entry.inputPos + tokenRange.first);
        cursor.setPosition(
            entry.inputPos + tokenRange.second,
            QTextCursor::KeepAnchor
        );
        cursor.insertText(completed.first);

        // Make a selection of the completed portion, as told to us by the
        // index reported back from the comletion

        cursor.setPosition(
            entry.inputPos + tokenRange.first + completed.second
        );
        cursor.setPosition(
            entry.inputPos + tokenRange.first + completed.first.length(),
            QTextCursor::KeepAnchor
        );
        setTextCursor(cursor);

        return;
    }


    if (key == Qt::Key_Backspace) {
        QTextCursor cursor = textCursor();

        if (cursor.position() <= entry.inputPos) {
            emit reportStatus(
                "Can't backspace beginning of input."
            );
            return;
        }

        QMutexLocker lock {&documentMutex};

        // Note: we'd like to outdent tabs
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

    if (key == Qt::Key_Delete) {
        QTextCursor cursor = textCursor();

        if (cursor.position() == endCursor().position()) {
            emit reportStatus(
                "Can't delete end of input."
            );
            return;
        }

        QMutexLocker lock {&documentMutex};

        if (cursor.anchor() != cursor.position()) {
            cursor.removeSelectedText();
            return;
        }

        cursor.setPosition(cursor.position() + 1, QTextCursor::KeepAnchor);
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

    QMutexLocker lock {&documentMutex};
    textCursor().insertText(event->text());
}

//
// Switch the input from single to multi-line mode
//
void ReplPad::switchToMultiline() {
    HistoryEntry & entry = history.back();

    // Save the string and current offsets for the selection start
    // and end points

    QString input = entry.getInput(*this);
    int position = this->textCursor().position() - entry.inputPos;
    int anchor = this->textCursor().anchor() - entry.inputPos;


    // Add a newline to the buffer and bump the position in special
    // case where you had an insertion point at the end of input
    //
    // https://github.com/metaeducation/ren-garden/issues/11

    if (
            (position == anchor) and (position == input.length())
            and (not input.isEmpty())
        ) {
            input += "\n";
            position++;
            anchor++;
        }


    // Clear the input area and then rewrite as a multi-line prompt

    clearCurrentInput();
    entry.multiline = true;
    rewritePrompt();

    // Put the buffer and selection back, now on its own line

    setBuffer(input, position, anchor);

    // It may be possible to detect when we undo backwards across
    // a multi-line switch and reset the history item, but until
    // then allowing an undo might mess with our history record

    this->document()->clearUndoRedoStacks();
}



//
// Testing fun gimmick for those upset over the loss of ordinary quit...a
// way to cancel and exit by holding down escape
//
void ReplPad::keyReleaseEvent(QKeyEvent * event) {

    // Strangely, on KDE/Linux at least...you get spurious key release events
    // after a timer, followed by another key press event.  Presumably a
    // guard against stuck keys in the GUI system?  You can work around it,
    // but *technically* it makes it impossible to tell if the user merely
    // released a key for a tiny amount of time or had it held down the whole
    // (a distinction unimportant here.)

    if ((event->key() == Qt::Key_Escape) and (not event->isAutoRepeat()))
        emit fadeOutToQuit(false);

    QTextEdit::keyReleaseEvent(event);
}



//
// There is a transformation process in input methods that actually mutate
// existing characters "out from under you".  So a half-formed character goes
// back and gets changed...it's not a "fully additive" process.
//
void ReplPad::inputMethodEvent(QInputMethodEvent * event) {

    // Give a hook opportunity to do something about this key, which is
    // asking to do some kind of modification when there may be an evaluation
    // running on another thread.

    if (not hooks.isReadyToModify(*this, false)) {
        followLatestOutput();
        return;
    }

    // If our current selection was made by an autocomplete, we want to
    // collapse it down to a single point instead of a range so that the
    // insertion won't overwrite it

    if (selectionWasAutocomplete) {
        QTextCursor cursor = textCursor();
        cursor.setPosition(cursor.position());
        setTextCursor(cursor);
    }

    // While we wanted to do all the changes to the QTextEdit ourselves with
    // the "text to be inserted" for complete control, that's not the case
    // with input method editing.  We pretty much always want to defer to that
    // logic!

    QMutexLocker lock (&documentMutex);

    QTextEdit::inputMethodEvent(event);
}



// Trap cut, paste so we can limit the selection to the edit buffer

void ReplPad::cutSafely() {
    containInputSelection();

    QMutexLocker lock {&documentMutex};
    QTextEdit::cut();
}

void ReplPad::pasteSafely() {
    containInputSelection();

    QClipboard *clipboard = QApplication::clipboard();
    QString clipboardText = clipboard->text();

    // Note that we're not checking for CR
    if (clipboardText.contains('\n', Qt::CaseInsensitive))
        switchToMultiline();

    QMutexLocker lock {&documentMutex};
    QTextEdit::paste();
}
