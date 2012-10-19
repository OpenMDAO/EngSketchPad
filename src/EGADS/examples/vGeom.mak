#
!include ..\include\$(GEM_ARCH).$(MSVC)
IDIR = ..\include
WDIR = ..\..\wvServer
LDIR = $(GEM_ROOT)\lib
BDIR = $(GEM_ROOT)\bin

$(BDIR)\vGeom.exe:	vGeom.obj $(LDIR)\egads.lib \
			$(LDIR)\wsserver.lib $(LDIR)\z.lib
	cl /Fe$(BDIR)\vGeom.exe vGeom.obj /link /LIBPATH:$(LDIR) \
		wsserver.lib z.lib egads.lib ws2_32.lib
	$(MCOMP) /manifest $(BDIR)\vGeom.exe.manifest \
		/outputresource:$(BDIR)\vGeom.exe;1

vGeom.obj:	vGeom.c $(IDIR)\egads.h $(IDIR)\egadsTypes.h \
			$(IDIR)\egadsErrors.h $(IDIR)\wsss.h $(IDIR)\wsserver.h
	cl /c $(COPTS) $(DEFINE) /I$(IDIR) /I$(WDIR)\win32helpers vGeom.c

clean:
        -del vGeom.obj

cleanall:
	-del vGeom.obj $(BDIR)\vGeom.exe $(BDIR)\vGeom.exe.manifest
