#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = $(GEM_ROOT)/bin

$(BDIR)/vTess:	vTess.o $(LDIR)/libwsserver.a
	$(CC) -o $(BDIR)/vTess vTess.o -L$(LDIR) -lwsserver -legads \
		-lpthread -lz -lm

vTess.o:	vTess.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
			$(IDIR)/egadsErrors.h $(IDIR)/wsserver.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) vTess.c

clean:
	-rm vTess.o

cleanall:
	-rm vTess.o $(BDIR)/vTess
