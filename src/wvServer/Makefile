#
include ../EGADS/include/$(GEM_ARCH)
LDIR  = $(GEM_ROOT)/lib

OBJS = base64-decode.o handshake.o client-handshake.o libwebsockets.o \
       extension-deflate-stream.o md5.o extension-x-google-mux.o parsers.o \
       extension.o sha-1.o server.o wv.o

server:	$(LDIR)/libwsserver.a server-test.o
	$(CCOMP) -o server server-test.o -L$(LDIR) -lwsserver -lpthread -lz -lm

server-test.o:	server.c 
	$(CCOMP) -c $(COPTS) -DLWS_NO_FORK -DSTANDALONE server.c \
		-I../EGADS/include -I. -o server-test.o

$(LDIR)/libwsserver.a:	$(OBJS)
	-rm $(LDIR)/libwsserver.a
	ar $(LOPTS) $(LDIR)/libwsserver.a $(OBJS)
	$(RANLB)

$(OBJS):	extension-deflate-stream.h libwebsockets.h \
		extension-x-google-mux.h private-libwebsockets.h \
		../EGADS/include/wsserver.h ../EGADS/include/wsss.h
.c.o:
	$(CCOMP) -c $(COPTS) -DLWS_NO_FORK -I../EGADS/include $< 

clean:
	-rm $(OBJS) server server-test.o

cleanall:
	-rm $(OBJS) server server-test.o $(LDIR)/libwsserver.a
