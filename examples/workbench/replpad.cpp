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

#include <cassert>

#include "replpad.h"



///
/// HISTORY RECORDS
///

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
    if (endPos == -1)
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    else
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    // The misleadingly named QTextCursor::​selectedText() gives you back
    // text with Unicode U+2029 paragraph separator characters instead of a
    // newline \n character, for reasons known only to Qt.

    return cursor.selection().toPlainText();
}

///
/// CONSOLE CONSTRUCTION
///

//
// Right now the console constructor is really mostly about setting up a
// long graphical banner.

ReplPad::ReplPad (QWidget * parent) :
    QTextEdit (parent),
    defaultFont (currentFont()),
    shouldFollow (true),
    isFormatPending (false),
    hasUndo (false),
    hasRedo (false)
{
    // Set up our safety hook to make sure modifications to the underlying
    // QTextDocument only happen when we explicitly asked for them.

    connect(
        this, &ReplPad::textChanged,
        this, &ReplPad::onTextChanged,
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
}


//
// ReplPad::onConsoleReset()
//

void ReplPad::onConsoleReset() {
    QMutexLocker locker {&modifyMutex};

    clear();
    appendNewPrompt();
}



///
/// RICH-TEXT CONSOLE BEHAVIOR
///


//
// ReplPad::onTextChanged()
//
// Because it's important to know what people did to cause this likely to
// be annoying problem, display an explanatory message before resetting.
//

void ReplPad::onTextChanged() {
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
            int last = (it->endPos == -1)
                ? endCursor().position()
                : it->endPos;

            if (textCursor().position() <= last)
                entryInside = &(*it);
            break;
        }
    }

    if (entryInside) {
        std::pair<int, int> range = getSyntaxer().rangeForWholeToken(
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

void ReplPad::appendText(QString const & text) {
    QTextCursor cursor = endCursor();

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



//
// ReplPad::appendPrompt()
//
// Append a prompt and remember the cursor's offset into the QTextDocument,
// which we'll use later to find the beginning of the user's input.
//

void ReplPad::appendNewPrompt() {

    int startPos = endCursor().position();
    QString prompt = getPromptString();

    // You always have to ask for multi-line mode with shift-enter.  Maybe
    // someone will prefer the opposite?  Not likely.

    history.emplace_back(startPos, prompt);
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

    bool saveShouldFollow = shouldFollow;
    shouldFollow = false;

    QTextCursor cursor {document()};
    cursor.beginEditBlock();

    QString buffer = history.back().getInput(*this);

    cursor.setPosition(history.back().startPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    cursor.removeSelectedText();
    setTextCursor(endCursor());

    QTextCharFormat promptFormat = history.back().meta
        ? promptFormatMeta
        : promptFormatNormal;

    pushFormat(promptFormat);
    appendText(history.back().prompt + ">>");

    QTextCharFormat inputFormat = history.back().meta
        ? inputFormatMeta
        : inputFormatNormal;

    if (history.back().multiLineMode) {
        QTextCharFormat hintFormat = promptFormat;
        hintFormat.setFontItalic(true);
        hintFormat.setFontWeight(QFont::Normal);

        pushFormat(hintFormat);
        appendText(" [ctrl-enter to evaluate]");

        pushFormat(inputFormat);
        appendText("\n");
    }
    else {
        pushFormat(inputFormat);
        appendText(" ");
    }

    history.back().inputPos = endCursor().position();

    appendText(buffer);


    // With the edit block closed, it's safe to set the text cursor, so update
    // the follow status and add an empty text string.

    cursor.endEditBlock();

    shouldFollow = saveShouldFollow;

    appendText("");
}


void ReplPad::clearCurrentInput() {
    QTextCursor cursor {document()};

    cursor.setPosition(history.back().inputPos, QTextCursor::MoveAnchor);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);

    cursor.removeSelectedText();
    setTextCursor(endCursor());

    history.back().endPos = -1;
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
    int const key = event->key();

    if (key == Qt::Key_Escape and (not event->isAutoRepeat()))
        emit fadeOutToQuit(true);


    // Putting this list here for convenience.  In theory commands could take
    // into account whether you hit the 1 on a numeric kepad or on the top
    // row of the keyboard....

    bool shifted = event->modifiers() & Qt::ShiftModifier;
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

    QString temp = event->text();

    bool hasRealText = not event->text().isEmpty();
    for (QChar ch : event->text()) {
        if (not ch.isPrint() or (ch == '\t')) {
            // Note that isPrint() includes whitespace, but not \r
            // we try to be sure by handling Enter and Return as if they
            // were not printable.  Apple keyboards have a key labeled both
            // "enter" and "return" that seem to produce "\r".  So it's
            // important to handle it by keycode vs. just by the whitespace
            // text produced produced.

            // As Rebol and Red are somewhat "religiously" driven languages,
            // tabs being invisible complexity in source is against that
            // religion.  So the console treats tabs as non-printables, and
            // will trap any attempt to insert the literal character (while
            // substituting with 4 spaces)

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
        return;
    }

    if (
        event->matches(QKeySequence::ZoomOut)
        or (ctrled and ((key == Qt::Key_Minus) or (event->text() == "-")))
    ) {
        zoomOut();
        return;
    }


    // If something has no printable representation, we usually assume
    // getting it in a key event isn't asking us to mutate the document.
    // Thus we can just call the default QTextEdithandling for the navigation
    // or accelerator handling that is needed.
    //
    // There are some exceptions, so we form it as a while loop to make it
    // easier to style with breaks.

    while ((not hasRealText) or alted or metaed) {

        if (
            (key == Qt::Key_Return)
            or (key == Qt::Key_Enter)
            or (key == Qt::Key_Backspace)
            or (key == Qt::Key_Delete)
            or (key == Qt::Key_Tab)
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

        if (
            event->matches(QKeySequence::Cut)
            or event->matches(QKeySequence::Paste)
            or event->matches(QKeySequence::Delete)
            or event->matches(QKeySequence::DeleteCompleteLine)
            or event->matches(QKeySequence::DeleteEndOfLine)
            or event->matches(QKeySequence::DeleteEndOfWord)
            or event->matches(QKeySequence::DeleteStartOfWord)
        ) {
            containInputSelection();
            QMutexLocker lock {&modifyMutex};
            QTextEdit::keyPressEvent(event);
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

    if (not isReadyToModify(event))
        return;


    // Whatever we do from here should update the status bar, even to clear
    // it.  Rather than simply clearing it, we should set up something more
    // formal to ensure all paths have some kind of success or failure report

    emit reportStatus("");


    // Temporarily allow writing to the console during the rest of this
    // routine.  Any return or throw will result in the release of the lock
    // by the QMutexLocker's destructor.

    QMutexLocker locker {&modifyMutex};


    // What ctrl means on the platforms is different:
    //
    // http://stackoverflow.com/questions/16809139/

    if ((key == Qt::Key_Space) and ctrled) {
        history.back().meta = not history.back().meta;
        rewritePrompt();
        return;
    }


    // If they have made a selection and have intent to modify, we must
    // contain that selection within the boundaries of the editable area.

    containInputSelection();


    // Command history browsing via up and down arrows currently unwritten.
    // Will need some refactoring to flip the input between single and multi
    // line modes.  Might present some visual oddity swapping really long
    // program segments with really short ones.

    if ((key == Qt::Key_Up) or (key == Qt::Key_Down)) {
        assert(not history.back().multiLineMode);
        emit reportStatus("UP/DOWN history nav not finished.");
        return;
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
            escape();
            return;
        }

        escapeTimer.start();

        // If there's any text, clear it and take us back.

        if (not history.back().getInput(*this).isEmpty()) {
            clearCurrentInput();
            history.back().multiLineMode = false;
            rewritePrompt();
        }

        // If there's no text but we're in meta mode, get rid of it.

        if (history.back().meta) {
            history.back().meta = false;
            rewritePrompt();
        }
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

        if (not history.back().getInput(*this).isEmpty()) {
            clearCurrentInput();
            document()->clearUndoRedoStacks();
            return;
        }

        if (history.back().multiLineMode or history.back().meta) {
            history.back().multiLineMode = false;
            history.back().meta = false;
            rewritePrompt();
            document()->clearUndoRedoStacks();
        }

        emit reportStatus("Nothing available for undo.");
        return;
    }


    // No magical redo as of yet...

    if (event->matches(QKeySequence::Redo)) {
        if (hasRedo) {
            redo();
            return;
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

        if (
            (not history.back().multiLineMode) and (shifted and (not ctrled))
        ) {
            // Switch from single to multi-line mode

            QString input = history.back().getInput(*this);
            clearCurrentInput();

            history.back().multiLineMode = true;
            rewritePrompt();

            // It may be possible to detect when we undo backwards across
            // a multi-line switch and reset the history item, but until
            // then allowing an undo might mess with our history record

            document()->clearUndoRedoStacks();
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
            or (extraneousNewlines > 1)
        ) {
            // Perform an evaluation.  But first, clean up all the whitespace
            // at the tail of the input (if it happens after our cursor
            // position.)

            history.back().evalCursorPos = textCursor().position();

            QTextCursor cursor = endCursor();
            cursor.setPosition(
                std::max(
                    textCursor().position(),
                    lastGoodPosition
                ),
                QTextCursor::KeepAnchor
            );
            cursor.removeSelectedText();

            history.back().endPos = cursor.position();

            appendText("\n");

            QString input = history.back().getInput(*this);
            if (input.isEmpty()) {
                appendNewPrompt();
                followLatestOutput();
            }
            else {
                followLatestOutput();
                // implementation may (does) queue...
                evaluate(input, history.back().meta);
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
            // didn't want me to erase it all?  Hmmm...what this entabbing
            // and detabbing is you mention?  With that, I'd have
            // to hit BACKSPACE and tab.  Twice as many keystrokes for an
            // *extremely* common operation..." :-P
            //
            // Tab with a selection should entab and detab, but with spaces.

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
            emit reportStatus(
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

    if (ctrled) {
        // For whatever reason, the usual behavior in widgets is to go ahead
        // and consider hitting something like "control backslash" to mean
        // the same thing as backslash.  We throw these out if they made
        // it this far without special handling.

        return;
    }

    textCursor().insertText(event->text());
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



// Trap cut, paste so we can limit the selection to the edit buffer

void ReplPad::cutSafely() {
    containInputSelection();
    QTextEdit::cut();
}

void ReplPad::pasteSafely() {
    containInputSelection();
    QTextEdit::paste();
}
