#ifndef FRAMEPARSER_H
#define FRAMEPARSER_H

#include <QByteArray>
#include <QObject>

#include "sample.h"

// 串口数据到达时是一次一段的，可能：
//   1. 一帧被拆成两次收到（半包）；
//   2. 一次收到好几帧（粘包）；
//   3. 中间夹着杂字节。
// FrameParser 负责把这些情况整理成一条条完整的 Sample。
class FrameParser : public QObject
{
    Q_OBJECT

public:
    explicit FrameParser(QObject *parent = nullptr);

    void feed(const QByteArray &bytes);
    void reset();

    int frameCount() const { return m_frameCount; }
    int errorCount() const { return m_errorCount; }

signals:
    void sampleReceived(const Sample &sample);
    void errorOccurred(const QString &message);

private:
    void processLine(const QByteArray &line);
    void reportError(const QString &message);

    QByteArray m_buffer;
    int m_frameCount = 0;
    int m_errorCount = 0;
};

#endif // FRAMEPARSER_H
