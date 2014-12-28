#ifndef RENCONSOLE_H
#define RENCONSOLE_H

#include <QTextEdit>
#include <QMutex>
#include <QThread>

#include "rencpp/ren.hpp"

class FakeStdout;
class FakeStdoutBuffer;
class MainWindow;

class RenConsole : public QTextEdit
{
    Q_OBJECT

public:
    RenConsole (MainWindow * parent);
    ~RenConsole () override;

protected:
    bool shouldFollow;
    QTextCursor endCursor() const;
    void appendText(
        QString const & text,
        QTextCharFormat const & format = QTextCharFormat {}
    );
signals:
    void needTextAppend(QString text, QTextCharFormat format);
private slots:
    void followLatestOutput();
    void dontFollowLatestOutput();
    void onAppendText(QString const & text, QTextCharFormat const & format);

protected:
    void printBanner();
    void appendNewPrompt();
    QString getCurrentInput() const;
    void clearCurrentInput();
    void containInputSelection();

protected:
    void keyPressEvent(QKeyEvent * event) override;
    void mousePressEvent(QMouseEvent * event) override;

private:
    friend class FakeStdoutBuffer;
    QMutex modifyMutex;
signals:
    void requestConsoleReset();
private slots:
    void onTextChanged();
    void onConsoleReset();

private:
    MainWindow * parent;
    QThread workerThread;
    QSharedPointer<FakeStdout> fakeOut;

private:
    bool evaluating;

public slots:
    void handleResults(
        bool success,
        ren::Value const & result,
        ren::Value const & delta
    );
signals:
    void operate(QString const & input);

private:
    bool hasUndo;
    class HistoryEntry {
    public:
        int inputPos;
        bool multiLineMode;
        int evalCursorPos;
        int endPos;
    public:
        HistoryEntry (int inputPos) :
            inputPos (inputPos),
            multiLineMode (false),
            evalCursorPos (-1),
            endPos (-1)
        {
        }
    };
    std::vector<HistoryEntry> history;
};

#endif
