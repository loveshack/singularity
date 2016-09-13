#!/bin/bash

# Converting docker images to singularity images.

# Copyright (c) 2016  Dave Love, University of Liverpool
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:

# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# The basic technique is:
#   * Create a docker container from the image
#   * Estimate the singularity container size from that, and iterate if
#     the contents don't fit
#   * Create the singularity container
#   * docker export | singularity import
#   * Clean up
# The structure is partly obscured by error checking and recovery in verbose
# if blocks in Singularity style; also determining the size and possible
# /singularity contents is complex.
# Apart from docker, mktemp and awk are required.

# Fixme (and others below):
#  Check docker "Os" is "linux" [can it be anything else?]
#    Are there any guarantees of contents, like coreutils?  (It seems not,
#    and see fixmes around /singularity.)
#  Maybe accept a docker container instead of an image, and do a pull if
#    necessary
#  Options to set $extra and $tarfactor in import.exec (messy because
#    they're probably specific to the source, and other source types
#    should be supported)
#  Convention for singularity container metadata that could be
#    extracted from docker in this case?
#  Can it be sped up somehow?  This takes ~1 min with the minimal
#    fedora image, 45s for busybox, and ~75s with an image of ~1GB.
#  Treatment of entrypoint/cmd likely still doesn't match docker
#    properly, in particular "shell" v. "exec", but documentation/stability
#    lacking for singularity
#  Consider the locale, e.g. for use of =~, but that probably should be
#    done generally

# Avoid the set -u horror
set +u

docker_cleanup() {
    if [[ -n $id ]]; then
        message 1 "Cleaning up Docker container...\n"
        sudo docker rm -f $id >/dev/null
    fi
    rm -f "$tmp"
}

# If we've started a container, we want to remove it on exit.
trap docker_cleanup 0

# Used below for copying into image
tmp=$(mktemp)
chmod 0644 "$tmp"

if [[ -z $FILE ]]; then
    message ERROR "No Docker image specified (with --file)\n"
    exit 1
fi
dock=$FILE
sing=$1

if [[ -f $sing ]]; then
    message ERROR "$sing exists -- not over-written\n"
    exit 1
fi

if ! singularity_which docker >/dev/null; then
    message ERROR "docker command not found\n"
    exit 1
fi

# You get a pretty obscure error (with docker 1.7)
if ! docker version >/dev/null; then
    message ERROR "Docker daemon not running?\n"
    exit 1
fi

op=$(docker inspect --format='{{json .State}}' "$dock" 2>&1)
case $op in
    Error:*) message ERROR "Docker $op\n"; exit 1;;
    null) :;;
    *) message ERROR "Local Docker image required, not container\n"; exit 1;;
esac

# We need to have the default entrypoint to run df, and we'd have to
# generate /singularity differently with a non-default one.
if ! entry=$(docker inspect --format='{{json .Config.Entrypoint}}' "$dock"); then
    # Assume any error will give an obvious "not found" message
    exit 1
fi

# Create a container from the image and stash its id.  There's
# probably no advantage to generating a name for the container and
# using that.
# Give it a command to provide the root size when we start it.  Assume
# we can run df.  (There doesn't seem to be any useful information
# available with inspect on either the docker image or container;
# VirtualSize is the sum of all the layers and Size may be zero,
# however it's defined.)  If that won't work we have to fall back to
# measuring the export stream before exporting it again for real and
# guessing on the basis of that.  Note that the filesystem is
# typically significantly bigger (~20% in cases I've looked at,
# surprisingly) than the tar stream.  Also the two containers may use
# different filesystem types, which will affect their relative sizes.

message 1 "Patience...\nCreating Docker container...\n"
if ! id=$(docker create "$dock" df -k -P /); then
    message ERROR "docker create failed\n"
    exit 1
fi

# Fixme: These should come from import.exec
extra=${extra:-50}
tarfactor=${tarfactor:-1.5}

# Estimate the size of the running root.

# Add an arbitrary $extra headroom for now in case of alterations or
# as a possible fiddle factor for the filesystem, and hope $tarfactor
# is a big enough factor by which to expand the tar stream for
# singularity, else try again.  I.e. the initial attempt for the
# singularity size is estimate*tarfactor+extra.

