/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Chamfer an Existing Model at 45 degrees
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <math.h>
#include "egads.h"


/* since the chamfers are symmetric we just need any touching face */
static void
getFace(ego body, int n, ego *edges, ego *faces)
{
  int i, nf, err;
  ego *ofaces;

  for (i = 0; i < n; i++) {
    err = EG_getBodyTopos(body, edges[i], FACE, &nf, &ofaces);
    if (err != EGADS_SUCCESS) {
      printf("   EG_getBodyTopos = %d for Edges %d!\n", err, i);
      continue;
    }
    if (nf != 2) printf("   Edge %d has %d Faces!\n", i, nf);
    faces[i] = ofaces[0];
    EG_free(ofaces);
  }
}


int main(int argc, char *argv[])
{
  int    i, j, n, mtype, oclass, nbody, nedge, *senses;
  double size, box[6];
  ego    context, model, newModel, geom, body, *bodies, *edges, *fedge;
  
  if (argc < 3) {
    printf("\n Usage: chamfer filename relSize [edge# ... edge#]\n\n");
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
    /* chamfer all of them */
    printf(" \n");
    fedge = (ego *) malloc(nedge*sizeof(ego));
    if (fedge == NULL) {
      printf(" MALLOC ERROR!\n");
      EG_free(edges);
      printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
      printf(" EG_close          = %d\n", EG_close(context));
      return 1;
    }
    getFace(bodies[0], nedge, edges, fedge);
    printf(" EG_chamfer        = %d\n", EG_chamferBody(bodies[0], nedge, edges, fedge,
                                                       0.5*size, 0.5*size, &body));
  } else {
    n     = argc-3;
    fedge = (ego *) malloc(2*n*sizeof(ego));
    if (fedge == NULL) {
      printf(" MALLOC ERROR!\n");
      EG_free(edges);
      printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
      printf(" EG_close          = %d\n", EG_close(context));
      return 1;
    }
    printf("\n fillet: Using Edges = ");
    for (i = 0; i < n; i++) {
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
    getFace(bodies[0], n, fedge, &fedge[n]);
    printf(" EG_chamfer        = %d\n", EG_chamferBody(bodies[0], n, fedge, &fedge[n],
                                                       0.5*size, 0.5*size, &body));
  }
  free(fedge);

  /* make a model and write it out */
  if (body != NULL) {
    printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                        NULL, 1, &body, NULL, &newModel));                                               
    printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "chamfer.egads"));
    printf("\n");
    printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  }
                                                
  EG_free(edges);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
