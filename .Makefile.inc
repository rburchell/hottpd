#             InspIRCd Main Makefile
#
#     (C) InspIRCd Development Team, 2002-2007
# This file is automagically generated by configure, from
#    .Makefile.inc. Any changes made will go away! :P
# Please do not edit unless you know what you're doing.
#

CC = @CC@
MAKE = @MAKEPROG@
PROGS = hottpd
FLAGS = @FLAGS@
DEBUG = @DEVELOPER@
LDLIBS = @LDLIBS@
BASE = $(DESTDIR)@BASE_DIR@
CONPATH = $(DESTDIR)@CONFIG_DIR@
MODPATH = $(DESTDIR)@MODULE_DIR@
BINPATH = $(DESTDIR)@BINARY_DIR@
LAUNCHDPATH = $(DESTDIR)/System/Library/LaunchDaemons
LIBPATH = $(DESTDIR)@LIBRARY_DIR@
MODULES = @MODULES@
INSTMODE = 0755
MAKEARGS = 'PROGS=${PROGS}' 'FLAGS=${FLAGS}' 'CC=${CC}' 'LDLIBS=${LDLIBS}' 'MODULES=${MODULES}' 'MODPATH=${MODPATH}' 'LIBPATH=${LIBPATH}' 'INSTMODE=${INSTMODE}'

all: @MAKEORDER@ finishmessage

debug:
	@echo ""
	@echo "*************************************"
	@echo "*    BUILDING WITH DEBUG SYMBOLS    *"
	@echo "*                                   *"
	@echo "*  Please be aware that this build  *"
	@echo "*  will consume a very large amount *"
	@echo "*   of disk space (350mb+) and may  *"
	@echo "*  run noticably slower. Only build *"
	@echo "*    hottpd with debug symbols if   *"
	@echo "* you are developing modules or are *"
	@echo "*      experiencing problems.       *"
	@echo "*                                   *"
	@echo "*************************************"
	${MAKE} -C . 'FLAGS=${DEBUG}' all

mods:
	@echo "Making modules..."
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS)

# Make this depend on 'ircd' and 'mods', so its always output last even with -j
finishmessage: ircd mods
	@echo ""
	@echo "*************************************"
	@echo "*        BUILD COMPLETE!            *"
	@echo "*                                   *"
	@echo "*     To install hottpd, type:      *"
	@echo "*         make install              *"
	@echo "*************************************"

ircd:
	@echo "*************************************"
	@echo "*         BUILDING HOTTPD           *"
	@echo "*                                   *"
	@echo "*   This may take a *long* time.    *"
	@echo "*          Be patient. :)           *"
	@echo "*************************************"
	${MAKE} -C src DIRNAME="src" $(MAKEARGS)

install: all@EXTRA_DIR@
	@-install -d -m $(INSTMODE) $(BASE)
	@-install -d -m $(INSTMODE) $(LIBPATH)
	@-install -d -m $(INSTMODE) $(BINPATH)
	@-install -d -m $(INSTMODE) $(CONPATH)
	@-install -d -m $(INSTMODE) $(MODPATH)
	-install -m $(INSTMODE) @STARTSCRIPT@ $(@DESTINATION@) 2>/dev/null
@INSTALL_LIST@	@echo ""
	@echo "*************************************"
	@echo "*        INSTALL COMPLETE!          *"
	@echo "*                                   *"
	@echo "* It is safe to ignore any messages *"
	@echo "* related to copying of conf files. *"
	@echo "*                                   *"
	@echo "* REMEMBER TO EDIT YOUR CONFIG FILE *"
	@echo "*************************************"

module:
	${MAKE} -C src/modules DIRNAME="src/modules" $(MAKEARGS) ${name}

clean:
	@echo Cleaning...
	@rm -rvf src/*.so src/*.dylib src/*.o src/hottpd src/modules/*.so src/modules/*.o *~ src/*~ src/modules/*~ src/modules/extra/*~ src/modes/*~ src/modes/*.o src/modes/*.a src/commands/*.so src/commands/*.o src/modules/*/*.o src/modules/*/*.so src/socketengines/*.o
	@echo Completed.

modclean:
	rm -rf modules/*.so
	rm -rf src/modules/*.so
	rm -rf src/modules/*.o
	rm -rf src/modules/*/*.so
	rm -rf src/modules/*/*.o
	rm -rf lib/*.so
	rm -f bin/hottpd

deinstall:
@UNINSTALL_LIST@

squeakyclean: distclean

launchd_dir:
	@-install -d -m $(INSTMODE) ${LAUNCHDPATH}

configureclean:
	rm -f .config.cache
	rm src/modules/Makefile
	rm src/Makefile
	rm Makefile
	rm include/inspircd_config.h
	rm include/inspircd_se_config.h

distclean: modclean clean configureclean

