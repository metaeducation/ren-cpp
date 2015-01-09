//
// renshell.cpp
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

#include <QMessageBox>

#include "renshell.h"

#include "rencpp/ren.hpp"


///
/// WORKER OBJECT FOR HANDLING SHELL REQUESTS
///

//
// We push this item to the worker thread and let it talk to the shell
// process while we keep monitoring the GUI for an interrupt or lock
// up the evaluator thread (because it was the evaluator that invoked
// the shell [...] call, and we don't want to return from it until
// it is finished).
//
// http://doc.qt.io/qt-5/qthread.html#details
//
// We assume the writing mutex is locked, the evaluator is frozen, and
// so writing to the console is all right.
//

class ShellWorker : public QObject
{
    Q_OBJECT

private:
    QProcess * process;

    // We need to keep a buffer at least as long as the token string we are
    // looking for as a prompt before flushing the data we get back

    QByteArray token;
    bool discardUntilFlag;
    QByteArray buffer;
    int tokenCount;

public:
    ShellWorker(QObject * parent = nullptr) :
        QObject (parent),
        process (nullptr),
        token ("***see RenGarden/renshell.cpp***"),
        tokenCount (0)
    {
    }

private:
    void initProcess();

public slots:
    void doWork(QString const & input) {

        // Initialize process if we haven't already; easier to write this
        // synchronously...

        if (process == nullptr) {
            initProcess();
            if (not process->waitForStarted(3000)) {
                assert(false);
                emit resultReady(
                    process->error()
                    /*, process->errorString() */
                );
            }

            // Set up a weird prompt string we can uniquely recognize.
            // http://www.cyberciti.biz/tips/howto-linux-unix-bash-shell-setup-prompt.html

            // This technique is not foolproof by any means.  It may make the
            // shell appear to hang.  Technically we can process the escape
            // and kill the process and recover things; unlike being able
            // to recover from the evaluator.

            // first token will be the shell start and any startup message,
            // we toss that for now

            process->write("PS1=\"");

            process->write(token);

            process->write("\"\n");

            tokenCount++;

            process->write("PS2=\"\"\n");

            tokenCount++;

            process->write("PS3=\"\"\n");

            tokenCount++;

            process->write("PS4=\"\"\n");

            tokenCount++;
        }

        // assume they didn't provide the line feed, for now.

        QByteArray buffer = input.toUtf8();
        process->write(buffer.data(), buffer.size());
        process->write("\n");

        tokenCount++;

        // Process the loop of information, easier asynchronously
    }

private slots:
    void onError(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onReadyReadStandardError();
    void onReadyReadStandardOutput();
    void onStateChanged(QProcess::ProcessState newState);


signals:
    void resultReady(int status);
};


void ShellWorker::initProcess() {
    // Thread managing slots must be the one starting the process
    process = new QProcess (this);

    // Don't know what signals or slots we'll end up needing, just connecting
    // them all for starters to have it here what they do in case it comes
    // in handy at some point.

    // Note on error() and finished():
    // "Today's lesson is: do not overload your signals and slots!"
    // (Issue slated for death in Qt6)
    //
    // http://stackoverflow.com/a/16795664/211160

    using P = QProcess;

    connect(
        process, static_cast<void (P::*)(P::ProcessError)>(&P::error),
        this, &ShellWorker::onError,
        Qt::DirectConnection
    );

    connect(
        process, static_cast<void (P::*)(int, P::ExitStatus)>(&P::finished),
        this, &ShellWorker::onFinished,
        Qt::DirectConnection
    );

    connect(
        process, &QProcess::readyReadStandardError,
        this, &ShellWorker::onReadyReadStandardError,
        Qt::DirectConnection
    );

    connect(
        process, &QProcess::readyReadStandardOutput,
        this, &ShellWorker::onReadyReadStandardOutput,
        Qt::DirectConnection
    );

    connect(
        process, &QProcess::stateChanged,
        this, &ShellWorker::onStateChanged,
        Qt::DirectConnection
    );


    // We need to detect the shell or let them override it in the dialect.
    // How will the dialect do this?  if shell [ls -alF] runs the code,
    // then how to do options?  shell/config [...] perhaps?  If that's the
    // case, how to access config if we are replacing "DO"?  A meta key
    // which knows to add /config when it's running?  Maybe shell/meta?

    QString program = "/bin/sh";
    QStringList arguments;

    // Force interactivity

    arguments << "-i";

    // Read commands from standard input (set automatically if no file
    // arguments are present, so unnecessary.

    arguments << "-s";

    // we merge standard output and standard error ATM.

    process->setProcessChannelMode(QProcess::MergedChannels);

    process->start(program, arguments);
}


void ShellWorker::onError(QProcess::ProcessError error) {
    switch (error) {
        case QProcess::FailedToStart:
            // Either the invoked program is missing, or you may have
            // insufficient permissions to invoke the program.
            break;

        case QProcess::Crashed:
            // The process crashed some time after starting successfully.
            break;

        case QProcess::Timedout:
        case QProcess::WriteError:
            // An error occurred when attempting to write to the process. For
            // example, the process may not be running, or it may have
            // closed its input channel.
            break;

        case QProcess::ReadError:
            // An error occurred when attempting to read from the process. For
            // example, the process may not be running.
            break;

        case QProcess::UnknownError:
            // This is the default return value of error().
            break;

        default:
            UNREACHABLE_CODE();
    }

    // we'd need a thing saying shell exited, and then allow
    // people to try running it again.  Presumably you get this if you exit,
    // but otherwise an error?

    assert(false);
}


void ShellWorker::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    switch (exitStatus) {
        case QProcess::NormalExit:
            ren::print("process exited", exitCode);
            break;

        case QProcess::CrashExit:
            assert(false);
            break;

        default:
            UNREACHABLE_CODE();
    }
}


