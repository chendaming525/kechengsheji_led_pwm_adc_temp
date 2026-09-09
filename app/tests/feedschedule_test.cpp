#include "feedschedule.h"
#include <cstdio>

#define CHECK(condition) do { if (!(condition)) { std::printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

static QDateTime at(int day, int hour, int minute, int second = 0)
{
    return QDateTime(QDate(2026, 9, day), QTime(hour, minute, second));
}

int main()
{
    FeedSchedule schedule;
    CHECK(schedule.add("08:30"));
    CHECK(!schedule.add("08:30"));
    CHECK(!schedule.add("25:00"));
    CHECK(schedule.takeDue(at(9, 8, 29, 59)).isEmpty());
    CHECK(schedule.takeDue(at(9, 8, 30)).size() == 1);
    CHECK(schedule.takeDue(at(9, 8, 30, 45)).isEmpty());
    CHECK(schedule.takeDue(at(10, 8, 30)).size() == 1);
    schedule.times.removeAll("08:30");
    CHECK(schedule.takeDue(at(11, 8, 30)).isEmpty());

    FeedSchedule midnight;
    midnight.add("23:59");
    midnight.takeDue(at(9, 23, 58, 59));
    CHECK(midnight.takeDue(at(10, 0, 0, 1)).size() == 1);
    CHECK(midnight.takeDue(at(10, 0, 0, 2)).isEmpty());

    FeedSchedule delayed;
    delayed.add("08:30");
    delayed.takeDue(at(9, 8, 29, 59));
    CHECK(delayed.takeDue(at(9, 8, 31, 1)).size() == 1);

    FeedSchedule suspended;
    suspended.add("08:30");
    suspended.takeDue(at(9, 8, 0));
    CHECK(suspended.takeDue(at(9, 10, 0)).isEmpty());
    suspended.times.clear();
    CHECK(suspended.takeDue(at(10, 8, 30)).isEmpty());
    std::puts("PASS: add, duplicate, invalid time, due, once per day, delete, clear, midnight, delay, suspension");
    return 0;
}
