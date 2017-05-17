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
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include "util/file.h"
#include "util/util.h"
#include "lib/message.h"
#include "lib/privilege.h"

void action_run_init(void) {
    return;
}

void action_run_do(int argc __attribute__((unused)), char **argv) {
    singularity_message(VERBOSE, "Exec'ing /singularity\n");

    if ( is_exec("/.run") == 0 ) {
        if ( execv("/.run", argv) < 0 ) { // Flawfinder: ignore
            singularity_message(ERROR, "Failed to execv() /.run, continuing to /bin/sh: %s\n", strerror(errno));
        }
    } else if ( is_exec("/singularity") == 0 ) {
        singularity_message(DEBUG, "Exec'ing /singularity\n");
        if ( execv("/singularity", argv) < 0 ) { // Flawfinder: ignore
            singularity_message(ERROR, "Failed to execv() /singularity: %s\n", strerror(errno));
            ABORT(255);
        }
    } else {
        singularity_message(ERROR, "No run driver found inside container\n");
        ABORT(255);
    }


    singularity_message(ERROR, "We should never get here... Grrrrrr!\n");
    ABORT(255);
}
