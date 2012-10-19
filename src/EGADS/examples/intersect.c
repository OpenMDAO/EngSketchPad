/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Intersect a Body in a Model by a Face in another
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


int main(int argc, char *argv[])
{
  int i, mtype, oclass, nbody, nface, nedge, iface, *senses;
  ego context, model1, model2, wModel, newModel, geom, body;
  ego *bodies1, *bodies2, *faces, *facEdge;

  if (argc != 4) {
    printf("\n Usage: intersect model(body) model(face) face#\n\n");
    return 1;
  }
  iface = atoi(argv[3]);

  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel 1    = %d\n", EG_loadModel(context, 0, argv[1], &model1));
  printf(" EG_loadModel 2    = %d\n", EG_loadModel(context, 0, argv[2], &model2));
  /* get all bodies */
  printf(" EG_getTopology 1  = %d\n", EG_getTopology(model1, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies1, &senses));
  printf(" EG_getTopology 2  = %d\n", EG_getTopology(model2, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies2, &senses));
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(bodies2[0], NULL, FACE,
                                                      &nface, &faces));
  if ((iface < 1) || (iface > nface)) {
    printf(" ERROR: face # = %d [1-%d]!\n\n", iface, nface);
    printf(" EG_deleteObject   = %d\n", EG_deleteObject(model2));
    printf(" EG_deleteObject   = %d\n", EG_deleteObject(model1));
    printf(" EG_close          = %d\n", EG_close(context));
    return 1;
  }
  iface--;
  printf(" \n");
  
  printf(" EG_intersection   = %d\n", EG_intersection(bodies1[0], faces[iface],
                                                      &nedge, &facEdge, 
                                                      &wModel));
  printf("             nedge = %d\n", nedge);
  printf(" EG_saveModel      = %d\n", EG_saveModel(wModel, "wModel.egads"));

  printf(" EG_imprintBody    = %d\n", EG_imprintBody(bodies1[0], nedge, facEdge,
                                                     &body));
  for (i = 0; i < nedge; i++) facEdge[2*i] = faces[2];
  printf(" EG_imprintBody    = %d\n", EG_imprintBody(bodies2[0], nedge, facEdge,
                                                     &body));
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                      NULL, 1, &body, NULL,
                                                      &newModel));
  printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "newModel.egads"));
                                                
  printf("\n");
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(  wModel));
  EG_free(facEdge);
  EG_free(faces);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model2));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model1));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
