/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Fillet an Existing Model
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <math.h>
#include "egads.h"


int main(int argc, char *argv[])
{
  int    i, j, mtype, oclass, nbody, nedge, *senses;
  double size, box[6];
  ego    context, model, newModel, geom, body, *bodies, *edges, *fedge;
  
  if (argc < 3) {
    printf("\n Usage: fillet filename relSize [edge# ... edge#]\n\n");
    return 1;
  }
  
  sscanf(argv[2], "%lf", &size);
  printf("\n fillet: Using Relative Size = %lf\n", size);

  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel      = %d\n", EG_loadModel(context, 0, argv[1], &model));
  if (model == NULL) return 1;

  /* get all bodies */
  printf(" EG_getTopology    = %d\n", EG_getTopology(model, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies, &senses));
  /* get all edges in body */
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(bodies[0], NULL, EDGE,
                                                      &nedge, &edges));
  printf(" EG_getBoundingBox = %d\n", EG_getBoundingBox(bodies[0], box));
  printf("       BoundingBox = %lf %lf %lf\n", box[0], box[1], box[2]);
  printf("                     %lf %lf %lf\n", box[3], box[4], box[5]);  

  size *= sqrt((box[0]-box[3])*(box[0]-box[3]) + (box[1]-box[4])*(box[1]-box[4]) +
               (box[2]-box[5])*(box[2]-box[5]));
  
  if (argc == 3) {
    /* fillet all of them */
    printf(" \n");
    printf(" EG_fillet         = %d\n", EG_filletBody(bodies[0], nedge, edges, size,
                                                      &body));
  } else {
    fedge = (ego *) malloc((argc-3)*sizeof(ego));
    if (fedge == NULL) {
      printf(" MALLOC ERROR!\n");
      EG_free(edges);
      printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
      printf(" EG_close          = %d\n", EG_close(context));
      return 1;
    }
    printf("\n fillet: Using Edges = ");
    for (i = 0; i < argc-3; i++) {
      sscanf(argv[i+3], "%d", &j);
      if ((j < 0) || (j > nedge)) {
        printf(" ERROR: Argument %d is %d [1-%d]!\n", i+3, j, nedge);
        free(fedge);
        EG_free(edges);
        printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
        printf(" EG_close          = %d\n", EG_close(context));
        return 1;
      }
      fedge[i] = edges[j-1];
      printf(" %d", j);
    }
    printf("\n\n");
    /* fillet each of them */
    printf(" EG_fillet         = %d\n", EG_filletBody(bodies[0], argc-3, fedge, size,
                                                      &body));
    free(fedge);
  }

  /* make a model and write it out */
  if (body != NULL) {
    printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                        NULL, 1, &body, NULL, &newModel));                                               
    printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "fillet.egads"));
    printf("\n");
    printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  }
                                                
  EG_free(edges);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
