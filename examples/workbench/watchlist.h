#ifndef WATCHLIST_H
#define WATCHLIST_H

//
// watchlist.h
// This file is part of Ren Garden
// Copyright (C) 2014 HostileFork.com
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
// See http://ren-garden.hostilefork.com/ for more information on this project
//


#include <QTableWidget>

#include "rencpp/ren.hpp"

class MainWindow;

class WatchList : public QTableWidget
{
    Q_OBJECT

public:
    WatchList (QWidget * parent = nullptr);

signals:
    void watchCalled(
        ren::Value vars,
        bool useCell,
        ren::Value label
    );

    void showDockRequested();

    void hideDockRequested();

    void watchItemPushed(); // last element in the vector

    void removeWatchItemRequested(int index);

public slots:
    void updateWatches();

private slots:
    void handlePushedWatchItem();

    void handleRemoveWatchItemRequest(int index);

protected:
    void mousePressEvent(QMouseEvent * event) override;

private:
    class WatchItem {
        ren::Value watch;
        bool useCell;
        ren::Value value;
        ren::Value error;
        ren::Tag tag;

    public:
        // Construct will also evaluate to capture at the time of the watch
        // being added (particularly important if it's a cell)
        WatchItem (
            ren::Value const & watch,
            bool useCell,
            ren::Tag const & tag
        );

        // Evaluates and returns error if there was one, or none
        void evaluate(bool firstTime = false);

        QString getWatchString() const;

        QString getValueString() const;

        ren::Value getValue() const;

        ren::Value getError() const;

        bool hadError() const;

        bool isLabeled() const;

        bool isCell() const;
    };

    std::vector<WatchItem> watchList;

private:
    // aaaand... magic! :-)
    ren::Value watchDialect(
        ren::Value const & arg,
        ren::Value const & useCell,
        ren::Value const & useLabel,
        ren::Tag const & tag
    );
};

#endif
