#ifndef SAMPLE_H
#define SAMPLE_H

#include <QDateTime>
#include <QMetaType>
#include <QtGlobal>
#include <cmath>

// 一次采集的数据：时间、温度、湿度。
// 湿度允许缺失（NaN），这样只上报温度的设备也能用。
struct Sample
{
    QDateTime timestamp;
    double temperature = qQNaN();
    double humidity = qQNaN();

    bool hasTemperature() const { return !std::isnan(temperature); }
    bool hasHumidity() const { return !std::isnan(humidity); }
};

Q_DECLARE_METATYPE(Sample)

#endif // SAMPLE_H
