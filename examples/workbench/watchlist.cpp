//
// watchlist.cpp
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

#include <QTableWidget>

#include "watchlist.h"
#include "mainwindow.h"

#include "rencpp/ren.hpp"
#include "rencpp/runtime.hpp"



///
/// WATCH WIDGET ITEM REPRESENTING A SINGLE CELL IN THE WATCH TABLE
///

//
// Even if we allowed editing in the watch widget table cells (such as to poke
// into the memory), we'd still want to subclass the cells to get notified
// *prior* to the edit to do data validation.  As it is, allowing the user
// to select in and edit copy/paste is a convenience...we just reject any
// actual commits to that tinkering.
//

class WatchWidgetItem : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem; // inherit constructors

public:
    void setData(int role, QVariant const & value) override {
        if (role != Qt::EditRole) {
            // We only trap attempts to edit the shown string (not
            // programmatic changes to color, etc.)

            QTableWidgetItem::setData(role, value);
            return;
        }

        auto watchList = qobject_cast<WatchList *>(tableWidget());
        watchList->setItemData(this, value);
    }
};

void WatchList::setItemData(WatchWidgetItem * item, QVariant const & value) {

    switch (item->column()) {
        case 0: {
            // If they type text into the first column and it doesn't match the
            // text that was there, then consider it to be a label.  If they
            // delete everything, consider it to be "unlabeling"

            QString contents = value.toString();
            if (contents.isEmpty()) {
                watchList[item->row()].tag = ren::none;
                item->QTableWidgetItem::setTextColor(Qt::black);
            } else {
                watchList[item->row()].tag = ren::String {
                    QString("{") + contents + "}"
                };
                item->QTableWidgetItem::setTextColor(Qt::darkMagenta);
            }

            item->QTableWidgetItem::setData(
                Qt::EditRole,
                watchList[item->row()].getWatchString()
            );
            break;
        }

        case 1:
            watchStatus(
                "Memory editing of variables not supported (yet...)"
            );
            break;

        default:
            throw std::runtime_error("Illegal column selected");
    }
}



///
/// WATCHER CLASS REPRESENTING A SINGLE WATCHED VALUE OR EXPRESSION
///


WatchList::Watcher::Watcher (
    ren::Value const & watch,
    bool useCell,
    ren::Value const & tag
) :
    watch (watch),
    useCell (useCell),
    tag (tag),
    frozen (false)
{
    evaluate(true);
}


void WatchList::Watcher::evaluate(bool firstTime) {
    try {
        if (firstTime or (not useCell) or (not frozen))
            value = watch(); // apply it
    }
    catch (ren::evaluation_error & e) {
        value = ren::Value {};
        error = e.error();
        return;
    }

    error = ren::none;
}


QString WatchList::Watcher::getWatchString() const {
    if (tag) {
        ren::String str {tag};
        return str.spellingOf<QString>();
    }

    // Should there be a way to automatically debug based on the
    // address values of the cell?  That could be helpful.  Could
    // also just have a mode that shows that anyway...tooltip?

    return static_cast<QString>(watch);
}


QString WatchList::Watcher::getValueString() const {
    if (error)
        return static_cast<QString>(error);
    return static_cast<QString>(value);
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

    // Lots of interesting options for right click menus on watch items,
    // although we are exploring what can be done with the "watch dialect"
    // to keep your hands off the mouse and still be productive

    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(
        this, &WatchList::customContextMenuRequested,
        this, &WatchList::customMenuRequested,
        Qt::DirectConnection
    );


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
                ren::Block blockArg = static_cast<ren::Block>(arg);
                auto it = blockArg.begin();
                while (it != blockArg.end()) {
                    ren::print(*it);
                    it++;
                }
                /*
                for (auto item : static_cast<ren::Block>(arg)) {
                    watchDialect(item, false, false, ren::none);
                }*/
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
        this, &WatchList::pushWatcherRequested,
        this, &WatchList::pushWatcher,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &WatchList::removeWatcherRequested,
        this, &WatchList::removeWatcher,
        Qt::BlockingQueuedConnection
    );

    connect(
        this, &WatchList::freezeItemRequested,
        this, &WatchList::freezeWatcher,
        Qt::BlockingQueuedConnection
    );
}


void WatchList::pushWatcher() {
    Watcher & watcher = watchList.back();

    WatchWidgetItem * name = new WatchWidgetItem;
    WatchWidgetItem * value = new WatchWidgetItem;

    name->setText(watcher.getWatchString());
    if (watcher.tag or watcher.useCell)
        name->setForeground(Qt::darkMagenta);

    QString temp = watcher.getValueString();
    value->setText(watcher.getValueString());

    if (watcher.useCell) {
        value->setForeground(Qt::darkGreen);
    }
    else if (watcher.error) {
        value->setForeground(Qt::darkRed);
    }

    int count = rowCount();
    insertRow(count);

    setItem(count, 0, name);
    setItem(count, 1, value);

    emit showDockRequested();
}


void WatchList::removeWatcher(int index) {
    if (static_cast<size_t>(index) > watchList.size())
        return;

    watchList.erase(std::begin(watchList) + (index - 1));
    removeRow(index - 1);

    return;
}


