#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = $(GEM_ROOT)/bin

$(BDIR)/vGeom:	vGeom.o $(LDIR)/libwsserver.a
	$(CC) -o $(BDIR)/vGeom vGeom.o -L$(LDIR) -lwsserver -legads \
		-lpthread -lz -lm

vGeom.o:	vGeom.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
			$(IDIR)/egadsErrors.h $(IDIR)/wsserver.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) vGeom.c

clean:
	-rm vGeom.o

cleanall:
	-rm vGeom.o $(BDIR)/vGeom
