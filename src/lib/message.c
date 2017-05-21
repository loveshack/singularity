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


#include "config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>
#include <limits.h>
#include <errno.h>

#include "util/util.h"
#include "lib/message.h"

static int messagelevel = -1;

extern const char *__progname;

static void init(void) {
    char *messagelevel_string = getenv("MESSAGELEVEL"); // Flawfinder: ignore 
    char **endptr = &messagelevel_string;
    long l;

    openlog("Singularity", LOG_CONS | LOG_NDELAY, LOG_LOCAL0);

    if ( messagelevel_string == NULL ) {
        messagelevel = 1;
    } else {
        errno = 0;
        l = strtol(messagelevel_string, endptr, 10);
        if (LONG_MIN == l || LONG_MAX == l || errno || l < 0 || l > 9
            || (*messagelevel_string != '\0' && **endptr != '\0')) {
            singularity_message(VERBOSE, "Bad MESSAGELEVEL: %s\n", messagelevel_string);
        }
        messagelevel = l;
        if ( messagelevel < 0 ) {
            messagelevel = 0;
        } else if ( messagelevel > 9 ) {
            messagelevel = 9;
        }
        singularity_message(VERBOSE, "Set messagelevel to: %d\n", messagelevel);
    }

}


void _singularity_message(int level, const char *function, const char *file, int line, char *format, ...) {
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

    while( ( ! isalpha(file[0]) ) && ( file[0] != '\0') ) {
        file++;
    }

    switch (level) {
        case ABRT:
            prefix = "ABORT";
            syslog_level = LOG_ALERT;
            break;
        case ERROR:
            prefix = "ERROR";
            syslog_level = LOG_ERR;
            break;
        case  WARNING:
            prefix = "WARNING";
            syslog_level = LOG_WARNING;
            break;
        case LOG:
            prefix = "LOG";
            break;
        case DEBUG:
            prefix = "DEBUG";
            break;
        case INFO:
            prefix = "INFO";
            break;
        default:
            prefix = "VERBOSE";
            break;
    }

    if ( level <= LOG ) {
        syslog(syslog_level, "%s (U=%d,P=%d)> %s", __progname, geteuid(), getpid(), message);
    }

    if ( level <= messagelevel ) {
        char header_string[95];

        if ( messagelevel >= DEBUG ) {
            char debug_string[25];
            char location_string[60];
            char tmp_header_string[86];
            snprintf(location_string, 60, "%s:%d:%s()", file, line, function); // Flawfinder: ignore
            location_string[59] = '\0';
            snprintf(debug_string, 25, "[U=%d,P=%d]", geteuid(), getpid()); // Flawfinder: ignore
            debug_string[24] = '\0';
            snprintf(tmp_header_string, 86, "%-18s %s", debug_string, location_string); // Flawfinder: ignore
            tmp_header_string[85] = '\0';
            snprintf(header_string, 95, "%-7s %-62s: ", prefix, tmp_header_string); // Flawfinder: ignore
            header_string[94] = '\0';
        } else {
            snprintf(header_string, 10, "%-7s: ", prefix); // Flawfinder: ignore
            header_string[9] = '\0';
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
