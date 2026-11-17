#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDateTime>
#include <QMainWindow>
#include <QTimer>
#include <QVector>

#include "frameparser.h"
#include "recorder.h"
#include "sample.h"
#include "serialmanager.h"
#include "statistics.h"
#include "virtualserialport.h"

class QCloseEvent;
class QLabel;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 供 --demo 模式使用：直接开启模拟数据。
    void startSimulation();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshPorts();
    void toggleConnection();
    void onSerialStateChanged(bool opened);
    void onSerialData(const QByteArray &data);
    void onSerialError(const QString &message);
    void onSampleReceived(const Sample &sample);
    void onParserError(const QString &message);
    void onRecorderStateChanged(bool recording);
    void onRecorderError(const QString &message);
    void togglePause();
    void setAutoFollow(bool enabled);
    void clearData();
    void clearTable();
    void updateDataFreshness();
    void exportCsv();
    void toggleRecording();
    void sendTestFrame();
    void showAbout();

private:
    void setupCombos();
    void setupPlot();
    void setupConnections();
    void loadSettings();
    void saveSettings();

    void appendToPlot(const Sample &sample);
    void jumpToLatest();
    void appendToTable(const Sample &sample);
    void updateCurrentValue(const Sample &sample);
    void updateStatistics();
    void updateAlarm(double temperature);
    void updateConnectionUi(bool opened);
    void updateStatusBar();
    void setRecordingUi(bool recording, const QString &filePath);
    SerialConfig currentSerialConfig() const;
    QString defaultFilePath() const;
    QString settingsFilePath() const;

    bool openSelectedTransport(QString *errorMessage);
    void closeTransport();
    bool isTransportOpen() const;

    Ui::MainWindow *ui;
    SerialManager m_serial;
    VirtualSerialPort m_virtualSerial;
    Transport *m_activeTransport = nullptr;
    FrameParser m_parser;
    Recorder m_recorder;
    Statistics m_statistics;

    QVector<Sample> m_history;
    quint64 m_bytesReceived = 0;
    int m_plotWindowSeconds = 60;
    bool m_plotPaused = false;
    bool m_autoFollow = true;
    bool m_loadingSettings = false;
    QString m_recordPath;

    QTimer m_staleTimer;
    QLabel *m_staleLabel = nullptr;
    QDateTime m_lastSampleTime;
};

#endif // MAINWINDOW_H
