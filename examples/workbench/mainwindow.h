#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//
// mainwindow.h
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


#include <QMainWindow>

class RenConsole;
class WatchList;

class QAction;
class QMenu;

extern bool forcingQuit;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void cut();
    void copy();
    void paste();

    void about();
    void updateMenus();
    void switchLayoutDirection();

private:
    // Cool trick... hold down escape to fade window and "poof".

    // We start the opacity value a little bit higher than 1.0, so the first
    // short while after requesting a fade out no effect is seen (and we don't
    // need a separate timing for that).

    static constexpr qreal const initialOpacity = 1.1;
    static constexpr qreal const quittingOpacity = 0.5;
    static constexpr qreal const deltaOpacity = 0.05;
    static int const msecInterval = 150;

    qreal opacity;
    bool fading;
    QTimer * fadeTimer;

    void onFadeOutToQuit(bool active);

private:
    void createActions();
    void createMenus();
    void createStatusBar();
    void readSettings();
    void writeSettings();

    // Should be private but just doing hack-and-slash testing right now
    // of Rencpp, not overly concerned about the elegance of this program
    // until that is assuredly elegant...!
public:
    RenConsole * console;
    QDockWidget * dockWatch;

private:
    QMenu * fileMenu;
    QMenu * editMenu;
    QMenu * windowMenu;
    QMenu * helpMenu;

    QAction * exitAct;

    QAction * cutAct;
    QAction * copyAct;
    QAction * pasteAct;

    QAction * newTabAct;
    QAction * nextTabAct;
    QAction * previousTabAct;
    QAction * closeTabAct;

    QAction * separatorAct;

    QAction * aboutAct;

private slots:
    void onShowDockRequested(WatchList * watchList);
    void onHideDockRequested(WatchList * watchList);
};

#endif
