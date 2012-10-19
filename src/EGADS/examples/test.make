#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

default:	$(BDIR)/testc $(BDIR)/parsec

$(BDIR)/testc:		testc.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/testc testc.o -L$(LDIR) -legads -lm

testc.o:	testc.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) $(INCS) testc.c

$(BDIR)/parsec:		parsec.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/parsec parsec.o -L$(LDIR) -legads -lm

parsec.o:	parsec.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) $(INCS) parsec.c

clean:
	-rm testc.o parsec.o

cleanall:
	-rm testc.o parsec.o $(BDIR)/testc $(BDIR)/parsec
