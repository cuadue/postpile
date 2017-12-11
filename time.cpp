#include "time.hpp"

#define NOON 48
#define MIDNIGHT (2*NOON)
#define YEAR_LENGTH 73

glm::vec2 Time::vec() const
{
    float t = 2*M_PI*(float)time_of_day / (float)MIDNIGHT;
    return glm::vec2(cos(t), sin(t));
}

void Time::advance_hour()
{
    time_of_day++;
    if (time_of_day >= MIDNIGHT) {
        time_of_day -= MIDNIGHT;

        day_of_year++;
        day_of_year %= YEAR_LENGTH;
        if (day_of_year >= YEAR_LENGTH) {
            day_of_year -= YEAR_LENGTH;
            year++;
        }
    }
}

void Time::step()
{
    filter.next(vec());
}

float Time::fractional_day() const
{
    float ret = atan2(filter.get().y, filter.get().x) / (2*M_PI);
    if (ret < 0) ret += 1;
    return ret;
}

float Time::fractional_night() const
{
    float ret = fractional_day() + 0.5;
    if (ret > 1) ret -= 1;
    return ret;
}

int Time::whole_day() const
{
    return time_of_day;
}

float Time::fractional_year() const
{
    return ((float)day_of_year + fractional_day()) / (float)YEAR_LENGTH;
}
