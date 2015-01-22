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

#include <memory>
#include <vector>

#include "optional/optional.hpp"

#include <QTableWidget>

#include "rencpp/ren.hpp"

class MainWindow;

class WatchList : public QTableWidget
{
    Q_OBJECT

public:
    class Watcher {
        friend class WatchList;

        ren::Value watch;
        bool recalculates;
        ren::Value value;
        ren::Value error;
        std::experimental::optional<ren::Tag> label;
        bool frozen;

    public:
        // Construct will also evaluate to capture at the time of the watch
        // being added (particularly important if it's a cell)
        Watcher (
            ren::Value const & watch,
            bool recalculates,
            std::experimental::optional<ren::Tag> const & label
        );

        // Evaluates and returns error if there was one, or none
        void evaluate(bool firstTime = false);

        QString getWatchString() const;

        QString getValueString() const;
    };

    std::vector<std::unique_ptr<Watcher>> watchers;

public:
    WatchList (QWidget * parent = nullptr);

protected slots:
    void customMenuRequested(QPoint pos);

signals:
    void watchCalled(
        ren::Value vars,
        bool recalculates,
        ren::Value label
    );

    void showDockRequested();

    void hideDockRequested();

    void pushWatcherRequested(Watcher * watcherUnique);

    void removeWatcherRequested(int index);

    void freezeItemRequested(int index, bool frozen);

    void recalulatesItemRequested(int index, bool recalculates);

    void reportStatus(QString message);

public slots:
    void updateWatcher(int index);

    void updateAllWatchers();

private slots:
    void pushWatcher(Watcher * watcherUnique);

    void removeWatcher(int index);

    void duplicateWatcher(int index);

    void setFreezeState(int index, bool frozen);

    void setRecalculatesState(int index, bool recalculates);

    void onItemChanged(QTableWidgetItem * item);

protected:
    void mousePressEvent(QMouseEvent * event) override;

private:
    // aaaand... magic! :-)
    ren::Value watchDialect(
        ren::Value const & arg,
        bool recalculates,
        ren::Value const & tag
    );
};

#endif
