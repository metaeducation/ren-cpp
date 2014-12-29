#ifndef WATCHLIST_H
#define WATCHLIST_H

#include <QTableWidget>

#include "rencpp/ren.hpp"

class MainWindow;

class WatchList : public QTableWidget
{
    Q_OBJECT

public:
    WatchList (QWidget * parent = nullptr);
    ~WatchList () override;

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

private:
    class WatchItem {
        ren::Value watch;
        bool useCell;
        ren::Value value;
        ren::Value error;
        ren::Value label;

    public:
        // Construct will also evaluate to capture at the time of the watch
        // being added (particularly important if it's a cell)
        WatchItem (
            ren::Value const & watch,
            bool useCell,
            ren::Value const & label
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
};

#endif
