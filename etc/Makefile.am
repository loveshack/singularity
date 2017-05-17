
confdir = $(sysconfdir)/singularity/
completiondir = $(sysconfdir)/bash_completion.d/

dist_conf_DATA = default-nsswitch.conf singularity.conf init
dist_completion_DATA =  bash_completion.d/singularity

bashcompletiondir = $(BASH_COMPLETION_DIR)
dist_bashcompletion_DATA = bash_completion.d/singularity

MAINTAINERCLEANFILES = Makefile.in

# On a Debian system, for instance, we can end up with root.staff ownership
# after sudo make install
install-data-hook:
	[ $$(whoami) = root ] && chown root.root $(sysconfdir)/singularity/singularity.conf || true
