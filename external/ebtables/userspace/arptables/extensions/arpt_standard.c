#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <arptables.h>

static void
help(void)
{
	printf(
"Standard v%s options:\n"
"(If target is DROP, ACCEPT, RETURN or nothing)\n", ARPTABLES_VERSION);
}

static struct option opts[] = {
	{0}
};

static void
init(struct arpt_entry_target *t)
{
}

static int
parse(int c, char **argv, int invert, unsigned int *flags,
      const struct arpt_entry *entry,
      struct arpt_entry_target **target)
{
	return 0;
}

static void final_check(unsigned int flags)
{
}

static void
save(const struct arpt_arp *ip, const struct arpt_entry_target *target)
{
}

static
struct arptables_target standard
= { NULL,
    "standard",
    ARPTABLES_VERSION,
    ARPT_ALIGN(sizeof(int)),
    ARPT_ALIGN(sizeof(int)),
    &help,
    &init,
    &parse,
    &final_check,
    NULL, 
    &save,
    opts
};

static void _init(void) __attribute__ ((constructor));
static void _init(void)
{
	register_target(&standard);
}
