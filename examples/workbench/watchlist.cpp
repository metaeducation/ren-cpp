//
// watchlist.cpp
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


#include <QtWidgets>

#include <QTableWidget>

#include "watchlist.h"
#include "mainwindow.h"

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"



///
/// WATCHITEM
///


WatchList::WatchItem::WatchItem (
    ren::Value const & watch,
    bool useCell,
    ren::Value const & tag
) :
    watch (watch),
    useCell (useCell),
    tag (tag)
{
    evaluate(true);
}


void WatchList::WatchItem::evaluate(bool firstTime) {
    try {
        if (firstTime or not useCell)
            value = watch(); // apply it
    }
    catch (ren::evaluation_error & e) {
        value = ren::Value {};
        error = e.error();
        return;
    }

    error = ren::none;
}


QString WatchList::WatchItem::getWatchString() const {
    if (tag) {
        return static_cast<QString>(tag);
    }

    // Should there be a way to automatically debug based on the
    // address values of the cell?  That could be helpful.  Could
    // also just have a mode that shows that anyway...tooltip?

    return static_cast<QString>(watch);
}


QString WatchList::WatchItem::getValueString() const {
    if (error)
        return static_cast<QString>(error);
    return static_cast<QString>(value);
}


ren::Value WatchList::WatchItem::getValue() const {
    return value;
}


bool WatchList::WatchItem::hadError() const {
    return static_cast<bool>(error);
}


ren::Value WatchList::WatchItem::getError() const {
    return error;
}


bool WatchList::WatchItem::isCell() const {
    return useCell;
}


bool WatchList::WatchItem::isLabeled() const {
    return static_cast<bool>(tag);
}



///
/// WATCHLIST CONSTRUCTOR
///

WatchList::WatchList(QWidget * parent) :
    QTableWidget (0, 2, parent)
{
    setHorizontalHeaderLabels(QStringList() << "name" << "value");

    // We want the value column of TableWidget to get wider if the splitter
    // or dock widget give it more space

    horizontalHeader()->setStretchLastSection(true);

    // Create the function that will be added to the environment to be bound
    // to the word WATCH.  If you give it a word or path it will be quoted,
    // but if you give it a paren it will be an expression to evaluate.  If
    // you give it a block it will be interpreted in the "watch dialect".
    // Feature under development.  :-)

    // We face a problem here that Ren is not running on the GUI thread.
    // That's because we want to be able to keep the GUI responsive while
    // running.  But we have to manage our changes on the data structures
    // by posting messages.

    // Because we're quoting it's hard to get a logic, so the reserved
    // words for on, off, true, false, yes, and no are recognized explicitly
    // Any logic value could be used with parens however, and if those words
    // have been reassigned to something else the parens could work for that
    // as well.

    auto watchFunction = ren::makeFunction(
        "{WATCH dialect for monitoring and un-monitoring in the Ren Workbench}"
        ":arg [word! path! block! paren! integer!]"
        "    {word to watch or other legal parameter, see documentation)}",

        REN_STD_FUNCTION,

        [this](ren::Value const & arg) -> ren::Value {

            if (arg.isBlock()) {
                ren::print("First argument is", arg);
                for (auto item : ren::Block {arg}) {
                    //watchDialect(item, false, false, ren::none);
                }
                return ren::none;
            }

            return watchDialect(arg, false, false, ren::none);
        }
    );

    ren::runtime("watch: quote", watchFunction);

    // We also have to hook up the watchCalled and handleWatch signals and
    // slots.  This could be asynchronous, however we actually are
    // looking at some of the values and running the evaluator.  So we
    // have to hold up the running evaluator when watch is called

    connect(
        this, &WatchList::watchItemPushed,
        this, &WatchList::handlePushedWatchItem,
        Qt::BlockingQueuedConnection
    );

   connect(
        this, &WatchList::removeWatchItemRequested,
        this, &WatchList::handleRemoveWatchItemRequest,
        Qt::BlockingQueuedConnection
    );
}


void WatchList::handlePushedWatchItem() {
    WatchItem & watchItem = watchList.back();

    QTableWidgetItem * name = new QTableWidgetItem;
    QTableWidgetItem * value = new QTableWidgetItem;

    name->setText(watchItem.getWatchString());
    if (watchItem.isLabeled() or watchItem.isCell())
        name->setForeground(Qt::darkMagenta);

    QString temp = watchItem.getValueString();
    value->setText(watchItem.getValueString());

    if (watchItem.isCell()) {
        value->setForeground(Qt::darkGreen);
    }
    else if (watchItem.hadError()) {
        value->setForeground(Qt::darkRed);
    }

    int count = rowCount();
    insertRow(count);

    setItem(count, 0, name);
    setItem(count, 1, value);

    emit showDockRequested();
}



void WatchList::handleRemoveWatchItemRequest(int index) {
    if (static_cast<size_t>(index) > watchList.size())
        return;

    watchList.erase(std::begin(watchList) + (index - 1));
    removeRow(index - 1);

    return;
}


