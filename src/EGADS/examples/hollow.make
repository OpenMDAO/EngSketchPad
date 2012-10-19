#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/hollow:	hollow.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/hollow hollow.o -L$(LDIR) -legads -lm

hollow.o:	hollow.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) hollow.c

clean:
	-rm hollow.o

cleanall:
	-rm hollow.o $(BDIR)/hollow