void ShellWorker::onReadyReadStandardError() {
    assert(false); // sanity check...we're in merged mode, right?

    // We don't really want merged mode necessarily as stderr interferes
    // with output and if we were logging it or something that can be
    // an issue; we might want a separate window for it or a different
    // color... thoughts for future.
}


void ShellWorker::onReadyReadStandardOutput() {

    // we shouldn't be getting anything from the shell unless we are waiting
    // for at least one token... but this could change if they start background
    // processes or such.  While that's awkward, it's not ultimately any
    // more awkward than it is when you get interrupted by a background
    // message anywhere else... but handle those cases later as this is
    // starting as relatively structured for a first test.

    assert(tokenCount > 0);

    // Optimize later if and when it makes sense, circular buffer or whatever,
    // just getting it working for now.

    buffer += process->readAll();

    int index = 0;
    while ((tokenCount > 1) and (index != -1)) {
        index = buffer.indexOf(token);
        if (index != -1) {
            buffer = buffer.right(buffer.size() - (index + token.size()));
            tokenCount--;
        }
    }

    // If we're on our last token, we want to start writing everything
    // BUT the last token.

    if (tokenCount == 1) {
        // bad idea theater, just throw it into the default iostream
        // for this moment of testing...

        index = buffer.indexOf(token);
        if (index != -1) {
            assert(index + token.size() == buffer.size());
            buffer.truncate(index);
            ren::print(buffer.data());
            tokenCount--;
            buffer.clear();
        }
    }

    // we can only toss or print up to token's length - 1 bytes to hold for
    // the next check....
    int pos = buffer.size() - (token.size() - 1);
    if (pos > 0) {
        if (tokenCount == 1)
            ren::print(buffer.left(pos).data());

        buffer = buffer.right(buffer.size() - pos);
    }

    // Really we need a two phase to echo $? after we see the prompt response
    // and then grab the exit code... fancy feature :-/

    if (tokenCount == 0)
        emit resultReady(0);
}


void ShellWorker::onStateChanged(QProcess::ProcessState newState) {
    switch (newState) {
        case QProcess::NotRunning:
            // The process is not running.
            break;

        case QProcess::Starting:
            // Starting, but the program has not yet been invoked.
            break;

        case QProcess::Running:
            // The process is running and is ready for reading and writing.
            break;

        default:
            UNREACHABLE_CODE();
    }
}




