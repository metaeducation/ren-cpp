#ifndef REPLPAD_H
#define REPLPAD_H

//
// replpad.h
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

#include <QTextEdit>
#include <QElapsedTimer>

#include "syntaxer.h"

class ReplPad : public QTextEdit
{
    Q_OBJECT

public:
    ReplPad (QWidget * parent);

protected:
    QTextCharFormat promptFormatNormal;
    QTextCharFormat promptFormatMeta;
    QTextCharFormat inputFormatNormal;
    QTextCharFormat inputFormatMeta;
    QTextCharFormat outputFormat;
    QTextCharFormat errorFormat;

protected:
    QFont defaultFont;

private:
    // QTextEdit has a ZoomIn and a ZoomOut that we can call, but it doesn't
    // have a way to query it.  So we keep track of the net number of zoom
    // in and zoom out calls that hvae been made, so we can save it in the
    // preferences file to restore for the next session
    int zoomDelta;
public:
    int getZoom();
    void setZoom(int delta);

protected:
    QMutex modifyMutex;
signals:
    void requestConsoleReset();
    void fadeOutToQuit(bool escaping);
protected slots:
    void onTextChanged();
    void onConsoleReset();

private:
    bool shouldFollow;
    bool isFormatPending;
    QTextCursor endCursor() const;
    QTextCharFormat pendingFormat;
protected:
    virtual void appendText(QString const & text);
    void pushFormat(QTextCharFormat const & format);
public slots:
    void followLatestOutput();
    void dontFollowLatestOutput();

protected:
    void clearCurrentInput();
    void containInputSelection();

protected:
    virtual Syntaxer & getSyntaxer() = 0;

protected:
    virtual bool isReadyToModify(QKeyEvent * event) = 0;

    void keyPressEvent(QKeyEvent * event) override;
    void keyReleaseEvent(QKeyEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;
    void mouseDoubleClickEvent(QMouseEvent * event) override;

    // Trap cut, paste, del so we can limit the selection to the edit buffer
public slots:
    void cutSafely();
    void pasteSafely();

signals:
    void reportStatus(QString const & str);

protected:
    virtual void evaluate(QString const & input, bool meta) = 0;
    virtual QString getPromptString() = 0;

private:
    void rewritePrompt();

private:
    // "Escape" in this sense refers to "escaping the current mode".  The
    // way it is achieved by default is by pressing Qt::Key_Escape twice
    // within the double click period, but it is more a "conceptual escape"
    // that the virtual method is to implement.

    QElapsedTimer escapeTimer;
    virtual void escape() = 0;

private:
    bool hasUndo;
    bool hasRedo;
    class HistoryEntry {
    public:
        int startPos;
        QString prompt;
        int inputPos;
        bool multiLineMode;
        bool meta;
        int evalCursorPos;
        int endPos;
    public:
        HistoryEntry (int startPos, QString prompt) :
            startPos (startPos),
            prompt (prompt),
            inputPos (startPos),
            multiLineMode (false),
            meta (false),
            evalCursorPos (-1),
            endPos (-1)
        {
        }
    public:
        QString getInput(ReplPad & pad) const;
    };
    std::vector<HistoryEntry> history;
protected:
    void appendNewPrompt();

};

#endif
