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



int build_passwd(char *template, char *output) {
    FILE *output_fp;
    uid_t uid = getuid();
    struct passwd *pwent = getpwuid(uid);

    if (NULL == pwent) {
        message(ERROR, "Could not get account information for uid %ld: %s\n",
                (long) uid, strerror(errno));
        ABORT(255);
    }
    message(DEBUG, "Called build_passwd(%s, %s)\n", template, output);

    message(VERBOSE2, "Checking for template passwd file: %s\n", template);
    if ( is_file(template) < 0 ) {
        message(WARNING, "Template passwd not found: %s\n", template);
        return(-1);
    }

    message(VERBOSE2, "Copying template passwd file to sessiondir\n");
    if ( copy_file(template, output) < 0 ) {
        message(WARNING, "Could not copy %s to %s: %s\n", template, output, strerror(errno));
        return(-1);
    }

    message(VERBOSE, "Creating template passwd file and appending user data\n");
    if ( ( output_fp = fopen(output, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open template passwd file %s: %s\n", output, strerror(errno));
        ABORT(255);
    }
    if (fprintf(output_fp, "\n%s:x:%d:%d:%s:%s:%s\n", pwent->pw_name,
                pwent->pw_uid, pwent->pw_gid, pwent->pw_gecos,
                pwent->pw_dir, pwent->pw_shell) < 0) {
        message(ERROR, "Could not write to template passwd file %s: %s\n",
                output, strerror(errno));
        ABORT(255);
    }
    if (fclose(output_fp) < 0) {
        message(ERROR, "Could not close template passwd file %s: %s\n",
                output, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Returning build_passwd(%s, %s) = 0\n", template, output);

    return(0);
}


int build_group(char *template, char *output) {
    FILE *output_fp;
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

    message(DEBUG, "Called build_group(%s, %s)\n", template, output);

    message(VERBOSE2, "Checking for template group file: %s\n", template);
    if ( is_file(template) < 0 ) {
        message(WARNING, "Template group file not found: %s\n", template);
        return(-1);
    }

    message(VERBOSE2, "Copying template group file to sessiondir\n");
    if ( copy_file(template, output) < 0 ) {
        message(WARNING, "Could not copy %s to %s: %s\n", template, output, strerror(errno));
        return(-1);
    }


    message(VERBOSE, "Creating template group file and appending user data\n");
    if ( ( output_fp = fopen(output, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open template group file %s: %s\n", output, strerror(errno));
        ABORT(255);
    }
    if (fprintf(output_fp, "\n%s:x:%d:%s\n", grent->gr_name, grent->gr_gid, pwent->pw_name) < 0) {
        message(ERROR, "Could not write template group file %s: %s\n",
                output, strerror(errno));
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
            message(VERBOSE2, "Adding user's supplementary group ('%s') info to template group file\n", grent->gr_name);
            if (NULL == gr) {
                message(ERROR, "Could not get supplementary group information for gid %ld: %s\n",
                        (long)gids[i], strerror(errno));
                ABORT(255);
            }
            if (fprintf(output_fp, "%s:x:%d:%s\n", gr->gr_name, gr->gr_gid, pwent->pw_name) < 0) {
                message(ERROR, "Could not write to %s: %s\n", output,
                        strerror(errno));
                ABORT(255);
            }
        }
    }

    /* fixme: this is failing when not root */
    fclose(output_fp);

    message(DEBUG, "Returning build_group(%s, %s) = 0\n", template, output);

    return(0);
}
