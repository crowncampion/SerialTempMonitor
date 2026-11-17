#include "framecodec.h"

#include <QLatin1String>
#include <QList>

namespace FrameCodec
{

QByteArray encode(double temperature, double humidity)
{
    QString payload = QStringLiteral("T=%1").arg(temperature, 0, 'f', 2);
    if (!std::isnan(humidity))
        payload += QStringLiteral(",H=%1").arg(humidity, 0, 'f', 2);

    const QByteArray payloadBytes = payload.toLatin1();

    QByteArray frame;
    frame.reserve(payloadBytes.size() + 8);
    frame.append("$D,");
    frame.append(payloadBytes);
    frame.append('*');
    frame.append(QByteArray::number(checksum(payloadBytes), 16)
                     .rightJustified(2, '0')
                     .toUpper());
    frame.append("\r\n");
    return frame;
}

bool decode(const QByteArray &payload, Sample &sample, QString *errorMessage)
{
    bool hasTemperature = false;

    // 载荷形如 "T=23.50,H=45.20"，按逗号拆开逐项解析。
    const QList<QByteArray> fields = payload.split(',');
    for (const QByteArray &field : fields) {
        const int equals = field.indexOf('=');
        if (equals <= 0)
            continue;

        const QByteArray key = field.left(equals).trimmed().toUpper();
        const QByteArray value = field.mid(equals + 1).trimmed();

        bool ok = false;
        const double number = value.toDouble(&ok);
        if (!ok) {
            if (errorMessage)
                *errorMessage = QStringLiteral("字段 %1 的数值无效").arg(QString::fromLatin1(value));
            return false;
        }

        if (key == "T") {
            sample.temperature = number;
            hasTemperature = true;
        } else if (key == "H") {
            sample.humidity = number;
        }
        // 其他字段暂时忽略，方便以后扩展协议。
    }

    if (!hasTemperature) {
        if (errorMessage)
            *errorMessage = QStringLiteral("帧里缺少温度字段 T");
        return false;
    }
    return true;
}

quint8 checksum(const QByteArray &payload)
{
    quint8 result = 0;
    for (char byte : payload)
        result ^= static_cast<quint8>(byte);
    return result;
}

} // namespace FrameCodec
