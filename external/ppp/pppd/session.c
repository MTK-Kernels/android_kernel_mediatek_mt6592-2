/*
 * session.c - PPP session control.
 *
 * Copyright (c) 2007 Diego Rivera. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. The name(s) of the authors of this software must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission.
 *
 * 3. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Paul Mackerras
 *     <paulus@samba.org>".
 *
 * THE AUTHORS OF THIS SOFTWARE DISCLAIM ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Derived from auth.c, which is:
 *
 * Copyright (c) 1984-2000 Carnegie Mellon University. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any legal
 *    details, please contact
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#if !defined(__ANDROID__)
#include <crypt.h>
#endif
#ifdef HAS_SHADOW
#include <shadow.h>
#endif
#include <time.h>
#include <utmp.h>
#include <fcntl.h>
#include <unistd.h>
#include "pppd.h"
#include "session.h"

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif 

#define SET_MSG(var, msg) if (var != NULL) { var[0] = msg; }
#define COPY_STRING(s) ((s) ? strdup(s) : NULL)

#define SUCCESS_MSG "Session started successfully"
#define ABORT_MSG "Session can't be started without a username"
#define SERVICE_NAME "ppp"

#define SESSION_FAILED  0
#define SESSION_OK      1

static bool logged_in = 0;

#ifdef USE_PAM
static const char *PAM_username;
static const char *PAM_password;
static int   PAM_session = 0;
static pam_handle_t *pamh = NULL;


static int conversation (int num_msg,
#ifndef SOL2
    const
#endif
    struct pam_message **msg,
    struct pam_response **resp, void *appdata_ptr)
{
    int replies = 0;
    struct pam_response *reply = NULL;

    reply = malloc(sizeof(struct pam_response) * num_msg);
    if (!reply) return PAM_CONV_ERR;

    for (replies = 0; replies < num_msg; replies++) {
        switch (msg[replies]->msg_style) {
            case PAM_PROMPT_ECHO_ON:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_username);
                
                break;
            case PAM_PROMPT_ECHO_OFF:
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = COPY_STRING(PAM_password);
                
                break;
            case PAM_TEXT_INFO:
                
            case PAM_ERROR_MSG:
                
                reply[replies].resp_retcode = PAM_SUCCESS;
                reply[replies].resp = NULL;
                break;
            default:
                
                free (reply);
                return PAM_CONV_ERR;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}

static struct pam_conv pam_conv_data = {
    &conversation,
    NULL
};
#endif 

int
session_start(flags, user, passwd, ttyName, msg)
    const int flags;
    const char *user;
    const char *passwd;
    const char *ttyName;
    char **msg;
{
#ifdef USE_PAM
    bool ok = 1;
    const char *usr;
    int pam_error;
    bool try_session = 0;
#else 
    struct passwd *pw;
    char *cbuf;
#ifdef HAS_SHADOW
    struct spwd *spwd;
    struct spwd *getspnam();
    long now = 0;
#endif 
#endif 

    SET_MSG(msg, SUCCESS_MSG);

    
    if (!(SESS_ALL & flags)) {
        return SESSION_OK;
    }

#if defined(__ANDROID__)
    return SESSION_FAILED;
#endif

    if (user == NULL) {
       SET_MSG(msg, ABORT_MSG);
       return SESSION_FAILED;
    }

#ifdef USE_PAM
    
    
    if ((usr = strchr(user, '\\')) == NULL)
	usr = user;
    else
	usr++;

    PAM_session = 0;
    PAM_username = usr;
    PAM_password = passwd;

    dbglog("Initializing PAM (%d) for user %s", flags, usr);
    pam_error = pam_start (SERVICE_NAME, usr, &pam_conv_data, &pamh);
    dbglog("---> PAM INIT Result = %d", pam_error);
    ok = (pam_error == PAM_SUCCESS);

    if (ok) {
        ok = (pam_set_item(pamh, PAM_TTY, ttyName) == PAM_SUCCESS) &&
	    (pam_set_item(pamh, PAM_RHOST, ifname) == PAM_SUCCESS);
    }

    if (ok && (SESS_AUTH & flags)) {
        dbglog("Attempting PAM authentication");
        pam_error = pam_authenticate (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            
            dbglog("PAM Authentication OK for %s", user);
        } else {
            
            ok = 0;
            if (pam_error == PAM_USER_UNKNOWN) {
                dbglog("User unknown, failing PAM authentication");
                SET_MSG(msg, "User unknown - cannot authenticate via PAM");
            } else {
                
                dbglog("PAM Authentication failed: %d: %s", pam_error,
		       pam_strerror(pamh, pam_error));
                SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            }
        }
    }

    if (ok && (SESS_ACCT & flags)) {
        dbglog("Attempting PAM account checks");
        pam_error = pam_acct_mgmt (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            try_session = 1;
            dbglog("PAM Account OK for %s", user);
        } else {
            try_session = 0;
            if (pam_error == PAM_USER_UNKNOWN) {
                dbglog("User unknown, ignoring PAM restrictions");
                SET_MSG(msg, "User unknown - ignoring PAM restrictions");
            } else {
                
                ok = 0;
                dbglog("PAM Account checks failed: %d: %s", pam_error,
		       pam_strerror(pamh, pam_error));
                SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            }
        }
    }

    if (ok && try_session && (SESS_ACCT & flags)) {
        
        pam_error = pam_open_session (pamh, PAM_SILENT);
        if (pam_error == PAM_SUCCESS) {
            dbglog("PAM Session opened for user %s", user);
            PAM_session = 1;
        } else {
            dbglog("PAM Session denied for user %s", user);
            SET_MSG(msg, (char *) pam_strerror (pamh, pam_error));
            ok = 0;
        }
    }

    
    reopen_log();

    
    if (!ok) return SESSION_FAILED;

#elif !defined(__ANDROID__) 


    pw = NULL;
    if ((SESS_AUTH & flags)) {
	pw = getpwnam(user);

	endpwent();
	if (pw == NULL)
	    return SESSION_FAILED;

#ifdef HAS_SHADOW

	spwd = getspnam(user);
	endspent();

	if (spwd == NULL)
	    return SESSION_FAILED;

	now = time(NULL) / 86400L;
	if ((spwd->sp_expire > 0 && now >= spwd->sp_expire)
	    || ((spwd->sp_max >= 0 && spwd->sp_max < 10000)
	    && spwd->sp_lstchg >= 0
	    && now >= spwd->sp_lstchg + spwd->sp_max)) {
	    warn("Password for %s has expired", user);
	    return SESSION_FAILED;
	}

	
	pw->pw_passwd = spwd->sp_pwdp;

#endif 

        if (pw->pw_passwd == NULL || strlen(pw->pw_passwd) < 2)
            return SESSION_FAILED;
	cbuf = crypt(passwd, pw->pw_passwd);
	if (!cbuf || strcmp(cbuf, pw->pw_passwd) != 0)
            return SESSION_FAILED;
    }

#endif 


    if (SESS_ACCT & flags) {
	if (strncmp(ttyName, "/dev/", 5) == 0)
	    ttyName += 5;
	logwtmp(ttyName, user, ifname); 
	logged_in = 1;

#if defined(_PATH_LASTLOG) && !defined(USE_PAM)
	if (pw != NULL) {
            struct lastlog ll;
            int fd;
	    time_t tnow;

            if ((fd = open(_PATH_LASTLOG, O_RDWR, 0)) >= 0) {
                (void)lseek(fd, (off_t)(pw->pw_uid * sizeof(ll)), SEEK_SET);
                memset((void *)&ll, 0, sizeof(ll));
		(void)time(&tnow);
                ll.ll_time = tnow;
                (void)strncpy(ll.ll_line, ttyName, sizeof(ll.ll_line));
                (void)strncpy(ll.ll_host, ifname, sizeof(ll.ll_host));
                (void)write(fd, (char *)&ll, sizeof(ll));
                (void)close(fd);
            }
	}
#endif 
	info("user %s logged in on tty %s intf %s", user, ttyName, ifname);
    }

    return SESSION_OK;
}

void
session_end(const char* ttyName)
{
#ifdef USE_PAM
    int pam_error = PAM_SUCCESS;

    if (pamh != NULL) {
        if (PAM_session) pam_error = pam_close_session (pamh, PAM_SILENT);
        PAM_session = 0;
        pam_end (pamh, pam_error);
        pamh = NULL;
	
	reopen_log();
    }
#endif
    if (logged_in) {
	if (strncmp(ttyName, "/dev/", 5) == 0)
	    ttyName += 5;
	logwtmp(ttyName, "", ""); 
	logged_in = 0;
    }
}
