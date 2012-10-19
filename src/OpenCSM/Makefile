#	Makefile for OpenCSM
#
include ../EGADS/include/$(GEM_ARCH)
IDIR  = ../EGADS/include
LDIR  = $(GEM_ROOT)/lib
BDIR  = $(GEM_ROOT)/bin

default:	$(BDIR)/serveCSM $(LDIR)/import.so $(LDIR)/naca.so \
		$(LDIR)/ellipse.so $(LDIR)/waffle.so $(LDIR)/freeform.so

$(BDIR)/serveCSM:	serveCSM.o OpenCSM.o udp.o
	$(CCOMP) -o $(BDIR)/serveCSM serveCSM.o OpenCSM.o udp.o \
		-L$(LDIR) -lwsserver -legads -lpthread -lz -lm

serveCSM.o:	serveCSM.c OpenCSM.h common.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. serveCSM.c

OpenCSM.o:	OpenCSM.c OpenCSM.h common.h
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. OpenCSM.c

udp.o:		udp.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udp.c

$(LDIR)/import.so:	udpImport.o
	-rm $(LDIR)/import.so
	$(CCOMP) $(SOFLGS) -o $(LDIR)/import.so udpImport.o -L$(LDIR) -legads

udpImport.o:	udpImport.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udpImport.c

$(LDIR)/naca.so:	udpNaca.o
	-rm $(LDIR)/naca.so
	$(CCOMP) $(SOFLGS) -o $(LDIR)/naca.so udpNaca.o -L$(LDIR) -legads

udpNaca.o:	udpNaca.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udpNaca.c

$(LDIR)/ellipse.so:	udpEllipse.o
	-rm $(LDIR)/ellipse.so
	$(CCOMP) $(SOFLGS) -o $(LDIR)/ellipse.so udpEllipse.o -L$(LDIR) -legads

udpEllipse.o:	udpEllipse.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udpEllipse.c

$(LDIR)/waffle.so:	udpWaffle.o
	-rm $(LDIR)/waffle.so
	$(CCOMP) $(SOFLGS) -o $(LDIR)/waffle.so udpWaffle.o -L$(LDIR) -legads

udpWaffle.o:	udpWaffle.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udpWaffle.c

$(LDIR)/freeform.so:	udpFreeform.o
	-rm $(LDIR)/freeform.so
	$(CCOMP) $(SOFLGS) -o $(LDIR)/freeform.so udpFreeform.o -L$(LDIR) -legads

udpFreeform.o:	udpFreeform.c
	$(CCOMP) -c $(COPTS) $(DEFINE) -DGEOM_EGADS -I$(IDIR) -I. udpFreeform.c

clean:
	-rm udpFreeform.o udpWaffle.o udpEllipse.o udpNaca.o udpImport.o
	-rm serveCSM.o OpenCSM.o udp.o

cleanall:
	-rm udpFreeform.o udpWaffle.o udpEllipse.o udpNaca.o udpImport.o \
		serveCSM.o OpenCSM.o udp.o
	-rm $(BDIR)/serveCSM $(LDIR)/import.so $(LDIR)/naca.so \
		$(LDIR)/ellipse.so $(LDIR)/waffle.so $(LDIR)/freeform.so