void WatchList::freezeWatcher(int index, bool) {
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
    for (Watcher & watcher : watchList) {
        QString oldText = item(row, 1)->text();
        QBrush oldBrush = item(row, 1)->foreground();
        QBrush newBrush = QBrush(Qt::black);

        watcher.evaluate();
        if (watcher.useCell) {
            newBrush = QBrush(Qt::darkGreen);
        }
        else if (watcher.error) {
            newBrush = QBrush(Qt::darkRed);
        }

        QString newText = watcher.getValueString();

        // We only update the table if it has changed for (possible)
        // efficiency, but also to give visual feedback by selecting the
        // watches that have changed since the last update.  Esoteric
        // case of having a string that was formatted like an error
        // because of an evaluation problem vs. being the literal string
        // aren't so esoteric in systems where you convert errors to strings,
        // so checking the brush color is actually not pointless.

        item(row, 0)->setSelected(false);
        if ((oldText != newText) or (oldBrush != newBrush)) {
            WatchWidgetItem * newValueItem = new WatchWidgetItem;

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
            emit removeWatcherRequested(-index);
            return ren::unset;
        }

        // Experimental trick: apply the error, if it is none it is a no-op
        // Nifty benefit of Generalized Apply!  But double check that it's
        // an error or a none, first, here :-)

        Q_ASSERT(
            watchList[index - 1].error.isNone()
            or watchList[index - 1].error.isError()
        );

        watchList[index - 1].error(); // apply the error-or-none

        return watchList[index - 1].value();
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
    // for granted the creation of a Watcher for words.  Then we
    // can throw an error up to the console on a /cell request.
    // Because there's no use adding it to the watch list if it will
    // never be evaluated again (a regular watch may become good...)

    if (arg.isWord() or arg.isPath() or arg.isParen()) {

        Watcher watcher {
            arg,
            static_cast<bool>(useCell),
            useLabel ? ren::Tag {tag} : ren::Value {ren::none}
        };

        if (watcher.useCell and watcher.error) {
            watcher.error(); // should throw...

            throw std::runtime_error("Unreachable");
        }

        // we append to end instead of inserting at the top because
        // it keeps the numbering more consistent.  some people might
        // desire it added at the top and consider it better that way.

        watchList.push_back(watcher);

        emit pushWatcherRequested();
        return ren::unset;
    }

    throw std::runtime_error("unexpected type passed to watch dialect");

    return ren::unset;
}


///
/// POPUP CONTEXT MENU
///

//
// Because it's not particularly computationally intensive to create a
// popup menu, we make a new one on each request and use lambda functions
// instead of worrying about setting up a bunch of slots to deal with
// QAction triggers.
//

void WatchList::customMenuRequested(QPoint pos){

    QMenu * menu = new QMenu {this};

    int index = currentRow() + 1;


    //
    // Create the actions for the popup menu
    //

    QAction * frozenAction = new QAction("Frozen", this);
    frozenAction->setCheckable(true);
    frozenAction->setChecked(watchList[index].frozen);
    frozenAction->setStatusTip(
        QString("Freeze or unfreeze this watcher, or [watch [freeze +/-")
        + QString::number(index) + "]"
    );

    QAction * duplicateAction = new QAction("Duplicate", this);
    duplicateAction->setStatusTip(
        QString("Make a copy of this watcher, or [copy ")
        + QString::number(index) + "]"
    );

    QAction * unwatchAction = new QAction("Unwatch", this);
    unwatchAction->setShortcuts(
        QList<QKeySequence> {
            QKeySequence::Delete,
            QKeySequence(Qt::Key_Backspace)
        }
    );
    unwatchAction->setStatusTip(
        QString("Stop watching this expression, or [watch -")
        + QString::number(index) + "]"
    );


    //
    // Order the items in the menu.  Ideally this would use some kind of
    // heuristic about your common operations to order it (or be configurable)
    // but we try to guess so that the most useful items are at the top,
    // the most dangerous at the bottom, etc.
    //

    menu->addAction(frozenAction);
    menu->addAction(duplicateAction);
    menu->addAction(unwatchAction);


    //
    // Connect menu item actions to code that implements them
    //

    connect(
        unwatchAction, &QAction::triggered,
        [this, index](bool) { // bool is "checked"

            // Note the threading issues tied up here.  We are on the GUI
            // thread, we may be in mid-evaluation of code in the console,
            // etc.  Because we're on the GUI we shouldn't emit the blocking
            // queued signal for requesting a watch.  Review all this.

            removeWatcher(index);
        }
    );

    connect(
        frozenAction, &QAction::triggered,
        [this, index](bool frozen) { // bool is "checked"

            // Note the threading issues tied up here.  We are on the GUI
            // thread, we may be in mid-evaluation of code in the console,
            // etc.  Because we're on the GUI we shouldn't emit the blocking
            // queued signal for requesting a watch.  Review all this.

            if (frozen)
                freezeWatcher(index, frozen);
        }
    );

    //
    // Pop up the menu.  Note this call returns while the menu is up, so we
    // can't just delete it...we instead register a hook to clean it up when
    // it's hidden.
    //

    menu->popup(viewport()->mapToGlobal(pos));

    connect(
        menu, &QMenu::aboutToHide,
        menu, &QObject::deleteLater,
        Qt::DirectConnection // it's "deleteLater", already queued...
    );

}

