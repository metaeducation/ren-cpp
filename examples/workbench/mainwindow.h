#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//
// mainwindow.h
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


#include <QMainWindow>

class RenConsole;
class WatchList;

class QAction;
class QMenu;


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
    WatchList * watchList;

private:
    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *helpMenu;

    QAction *exitAct;

    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;

    QAction *separatorAct;

    QAction *aboutAct;
    QAction *aboutQtAct;
};

#endif
