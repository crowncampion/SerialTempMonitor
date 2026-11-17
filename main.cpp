#include "mainwindow.h"

#include <QApplication>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // QSettings 用这两个名字决定配置文件的位置。
    QCoreApplication::setOrganizationName(QStringLiteral("HarnessDemos"));
    QCoreApplication::setApplicationName(QStringLiteral("SerialTempMonitor"));

    const QStringList arguments = app.arguments();
    const bool demoMode = arguments.contains(QStringLiteral("--demo"));
    const bool selfTest = arguments.contains(QStringLiteral("--selftest"));

    // --screenshot=<文件路径> 或 --screenshot <文件路径>：配合 --demo 保存界面截图。
    QString screenshotPath;
    const QString screenshotPrefix = QStringLiteral("--screenshot=");
    for (int i = 1; i < arguments.size(); ++i) {
        const QString &argument = arguments.at(i);
        if (argument.startsWith(screenshotPrefix)) {
            screenshotPath = argument.mid(int(screenshotPrefix.size()));
        } else if (argument == QStringLiteral("--screenshot") && i + 1 < arguments.size()) {
            screenshotPath = arguments.at(i + 1);
        }
    }

    MainWindow window;
    window.show();

    if (demoMode)
        window.startSimulation();

    if (demoMode || selfTest || !screenshotPath.isEmpty()) {
        const int delayMs = demoMode ? 5000 : 1500;
        QTimer::singleShot(delayMs, &app, [&window, &app, screenshotPath]() {
            if (!screenshotPath.isEmpty())
                window.grab().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
