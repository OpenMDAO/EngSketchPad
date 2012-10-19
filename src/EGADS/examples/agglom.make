#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/agglom:	agglom.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/agglom agglom.o -L$(LDIR) -legads -lm

agglom.o:	agglom.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) agglom.c

clean:
	-rm agglom.o

cleanall:
	-rm agglom.o $(BDIR)/agglom
