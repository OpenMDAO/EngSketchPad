#
!include ..\include\$(GEM_ARCH).$(MSVC)
IDIR = ..\include
LDIR = $(GEM_ROOT)\lib
BDIR = $(GEM_ROOT)\bin

$(BDIR)\egads2cart.exe:	egads2cart.obj $(LDIR)\egads.lib
	cl /Fe$(BDIR)\egads2cart.exe egads2cart.obj $(LIBPTH) egads.lib
	$(MCOMP) /manifest $(BDIR)\egads2cart.exe.manifest \
		/outputresource:$(BDIR)\egads2cart.exe;1

egads2cart.obj:	egads2cart.c $(IDIR)\egads.h $(IDIR)\egadsTypes.h \
		$(IDIR)\egadsErrors.h
	cl /c $(COPTS) $(DEFINE) -I$(IDIR) egads2cart.c

clean:
	-del egads2cart.obj

cleanall:
	-del egads2cart.obj $(BDIR)\egads2cart.exe $(BDIR)\egads2cart.exe.manifest
