		WebViewer (wv) WebSocket Server lib README


Most of the files in this directory have come directly from the libwebsockets
open source project. All of the files in "lib" have been copied here as well
as the 2 directories from "win32port": "win32helpers" and "zlib". The version
used here is marked as: "libwebsockets-support-chrome-20-firefox-12".

This was done because of the build difficulties experienced on all architectures
of interest. The "configure", "make", "install" procedure was replaced by
simple makefiles (or nmake files) with great success. The following additional
changes were made:

1) libwebsockets.h: the local paths were removed in some includes.
2) In win32helpers:
   a) both gettimeofday.c and websock-w32.c are moved to this level.
   b) websock-w32.h: the define used for usleep was removed.
   c) unistd.h: a define macro is included that maps usleep to Sleep.
   d) the "getopt" functions and include were removed -- no longer used.

The following files have been added and are specifically included to enhance 
"libwebsockets" for the wv server library:
	../include/wsss.h
	../include/wsserver.h
	../include/wsserver.inc
	server.c
	wv.c
	fwv.c
	test.f

The build requires the EGADS environment variables to be set. This must be
done before building any applications that require a wv-based server.

For Windows:
	cd zlib
	nmake -f NMakefile
	cd ..
	nmake -f NMakefile

For LINUX/Mac OSX
	make