# Starting docker with the cmd we gave it is presumably no good with
# non-default entrypoint.  (We could actually use --entrypoint, though
# it looks as if that has to be single word.)
if [[ $entry = null ]]; then
    size=$(docker start -a $id 2>/dev/null |
           # Try to verify proper df output (checking the first line) to
           # see if running df worked, and calculate the size.
           awk -v extra="$extra" 'NR==1 && !/^Filesystem/ {exit 1}
                                  NR==2 {print int($3/1024+extra)}')
fi
if [[ $? -ne 0 || $entry = null || ! $size =~ [0-9]+ ]]; then
    # We have to fall back to guessing from the tar stream.
    # Use awk as we may not have bc for the floating point calc.
    if size=$(docker export $id | wc -c); then
        size=$(awk "END {print int($tarfactor*$size/1024/1024+$extra)}" </dev/null)
    else false
    fi
    if [[ $? -ne 0 ]]; then
        message ERROR "Can't extract container size\n"
        exit 1
    fi
fi

message 1 "Creating $sing...\n"
# redirect because -q doesn't work
if ! singularity -q create -s $size "$sing" >/dev/null; then
    message ERROR "failed: singularity create -s $size $sing\n"
    exit 1
fi

## Export

message 1 "Exporting/importing...\n"
if ! docker export $id; then
    message ERROR "docker export failed\n"
    exit 1
fi |
  # If the import fails (presumably for lack of space), we expand the
  # image and try again.  Sink stdout because of tar v and sink stderr
  # in case of not-enough-space errors.
  if ! singularity import "$sing" >/dev/null 2>&1; then
      if ! singularity -q expand "$sing"; then
          message ERROR "singularity expand failed\n"
          exit 1
      elif ! docker export $id | singularity import "$sing" >/dev/null; then
          message ERROR "singularity import failed\n"
          exit 1
      fi
  fi

# Populate /singularity to reflect Docker semantics.  Quoting is a pain.
# Note that the Docker semantics for the entrypoint and cmd aren't too
# clear <https://docs.docker.com/engine/reference/builder/>.

if ! cmd=$(docker inspect --format='{{json .Config.Cmd}}' "$dock"); then
    message ERROR "docker inspect failed\n"
    exit 1
fi

if ! env=$(docker inspect --format='{{json .ContainerConfig.Env}}' "$dock"); then
    message ERROR "docker inspect failed\n"
    exit 1
fi

# Split json lists into space-separated words
if [[ $cmd != null ]]; then
    cmd=$(IFS='[],'; echo $cmd)
    cmd=${cmd:1}            # no leading blank
fi
if [[ $entry != null ]]; then
    entry=$(IFS='[],'; echo $entry)
    entry=${entry:1}
fi

if [[ $env != null ]]; then
    # Need to lose outer quotes, hence eval below
    cat <<EOF >$tmp
set -a
$(IFS='[],'; eval echo $env)
set +a
EOF
    if ! singularity -q copy "$sing" -p "$tmp" /environment; then
       message ERROR "singularity copy failed\n"
       exit 1
   fi
fi

if ! with_mount "$sing" "test -a /mnt/bin/sh"; then
    # Fixme: Ignore environment and just link to /singularity in this case.
    # Probably better, have singularity set the container's environment.
    # Alternatively, maybe copy in static busybox?
    message WARNING "No /bin/sh in image: its use will be limited\n"
elif [[ $cmd != null || $entry != null ]]; then
    # It's difficult to avoid the tmp file by piping into singularity exec.
    # Fixme: Use singularity mount, as above, to avoid it?
    message 1 "Populating /singularity...\n"
    if [[ $entry = null ]]; then
        # Since the default entrypoint is /bin/sh -c, just inline the
        # command.  Docker semantics seem to say it should be
        # overridden by the command line.
        cat <<EOF >"$tmp"
#!/bin/sh
. /environment
$cmd "\$@"
EOF
    else
        # cmd is the single argument of the entrypoint, so we need an
        # extra level of quoting.
        cmd=${cmd//\\/\\\\}     # double \
        cmd=${cmd//\"/\\\"}     # quote "
        # Fixme: Expansion of $* isn't really right
        # Fixme: Is /bin/sh guaranteed to be there?  (As above.)
        cat <<EOF >"$tmp"
#!/bin/sh
. /environment
if [ -z "\$1" ]; then
    $entry "$cmd"
else
    $entry "\$*"
fi
EOF
    fi
    chmod 0755 "$tmp"
    if ! singularity -q copy "$sing" -p "$tmp" /singularity; then
       message ERROR "singularity copy failed\n"
       exit 1
   fi
fi

# trap will clean up for us
