#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/extrot:	extrot.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/extrot extrot.o -L$(LDIR) -legads -lm

extrot.o:	extrot.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) extrot.c

clean:
	-rm extrot.o

cleanall:
	-rm extrot.o $(BDIR)/extrot
