#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/rebuild:	rebuild.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/rebuild rebuild.o -L$(LDIR) -legads -lm

rebuild.o:	rebuild.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) rebuild.c 

clean:
	-rm rebuild.o

cleanall:
	-rm rebuild.o $(BDIR)/rebuild
