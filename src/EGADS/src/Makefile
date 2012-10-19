#
include ../include/$(GEM_ARCH)
IDIR  = ../include
LDIR  = $(GEM_ROOT)/lib

OBJSP = egadsGeom.o egadsHLevel.o egadsTopo.o  egadsCopy.o egadsIO.o
OBJS  = egadsBase.o egadsMemory.o egadsAttrs.o egadsTess.o egadsTris.o \
	egadsQuads.o
FOBJS = fgadsBase.o fgadsMemory.o fgadsAttrs.o fgadsTess.o \
	fgadsGeom.o fgadsHLevel.o fgadsTopo.o

default:	$(LDIR)/$(SHLIB) $(LDIR)/libegadstatic.a $(LDIR)/libfgads.a

$(LDIR)/libegads.so:		$(OBJSP) $(OBJS)
	-rm $(LDIR)/libegads.so
	$(CCOMP) -shared -o $(LDIR)/libegads.so $(OBJSP) $(OBJS) \
		$(LIBPATH) $(LIBS)

$(LDIR)/libegads.dylib:		$(OBJSP) $(OBJS)
	-rm $(LDIR)/libegads.dylib
	$(CCOMP) -dynamiclib -o $(LDIR)/libegads.dylib $(OBJSP) \
		$(OBJS) $(LIBPATH) $(LIBS) -undefined dynamic_lookup \
		-install_name libegads.dylib -compatibility_version $(CASREV) \
		-current_version $(EGREV)

$(LDIR)/libegadstatic.a:	$(OBJSP) $(OBJS)
	-rm $(LDIR)/libegadstatic.a
	ar $(LOPTS) $(LDIR)/libegadstatic.a $(OBJS) $(OBJSP)
	$(RANLB)

$(LDIR)/libfgads.a:		$(FOBJS)
	-rm $(LDIR)/libfgads.a
	ar $(LOPTS) $(LDIR)/libfgads.a $(FOBJS)
	$(RANLB)

$(OBJS):	$(IDIR)/egadsErrors.h egadsInternals.h \
		$(IDIR)/egadsTypes.h egadsTris.h
.c.o:
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) $<

$(FOBJS):	$(IDIR)/egadsErrors.h egadsInternals.h \
		$(IDIR)/egadsTypes.h 
.c.o:
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) $<

$(OBJSP):	$(IDIR)/egadsErrors.h egadsOCC.h egadsInternals.h \
		$(IDIR)/egadsTypes.h egadsClasses.h
.cpp.o:
	$(CC) -c $(CPPOPT) $(DEFINE) $(INCS) -I$(IDIR) $<

clean:
	-rm $(FOBJS) $(OBJSP) $(OBJS)

cleanall:
	-rm $(LDIR)/libegads* $(LDIR)/libfgads.a
	-rm $(FOBJS) $(OBJSP) $(OBJS)
