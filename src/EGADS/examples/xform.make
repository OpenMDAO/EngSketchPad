#
include ../include/$(GEM_ARCH)
IDIR = ../include
LDIR = $(GEM_ROOT)/lib
BDIR = .

$(BDIR)/xform:	xform.o $(LDIR)/$(SHLIB)
	$(CC) -o $(BDIR)/xform xform.o -L$(LDIR) -legads -lm

xform.o:	xform.c $(IDIR)/egads.h $(IDIR)/egadsTypes.h \
		$(IDIR)/egadsErrors.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -I$(IDIR) xform.c

clean:
	-rm xform.o

cleanall:
	-rm xform.o $(BDIR)/xform
