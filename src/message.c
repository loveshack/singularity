/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <limits.h>

#include "config.h"
#include "util.h"
#include "message.h"

static int messagelevel = -1;

extern const char *__progname;

void init(void) {
    char *messagelevel_string = getenv("MESSAGELEVEL"); // Flawfinder: ignore (need to get string, validation in strtol())
    char **endptr = &messagelevel_string;
    long l;

    openlog("Singularity", LOG_CONS | LOG_NDELAY, LOG_LOCAL0);

    if ( messagelevel_string == NULL ) {
        messagelevel = 1;
    } else {
        l = strtol(messagelevel_string, endptr, 10);
        if (LONG_MIN == l || LONG_MAX == l || l < 0 || l > 9
            || (*messagelevel_string != '\0' && **endptr != '\0')) {
            message(VERBOSE, "Bad MESSAGELEVEL: %s\n", messagelevel_string);
        }
        messagelevel = l;
        if ( messagelevel < 0 ) {
            messagelevel = 0;
        } else if ( messagelevel > 9 ) {
            messagelevel = 9;
        }
        message(VERBOSE, "Set messagelevel to: %d\n", messagelevel);
    }

}


void _message(int level, const char *function, const char *file, int line, char *format, ...) {
    int syslog_level = LOG_NOTICE;
    char message[512]; // Flawfinder: ignore (messages are truncated to 512 chars)
    char *prefix = "";
    va_list args;
    va_start (args, format);

    vsnprintf(message, 512, format, args); // Flawfinder: ignore (format is internally defined)

    va_end (args);

    if ( messagelevel == -1 ) {
        init();
    }

    switch (level) {
        case ABRT:
            prefix = xstrdup("ABORT");
            syslog_level = LOG_ALERT;
            break;
        case ERROR:
            prefix = xstrdup("ERROR");
            syslog_level = LOG_ERR;
            break;
        case  WARNING:
            prefix = xstrdup("WARNING");
            syslog_level = LOG_WARNING;
            break;
        case LOG:
            prefix = xstrdup("LOG");
            break;
        case DEBUG:
            prefix = xstrdup("DEBUG");
            break;
        case INFO:
            prefix = xstrdup("INFO");
            break;
        default:
            prefix = xstrdup("VERBOSE");
            break;
    }

    if ( level <= LOG ) {
        char syslog_string[540]; // Flawfinder: ignore (512 max message length + 28'ish chars for header)
        snprintf(syslog_string, sizeof syslog_string, "%s (U=%d,P=%d)> %s", __progname, geteuid(), getpid(), message); // Flawfinder: ignore

        syslog(syslog_level, "%s", syslog_string);
    }

    if ( level <= messagelevel ) {
        char header_string[80], debug_string[25], location_string[60];
        char tmp_header_string[80];

        if ( messagelevel >= DEBUG ) {
            snprintf(location_string, sizeof location_string, "%s:%d:%s()", file, line, function); // Flawfinder: ignore
            snprintf(debug_string, sizeof debug_string, "[U=%d,P=%d]", geteuid(), getpid()); // Flawfinder: ignore
            snprintf(tmp_header_string, sizeof tmp_header_string, "%-18s %s", debug_string, location_string); // Flawfinder: ignore
            snprintf(header_string, sizeof header_string, "%-7s %-62s: ", prefix, tmp_header_string); // Flawfinder: ignore
        } else {
            snprintf(header_string, sizeof header_string, "%-7s: ", prefix); // Flawfinder: ignore
        }

        if ( level == INFO && messagelevel == INFO ) {
            printf("%s", message);
        } else if ( level == INFO ) {
            printf("%s%s", header_string, message);
        } else if ( level == LOG && messagelevel <= INFO ) {
            // Don't print anything...
        } else {
            fprintf(stderr, "%s%s", header_string, message);
        }


        fflush(stdout);
        fflush(stderr);

    }

}

void singularity_abort(int retval) {
    message(ABRT, "Exiting with RETVAL=%d\n", retval);
    exit(retval);
}
