//
// main.cpp
// This file is part of Ren Garden
// Copyright (C) 2015-2018 MetÆducation
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

#include <QApplication>

#include <QThread>
#include <QMessageBox>

#include <QLibraryInfo>
#include <QTranslator>

#include "mainwindow.h"

#ifndef NDEBUG

#include <iostream>

// http://blog.hostilefork.com/qt-essential-noisy-debug-hook/
void noisyFailureMsgHandler(
    QtMsgType type,
    QMessageLogContext const &,
    QString const & msg
) {
    QByteArray bytes = msg.toLocal8Bit();

    std::cerr << bytes.data();
    std::cerr.flush();

    // Why didn't Qt want to make failed signal/slot connections qWarning?!
    // It is true that with C++11 the proper use of the method signature
    // prevents problems, but there may be a mixture of code (or even cases
    // inside of Qt itself?) which still uses the old string-based style.

    if ((type == QtDebugMsg) && msg.contains("::connect"))
        type = QtWarningMsg;


    // this is another one that doesn't make sense as just a debug message.
    // It's a pretty serious sign of a problem.  See link in blog entry.

    if (
        (type == QtDebugMsg)
        && msg.contains("QPainter::begin")
        && msg.contains("Paint device returned engine")
    ){
        type = QtWarningMsg;
    }


    // This qWarning about "Cowardly refusing to send clipboard message to
    // hung application..." is something that can easily happen if you are
    // debugging and the application is paused.  As it is so common, not worth
    // popping up a dialog.

    if (
        QString(msg).contains("Cowardly refusing to send clipboard message")
    ){
        type = QtDebugMsg;
    }


    // only the GUI thread should display message boxes.  If you are
    // writing a multithreaded application and the error happens on
    // a non-GUI thread, you'll have to queue the message to the GUI

    QCoreApplication * instance = QCoreApplication::instance();
    const bool isGuiThread = (
        (instance != nullptr)
        && (QThread::currentThread() == instance->thread())
    );

    if (isGuiThread) {
        QMessageBox box;
        switch (type) {
        case QtDebugMsg:
            return;
        case QtWarningMsg:
            box.setIcon(QMessageBox::Warning);
            box.setInformativeText(msg);
            box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            break;
        case QtCriticalMsg:
            box.setIcon(QMessageBox::Critical);
            box.setInformativeText(msg);
            box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            break;
        case QtFatalMsg:
            box.setIcon(QMessageBox::Critical);
            box.setInformativeText(msg);
            box.setStandardButtons(QMessageBox::Cancel);
            break;
    #if QT_VERSION >= QT_VERSION_CHECK(5, 5, 0)
        case QtInfoMsg:
            return;
    #endif
        }

        int ret = box.exec();
        if (ret == QMessageBox::Cancel)
            abort();
    }
    else {
        if (type != QtDebugMsg)
            abort(); // be NOISY unless overridden!
    }
}
#endif



#ifdef TO_WINDOWS

int main_core(int argc, char *argv[]);

#include <memory>
#include <vector>
#include <windows.h>
#include <shellapi.h>

//
// http://stackoverflow.com/a/9555595/211160
//


class Win32CommandLineConverter {
private:
    std::unique_ptr<char*[]> argv_;
    std::vector<std::unique_ptr<char[]>> storage_;
public:
    Win32CommandLineConverter()
    {
        LPWSTR cmd_line = GetCommandLineW();
        int argc;
        LPWSTR* w_argv = CommandLineToArgvW(cmd_line, &argc);
        argv_ = std::unique_ptr<char*[]>(new char*[argc]);
        storage_.reserve(argc);
        for(int i=0; i<argc; ++i) {
            storage_.push_back(ConvertWArg(w_argv[i]));
            argv_[i] = storage_.back().get();
        }
        LocalFree(w_argv);
    }
    int argc() const
    {
        return static_cast<int>(storage_.size());
    }
    char** argv() const
    {
        return argv_.get();
    }
    static std::unique_ptr<char[]> ConvertWArg(LPWSTR w_arg)
    {
        int size = WideCharToMultiByte(CP_UTF8, 0, w_arg, -1, nullptr, 0, nullptr, nullptr);
        std::unique_ptr<char[]> ret(new char[size]);
        WideCharToMultiByte(CP_UTF8, 0, w_arg, -1, ret.get(), size, nullptr, nullptr);
        return ret;
    }
};

int CALLBACK WinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, int /* nCmdShow */)
{
    Win32CommandLineConverter cmd_line;
    return main_core(cmd_line.argc(), cmd_line.argv());
}

#endif


#ifdef REN_GARDEN_BOXED

void ReportStep(TCHAR const * step, TCHAR const * msg = nullptr) {
    MessageBox(NULL, step, msg ? msg : L"No message", MB_OK);
}

