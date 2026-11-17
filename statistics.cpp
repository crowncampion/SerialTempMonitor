#include "statistics.h"

void Statistics::add(const Sample &sample)
{
    if (!sample.hasTemperature())
        return;

    if (m_count == 0) {
        m_minTemperature = sample.temperature;
        m_maxTemperature = sample.temperature;
    } else {
        if (sample.temperature < m_minTemperature)
            m_minTemperature = sample.temperature;
        if (sample.temperature > m_maxTemperature)
            m_maxTemperature = sample.temperature;
    }

    m_sumTemperature += sample.temperature;
    ++m_count;
}

void Statistics::reset()
{
    m_count = 0;
    m_minTemperature = qQNaN();
    m_maxTemperature = qQNaN();
    m_sumTemperature = 0.0;
}

double Statistics::averageTemperature() const
{
    return m_count > 0 ? m_sumTemperature / m_count : qQNaN();
}
