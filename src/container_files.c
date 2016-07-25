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



void update_passwd_file(char *file) {
    FILE *file_fp;
    uid_t uid = getuid();
    struct passwd *pwent = getpwuid(uid);

    message(DEBUG, "Called update_passwd_file(%s)\n", file);

    message(VERBOSE2, "Checking for passwd file: %s\n", file);
    if ( is_file(file) < 0 ) {
        message(WARNING, "Template passwd not found: %s\n", file);
        return;
    }

    message(VERBOSE, "Updating passwd file with user info\n");
    if ( ( file_fp = fopen(file, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open template passwd file %s: %s\n", file, strerror(errno));
        ABORT(255);
    }
    if (fprintf(file_fp, "\n%s:x:%d:%d:%s:%s:%s\n", pwent->pw_name, pwent->pw_uid, pwent->pw_gid, pwent->pw_gecos, pwent->pw_dir, pwent->pw_shell) < 0) {
        message(ERROR, "Could not write to template passwd file %s: %s\n",
                file, strerror(errno));
        ABORT(255);
    }
    if (fclose(file_fp) < 0) {
        message(ERROR, "Could not close template passwd file %s: %s\n",
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

    message(DEBUG, "Called update_group_file(%s)\n", file);

    message(VERBOSE2, "Checking for group file: %s\n", file);
    if ( is_file(file) < 0 ) {
        message(WARNING, "Template group file not found: %s\n", file);
        return;
    }

    message(VERBOSE, "Updating group file with user info\n");
    if ( ( file_fp = fopen(file, "a") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not open template group file %s: %s\n", file, strerror(errno));
        ABORT(255);
    }
    if (fprintf(file_fp, "\n%s:x:%d:%s\n", grent->gr_name, grent->gr_gid, pwent->pw_name) < 0) {
        message(ERROR, "Could not write template group file %s: %s\n",
                file, strerror(errno));
        ABORT(255);
    }

    message(DEBUG, "Getting supplementary group info\n");
    groupcount = getgroups(maxgroups, gids);

    for (i=0; i < groupcount; i++) {
        struct group *gr = getgrgid(gids[i]);
        message(VERBOSE3, "Found supplementary group membership in: %d\n", gids[i]);
        if ( gids[i] != gid ) {
            message(VERBOSE2, "Adding user's supplementary group ('%s') info to template group file\n", grent->gr_name);
      if (fprintf(file_fp, "%s:x:%d:%s\n", gr->gr_name, gr->gr_gid, pwent->pw_name)) {
                message(ERROR, "Could not write to %s: %s\n", file,
                        strerror(errno));
                ABORT(255);
      }
        }
    }

    /* fixme: this is failing when not root */
    fclose(file_fp);

}
