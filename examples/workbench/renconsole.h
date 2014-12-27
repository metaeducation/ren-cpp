#ifndef RENCONSOLE_H
#define RENCONSOLE_H

#include <QTextEdit>
#include <QThread>

#include "rencpp/ren.hpp"

class FakeStdout;

class MainWindow;


class RenConsole : public QTextEdit
{
    Q_OBJECT

public:
    RenConsole (MainWindow * parent);
    ~RenConsole () override;

protected:
    void keyPressEvent(QKeyEvent * event);

private:
    MainWindow * parent;
    QThread workerThread;
    QString prompt;
    QTextCharFormat promptFormat;
    int inputPos;
    QSharedPointer<FakeStdout> fakeOut;

public slots:
    void handleResults(
        bool success,
        ren::Value const & result,
        ren::Value const & delta
    );
signals:
    void operate(QString const & input);
};

#endif
