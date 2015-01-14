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

using namespace ren;

extern bool forcingQuit;


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

    std::ostream * outputStream;

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
        outputStream (nullptr),
        token ("***see RenGarden/renshell.cpp***"),
        tokenCount (0)
    {
    }

private:
    void initProcess();

public:
    bool hasProcess() { return process != nullptr; }

public slots:
    void doWork(QString const & input, std::ostream * os) {

        outputStream = os;

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
        #ifdef TO_WIN32
            process->write("PROMPT=");

            process->write(token);

            process->write("\n");

            tokenCount++;
        #else
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
        #endif
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

public:
    ~ShellWorker () {
        if (process) {
            process->write("exit\n");
            process->waitForFinished(2000);
            delete process;
        }
    }
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


    // These should be configurable with shell/meta, but going for just CMD.EXE
    // on Windows and /bin/sh on Unix for now.

    QString program;
    QStringList arguments;

#ifdef TO_WIN32
    program = "CMD.EXE";

    // We want to start the command with echo off, but the command for doing
    // that varies by language.  In English versions, it's /Q
    //
    // http://stackoverflow.com/questions/21649882/

    arguments << "/Q";
#else
    program = "/bin/sh";

    // Read commands from standard input (set automatically if no file
    // arguments are present, so unnecessary.

    arguments << "-s";

    // Run interactively (necessary for showing a prompt, which we need
    // as a signal for when a command output boundary is hit).

    arguments << "-i";
#endif

    // we merge standard output and standard error ATM.

    process->setProcessChannelMode(QProcess::MergedChannels);

    process->start(program, arguments);
}


void ShellWorker::onError(QProcess::ProcessError error) {
    switch (error) {
        case QProcess::FailedToStart:
            QMessageBox::information(
                nullptr,
                "Shell process failed to start",
                "Either the invoked program is missing, or you may have"
                " insufficient permissions to invoke the program"
            );
            break;

        case QProcess::Crashed:
            QMessageBox::information(
                nullptr,
                "Shell process crashed",
                "Either the invoked program is missing, or you may have"
                " insufficient permissions to invoke the program"
            );
            break;

        case QProcess::Timedout:
            // "The last waitFor...() function timed out. The state of QProcess is
            // unchanged, and you can try calling waitFor...() again."

            // Thanks for sending the message?  But we handle the
            // timed-outedness at the place we called the waitFor.
            break;

        case QProcess::WriteError:
            QMessageBox::information(
                nullptr,
                "Shell process write error",
                "An error occurred when attempting to write to the process. For"
                " example, the process may not be running, or it may have"
                " closed its input channel."
            );
            break;

        case QProcess::ReadError:
            QMessageBox::information(
                nullptr,
                "Shell process read error",
                "An error occurred when attempting to read from the process. For"
                " example, the process may not be running."
            );
            break;

        case QProcess::UnknownError:
            QMessageBox::information(
                nullptr,
                "Shell process sent us QProcess::UnknownError",
                "What went wrong?  Who knows.  This is everyone's favorite error"
                " condition.  Sorry...but it's not Ren Garden's fault there's"
                " nothing more to tell you.  Curse the fates and shake your fist"
                " at the screen."
            );
            break;

        default:
            UNREACHABLE_CODE();
    }

    // We don't close the shell when an error happens; it will either finish
    // itself or a /meta command for doing it.  When the /meta commands are
    // written, modify messages above to tell the user they can use them.
}


