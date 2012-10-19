#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/makeCone:	makeCone.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/makeCone makeCone.o -L$(LDIR) -legads -lm

makeCone.o:	makeCone.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) makeCone.c

clean:
	-rm makeCone.o

cleanall:
	-rm makeCone.o $(BDIR)/makeCone
