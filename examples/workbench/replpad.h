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

class ReplPad : public QTextEdit
{
    Q_OBJECT

public:
    ReplPad (QWidget * parent);

protected:
    QFont defaultFont;

protected:
    QMutex modifyMutex;
signals:
    void requestConsoleReset();
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
    QString getCurrentInput() const;
    void clearCurrentInput();
    void containInputSelection();

protected:
    virtual bool isReadyToModify(QKeyEvent * event) = 0;

    void keyPressEvent(QKeyEvent * event) override final;
    void mousePressEvent(QMouseEvent * event) override;

signals:
    void commandStatus(QString const & str);

protected:
    virtual void evaluate(QString const & input) = 0;
    virtual void printPrompt() = 0;
    virtual void printMultilinePrompt() = 0;

private:
    bool hasUndo;
    bool hasRedo;
    class HistoryEntry {
    public:
        int inputPos;
        bool multiLineMode;
        int evalCursorPos;
        int endPos;
    public:
        HistoryEntry (int inputPos) :
            inputPos (inputPos),
            multiLineMode (false),
            evalCursorPos (-1),
            endPos (-1)
        {
        }
    };
    std::vector<HistoryEntry> history;
protected:
    void appendNewPrompt();

};

#endif
