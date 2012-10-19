#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/sweep:	sweep.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/sweep sweep.o -L$(LDIR) -legads -lm

sweep.o:	sweep.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) sweep.c

clean:
	-rm sweep.o

cleanall:
	-rm sweep.o $(BDIR)/sweep