void WatchList::mousePressEvent(QMouseEvent * event) {
    setSelectionMode(QAbstractItemView::SingleSelection);
    QTableWidget::mousePressEvent(event);
}


void WatchList::updateWatches() {
    if (not isVisible())
        return;

    // We only temporarily do this selection mode to do our highlighting
    // it's turned off before you can ever click the mouse...

    setSelectionMode(QAbstractItemView::MultiSelection);

    int row = 0;
    for (WatchItem & watchItem : watchList) {
        QString oldText = item(row, 1)->text();
        QBrush oldBrush = item(row, 1)->foreground();
        QBrush newBrush = QBrush(Qt::black);

        watchItem.evaluate();
        if (watchItem.isCell()) {
            newBrush = QBrush(Qt::darkGreen);
        }
        else if (watchItem.hadError()) {
            newBrush = QBrush(Qt::darkRed);
        }

        QString newText = watchItem.getValueString();

        // We only update the table if it has changed for (possible)
        // efficiency, but also to give visual feedback by selecting the
        // watches that have changed since the last update.  Esoteric
        // case of having a string that was formatted like an error
        // because of an evaluation problem vs. being the literal string
        // aren't so esoteric in systems where you convert errors to strings,
        // so checking the brush color is actually not pointless.

        item(row, 0)->setSelected(false);
        if ((oldText != newText) or (oldBrush != newBrush)) {
            QTableWidgetItem * newValueItem = new QTableWidgetItem;

            newValueItem->setForeground(newBrush);
            newValueItem->setText(newText);
            setItem(row, 1, newValueItem);

            // Can't set a newly created element selected before insertion

            item(row, 1)->setSelected(true);
        }
        else {
            item(row, 1)->setSelected(false);
        }

        row++;
    }
}


ren::Value WatchList::watchDialect(
    ren::Value const & arg,
    bool useCell,
    bool useLabel,
    ren::Value const & tag
) {
    if (arg.isInteger()) {
        int signedIndex = ren::Integer {arg};
        if (signedIndex == 0) {
            ren::runtime("do make error! {Integer arg must be nonzero}");
            return ren::unset; // unreachable
        }

        bool removal = signedIndex < 0;
        size_t index = std::abs(signedIndex);

        // Positive integer is a request for the data held by the
        // watch, which we need to do here and return synchronously.
        // Negative integers affect the GUI and run on GUI thread.

        if (index > this->watchList.size()) {
            ren::runtime("do make error! {No such watchlist item index}");
            return ren::unset; // unreachable
        }

        if (removal) {
            emit removeWatchItemRequested(-index);
            return ren::unset;
        }

        if (watchList[index - 1].hadError()) {
            watchList[index - 1].getError()(); // apply the error
            return ren::unset; // unreachable
        }

        return watchList[index - 1].getValue();
    }

    // Let's use the evaluator for this trick.  :-)  Have it give us back
    // -1, 0, or 1... -1 meaning "word is not a logic synonym"

    int logicIndex = -1;

#ifdef GARDEN_CPP_WAY
    if (arg.isWord()) {
        static std::vector<QString> logicWords[2] =
            {{"off", "no", "false"}, {"on", "yes", "true"}};

        ren::Word word = ren::Word {arg};
        auto spelling = word.spellingOf<QString>();
        for (int index = 0; index < 2; index++) {
            if (
                logicWords[index].end()
                != std::find(
                    logicWords[index].begin(),
                    logicWords[index].end(),
                    spelling
                )
            ) {
                logicIndex = index;
                break;
            }
        }
    }

    if (arg.isLogic()) {
        logicIndex = static_cast<bool>(arg);
    }
#else
    logicIndex = ren::Integer {
        ren::runtime(
            "case", ren::Block {
            "    find [off no false] quote", arg, "[0]"
            "    find [on yes true] quote", arg, "[1]"
            "    true [-1]"
            }
        )
    };
#endif

    if (logicIndex != -1) {
        if (logicIndex)
            emit showDockRequested();
        else
            emit hideDockRequested();
        return ren::unset;
    }

    // With those words out of the way, we should be able to take
    // for granted the creation of a WatchItem for words.  Then we
    // can throw an error up to the console on a /cell request.
    // Because there's no use adding it to the watch list if it will
    // never be evaluated again (a regular watch may become good...)

    if (arg.isWord() or arg.isPath() or arg.isParen()) {

        WatchItem watchItem {
            arg,
            static_cast<bool>(useCell),
            useLabel ? ren::Tag {tag} : ren::Value {ren::none}
        };

        if (watchItem.isCell() and watchItem.hadError()) {
            watchItem.getError()(); // should throw...

            throw std::runtime_error("Unreachable");
        }

        // we append to end instead of inserting at the top because
        // it keeps the numbering more consistent.  some people might
        // desire it added at the top and consider it better that way.

        watchList.push_back(watchItem);

        emit watchItemPushed();
        return ren::unset;
    }

    throw std::runtime_error("unexpected type passed to watch dialect");

    return ren::unset;
}

