#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/fillet:	fillet.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/fillet fillet.o -L$(LDIR) -legads -lm

fillet.o:	fillet.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) fillet.c

clean:
	-rm fillet.o

cleanall:
	-rm fillet.o $(BDIR)/fillet
