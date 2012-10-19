#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/chamfer:	chamfer.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/chamfer chamfer.o -L$(LDIR) -legads -lm

chamfer.o:	chamfer.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) chamfer.c

clean:
	-rm chamfer.o

cleanall:
	-rm chamfer.o $(BDIR)/chamfer
