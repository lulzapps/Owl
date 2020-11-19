// Owl - www.owlclient.com
// Copyright (c) 2012-2019, Adalid Claure <aclaure@gmail.com>

#include <QDateTime>
#include "Moment.h"

namespace owl
{
    
Moment::Moment()
    : Moment(QDateTime::currentDateTime())
{
    // nothing to do
}

Moment::Moment(const QDateTime &dt)
    : _dt(dt)
{
    // nothing to do
}

const QString Moment::toString() const
{
    return toString(QDateTime::currentDateTime());
}

const QString Moment::toString(const QDateTime& now) const
{
    const auto secondsTo = _dt.secsTo(now);

    if (secondsTo < 0)
    {
        return _dt.toString("yyyy-M-d hh:mm");
    }

    if (secondsTo <= 45)
    {
        return QString("A few seconds ago");
    }
    else if (secondsTo <= 90)
    {
        return QString("A minute ago");
    }

    const auto minutesTo = (int)(secondsTo / 60);
    if (minutesTo <= 45)
    {
        return QString("%1 minutes ago").arg(minutesTo);
    }
    else if (minutesTo <= 60)
    {
        return QString("An hour ago");
    }

    const auto hoursTo = (int)(minutesTo / 60);
    if (hoursTo <= 22)
    {
        return QString("%1 hours ago").arg(hoursTo);
    }
    else if (hoursTo <= 36)
    {
        return QString("a day ago");
    }

    const auto daysTo = _dt.daysTo(now);
    if (daysTo <= 25)
    {
        return QString("%1 days ago").arg(daysTo);
    }

    return _dt.date().toString("yyyy-MM-dd");
}

int Moment::monthsTo(const QDateTime &start, const QDateTime& end) const
{
    int monthCount = 0;

    const QDate startDate = start.date();
    QDate tempDate = end.date();

    while (startDate < tempDate)
    {
        tempDate = tempDate.addMonths(-1);
        monthCount++;
    }

    return monthCount;
}

} // namespace