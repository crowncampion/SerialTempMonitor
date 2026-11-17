#ifndef STATISTICS_H
#define STATISTICS_H

#include "sample.h"

// 温度统计：样本数、最小值、最大值、平均值。
class Statistics
{
public:
    void add(const Sample &sample);
    void reset();

    int count() const { return m_count; }
    bool isEmpty() const { return m_count == 0; }

    // 没有样本时返回 qQNaN()，避免和真实的 0℃ 混淆。
    double minTemperature() const { return m_count > 0 ? m_minTemperature : qQNaN(); }
    double maxTemperature() const { return m_count > 0 ? m_maxTemperature : qQNaN(); }
    double averageTemperature() const;

private:
    int m_count = 0;
    double m_minTemperature = qQNaN();
    double m_maxTemperature = qQNaN();
    double m_sumTemperature = 0.0;
};

#endif // STATISTICS_H
