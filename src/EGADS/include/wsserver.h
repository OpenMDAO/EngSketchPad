#ifndef WSSERVER_H
#define WSSERVER_H
/*
 *	The Web Viewer
 *
 *		WebViewer WebSocket Server Prototypes
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "wsss.h"

#ifdef __ProtoExt__
#undef __ProtoExt__
#endif
#ifdef __cplusplus
extern "C" {
#define __ProtoExt__
#else
#define __ProtoExt__ extern
#endif

__ProtoExt__ int  wv_startServer( int port, /*@null@*/ char *interface, 
                                  /*@null@*/ char *cert_path, 
                                  /*@null@*/ char *key_path, int opts, 
                                  /*@only@*/ wvContext *WVcontext );
                                  
__ProtoExt__ int  wv_statusServer( int index );

__ProtoExt__ /*@null@*/ wvContext *
                  wv_createContext( int bias,   float fov,  float zNear, 
                                    float zFar, float *eye, float *center, 
                                    float *up );

__ProtoExt__ void wv_printGPrim( wvContext *cntxt, int index );

__ProtoExt__ int  wv_indexGPrim( wvContext *cntxt, char *name );

__ProtoExt__ int  wv_addGPrim( wvContext *cntxt, char *name, int gtype, 
                               int attrs, int nItems, wvData *items );

__ProtoExt__ void wv_removeGPrim( wvContext *cntxt, int index );
                               
__ProtoExt__ void wv_removeAll( wvContext *cntxt );
                               
__ProtoExt__ int  wv_modGPrim( wvContext *cntxt, int index, int nItems, 
                               wvData *items );
                               
__ProtoExt__ int  wv_addArrowHeads( wvContext *cntxt, int index, float size, 
                                    int nHeads, int *heads );

__ProtoExt__ int  wv_setData( int type, int len, void *data, int VBOtype, 
                              wvData *dstruct );

#ifndef STANDALONE
__ProtoExt__ void wv_sendText( void *wsi, char *text );
#endif

__ProtoExt__ void wv_broadcastText( char *text );
                              
__ProtoExt__ void wv_adjustVerts( wvData *dstruct, float *focus );

__ProtoExt__ void wv_cleanupServers( void );

#ifdef __cplusplus
}
#endif

#endif
