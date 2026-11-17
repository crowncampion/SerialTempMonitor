#include "recorder.h"

#include <QDir>
#include <QFileInfo>
#include <QStringConverter>

RecorderWorker::RecorderWorker(QObject *parent)
    : QObject(parent)
{
}

void RecorderWorker::start(const QString &filePath)
{
    if (m_active)
        stop();

    QDir().mkpath(QFileInfo(filePath).absolutePath());

    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit errorOccurred(QStringLiteral("无法创建记录文件：%1").arg(m_file.errorString()));
        return;
    }

    m_stream.setDevice(&m_file);
    m_stream.setEncoding(QStringConverter::Utf8);
    m_stream.setGenerateByteOrderMark(true); // 让 Excel 正确识别中文表头
    m_stream << QStringLiteral("时间,温度(℃),湿度(%)\n");
    m_stream.flush();

    m_pendingWrites = 0;
    m_active = true;
    emit started(filePath);
}

void RecorderWorker::append(const QDateTime &timestamp, double temperature, double humidity)
{
    if (!m_active)
        return;

    m_stream << timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) << ','
             << QString::number(temperature, 'f', 2) << ',';

    if (!std::isnan(humidity))
        m_stream << QString::number(humidity, 'f', 2);

    m_stream << '\n';

    // 每 20 条刷一次盘，兼顾实时性和写入效率。
    if (++m_pendingWrites >= 20) {
        m_stream.flush();
        m_pendingWrites = 0;

        if (m_file.error() != QFileDevice::NoError) {
            emit errorOccurred(QStringLiteral("写入记录文件失败：%1").arg(m_file.errorString()));
            stop();
        }
    }
}

void RecorderWorker::stop()
{
    if (!m_active)
        return;

    m_stream.flush();
    m_file.close();
    m_stream.setDevice(nullptr);
    m_active = false;
    emit stopped();
}

Recorder::Recorder(QObject *parent)
    : QObject(parent)
    , m_worker(new RecorderWorker)
{
    m_worker->moveToThread(&m_thread);

    connect(this, &Recorder::startRequested, m_worker, &RecorderWorker::start);
    connect(this, &Recorder::appendRequested, m_worker, &RecorderWorker::append);
    connect(this, &Recorder::stopRequested, m_worker, &RecorderWorker::stop);

    connect(m_worker, &RecorderWorker::errorOccurred, this, [this](const QString &message) {
        m_recording = false;
        emit errorOccurred(message);
        emit stateChanged(false);
    });
    connect(m_worker, &RecorderWorker::stopped, this, [this]() {
        if (m_recording) {
            m_recording = false;
            emit stateChanged(false);
        }
    });

    m_thread.start();
}

Recorder::~Recorder()
{
    if (m_thread.isRunning()) {
        // 析构时等 worker 把缓冲区刷完再退出线程。
        QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
        m_thread.quit();
        m_thread.wait(3000);
    }
    delete m_worker;
}

void Recorder::start(const QString &filePath)
{
    if (m_recording)
        return;

    m_recording = true;
    emit startRequested(filePath);
    emit stateChanged(true);
}

void Recorder::stop()
{
    if (!m_recording)
        return;

    m_recording = false;
    emit stopRequested();
    emit stateChanged(false);
}

void Recorder::record(const Sample &sample)
{
    if (!m_recording || !sample.hasTemperature())
        return;

    emit appendRequested(sample.timestamp, sample.temperature, sample.humidity);
}
