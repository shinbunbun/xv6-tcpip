#include "types.h"
#include "defs.h"
#include "date.h"

static int
isleapyear(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static int
ndays(int y, int m)
{
    int n = days[m];

    if (m == 2 && isleapyear(y)) {
        n++;
    }
    return n;
}

long
rtcdate2unixtime(struct rtcdate *r)
{
    const int epoch = 1970;
    int y, m;
    long unixtime = 0;

    for (y = epoch; y < r->year; y++) {
        unixtime += (isleapyear(y) ? 366 : 365) * 24 * 3600;
    }
    for (m = 1; m < r->month; m++) {
        unixtime += ndays(r->year, m) * 24 * 3600;
    }
    unixtime += (r->day - 1) * 24 * 3600;
    unixtime += r->hour * 3600;
    unixtime += r->minute * 60;
    unixtime += r->second;
    return unixtime;
}

struct rtcdate *
unixtime2rtcdate(long unixtime, struct rtcdate *r)
{   
    r->second = unixtime % 60;
    unixtime /= 60;
    r->minute = unixtime % 60;
    unixtime /= 60;
    r->hour = unixtime % 24;
    unixtime /= 24;
    
    int days = unixtime;
    
    int y = 1970;
    while (1) { 
        int n = isleapyear(y) ? 366 : 365;
        if (days < n) {
            break;
        }
        days -= n;
        y++;
    }
    r->year = y;
    
    int m = 1;
    while (1) { 
        int n = ndays(y, m);
        if (days < n) {
            break;
        }
        days -= n;
        m++;
    }
    r->month = m; 
    r->day = days + 1;
    return r;
}
