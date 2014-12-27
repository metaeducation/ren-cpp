#ifndef RENCONSOLE_H
#define RENCONSOLE_H

#include <QTextEdit>

class FakeStdout;

class MainWindow;

class RenConsole : public QTextEdit
{
    Q_OBJECT

public:
    RenConsole(MainWindow * parent);

protected:
    void keyPressEvent(QKeyEvent * event);

private:
    MainWindow * parent;
    QString prompt;
    QTextCharFormat promptFormat;
    int inputPos;
    QSharedPointer<FakeStdout> fakeOut;
};

#endif
