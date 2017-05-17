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
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>


#include "util/file.h"
#include "util/util.h"
#include "lib/message.h"
#include "lib/config_parser.h"
#include "lib/privilege.h"
#include "lib/fork.h"


static int enabled = -1;

int singularity_ns_ipc_enabled(void) {
    singularity_message(DEBUG, "Checking IPC namespace enabled: %d\n", enabled);
    return(enabled);
}

int singularity_ns_ipc_unshare(void) {

    singularity_config_rewind();
    if ( singularity_config_get_bool("allow ipc ns", 1) <= 0 ) {
        singularity_message(VERBOSE2, "Not virtualizing IPC namespace by configuration\n");
        return(0);
    }

    if ( envar_defined("SINGULARITY_UNSHARE_IPC") == FALSE ) {
        singularity_message(VERBOSE2, "Not virtualizing IPC namespace on user request\n");
        return(0);
    }

#ifdef NS_CLONE_NEWIPC
    singularity_message(DEBUG, "Using IPC namespace: CLONE_NEWIPC\n");
    singularity_priv_escalate();
    singularity_message(DEBUG, "Virtualizing IPC namespace\n");
    if ( unshare(CLONE_NEWIPC) < 0 ) {
        singularity_message(ERROR, "Could not virtualize IPC namespace: %s\n", strerror(errno));
        ABORT(255);
    }
    singularity_priv_drop();
    enabled = 0;

#else
    singularity_message(WARNING, "Skipping IPC namespace creation, support not available on host\n");
    return(0);
#endif

    return(0);
}


