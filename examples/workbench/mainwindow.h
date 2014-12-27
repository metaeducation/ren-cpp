#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class RenConsole;
class WatchList;

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class Qconsole;
class QMdiSubWindow;
class QSignalMapper;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
#ifndef QT_NO_CLIPBOARD
    void cut();
    void copy();
    void paste();
#endif
    void about();
    void updateMenus();
    void switchLayoutDirection();

private:
    void createActions();
    void createMenus();
    void createToolBars();
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
    QMenu *windowMenu;
    QMenu *helpMenu;
    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *saveAsAct;
    QAction *exitAct;
#ifndef QT_NO_CLIPBOARD
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
#endif
    QAction *closeAct;
    QAction *closeAllAct;
    QAction *tileAct;
    QAction *cascadeAct;
    QAction *nextAct;
    QAction *previousAct;
    QAction *separatorAct;
    QAction *aboutAct;
    QAction *aboutQtAct;
};

#endif
