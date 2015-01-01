#ifndef WATCHLIST_H
#define WATCHLIST_H

//
// watchlist.h
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


#include <QTableWidget>

#include "rencpp/ren.hpp"

class MainWindow;

class WatchWidgetItem;

class WatchList : public QTableWidget
{
    Q_OBJECT

public:
    WatchList (QWidget * parent = nullptr);

private:
    friend class WatchWidgetItem;
    void setItemData(WatchWidgetItem * item, const QVariant & value);

protected slots:
    void customMenuRequested(QPoint pos);

signals:
    void watchCalled(
        ren::Value vars,
        bool useCell,
        ren::Value label
    );

    void showDockRequested();

    void hideDockRequested();

    // last element in the vector ATM, probably should make Watcher able
    // to go against signal/slots with metaobject
    void pushWatcherRequested();

    void removeWatcherRequested(int index);

    void freezeItemRequested(int index, bool frozen);

    void watchStatus(QString message);

public slots:
    void updateWatches();

private slots:
    void pushWatcher();

    void removeWatcher(int index);

    void freezeWatcher(int index, bool frozen);

protected:
    void mousePressEvent(QMouseEvent * event) override;

private:
    class Watcher {
        friend class WatchList;

        ren::Value watch;
        bool useCell;
        ren::Value value;
        ren::Value error;
        ren::Value tag;
        bool frozen;

    public:
        // Construct will also evaluate to capture at the time of the watch
        // being added (particularly important if it's a cell)
        Watcher (
            ren::Value const & watch,
            bool useCell,
            ren::Value const & tag
        );

        // Evaluates and returns error if there was one, or none
        void evaluate(bool firstTime = false);

        QString getWatchString() const;

        QString getValueString() const;
    };

    std::vector<Watcher> watchList;

private:
    // aaaand... magic! :-)
    ren::Value watchDialect(
        ren::Value const & arg,
        bool useCell,
        bool useLabel,
        ren::Value const & tag
    );
};

#endif
