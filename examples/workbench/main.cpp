#include <QApplication>

#include <iostream>
#include <QThread>
#include <QMessageBox>

#include "mainwindow.h"

#ifdef QT_DEBUG

// http://blog.hostilefork.com/qt-essential-noisy-debug-hook/
void noisyFailureMsgHandler(
    QtMsgType type,
    QMessageLogContext const &,
    QString const & msg
) {
    QByteArray array = msg.toLocal8Bit();

    std::cerr << array.data();
    std::cerr.flush();

    // Why didn't Qt want to make failed signal/slot connections qWarning?!
    if ((type == QtDebugMsg)
            and msg.contains("::connect")) {
        type = QtWarningMsg;
    }

    // this is another one that doesn't make sense as just a debug message.
    // It's a pretty serious sign of a problem:
    //
    // http://www.developer.nokia.com/Community/Wiki/QPainter::begin:Paint_device_returned_engine_%3D%3D_0_(Known_Issue)
    if ((type == QtDebugMsg)
            and msg.contains("QPainter::begin")
            and msg.contains("Paint device returned engine")) {
        type = QtWarningMsg;
    }

    // This qWarning about "Cowardly refusing to send clipboard message to
    // hung application..." is something that can easily happen if you are
    // debugging and the application is paused.  As it is so common, not worth
    // popping up a dialog.
    if ((type == QtWarningMsg)
            and QString(msg).contains("QClipboard::event")
            and QString(msg).contains("Cowardly refusing")) {
        type = QtDebugMsg;
    }

    // only the GUI thread should display message boxes.  If you are
    // writing a multithreaded application and the error happens on
    // a non-GUI thread, you'll have to queue the message to the GUI
    QCoreApplication * instance = QCoreApplication::instance();
    const bool isGuiThread =
        instance and (QThread::currentThread() == instance->thread());

    if (isGuiThread) {
        QMessageBox messageBox;
        switch (type) {
        case QtDebugMsg:
            return;
        case QtWarningMsg:
            messageBox.setIcon(QMessageBox::Warning);
            messageBox.setInformativeText(msg);
            messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            break;
        case QtCriticalMsg:
            messageBox.setIcon(QMessageBox::Critical);
            messageBox.setInformativeText(msg);
            messageBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            break;
        case QtFatalMsg:
            messageBox.setIcon(QMessageBox::Critical);
            messageBox.setInformativeText(msg);
            messageBox.setStandardButtons(QMessageBox::Cancel);
            break;
        }

        int ret = messageBox.exec();
        if (ret == QMessageBox::Cancel)
            abort();
    } else {
        if (type != QtDebugMsg)
            abort(); // be NOISY unless overridden!
    }
}
#endif


int main(int argc, char *argv[])
{
    // Q_INIT_RESOURCE(ren-garden);

    QApplication app(argc, argv);

#ifdef QT_DEBUG
        // Because our "noisy" message handler uses the GUI subsystem for
        // message boxes, we can't install it until after the QApplication is
        // constructed.  But it is good to be the very next thing to run, to
        // start catching warnings ASAP.
        QtMessageHandler oldMsgHandler
            = qInstallMessageHandler(noisyFailureMsgHandler);

        Q_UNUSED(oldMsgHandler); // squash "didn't use" compiler warning
#endif

    MainWindow mainWin;
    mainWin.show();
    return app.exec();
}
