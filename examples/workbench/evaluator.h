#ifndef RENGARDEN_EVALUATOR_H
#define RENGARDEN_EVALUATOR_H

//
// evaluator.h
// This file is part of Ren Garden
// Copyright (C) 2015-2018 MetÆducation
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

// QMessageBox is not really used in this header but for some reason there are
// bizarre errors coming from the moc build if a GUI file is not included:
//     http://stackoverflow.com/questions/32773868/
#include <QMessageBox>

#include <QString>
#include <QObject>

#include "rencpp/ren.hpp"

//
// WORKER OBJECT FOR HANDLING REN EVALUATIONS
//

//
// We push this item to the worker thread and let it do the actual evaluation
// while we keep monitoring the GUI for an interrupt
//
// http://doc.qt.io/qt-5/qthread.html#details
//
// Ultimately it should be the case that the GUI never calls an "open coded"
// arbitrary evaluation of user code in the runtime.  Short things
// might be okay if you are *certain* the evaluator is not currently running.
//

class EvaluatorWorker : public QObject
{
    Q_OBJECT

public slots:
    // See notes on MainWindow about qRegisterMetaType about why dialect
    // and context are passed as ren::AnyValue instead of ren::Function and
    // ren::AnyContext (also why it needs the ren:: prefix for slots)
    void doWork(
        ren::AnyValue const & dialectValue,
        ren::AnyValue const & contextValue,
        QString const & input,
        bool meta
    );

    void initialize();

signals:
    void resultReady(
        bool success,
        ren::optional<ren::AnyValue> const & result // `ren::` needed for signal!
    );

    void initializeDone();

    void caughtNonRebolException(char const * what);
};

#endif
