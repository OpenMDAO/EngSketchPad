/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Sweep Test
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


int main(/*@unused@*/int argc, /*@unused@*/char *argv[])
{
  int mtype, oclass, nbody, nface, nedge, *senses;
  ego context, model, newModel, geom, body, *bodies, *faces, *edges;
  
  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel      = %d\n", EG_loadModel(context, 0, "Piston.BRep", 
                                                   &model));
  /* get all bodies */
  printf(" EG_getTopology    = %d\n", EG_getTopology(model, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies, &senses));
  /* get all faces in body */
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(bodies[0], NULL, FACE,
                                                      &nface, &faces));
                                                      
  /* get all edges in body */
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(bodies[0], NULL, EDGE,
                                                      &nedge, &edges));
  printf(" \n");

  printf(" EG_sweep          = %d\n", EG_sweep(faces[5], edges[1], &body));

  /* make a model and write it out */
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                      NULL, 1, &body, NULL, 
                                                      &newModel));
  printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "sweep.egads"));
                                                
  printf("\n");
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  EG_free(faces);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
