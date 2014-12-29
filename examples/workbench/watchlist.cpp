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
    ren::Value const & label
) :
    watch (watch),
    useCell (useCell),
    label (label)
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
    if (label) {
        ren::Word word {label};
        return word.spellingOf().c_str();
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
    return static_cast<bool>(label);
}



///
/// WATCHLIST CONSTRUCTOR
///

WatchList::WatchList(MainWindow * mainWindow, QWidget * parent) :
    QTableWidget (0, 2, parent),
    mainWindow (mainWindow)
{
    setHorizontalHeaderLabels(QStringList() << "name" << "value");

    setSelectionMode(QAbstractItemView::MultiSelection);

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

    auto watchFunction = ren::make_Extension(
        "{WATCH dialect for monitoring and un-monitoring in the Ren Workbench}"
        ":watches [word! path! block! paren! integer!]"
        "    {what to add to the list for evaluation}"
        "/cell {Monitor the resulting boxed cell, not the expression}"
        "/label name [word!] {Label the expression in the view.}",

        [this](
            ren::Value && vars,
            ren::Value && useCell,
            ren::Value && useLabel,
            ren::Value && label
        )
            -> ren::Value
        {
            ren::Value result;
            if (vars.isInteger()) {
                ren::Integer index {vars};

                // Positive integer is a request for the data held by the
                // watch, which we need to do here and return synchronously.
                // Negative integers affect the GUI and run on GUI thread,
                // and do not return anything.  Zero should throw an error
                // but we'll just do unset for now.

                if (index >= 0) {
                    if (static_cast<size_t>(index) > this->watchList.size())
                        return result; // unset

                    return this->watchList[index - 1].getValue();
                }

                emit removeWatchItemRequested(-index);
                return result; // unset;
            }

            // Should this be a common routine?
            int logicIndex = -1;
            if (vars.isWord()) {
                static std::vector<QString> logicWords[2] =
                    {{"off", "no", "false"}, {"on", "yes", "true"}};

                ren::Word word = ren::Word {vars};
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

            if (vars.isLogic()) {
                logicIndex = static_cast<bool>(vars);
            }

            if (logicIndex != -1) {
                if (logicIndex)
                    emit showDockRequested();
                else
                    emit hideDockRequested();
                return result; // unset
            }

            // With those words out of the way, we should be able to take
            // for granted the creation of a WatchItem for words.  Then we
            // can throw an error up to the console on a /cell request.
            // Because there's no use adding it to the watch list if it will
            // never be evaluated again (a regular watch may become good...)

            if (vars.isWord() or vars.isPath() or vars.isParen()) {

                WatchItem watchItem {
                    vars,
                    static_cast<bool>(useCell),
                    useLabel ? label : label
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
                return result; // unset;
            }

           if (vars.isBlock()) {
                // Lots of ideas one could have for a block-based dialect.
                // [on x y z off q/r/s :foo]

                // Let's try this by making an error object and then applying

                ren::Value error = ren::runtime(
                    "make error! {No dialect yet for watch!}"
                );

                throw ren::evaluation_error(error);

                throw std::runtime_error("unreachable");
            }

            return result; // unset
        }
    );

    // Now bind the function to the word.  But see remarks on using function
    // values as "unactivated" for assignment directly in a series here, need
    // to put it in a block and use "first" (or similar)
    //
    //     http://stackoverflow.com/q/27641809/211160

    ren::runtime("watch: first", ren::Block {watchFunction});

    // We also have to hook up the watchCalled and handleWatch signals and
    // slots.  This could be asynchronous, however we actually are
    // looking at some of the values and running the evaluator.  So we
    // have to hold up the running evaluator when watch is called

    connect(
        this, &WatchList::showDockRequested,
        mainWindow->dockWatch, &QDockWidget::show,
        Qt::QueuedConnection
    );

    connect(
        this, &WatchList::hideDockRequested,
        mainWindow->dockWatch, &QDockWidget::hide,
        Qt::QueuedConnection
    );

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

    mainWindow->dockWatch->setVisible(true);
}



void WatchList::handleRemoveWatchItemRequest(int index) {
    if (static_cast<size_t>(index) > watchList.size())
        return;

    watchList.erase(std::begin(watchList) + (index - 1));
    removeRow(index - 1);

    return;
}



void WatchList::updateWatches() {
    if (not isVisible())
        return;

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

        if ((oldText != newText) or (oldBrush != newBrush)) {
            QTableWidgetItem * newValueItem = new QTableWidgetItem;

            newValueItem->setForeground(newBrush);
            newValueItem->setText(newText);
            setItem(row, 1, newValueItem);

            // Can't set selected before insertion
            item(row, 1)->setSelected(true);
        }
        else {
            item(row, 1)->setSelected(false);
        }

        row++;
    }
}



///
/// DESTRUCTOR
///

WatchList::~WatchList() {
}

