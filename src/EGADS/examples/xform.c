/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Transform test
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */
 
#include "egads.h"


int main(int argc, char *argv[])
{
  int    i, mtype, oclass, nBody, ntopos, n, *senses;
  double xform[12], limits[4];
  ego    context, model, oform, newModel, geom, body, newTopo;
  ego    *bodies, *topos, *dum;

  if (argc <= 3) {
    printf("\n Usage: xform filename [x/y/z/s #]\n\n");
    return 1;
  }

  xform[0] = xform[5] = xform[10] = 1.0;
  xform[1] = xform[2] = xform[ 3] = 0.0;
  xform[4] = xform[6] = xform[ 7] = 0.0;
  xform[8] = xform[9] = xform[11] = 0.0;
  
  for (i = 2; i < argc; i+=2)
    if ((argv[i][0] == 'x') || (argv[i][0] == 'X')) {
      sscanf(argv[i+1], "%lf", &xform[ 3]);
    } else if ((argv[i][0] == 'y') || (argv[i][0] == 'Y')) {
      sscanf(argv[i+1], "%lf", &xform[ 7]);
    } else if ((argv[i][0] == 'z') || (argv[i][0] == 'Z')) {
      sscanf(argv[i+1], "%lf", &xform[11]);
    } else if ((argv[i][0] == 's') || (argv[i][0] == 'S')) {
      sscanf(argv[i+1], "%lf", &xform[0]);
      xform[5] = xform[10] = xform[0];
    } else {
      printf("\n Usage: xform filename [x/y/z #]\n");
      printf("        Expecting x/y/z got %c!\n\n", argv[i][0]);
      return 1;
    }
  
  /* initialize */
  printf(" EG_open          = %d\n", EG_open(&context));
  printf(" EG_loadModel     = %d\n", EG_loadModel(context, 0, argv[1], &model));
  printf(" EG_makeTransform = %d\n", EG_makeTransform(context, xform, &oform));
#ifndef OUTSHELL
  printf(" EG_copyObject    = %d\n", EG_copyObject(model, oform, &newModel));
#else
  printf(" EG_getTopology   = %d\n", EG_getTopology(model, &geom, &oclass,
                                                    &mtype, limits, &nBody,
                                                    &bodies, &senses));

  printf(" EG_getBodyTopos  = %d\n", EG_getBodyTopos(bodies[0], NULL, SHELL,
                                                     &ntopos, &topos));

  printf(" EG_copyObject    = %d\n", EG_copyObject(topos[0], oform, &newTopo));
/*
  printf(" EG_flipObject    = %d\n", EG_flipObject(topos[0], &newTopo));
*/
  printf(" EG_makeTopology  = %d\n", EG_makeTopology(context, NULL, BODY,
                                                     SHEETBODY, NULL, 1, 
                                                     &newTopo, NULL, &body));
/*
  printf(" EG_getBodyTopos  = %d\n", EG_getBodyTopos(bodies[0], NULL, LOOP,
                                                     &ntopos, &topos));
  printf(" EG_getTopology   = %d\n", EG_getTopology(topos[1], &geom, &oclass, 
                                                    &mtype, limits, &n, &dum,
                                                    &senses)); 
  printf("             geom = %lx\n", geom);
  printf(" EG_copyObject    = %d\n", EG_copyObject(topos[1], oform, &newTopo));
  printf(" EG_makeTopology  = %d\n", EG_makeTopology(context, NULL, BODY,
                                                     WIREBODY, NULL, 1, 
                                                     &newTopo, NULL, &body));
*/
  printf(" EG_makeTopology  = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                     NULL, 1, &body, NULL,
                                                     &newModel));
#endif
  printf(" EG_saveModel     = %d\n", EG_saveModel(newModel, "newModel.egads"));
  printf(" \n");

#ifdef OUTSHELL
  printf(" EG_deleteObject  = %d\n", EG_deleteObject(newTopo));
#endif
  printf(" EG_deleteObject  = %d\n", EG_deleteObject(newModel));
  printf(" EG_deleteObject  = %d\n", EG_deleteObject(oform));
  printf(" EG_deleteObject  = %d\n", EG_deleteObject(model));
  printf(" EG_close         = %d\n", EG_close(context));
  return 0;
}
