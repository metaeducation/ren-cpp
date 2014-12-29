#ifndef RENCONSOLE_H
#define RENCONSOLE_H

#include <QTextEdit>
#include <QMutex>
#include <QThread>

#include "rencpp/ren.hpp"

#include "replpad.h"

class FakeStdout;
class FakeStdoutBuffer;
class MainWindow;

class RenConsole : public ReplPad
{
    Q_OBJECT

public:
    RenConsole (QWidget * parent = nullptr);
    ~RenConsole () override;

private:
    QTextCharFormat promptFormat;
    QTextCharFormat hintFormat;
    QTextCharFormat inputFormat;
    QTextCharFormat outputFormat;
    QTextCharFormat errorFormat;

protected:
    void printBanner();
    void printPrompt() override;
    void printMultilinePrompt() override;

protected:
    friend class FakeStdoutBuffer;
    QSharedPointer<FakeStdout> fakeOut;
signals:
    void needTextAppend(QString text);
private slots:
    void onAppendText(QString const & text);
protected:
    void appendText(QString const & text) override;

protected:
    void modifyingKeyPressEvent(QKeyEvent * event) override;

private:
    bool evaluating;
    QThread workerThread;

public slots:
    void handleResults(
        bool success,
        ren::Value const & result,
        ren::Value const & delta
    );
signals:
    void operate(QString const & input); // keep terminology from Qt sample
    void finishedEvaluation();
protected:
    void evaluate(QString const & input) override;
};

#endif
