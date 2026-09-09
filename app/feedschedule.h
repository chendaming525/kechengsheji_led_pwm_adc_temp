#ifndef FEEDSCHEDULE_H
#define FEEDSCHEDULE_H

#include <QDateTime>
#include <QMap>
#include <QStringList>

// Daily reminders, checked against the computer's local clock.
class FeedSchedule
{
public:
    QStringList times;

    bool add(const QString &time)
    {
        if (!QTime::fromString(time, "HH:mm").isValid() || times.contains(time))
            return false;
        times.append(time);
        times.sort();
        return true;
    }

    QStringList takeDue(const QDateTime &now)
    {
        QStringList due;
        foreach (const QString &time, times) {
            QDateTime target(now.date(), QTime::fromString(time, "HH:mm"));
            // Also catch a delayed timer tick across midnight, but do not replay
            // stale reminders after a long suspension or a large clock change.
            if (target > now)
                target = target.addDays(-1);
            const bool currentMinute = target.date() == now.date()
                && time == now.time().toString("HH:mm");
            const bool crossed = lastCheck.isValid() && lastCheck < target
                && target <= now && lastCheck.secsTo(now) <= 90;
            if ((currentMinute || crossed) && fired.value(time) != target.date()) {
                fired.insert(time, target.date());
                due.append(time);
            }
        }
        lastCheck = now;
        return due;
    }

private:
    QDateTime lastCheck;
    QMap<QString, QDate> fired;
};

#endif
