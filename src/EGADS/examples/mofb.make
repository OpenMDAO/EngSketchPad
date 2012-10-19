#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/mofb:	mofb.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/mofb mofb.o -L$(LDIR) -legads -lm

mofb.o:	mofb.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) mofb.c 

clean:
	-rm mofb.o

cleanall:
	-rm mofb.o $(BDIR)/mofb
