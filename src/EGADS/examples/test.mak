#
!include ..\include\$(GEM_ARCH).$(MSVC)
IDIR = ..\include
LDIR = $(GEM_ROOT)\lib

default:	testc.exe parsec.exe 

testc.exe:	testc.obj $(LDIR)\egads.lib
	cl testc.obj $(LIBPTH) egads.lib
	$(MCOMP) /manifest testc.exe.manifest /outputresource:testc.exe;1

testc.obj:	testc.c $(IDIR)\egads.h $(IDIR)\egadsTypes.h \
		$(IDIR)\egadsErrors.h
	cl /c $(COPTS) $(DEFINE) -I$(IDIR) testc.c

parsec.exe:	parsec.obj $(LDIR)\egads.lib
	cl parsec.obj $(LIBPTH) egads.lib
	$(MCOMP) /manifest parsec.exe.manifest /outputresource:parsec.exe;1

parsec.obj:	parsec.c $(IDIR)\egads.h $(IDIR)\egadsTypes.h \
		$(IDIR)\egadsErrors.h
	cl /c $(COPTS) $(DEFINE) -I$(IDIR) parsec.c

clean:
	-del testc.obj parsec.obj

cleanall:
	-del testc.obj parsec.obj parsec.exe testc.exe parsec.exe.manifest testc.exe.manifest 
