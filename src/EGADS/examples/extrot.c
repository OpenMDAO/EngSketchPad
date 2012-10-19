/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Extrude & Rotate Test
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */


#include "egads.h"


int main(/*@unused@*/int argc, /*@unused@*/char *argv[])
{
  int    n, mtype, oclass, nbody, nface, *senses;
  double dir[3]  = {1.0, 0.0, 0.0};
  double axis[6] = {0.0, 0.0, -200.0, 0.0, 1.0, 0.0};
  ego    context, model, newModel, geom, body, obj, *bodies, *faces, *loops;
  
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
  /* look at fifth face */
  obj = faces[4];
#ifdef SHEET
  printf(" EG_getTopology    = %d\n", EG_getTopology(obj, &geom, &oclass, 
                                                     &mtype, NULL, &n,
                                                     &loops, &senses));
  obj = loops[0];
#endif
  printf(" \n");

#ifdef EXTRUDE
  /* extrude object in Y */
  printf(" EG_extrude        = %d\n", EG_extrude(obj, 150.0, dir, &body));
#else
  /* rotate object about X */
  printf(" EG_rotate         = %d\n", EG_rotate(obj, 360.0, axis, &body));
#endif

  /* make a model and write it out */
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                      NULL, 1, &body, NULL, 
                                                      &newModel));
  printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "extrot.egads"));
                                                
  printf("\n");
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  EG_free(faces);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
