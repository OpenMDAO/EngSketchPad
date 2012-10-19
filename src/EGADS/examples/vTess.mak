#
!include ..\include\$(GEM_ARCH).$(MSVC)
IDIR = ..\include
WDIR = ..\..\wvServer
LDIR = $(GEM_ROOT)\lib
BDIR = $(GEM_ROOT)\bin

$(BDIR)\vTess.exe:	vTess.obj $(LDIR)\egads.lib \
			$(LDIR)\wsserver.lib $(LDIR)\z.lib
	cl /Fe$(BDIR)\vTess.exe vTess.obj /link /LIBPATH:$(LDIR) \
		wsserver.lib z.lib egads.lib ws2_32.lib
	$(MCOMP) /manifest $(BDIR)\vTess.exe.manifest \
		/outputresource:$(BDIR)\vTess.exe;1

vTess.obj:	vTess.c $(IDIR)\egads.h $(IDIR)\egadsTypes.h \
			$(IDIR)\egadsErrors.h $(IDIR)\wsss.h $(IDIR)\wsserver.h
	cl /c $(COPTS) $(DEFINE) /I$(IDIR) /I$(WDIR)\win32helpers vTess.c 

clean:
        -del vTess.obj

cleanall:
	-del vTess.obj $(BDIR)\vTess.exe $(BDIR)\vTess.exe.manifest