void PreQApplicationInitForPacking() {

    // If we are distributing Ren Garden as a virtualized executable with
    // something like MoleBox or BoxedApp, the hack they use to get 32-bit
    // Windows XP (or similar) to be able to LoadLibrary inside the virtual
    // file system does not work.  That means qwindows.dll - which is not
    // part of the natural link specification, but loaded from /platforms
    // ("as determined 'somehow' where platforms is") - won't load out.

    // So we have to pull it out into the ordinary file system space where
    // it can be found by the loader.  We choose to ask Windows where it
    // wants us to put temp files, and go with that.

    TCHAR qwindowsInner[MAX_PATH];
    GetCurrentDirectory(MAX_PATH, qwindowsInner);
    wcscat(qwindowsInner, L"\\platforms\\qwindows.dll"); // no trailing "\"?

    TCHAR tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);

    // http://stackoverflow.com/a/25266269/211160

    // By running this before the QApplication structure, we override
    // defaults; it will *only* look in these paths for plugins.

    QApplication::addLibraryPath(".\\");
    QApplication::addLibraryPath(QString::fromWCharArray(tempPath));

    // Qt always looks for qwindows.dll in a directory called "platforms"
    // under the library paths.  That's hardcoded.  Crete if it doesn't exist

    TCHAR qwindowsOuter[MAX_PATH];
    GetTempPath(MAX_PATH, qwindowsOuter);
    wcscat(qwindowsOuter, L"platforms"); // this one *has* a trailing "\"?

    CreateDirectory(qwindowsOuter, NULL); // false if failure, ignore

    wcscat(qwindowsOuter, L"\\qwindows.dll");

    // CopyFile does not appear to work in Windows 8 with the virtualized
    // file system used by MoleBox (just as LoadLibrary seems to not work
    // with it either...)  Great.  Roll our own lame copy routine.

    DWORD bytesRead, bytesWritten;
    size_t const bufferSize = 4096;
    char buffer[4096];

    HANDLE inFile = CreateFile (
        qwindowsInner,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (inFile == INVALID_HANDLE_VALUE)
    {
       MessageBox(
           NULL,
           L"DLL not found in ren-garden/platforms",
           qwindowsInner,
           MB_OK
       );
       return;
    }

    HANDLE outFile = CreateFile (
        qwindowsOuter,
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (outFile == INVALID_HANDLE_VALUE)
    {
        // again quietly fail, hope it's because it was there already
        CloseHandle(inFile);
        return;
    }

    while (
        ReadFile (inFile, buffer, bufferSize, &bytesRead, NULL)
        and (bytesRead != 0)
    ) {
        if (
            not WriteFile (
                outFile,
                buffer,
                bytesRead,
                &bytesWritten,
                NULL
            )
        ) {
            CloseHandle(inFile);
            CloseHandle(outFile);
            ReportStep(L"Write failure during copy");
            return;
        }
    }

    // Copy successful...close both files.

    CloseHandle(inFile);
    CloseHandle(outFile);
}

#endif

#include "rencpp/ren.hpp"
using namespace ren;

int main_core(int argc, char *argv[])
{
    // There are various complications with the search logic for where the
    // "platforms" are found, this lets you put qwindows.dll (or qwindowsd.dll
    // if a debug build) in a subdirectroy %platforms/ of the same directory
    // where the executable lives.
    //
    // https://stackoverflow.com/a/42767666/211160

    QCoreApplication::addLibraryPath(".");

    // Sometimes you use Q_INIT_RESOURCE, don't think it's applicable ATM
    // Q_INIT_RESOURCE(ren-garden);

#ifdef REN_GARDEN_BOXED
    PreQApplicationInitForPacking();
#endif

    QApplication app(argc, argv);

    // Should we delete the file we unpacked here, or leave it?

#ifndef NDEBUG
    // Because our "noisy" message handler uses the GUI subsystem for
    // message boxes, we can't install it until after the QApplication is
    // constructed.  But it is good to be the very next thing to run, to
    // start catching warnings ASAP.

    qInstallMessageHandler(noisyFailureMsgHandler);
#endif

    // Install translator

    QTranslator qtTranslator;
    qtTranslator.load("qt_" + QLocale::system().name(),
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QTranslator rengardenTranslator;
    rengardenTranslator.load("rengarden_" + QLocale::system().name());
    app.installTranslator(&rengardenTranslator);

    MainWindow mainWin;
    // We do not call `mainWin.show()` immediately because we need to start
    // the evaluator thread, then once the evaluator has been initialized
    // on the worker (and has first dibs for thread affinity purposes) we
    // can apply the configuration settings to the GUI-thread widgets that
    // also use Ren/C++

    return app.exec();
}


//
// ISO C++ forbids you from calling the actual main() yourself, and our
// arg converter wishes to call it...so we need another function.
//
int main(int argc, char *argv[]) {
    return main_core(argc, argv);
}
