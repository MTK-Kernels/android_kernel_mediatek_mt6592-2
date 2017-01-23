
#include <syslog.h>

int main()
{
    syslog(LOG_USER|LOG_DEBUG, "valgrind/none/tests/syslog: test message");
    return 0;
}
