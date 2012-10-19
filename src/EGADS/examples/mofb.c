/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Intersect a complete Model by a Face Body
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


int main(int argc, char *argv[])
{
  int mtype, oclass, nbody, *senses;
  ego context, model1, model2, wModel, geom, *bodies2;

  if (argc != 3) {
    printf("\n Usage: mofb model1 model2\n\n");
    return 1;
  }

  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel 1    = %d\n", EG_loadModel(context, 0, argv[1], &model1));
  printf(" EG_loadModel 2    = %d\n", EG_loadModel(context, 0, argv[2], &model2));
  /* get all bodies */
  printf(" EG_getTopology 2  = %d\n", EG_getTopology(model2, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies2, &senses));
  printf(" \n");
  
  printf(" EG_intersection   = %d\n", EG_solidBoolean(model1, bodies2[0], 
                                                      INTERSECTION, &wModel));
  printf(" EG_saveModel      = %d\n", EG_saveModel(wModel, "mofb.egads"));
  printf("\n");
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(wModel));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model2));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model1));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
