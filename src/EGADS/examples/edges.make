#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/edges:	edges.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/edges edges.o -L$(LDIR) -legads -lm

edges.o:	edges.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) edges.c

clean:
	-rm edges.o

cleanall:
	-rm edges.o $(BDIR)/edges
