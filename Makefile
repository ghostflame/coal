DIRS   = lib src
TARGET = all

INSDIR = /opt/coal
SUBDIR = bin conf logs data

all:
	@mkdir -p bin logs
	@-for d in $(DIRS); do ( cd $$d && $(MAKE) $(MFLAGS) $(TARGET) ); done

debug: TARGET = debug
debug: all

fast:  TARGET = fast
fast:  all

install:
	@mkdir $(INSDIR)
	@-for d in $(SUBDIR); do mkdir -p $(INSDIR)/$(SUBDIR); done
	@echo "Created coal install location $(INSDIR)"
	@export COAL_INSTALL_DIR=$(INSDIR)
	@export COAL_BIN_DIR=$(INSDIR)/bin
	@-for d in $(DIRS); do ( cd $$d && $(MAKE) $(MFLAGS) install ); done

uninstall:
	@echo "Warning: this may delete your coal data files!"
	@echo "Use make target 'remove' to actually remove coal."

remove:
	@rm -rf $(INSDIR)

clean:
	@-for d in $(DIRS); do ( cd $$d && $(MAKE) $(MFLAGS) clean ); done
	@rm -f logs/* core*
	@rm -rf jail/
	@mkdir jail
	@echo -n "Removing nodes... "
	@rm -rf nodes
	@echo "done."

