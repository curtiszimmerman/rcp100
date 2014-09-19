all: exec

.PHONY: cmds
cmds:
	$(MAKE) -C $@ $(MFLAGS)

.PHOONY: exec
exec: cmds
	$(MAKE) -C $@ $(MFLAGS)

DIR = cmds exec
clean:;
	for dir in $(DIR); do \
		$(MAKE) -C $$dir clean; \
	done