void ShellWorker::onFinished(int /*exitCode*/, QProcess::ExitStatus exitStatus)
{
    switch (exitStatus) {
        case QProcess::NormalExit:
            // This can happen if you type in SHELL [exit], for instance
            break;

        case QProcess::CrashExit:
            QMessageBox::information(
                nullptr,
                "Your shell process crashed",
                "For some reason, your shell process crashed.  The next"
                " shell dialect command you run will start a new one."
            );
            break;

        default:
            UNREACHABLE_CODE();
    }
    delete process;
    process = nullptr;
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
        index = buffer.indexOf(token);
        if (index != -1) {
            assert(index + token.size() == buffer.size());
            buffer.truncate(index);

            // The linux shell doesn't always enforce a newline, but CMD.EXE
            // does.  Powershell offers a -NoNewLine option
            //
            // http://stackoverflow.com/a/21368314/211160
            //
            // I'm a little uncomfortable about such mutations, because
            // they obscure what is actually going over the line.  But it
            // would be ugly if we didn't do it, since we put our own
            // newlines in.

        #ifdef TO_WIN32
            int count = 1;
        #else
            int count = 0;
        #endif

            while (count > 0) {
                if (buffer[index - 1] == '\n') {
                    index--;
                    buffer.truncate(index);
                }
                if (buffer[index - 1] == '\r') {
                    index--;
                    buffer.truncate(index);
                }
                count--;
            }

            *outputStream << buffer.data();

            tokenCount--;
            buffer.clear();
        }
    }

    // Our theoretical limit would be that we can only toss or print up to
    // token's length - 1 bytes to hold for the next check.  Beyond that
    // theoretical limit, we want to be able to strip the last \r\n on
    // Windows (or \n) so as not to print it, because the shell always
    // puts one on.  So hold off on dumping at least 2 more chars from
    // the buffer.

    int pos = buffer.size() - ((token.size() - 1) + 2);
    if (pos > 0) {
        if (tokenCount == 1) {
            *outputStream << buffer.left(pos).data();
        }

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
    QObject (parent),
    testMode (true)
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

    shellFunction = makeFunction(

        "{SHELL dialect for interacting with an OS shell process}"
        "'arg [unset! word! lit-word! block! paren! string!]"
        "    {block in dialect or other instruction (see documentation)}"
        "/meta {Interpret in 'meta mode' for controlling the dialect}",

        REN_STD_FUNCTION,

        [this, worker](Value const & arg, Value const & meta)
            -> Value
        {
            if (arg.isUnset()) {
                // Uses the "unset quoted" trick, same as HELP, to fake up the
                // ability to have one less arity when used at the end of an
                // evaluation.  Only sensible for interactive commands!

                runtime("console quote", shellFunction);
                return unset;
            }

            if (meta) {
                if (arg.isEqualTo<Word>("running?"))
                    return worker->hasProcess();

                // How about "kill", or maybe "on" and "off"...?

                // Self awareness for things like name, which can be detected
                // and used by the prompt.  It's the first need of the "meta"
                // protocol, so hard to say if asking for a key by word is
                // better than a properties object and then getting the name
                // out of that (because this will consume NAME from the meta
                // dialect, as written)

                if (arg.isEqualTo<LitWord>("prompt")) {
                
                    // Currently the GUI is the one asking for the prompt.
                    // Like many situations where the GUI is doing an
                    // evaluation of arbitrary user code, this is a bad
                    // idea over the long run (and eventually the evalutor
                    // should assert you never call it from the GUI, as
                    // the demo matures).  But here it isn't just a bad idea,
                    // it actually deadlocks...
                    
                #ifdef THREAD_FOR_PROMPT_SAFE
                    std::stringstream pathCapture;

                    QMutexLocker lock {&shellDoneMutex};
                    evaluate("pwd", pathCapture);
                    shellDone.wait(lock.mutex());
                    return String {pathCapture.str()};
                #endif
                
                    return String {"shell"};
                }

                // Should there be a banner?  The banner could start up the
                // process and return things.

                // Meta protocol may ask you for things you don't know about,
                // so gracefully ignore them.
                if (arg.isLitWord())
                    return none;

                if (not arg.isBlock()) {
                    runtime("do make error! {Unknown meta command}");
                    return unset;
                }

                auto blk = static_cast<Block>(arg);

                if ((*blk).isEqualTo<Word>("test")) {
                    blk++;
                    testMode = (*blk).isEqualTo<Word>("on");
                    return unset;
                };

                runtime("do make error! {Unknown meta command}");

                return unset;
            }

        #ifdef TO_WIN32
            static const bool windows = true;
        #else
            static const bool windows = false;
        #endif

            // generally easier to implement the dialect logic in Rebol code,
            // see ren-garden.reb (it's built in as part of the resource file)

            auto commands = static_cast<Block>(
                runtime(
                    "ren-garden/shell-dialect-to-strings", arg, windows
                )
            );

            if (testMode) {
                for (auto str : commands)
                    print(str);

                return unset;
            }

            std::vector<int> results;


            QMutexLocker lock {&shellDoneMutex};
            for (auto str : commands) {
                evaluate(
                    static_cast<String>(str),
                    Engine::runFinder().getOutputStream()
                );

                // unlike when the GUI requested the evaluation and kept
                // running, we do NOT want to keep running while the
                // arbitrarily long shell process blocks.

                shellDone.wait(lock.mutex());

                results.push_back(shellDoneResult);
            }

            // for now just return result of last command?  Or block of
            // result codes from the shell calls? :-/

            if (results.back() == 0) {
                // it's annoying when sucessful shell commands print some kind
                // of evaluative result on every command, so we transform 0
                // to unset

                return unset;
            }

            return results.back();
        }
    );
}


void RenShell::handleResults(int result) {
    // while it's nice to emit the finishedEvaluation signal for any clients
    // interested in that signal, it's of no benefit to the blocked thread
    // that was doing the eval.  So signal the wait condition.

    QMutexLocker lock {&shellDoneMutex};

    shellDoneResult = result;
    shellDone.wakeAll();

    emit finishedEvaluation();
}


void RenShell::evaluate(QString const & input, std::ostream & os) {
    emit operate(input, &os);
}


RenShell::~RenShell () {
    workerThread.quit();
    if ((not workerThread.wait(1000) and (not forcingQuit))) {
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
