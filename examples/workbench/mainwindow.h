#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
