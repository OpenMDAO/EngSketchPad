/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Display the EGADS Geometry using wv (the WebViewer)
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <math.h>
#include <string.h>
#include <unistd.h>		// usleep

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

#include "egads.h"
#include "wsserver.h"


/* structure to hold on to the EGADS discretization per Body */
typedef struct {
  ego *faces;
  ego *faceTess;
  ego *edges;
  ego *edgeTess;
  ego body;
  int mtype;
  int nfaces;
  int nedges;
} bodyData;


  
int main(int argc, char *argv[])
{
  int       i, j, k, m, ibody, stat, oclass, mtype, len, ntri, sum, nbody;
  int       nloops, nseg, *segs, *senses, sizes[2], *tris, per, nnodes;
  float     color[3], focus[4];
  double    box[6], tlimits[4], size, *xyzs;
  char      gpname[33], *startapp;
  ego       context, model, geom, *bodies, *dum, *loops, *nodes;
  wvData    items[5];
  wvContext *cntxt;
  bodyData  *bodydata;
  float     eye[3]    = {0.0, 0.0, 7.0};
  float     center[3] = {0.0, 0.0, 0.0};
  float     up[3]     = {0.0, 1.0, 0.0};

  /* get our starting application line 
   * 
   * for example on a Mac:
   * setenv wvStart "open -a /Applications/Firefox.app ../client/wv.html"
   */
  startapp = getenv("wvStart");
  
  if (argc != 2) {
    printf("\n Usage: vGeom filename\n\n");
    return 1;
  }

  /* look at EGADS revision */
  EG_revision(&i, &j);
  printf("\n Using EGADS %2d.%02d\n\n", i, j);

  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel      = %d\n", EG_loadModel(context, 0, argv[1], &model));
  printf(" EG_getBoundingBox = %d\n", EG_getBoundingBox(model, box));
  printf("       BoundingBox = %lf %lf %lf\n", box[0], box[1], box[2]);
  printf("                     %lf %lf %lf\n", box[3], box[4], box[5]);  
  printf(" \n");
  
                            size = box[3]-box[0];
  if (size < box[4]-box[1]) size = box[4]-box[1];
  if (size < box[5]-box[2]) size = box[5]-box[2];

  focus[0] = 0.5*(box[0]+box[3]);
  focus[1] = 0.5*(box[1]+box[4]);
  focus[2] = 0.5*(box[2]+box[5]);
  focus[3] = size;
  
  /* get all bodies */
  stat = EG_getTopology(model, &geom, &oclass, &mtype, NULL, &nbody,
                        &bodies, &senses);
  if (stat != EGADS_SUCCESS) {
    printf(" EG_getTopology = %d\n", stat);
    return 1;
  }
  printf(" EG_getTopology:     nBodies = %d\n", nbody);
  bodydata = (bodyData *) malloc(nbody*sizeof(bodyData));
  if (bodydata == NULL) {
    printf(" MALLOC Error on Body storage!\n");
    return 1;
  }
  
  /* fill our structure a body at at time */
  for (ibody = 0; ibody < nbody; ibody++) {
  
    mtype = 0;
    EG_getTopology(bodies[ibody], &geom, &oclass, 
                   &mtype, NULL, &j, &dum, &senses);
    bodydata[ibody].body  = bodies[ibody];
    bodydata[ibody].mtype = mtype;
    if (mtype == WIREBODY) {
      printf(" Body %d: Type = WireBody\n", ibody+1);
    } else if (mtype == FACEBODY) {
      printf(" Body %d: Type = FaceBody\n", ibody+1);
    } else if (mtype == SHEETBODY) {
      printf(" Body %d: Type = SheetBody\n", ibody+1);
    } else {
      printf(" Body %d: Type = SolidBody\n", ibody+1);
    }

    stat = EG_getBodyTopos(bodies[ibody], NULL, FACE, 
                           &bodydata[ibody].nfaces, &bodydata[ibody].faces);
    i    = EG_getBodyTopos(bodies[ibody], NULL, EDGE, 
                           &bodydata[ibody].nedges, &bodydata[ibody].edges);
    if ((stat != EGADS_SUCCESS) || (i != EGADS_SUCCESS)) {
      printf(" EG_getBodyTopos Face = %d\n", stat);
      printf(" EG_getBodyTopos Edge = %d\n", i);
      free(bodydata);
      return 1;
    }
    
    printf(" EG_getBodyTopos:    %d nFaces  = %d\n", 
           ibody+1, bodydata[ibody].nfaces);
    printf(" EG_getBodyTopos:    %d nEdges  = %d\n", 
           ibody+1, bodydata[ibody].nedges);
    bodydata[ibody].faceTess = (ego *) 
                        malloc(bodydata[ibody].nfaces*sizeof(ego));
    bodydata[ibody].edgeTess = (ego *) 
                        malloc(bodydata[ibody].nedges*sizeof(ego));
    if ((bodydata[ibody].faceTess == NULL) || 
        (bodydata[ibody].edgeTess == NULL)) {
      printf(" CAN NOT Allocate Face Tessellation Objects!\n");
      free(bodydata);
      return 1;
    }
    
    for (i = 0; i < bodydata[ibody].nfaces; i++) {
      bodydata[ibody].faceTess[i] = NULL;
      stat = EG_getTopology(bodydata[ibody].faces[i], &geom, &oclass, 
                            &mtype, NULL, &nloops, &loops, &senses);
      if (stat != EGADS_SUCCESS) continue;
      printf(" EG_getTopology:     %d Face %d -- nLoops = %d\n",
             ibody+1, i+1, nloops);
      stat = EG_getRange(bodydata[ibody].faces[i], tlimits, &per);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_getRange Face return = %d!\n", stat);
        free(bodydata);
        return 1;
      }
      sizes[0] = 37;
      sizes[1] = 37;
      if (mtype == SREVERSE) sizes[0] = -37;
      stat = EG_makeTessGeom(geom, tlimits, sizes,
                             &bodydata[ibody].faceTess[i]);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_makeTessGeom Face return = %d!\n", stat);
        free(bodydata);
        return 1;
      }
    }
    for (j = i = 0; i < bodydata[ibody].nedges; i++) {
      stat = EG_getTopology(bodydata[ibody].edges[i], &geom, &oclass, 
                            &mtype, NULL, &nnodes, &nodes, &senses);
      if (stat != EGADS_SUCCESS) continue;
      if (mtype == DEGENERATE) continue;
      printf(" EG_getTopology:     %d Edge %d -- nNodes = %d\n",
             ibody+1, i+1, nnodes);
      stat = EG_getRange(bodydata[ibody].edges[i], tlimits, &per);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_getRange Edge return = %d!\n", stat);
        free(bodydata);
        return 1;
      }
      sizes[0] = 37;
      sizes[1] = 0;               /* not required */
      stat = EG_makeTessGeom(geom, tlimits, sizes, 
                             &bodydata[ibody].edgeTess[j]);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_makeTessGeom Edge return = %d!\n", stat);
        free(bodydata);
        return 1;
      }
      bodydata[ibody].edges[j] = bodydata[ibody].edges[i];
      j++;
    }
    if (j != bodydata[ibody].nedges) {
      printf(" NOTE: %d Degenerate Edges removed!\n", 
             bodydata[ibody].nedges-j);
      bodydata[ibody].nedges = j;
    }
  }
  printf(" \n");

  /* create the WebViewer context */
  cntxt = wv_createContext(1, 30.0, 1.0, 10.0, eye, center, up);
  if (cntxt == NULL) printf(" failed to create wvContext!\n");
        
  /* make the scene */
  for (sum = stat = ibody = 0; ibody < nbody; ibody++) {
  
    /* get faces */
    for (i = 0; i < bodydata[ibody].nfaces; i++) {
      stat  = EG_getTessGeom(bodydata[ibody].faceTess[i], sizes, &xyzs);
      if (stat != EGADS_SUCCESS) continue;
      len  = sizes[0]*sizes[1];
      ntri = 2*(sizes[0]-1)*(sizes[1]-1);
      tris = (int *) malloc(3*ntri*sizeof(int));
      if (tris == NULL) {
        printf(" Can not allocate %d triangles for Body %d Face %d\n",
               ntri, ibody, i+1);
        continue;
      }
      nseg = (sizes[0]-1)*sizes[1] + sizes[0]*(sizes[1]-1);
      segs = (int *) malloc(2*nseg*sizeof(int));
      if (segs == NULL) {
        printf(" Can not allocate %d segments for Body %d Face %d\n",
               nseg, ibody, i+1);
        free(tris);
        continue;
      }
      for (m = k = 0; k < sizes[1]-1; k++)
        for (j = 0; j < sizes[0]-1; j++) {
          tris[3*m  ] = j + sizes[0]*k     + 1;
          tris[3*m+1] = j + sizes[0]*k     + 2;
          tris[3*m+2] = j + sizes[0]*(k+1) + 2;
          m++;
          tris[3*m  ] = j + sizes[0]*(k+1) + 2;
          tris[3*m+1] = j + sizes[0]*(k+1) + 1;
          tris[3*m+2] = j + sizes[0]*k     + 1;
          m++;          
        }
      for (m = k = 0; k < sizes[1]; k++)
        for (j = 0; j < sizes[0]-1; j++) {
          segs[2*m  ] = j + sizes[0]*k + 1;
          segs[2*m+1] = j + sizes[0]*k + 2;
          m++;          
        }
      for (k = 0; k < sizes[1]-1; k++)
        for (j = 0; j < sizes[0]; j++) {
          segs[2*m  ] = j + sizes[0]*k     + 1;
          segs[2*m+1] = j + sizes[0]*(k+1) + 1;
          m++;          
        }

      sprintf(gpname, "Body %d Face %d", ibody+1, i+1);
      stat = wv_setData(WV_REAL64, len, (void *) xyzs,  WV_VERTICES, &items[0]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 0!\n", i, gpname);
      wv_adjustVerts(&items[0], focus);
      stat = wv_setData(WV_INT32, 3*ntri, (void *) tris, WV_INDICES, &items[1]);
      free(tris);
      if (stat < 0) printf(" wv_setData = %d for %s/item 1!\n", i, gpname);
      color[0]  = 1.0;
      color[1]  = ibody;
      color[1] /= nbody;
      color[2]  = 0.0;
      stat = wv_setData(WV_REAL32, 1, (void *) color,  WV_COLORS, &items[2]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 2!\n", i, gpname);
      stat = wv_setData(WV_INT32, 2*nseg, (void *) segs, WV_LINDICES, &items[3]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 3!\n", i, gpname);
      free(segs);
/*    color[0] = color[1] = color[2] = 0.8;  */
      color[0] = color[1] = color[2] = 0.0;
      stat = wv_setData(WV_REAL32, 1, (void *) color,  WV_LCOLOR, &items[4]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 4!\n", i, gpname);
      stat = wv_addGPrim(cntxt, gpname, WV_TRIANGLE, 
                         WV_ON|WV_ORIENTATION, 5, items);
      if (stat < 0) {
        printf(" wv_addGPrim = %d for %s!\n", stat, gpname);
      } else {
        if (cntxt != NULL)
          if (cntxt->gPrims != NULL) cntxt->gPrims[stat].lWidth = 1.0;
      }
      sum += ntri;
    }
    
    /* get edges */
    color[0] = color[1] = 0.0;
    color[2] = 1.0;
    for (i = 0; i < bodydata[ibody].nedges; i++) {
      stat  = EG_getTessGeom(bodydata[ibody].edgeTess[i], sizes, &xyzs);
      if (stat != EGADS_SUCCESS) continue;
      len  = sizes[0];
      nseg = sizes[0]-1;
      segs = (int *) malloc(2*nseg*sizeof(int));
      if (segs == NULL) {
        printf(" Can not allocate %d segments for Body %d Edge %d\n",
               nseg, ibody, i+1);
        continue;
      }
      for (j = 0; j < sizes[0]-1; j++) {
        segs[2*j  ] = j + 1;
        segs[2*j+1] = j + 2;          
      }

      sprintf(gpname, "Body %d Edge %d", ibody+1, i+1);
      stat = wv_setData(WV_REAL64, len, (void *) xyzs, WV_VERTICES, &items[0]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 0!\n", i, gpname);
      wv_adjustVerts(&items[0], focus);
      stat = wv_setData(WV_REAL32, 1, (void *) color,  WV_COLORS,   &items[1]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 1!\n", i, gpname);
      stat = wv_setData(WV_INT32, 2*nseg, (void *) segs, WV_INDICES,  &items[2]);
      if (stat < 0) printf(" wv_setData = %d for %s/item 2!\n", i, gpname);
      free(segs);
      stat = wv_addGPrim(cntxt, gpname, WV_LINE, WV_ON, 3, items);
      if (stat < 0) {
        printf(" wv_addGPrim = %d for %s!\n", stat, gpname);
      } else {
        if (cntxt != NULL)
          if (cntxt->gPrims != NULL) cntxt->gPrims[stat].lWidth = 1.5;
      }
    }
  }
  printf(" ** %d gPrims with %d triangles **\n", stat+1, sum);

  /* start the server code */

  stat = 0;
  if (wv_startServer(7681, NULL, NULL, NULL, 0, cntxt) == 0) {

    /* we have a single valid server -- stay alive a long as we have a client */
    while (wv_statusServer(0)) {
      usleep(500000);
      if (stat == 0) {
        if (startapp != NULL) system(startapp);
        stat++;
      }
/*    wv_broadcastText("I'm here!");  */
    }
  }
  wv_cleanupServers();

  /* finish up */
  for (ibody = 0; ibody < nbody; ibody++) {
    for (i = 0; i < bodydata[ibody].nedges; i++) 
      EG_deleteObject(bodydata[ibody].edgeTess[i]);
    free(bodydata[ibody].edgeTess);
    for (i = 0; i < bodydata[ibody].nfaces; i++) 
      EG_deleteObject(bodydata[ibody].faceTess[i]);
    free(bodydata[ibody].faceTess);
    EG_free(bodydata[ibody].faces);
    EG_free(bodydata[ibody].edges);
  }
  free(bodydata);

  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}


/* call-back invoked when a message arrives from the browser */

void browserMessage(/*@unused@*/ void *wsi, char *text, /*@unused@*/ int lena)
{
  printf(" RX: %s\n", text);
  /* ping it back
  wv_sendText(wsi, text); */
}