///
/// SHELL MANAGER OBJECT CALLED FROM EVALUATOR WORKER THREAD
///

//
// The evaluator thread wants to be able to block and let the shell process
// run the show until it finishes.  But if it blocks deep in an evaluation
// stack, it can't (shouldn't) be pumping messages that are required to
// handle communication with the process.  We create a worker to handle it
// and RenShell acts as the interface to that worker
//

RenShell::RenShell (QObject * parent) :
    QObject (parent)
{
    // Set up the Evaluator so it's wired up for signals and slots
    // and on another thread from the GUI.  This technique is taken directly
    // from the Qt5 example, and even uses the same naming:
    //
    //   http://doc.qt.io/qt-5/qthread.html#details

    ShellWorker * worker = new ShellWorker;
    worker->moveToThread(&workerThread);
    connect(
        &workerThread, &QThread::finished,
        worker, &QObject::deleteLater,
        Qt::DirectConnection
    );
    connect(
        this, &RenShell::operate,
        worker, &ShellWorker::doWork,
        Qt::QueuedConnection
    );
    connect(
        worker, &ShellWorker::resultReady,
        this, &RenShell::handleResults,
        // technically it can't handle more results...but better perhaps
        // to crash than to block if some new invariant is introduced and
        // make it look like it's working.
        Qt::QueuedConnection
    );
    workerThread.start();

    // I keep saying it but... magic!

    dialect = ren::makeFunction(

        "{SHELL dialect for interacting with an OS shell process}"
        "'arg [word! block! paren! string!]"
        "    {block in dialect or other to execute, see documentation)}",

        REN_STD_FUNCTION,

        [this](ren::Value const & arg) -> ren::Value
        {
            shellDoneMutex.lock();

            if (arg.isString()) {
                evaluate(ren::String {arg});
            }
            else if (arg.isWord()) {
                evaluate(static_cast<QString>(arg));
            }
            else if (arg.isBlock()) {
                auto composed
                    = ren::Block {ren::runtime("compose/deep", arg)};

                evaluate(static_cast<QString>(composed));
            }
            else if (arg.isParen()) {
                evaluate(static_cast<QString>(arg.apply()));
            }
            else {
                ren::runtime("Hold your horses.");
            }

            // unlike when the GUI requested the evaluation and kept running,
            // we do NOT want to keep running while the arbitrarily long
            // shell process blocks.  And my savvy CS readership what do we
            // need, then?  Wait condition.

            shellDone.wait(&shellDoneMutex);
            int result = shellDoneResult;
            shellDoneMutex.unlock();

            if (result != 0) {
                // should we do an error here or just an integer?
                return result;
            }

            // it's annoying when sucessful shell commands print some kind
            // of evaluative result, so we transform 0 to unset

            return ren::unset;
        }
    );
}


void RenShell::handleResults(int result) {
    // while it's nice to emit the finishedEvaluation signal for any clients
    // interested in that signal, it's of no benefit to the blocked thread
    // that was doing the eval.  So signal the wait condition.

    shellDoneMutex.lock();
    shellDoneResult = result;
    shellDone.wakeAll();
    shellDoneMutex.unlock();

    emit finishedEvaluation();
}


void RenShell::evaluate(QString const & input) {
    emit operate(input);
}


RenShell::~RenShell () {
    workerThread.quit();
    if (not workerThread.wait(1000)) {
        // How to print to console about quitting
        QMessageBox::information(
            nullptr,
            "Ren Garden Terminated Abnormally",
            "A cancel request was sent to the shell but the thread it was"
            " running didn't exit in a timely manner.  This should not happen,"
            " so if you can remember what you were doing or reproduce it then"
            " please report it on the issue tracker!"
        );
        exit(1337); // REVIEW: What exit codes will Ren Garden use?
    }
}


// This bit is necessary because we're defining a Q_OBJECT class in a .cpp
// file instead of a header file (Worker)

#include "renshell.moc"
