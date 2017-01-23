/***********************************************************************
*
* radattr.c
*
* A plugin which is stacked on top of radius.so.  This plugin writes
* all RADIUS attributes from the server's authentication confirmation
* into /var/run/radattr.pppN.  These attributes are available for
* consumption by /etc/ppp/ip-{up,down} scripts.
*
* Copyright (C) 2002 Roaring Penguin Software Inc.
*
* This plugin may be distributed according to the terms of the GNU
* General Public License, version 2 or (at your option) any later version.
*
***********************************************************************/

static char const RCSID[] =
"$Id: radattr.c,v 1.2 2004/10/28 00:24:40 paulus Exp $";

#include "pppd.h"
#include "radiusclient.h"
#include <stdio.h>

extern void (*radius_attributes_hook)(VALUE_PAIR *);
static void print_attributes(VALUE_PAIR *);
static void cleanup(void *opaque, int arg);

char pppd_version[] = VERSION;

void
plugin_init(void)
{
    radius_attributes_hook = print_attributes;

#if 0
    add_notifier(&link_down_notifier, cleanup, NULL);
#endif

    
    add_notifier(&exitnotify, cleanup, NULL);
    info("RADATTR plugin initialized.");
}

static void
print_attributes(VALUE_PAIR *vp)
{
    FILE *fp;
    char fname[512];
    char name[2048];
    char value[2048];
    int cnt = 0;

    slprintf(fname, sizeof(fname), "/var/run/radattr.%s", ifname);
    fp = fopen(fname, "w");
    if (!fp) {
	warn("radattr plugin: Could not open %s for writing: %m", fname);
	return;
    }

    for (; vp; vp=vp->next) {
	if (rc_avpair_tostr(vp, name, sizeof(name), value, sizeof(value)) < 0) {
	    continue;
	}
	fprintf(fp, "%s %s\n", name, value);
	cnt++;
    }
    fclose(fp);
    dbglog("RADATTR plugin wrote %d line(s) to file %s.", cnt, fname);
}

static void
cleanup(void *opaque, int arg)
{
    char fname[512];

    slprintf(fname, sizeof(fname), "/var/run/radattr.%s", ifname);
    (void) remove(fname);
    dbglog("RADATTR plugin removed file %s.", fname);
}
