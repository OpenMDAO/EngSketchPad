#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/intersect:	intersect.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/intersect intersect.o -L$(LDIR) -legads -lm

intersect.o:	intersect.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) intersect.c 

clean:
	-rm intersect.o

cleanall:
	-rm intersect.o $(BDIR)/intersect
