/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * If you have questions about your rights to use or distribute this software,
 * please contact Berkeley Lab's Innovation & Partnerships Office at
 * IPO@lbl.gov.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <errno.h> 
#include <signal.h>
#include <sched.h>
#include <string.h>
#include <fcntl.h>  
#include <grp.h>
#include <libgen.h>

#include "config.h"
#include "mounts.h"
#include "util.h"
#include "file.h"
#include "loop-control.h"


#ifndef LIBEXECDIR
#define LIBEXECDIR "undefined"
#endif
#ifndef SYSCONFDIR
#define SYSCONFDIR "/etc"
#endif
#ifndef LOCALSTATEDIR
#define LOCALSTATEDIR "/var/"
#endif

#ifndef MS_PRIVATE
#define MS_PRIVATE (1<<18)
#endif
#ifndef MS_REC
#define MS_REC 16384
#endif



pid_t namespace_fork_pid = 0;
pid_t exec_fork_pid = 0;



void sighandler(int sig) {
    signal(sig, sighandler);

    printf("Caught signal: %d\n", sig);
    fflush(stdout);

    if ( exec_fork_pid > 0 ) {
        fprintf(stderr, "Singularity is sending SIGKILL to child pid: %d\n", exec_fork_pid);

        kill(exec_fork_pid, SIGKILL);
    }
    if ( namespace_fork_pid > 0 ) {
        fprintf(stderr, "Singularity is sending SIGKILL to child pid: %d\n", namespace_fork_pid);

        kill(namespace_fork_pid, SIGKILL);
    }
}



int main(int argc, char ** argv) {
    FILE *loop_fp = 0;
    FILE *containerimage_fp;
    char *containerimage;
    char *mountpoint;
    char *loop_dev;
    char *shell;
    int retval = 0;
    uid_t uid = geteuid();

    signal(SIGINT, sighandler);
    signal(SIGKILL, sighandler);
    signal(SIGQUIT, sighandler);


    if ( uid != 0 ) {
        fprintf(stderr, "ABORT: Calling user must be root\n");
        return(1);
    }

    if ( argv[1] == NULL || argv[2] == NULL ) {
        fprintf(stderr, "USAGE: %s [singularity container image] [mount point] (shell container args)\n", argv[0]);
        return(1);
    }

    containerimage = strdup(argv[1]);
    mountpoint = strdup(argv[2]);
    shell = getenv("SHELL");

    if ( is_file(containerimage) < 0 ) {
        fprintf(stderr, "ABORT: Container image not found: %s\n", containerimage);
        return(1);
    }

    if ( is_dir(mountpoint) < 0 ) {
        fprintf(stderr, "ABORT: Mount point must be a directory: %s\n", mountpoint);
        return(1);
    }

    loop_dev = obtain_loop_dev();

    if ( !( containerimage_fp = fopen(containerimage, "r+") ) ) {
        fprintf(stderr, "ERROR: Could not open image %s: %s\n", containerimage, strerror(errno));
        return(255);
    }

    if ( !( loop_fp = fopen(loop_dev, "r+") ) ) {
        fprintf(stderr, "ERROR: Failed to open loop device %s: %s\n", loop_dev, strerror(errno));
        return(-1);
    }

    if ( associate_loop(containerimage_fp, loop_fp, 1) < 0 ) {
        fprintf(stderr, "ERROR: Could not associate %s to loop device %s\n", containerimage, loop_dev);
        return(255);
    }

    if ( shell == NULL ) {
        shell = strdup("/bin/bash");
    }

    namespace_fork_pid = fork();

    if ( namespace_fork_pid == 0 ) {

        if ( unshare(CLONE_NEWNS) < 0 ) {
            fprintf(stderr, "ABORT: Could not virtualize mount namespace: %s\n", strerror(errno));
            return(255);
        }

        if ( mount(NULL, "/", NULL, MS_PRIVATE|MS_REC, NULL) < 0 ) {
            fprintf(stderr, "ABORT: Could not make mountspaces private: %s\n", strerror(errno));
            return(255);
        }


        if ( mount_image(loop_dev, mountpoint, 1) < 0 ) {
            fprintf(stderr, "ABORT: exiting...\n");
            return(255);
        }

        exec_fork_pid = fork();

        if ( exec_fork_pid == 0 ) {

            argv[2] = strdup(shell);

            if ( execv(shell, &argv[2]) != 0 ) {
                fprintf(stderr, "ABORT: exec of bash failed: %s\n", strerror(errno));
            }

        } else if ( exec_fork_pid > 0 ) {
            int tmpstatus;

            strncpy(argv[0], "Singularity: exec", strlen(argv[0]));

            waitpid(exec_fork_pid, &tmpstatus, 0);
            retval = WEXITSTATUS(tmpstatus);

            return(retval);
        } else {
            fprintf(stderr, "ABORT: Could not exec child process: %s\n", strerror(errno));
            retval++;
        }

    } else if ( namespace_fork_pid > 0 ) {
        int tmpstatus;
        
        strncpy(argv[0], "Singularity: namespace", strlen(argv[0]));
        
        waitpid(namespace_fork_pid, &tmpstatus, 0);
        retval = WEXITSTATUS(tmpstatus);

    } else {
        fprintf(stderr, "ABORT: Could not fork management process: %s\n", strerror(errno));
        return(255);
    }

    if ( disassociate_loop(loop_fp) < 0 ) {
        fprintf(stderr, "ERROR: Failed to detach loop device: %s\n", loop_dev);
        return(255);
    }

    return(retval);
}
