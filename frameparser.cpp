#include "frameparser.h"

#include "framecodec.h"

namespace {
// 缓冲区上限：防止设备一直不发换行把内存吃光。
const int kMaxBufferSize = 4096;
}

FrameParser::FrameParser(QObject *parent)
    : QObject(parent)
{
}

void FrameParser::feed(const QByteArray &bytes)
{
    m_buffer.append(bytes);

    // 丢掉帧头之前的杂字节。
    // 帧起始用 '$'，沿用 NMEA 0183 的习惯;
    const int firstDollar = m_buffer.indexOf('$');
    if (firstDollar < 0) {
        // 没有帧头，说明这段全是杂字节；超过上限时顺便报一次溢出。
        if (m_buffer.size() > kMaxBufferSize)
            reportError(QStringLiteral("接收缓冲区溢出，已丢弃 %1 字节").arg(m_buffer.size()));
        m_buffer.clear();
        return;
    }
    if (firstDollar > 0)
        m_buffer.remove(0, firstDollar);

    // 先处理所有已经完整的行。
    // 一次 readyRead 可能带来很多帧，不能因为总量超过上限就把它们整体丢掉；
    // 只有处理完完整帧后，剩下的“半帧”才需要考虑溢出。
    while (true) {
        const int newline = m_buffer.indexOf('\n');
        if (newline < 0)
            break;

        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);

        line = line.trimmed();
        if (!line.isEmpty())
            processLine(line);
    }

    // 剩下的半帧仍然超过上限，说明设备一直没发换行，再留着只会无限增长。
    if (m_buffer.size() > kMaxBufferSize) {
        reportError(QStringLiteral("接收缓冲区溢出，已丢弃 %1 字节").arg(m_buffer.size()));
        m_buffer.clear();
    }
}

void FrameParser::reset()
{
    m_buffer.clear();
    m_frameCount = 0;
    m_errorCount = 0;
}

void FrameParser::processLine(const QByteArray &line)
{
    if (!line.startsWith("$D,")) {
        reportError(QStringLiteral("未知帧头：%1").arg(QString::fromLatin1(line.left(16))));
        return;
    }

    const int star = line.lastIndexOf('*');
    if (star < 3) {
        reportError(QStringLiteral("帧缺少校验符 *"));
        return;
    }

    const QByteArray payload = line.mid(3, star - 3);
    const QByteArray checksumText = line.mid(star + 1).trimmed();
    if (checksumText.size() != 2) {
        reportError(QStringLiteral("校验值长度不正确"));
        return;
    }

    bool ok = false;
    const uint expected = checksumText.toUInt(&ok, 16);
    if (!ok) {
        reportError(QStringLiteral("校验值不是十六进制"));
        return;
    }

    if (expected != FrameCodec::checksum(payload)) {
        reportError(QStringLiteral("校验失败，帧已丢弃"));
        return;
    }

    Sample sample;
    QString error;
    if (!FrameCodec::decode(payload, sample, &error)) {
        reportError(error);
        return;
    }

    sample.timestamp = QDateTime::currentDateTime();
    ++m_frameCount;
    emit sampleReceived(sample);
}

void FrameParser::reportError(const QString &message)
{
    ++m_errorCount;
    emit errorOccurred(message);
}
