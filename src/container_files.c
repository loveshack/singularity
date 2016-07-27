/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * Copyright (C) 2016  Dave Love, University of Liverpool
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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <errno.h> 
#include <string.h>
#include <grp.h>


#include "config.h"
#include "file.h"
#include "util.h"
#include "message.h"



void update_passwd_file(char *file) {
    FILE *file_fp;
    uid_t uid = getuid();
    struct passwd *pwent = getpwuid(uid);

    if (NULL == pwent) {
        message(ERROR, "Could not get account information for uid %ld: %s\n",
                (long) uid, strerror(errno));
        ABORT(255);
    }
    message(DEBUG, "Called update_passwd_file(%s)\n", file);

    message(VERBOSE2, "Checking for passwd file: %s\n", file);
    if ( is_file(file) < 0 ) {
        message(WARNING, "passwd file not found: %s\n", file);
        return;
    }

    message(VERBOSE, "Updating passwd file with user info\n");
    if ( ( file_fp = fopen(file, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open passwd file %s: %s\n", file, strerror(errno));
        ABORT(255);
    }
    if (fprintf(file_fp, "\n%s:x:%d:%d:%s:%s:%s\n", pwent->pw_name,
                pwent->pw_uid, pwent->pw_gid, pwent->pw_gecos,
                pwent->pw_dir, pwent->pw_shell) < 0) {
        message(ERROR, "Could not write to passwd file %s: %s\n",
                file, strerror(errno));
        ABORT(255);
    }
    if (fclose(file_fp) != 0) {
        message(ERROR, "Could not close passwd file %s: %s\n",
                file, strerror(errno));
        ABORT(255);
    }

}


void update_group_file(char *file) {
    FILE *file_fp;
    int groupcount;
    int i;
    int maxgroups = sysconf(_SC_NGROUPS_MAX) + 1;
    uid_t uid = getuid();
    uid_t gid = getgid();
    gid_t gids[maxgroups];
    struct passwd *pwent = getpwuid(uid);
    struct group *grent = getgrgid(gid);

    if (NULL == pwent) {
        message(ERROR, "Could not get account information for uid %ld: %s\n",
                (long) uid, strerror(errno));
        ABORT(255);
    }
    if (NULL == grent) {
        message(ERROR, "Could not get group information for gid %ld: %s\n",
                (long) gid, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Called update_group_file(%s)\n", file);

    message(VERBOSE2, "Checking for group file: %s\n", file);
    if ( is_file(file) < 0 ) {
        message(WARNING, "group file not found: %s\n", file);
        return;
    }

    message(VERBOSE, "Updating group file with user info\n");
    if ( ( file_fp = fopen(file, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open group file %s: %s\n", file, strerror(errno));
        ABORT(255);
    }
    if (fprintf(file_fp, "\n%s:x:%d:%s\n", grent->gr_name, grent->gr_gid, pwent->pw_name) < 0) {
        message(ERROR, "Could not write group file %s: %s\n",
                file, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Getting supplementary group info\n");
    if ( (groupcount = getgroups(maxgroups, gids)) < 0 ) {
        message(ERROR, "Failed to get supplementary group list: %s\n",
                strerror(errno));
        ABORT(255);
    }

    for (i=0; i < groupcount; i++) {
        struct group *gr = getgrgid(gids[i]);
        if (!gr) {
            message(ERROR, "Failed to get supplementary group list: %s\n",
                    strerror(errno));
            ABORT(255);
        }
        message(VERBOSE3, "Found supplementary group membership in: %d\n", gids[i]);
        if ( gids[i] != gid ) {
            message(VERBOSE2, "Adding user's supplementary group ('%s') info to group file\n", grent->gr_name);
            if (NULL == gr) {
                message(ERROR, "Could not get supplementary group information for gid %ld: %s\n",
                        (long)gids[i], strerror(errno));
                ABORT(255);
            }
            if (fprintf(file_fp, "%s:x:%d:%s\n", gr->gr_name, gr->gr_gid, pwent->pw_name) < 0) {
                message(ERROR, "Could not write to %s: %s\n", file,
                        strerror(errno));
                ABORT(255);
            }
        }
    }

    /* fixme: this is failing when not root */
    fclose(file_fp);

}
