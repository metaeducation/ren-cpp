#ifndef RENGARDEN_RENSHELL_H
#define RENGARDEN_RENSHELL_H

//
// renshell.h
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

#include <QProcess>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include "rencpp/ren.hpp"

//
// Implements the shelldialect by using QProcess to do I/O with bash
// or sh or command...while utilizing RenCpp to do processing on that
// input or maybe output.
//
// VERY EARLY TECHNICAL HACK DRAFT JUST TO SEE HOW TO GET IT WORKING
//

class RenShell : public QObject
{
    Q_OBJECT

public:
    RenShell (ren::Context const & helpers, QObject * parent = nullptr);
    ~RenShell () override;

private:
    QMutex shellDoneMutex;
    QWaitCondition shellDone;
    bool shellDoneSuccess;
    int shellDoneResult;

private:
    ren::Context helpers;
    ren::AnyValue shellFunction;
    bool testMode;
public:
    ren::Function getShellDialectFunction() {
        return static_cast<ren::Function>(shellFunction);
    }

private:
    QThread workerThread;

public slots:
    void handleResults(int result);
signals:
    // keep terminology from Qt sample
    void operate(QString const & input, std::ostream * os);
    void finishedEvaluation();
public:
    void evaluate(QString const & input, std::ostream & os);
};

#endif
