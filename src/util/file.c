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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h> 
#include <string.h>
#include <fcntl.h>  
#include <libgen.h>
#include <assert.h>
#include <ftw.h>
#include <time.h>

#include "config.h"
#include "util/util.h"
#include "lib/message.h"
#include "lib/privilege.h"

char *file_id(char *path) {
    struct stat filestat;
    char *ret;
    uid_t uid = singularity_priv_getuid();

    singularity_message(DEBUG, "Called file_id(%s)\n", path);

    // Stat path
    if (lstat(path, &filestat) < 0) {
        return(NULL);
    }

    ret = (char *) xmalloc(128);
    snprintf(ret, 128, "%d.%d.%lu", (int)uid, (int)filestat.st_dev, (long unsigned)filestat.st_ino); // Flawfinder: ignore

    singularity_message(VERBOSE2, "Generated file_id: %s\n", ret);

    singularity_message(DEBUG, "Returning file_id(%s) = %s\n", path, ret);
    return(ret);
}


int is_file(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISREG(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_fifo(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISFIFO(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_link(char *path) {
    struct stat filestat;

    // Stat path
    if (lstat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISLNK(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_dir(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISDIR(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_suid(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( (S_ISUID & filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_exec(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( (S_IXUSR & filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_write(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( (S_IWUSR & filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

// fixme: does this need to check the tree up to root?
int is_owner(char *path, uid_t uid, gid_t gid) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    if ( uid == filestat.st_uid && gid == filestat.st_gid ) {
        return(0);
    }

    return(-1);
}

int is_blk(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISBLK(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}


int is_chr(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISCHR(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}


int s_mkpath(char *dir, mode_t mode) {
    if (!dir) {
        return(-1);
    }

    if (strlength(dir, 2) == 1 && dir[0] == '/') {
        return(0);
    }

    if ( is_dir(dir) == 0 ) {
        // Directory already exists, stop...
        return(0);
    }

    if ( s_mkpath(dirname(xstrdup(dir)), mode) < 0 ) {
        // Return if priors failed
        return(-1);
    }

    singularity_message(DEBUG, "Creating directory: %s\n", dir);
    mode_t mask = umask(0); // Flawfinder: ignore
    int ret = mkdir(dir, mode);
    umask(mask); // Flawfinder: ignore

    if ( ret < 0 ) {
        if ( is_dir(dir) < 0 ) { // It is possible that the directory was created between above check and mkdir()
            singularity_message(DEBUG, "Opps, could not create directory %s: (%d) %s\n", dir, errno, strerror(errno));
            return(-1);
        }
    }

    return(0);
}

int _unlink(const char *fpath, const struct stat *sb __attribute__((unused)),
            int typeflag __attribute__((unused)),
            struct FTW *ftwbuf __attribute__((unused))) {
//    printf("remove(%s)\n", fpath);
    return(remove(fpath));
}

int s_rmdir(char *dir) {

    singularity_message(DEBUG, "Removing directory: %s\n", dir);
    return(nftw(dir, _unlink, 32, FTW_DEPTH|FTW_MOUNT|FTW_PHYS));
}

int copy_file(char * source, char * dest) {
    struct stat filestat;
    int c;
    FILE * fp_s;
    FILE * fp_d;

    singularity_message(DEBUG, "Called copy_file(%s, %s)\n", source, dest);

    if ( is_file(source) < 0 ) {
        singularity_message(ERROR, "Could not copy from non-existent source: %s\n", source);
        return(-1);
    }

    singularity_message(DEBUG, "Opening source file: %s\n", source);
    if ( ( fp_s = fopen(source, "r") ) == NULL ) { // Flawfinder: ignore
        singularity_message(ERROR, "Could not read %s: %s\n", source, strerror(errno));
        return(-1);
    }

    singularity_message(DEBUG, "Opening destination file: %s\n", dest);
    if ( ( fp_d = fopen(dest, "w") ) == NULL ) { // Flawfinder: ignore
        if (fclose(fp_s)) {
            singularity_message(ERROR, "Could not close %s: %s\n", dest, strerror(errno));
            ABORT(255);
        }
        singularity_message(ERROR, "Could not write %s: %s\n", dest, strerror(errno));
        return(-1);
    }

    singularity_message(DEBUG, "Calling fstat() on source file descriptor: %d\n", fileno(fp_s));
    if ( fstat(fileno(fp_s), &filestat) < 0 ) {
        singularity_message(ERROR, "Could not fstat() on %s: %s\n", source, strerror(errno));
        fclose(fp_d);
        return(-1);
    }

    singularity_message(DEBUG, "Cloning permission string of source to dest\n");
    if ( fchmod(fileno(fp_d), filestat.st_mode) < 0 ) {
        singularity_message(ERROR, "Could not set permission mode on %s: %s\n", dest, strerror(errno));
        fclose(fp_d);
        return(-1);
    }

    singularity_message(DEBUG, "Copying file data...\n");
    while ( ( c = fgetc(fp_s) ) != EOF ) { // Flawfinder: ignore (checked boundries)
        if (fputc(c, fp_d) == EOF) {
            singularity_message(ERROR, "Copying failed: %s\n", strerror(errno));
            ABORT(255);
        }
    }

    singularity_message(DEBUG, "Done copying data, closing file pointers\n");
    if (fclose(fp_s) || fclose(fp_d)) {
        singularity_message(ERROR, "Could not close file: %s\n", strerror(errno));
        ABORT(255);
    }

    singularity_message(DEBUG, "Returning copy_file(%s, %s) = 0\n", source, dest);

    return(0);
}


int fileput(char *path, char *string) {
    FILE *fd;

    singularity_message(DEBUG, "Called fileput(%s, %s)\n", path, string);
    if ( ( fd = fopen(path, "w") ) == NULL // Flawfinder: ignore
         || fprintf(fd, "%s", string) < 0 ) {
        singularity_message(ERROR, "Could not write to %s: %s\n", path, strerror(errno));
        return(-1);
    }

    if (fclose(fd)) {
        singularity_message(ERROR, "Could not close %s: %s\n", path, strerror(errno));
        ABORT(255);
    }

    return(0);
}

char *filecat(char *path) {
    char *ret;
    FILE *fd;
    int c;
    long length;
    long pos = 0;

    singularity_message(DEBUG, "Called filecat(%s)\n", path);
    
    if ( is_file(path) < 0 ) {
        singularity_message(ERROR, "Could not find %s\n", path);
        return(NULL);
    }

    if ( ( fd = fopen(path, "r") ) == NULL ) { // Flawfinder: ignore
        singularity_message(ERROR, "Could not read from %s: %s\n", path, strerror(errno));
        return(NULL);
    }


    if ( fseek(fd, 0L, SEEK_END) < 0 ) {
        singularity_message(ERROR, "Could not seek to end of file %s: %s\n", path, strerror(errno));
        fclose(fd);
        return(NULL);
    }

    length = ftell(fd);

    rewind(fd);

    ret = (char *) xmalloc(length+1);

    while ( ( c = fgetc(fd) ) != EOF ) { // Flawfinder: ignore (checked boundries)
        ret[pos] = c;
        pos++;
    }
    ret[pos] = '\0';

    if (fclose(fd) != 0) {
        singularity_message(ERROR, "Could not close file %s: %s", path, strerror(errno));
        ABORT(255);
    }

    return(ret);
}


char *basedir(char *dir) {
    char *testdir = xstrdup(dir);
    char *ret = NULL;

    singularity_message(DEBUG, "Obtaining basedir for: %s\n", dir);

    while ( strcmp(testdir, "/") != 0 ) {
        singularity_message(DEBUG, "Iterating basedir: %s\n", testdir);

        ret = xstrdup(testdir);
        testdir = dirname(xstrdup(testdir));
    }

    return(ret);
}




