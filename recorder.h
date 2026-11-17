#ifndef RECORDER_H
#define RECORDER_H

#include <QDateTime>
#include <QFile>
#include <QObject>
#include <QTextStream>
#include <QThread>

#include "sample.h"

// 真正写文件的对象，运行在独立线程里，避免频繁写盘卡住界面。
class RecorderWorker : public QObject
{
    Q_OBJECT

public:
    explicit RecorderWorker(QObject *parent = nullptr);

public slots:
    void start(const QString &filePath);
    void append(const QDateTime &timestamp, double temperature, double humidity);
    void stop();

signals:
    void started(const QString &filePath);
    void stopped();
    void errorOccurred(const QString &message);

private:
    QFile m_file;
    QTextStream m_stream;
    bool m_active = false;
    int m_pendingWrites = 0;
};

// 外面使用的门面：负责创建线程、转发数据、维护“是否正在记录”状态。
class Recorder : public QObject
{
    Q_OBJECT

public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder() override;

    bool isRecording() const { return m_recording; }
    void start(const QString &filePath);
    void stop();
    void record(const Sample &sample);

signals:
    void stateChanged(bool recording);
    void errorOccurred(const QString &message);

    // 内部信号：通过队列连接跨线程发给 worker。
    void startRequested(const QString &filePath);
    void appendRequested(const QDateTime &timestamp, double temperature, double humidity);
    void stopRequested();

private:
    QThread m_thread;
    RecorderWorker *m_worker;
    bool m_recording = false;
};

#endif // RECORDER_H
