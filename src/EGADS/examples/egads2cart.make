#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = $(GEM_ROOT)/bin

$(BDIR)/egads2cart:	egads2cart.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/egads2cart egads2cart.o -L$(LDIR) -legads -lm

egads2cart.o:	egads2cart.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) egads2cart.c 

clean:
	-rm egads2cart.o

cleanall:
	-rm egads2cart.o $(BDIR)/egads2cart
