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

#ifndef RENCONSOLE_H
#define RENCONSOLE_H

#include <QTextEdit>
#include <QMutex>
#include <QThread>

#include "rencpp/ren.hpp"

#include "replpad.h"
#include "rensyntaxer.h"
#include "renshell.h"

class FakeStdout;
class FakeStdoutBuffer;
class MainWindow;

class RenConsole : public ReplPad
{
    Q_OBJECT

private:
    RenSyntaxer syntaxer;
    RenSyntaxer & getSyntaxer() override { return syntaxer; }

    RenShell shell;

public:
    RenConsole (QWidget * parent = nullptr);
    ~RenConsole () override;

protected:
    bool bannerPrinted;
    void printBanner();
    QString getPromptString() override;

protected:
    friend class FakeStdoutBuffer;
    QSharedPointer<FakeStdout> fakeOut;
signals:
    void needTextAppend(QString text);
private slots:
    void onAppendText(QString const & text);
protected:
    void appendText(QString const & text) override;

protected:
    bool isReadyToModify(QKeyEvent * event) override;

private:
    bool evaluating;
    QThread workerThread;

private:
    ren::Value consoleFunction;

    // Function representing the current dialect (registered with CONSOLE)
    ren::Value dialect;

    void escape() override;

private:
    // Experimental facility for writing the shell's output to a string

    ren::Value target;

    // You can set up a value that represents what the pending console
    // buffer will be.  It is evaluated after the command is done to
    // produce a string, and will be forgotten once used.  If there is
    // an error in the command, no buffer will be loaded.

    QString pendingBuffer;
    int pendingAnchor;
    int pendingPosition;

public slots:
    void handleResults(
        bool success,
        ren::Value const & result
    );
signals:
    // keep terminology from Qt sample
    void operate(
        ren::Value const & dialect,
        QString const & input,
        bool meta
    );
    void finishedEvaluation();
protected:
    void evaluate(QString const & input, bool meta) override;
};

#endif
