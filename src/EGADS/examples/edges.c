/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Reoprt info on Edges
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


int main(int argc, char *argv[])
{
  int    i, j, k, m, stat, mtype, oclass, nbody, nnode, nedge, nface, *senses;
  int    nnodet, nfacet;
  double props[14];
  ego    context, model, geom, *bodies, *nodes, *edges, *faces;
  ego    *nodet, *facet;
  
  if (argc != 2) {
    printf("\n Usage: edges modelFile\n\n");
    return 1;
  }

  /* initialize */
  printf(" EG_open          = %d\n", EG_open(&context));
  printf(" EG_loadModel     = %d\n", EG_loadModel(context, 0, argv[1], &model));
  /* get all bodies */
  printf(" EG_getTopology   = %d\n", EG_getTopology(model, &geom, &oclass, 
                                                    &mtype, NULL, &nbody,
                                                    &bodies, &senses));

  for (i = 0; i < nbody; i++) {
    stat = EG_getBodyTopos(bodies[i], NULL, EDGE, &nedge, &edges);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_getBodyToposE = %d for Body %d\n", stat, i+1);
      continue;
    }
    stat = EG_getBodyTopos(bodies[i], NULL, FACE, &nfacet, &facet);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_getBodyToposF = %d for Body %d\n", stat, i+1);
      EG_free(edges);
      continue;
    }
    stat = EG_getBodyTopos(bodies[i], NULL, NODE, &nnodet, &nodet);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_getBodyToposN = %d for Body %d\n", stat, i+1);
      EG_free(facet);
      EG_free(edges);
      continue;
    }
    printf("\n Body #%d:  nNodes = %d   nFaces = %d\n", i+1, nnodet, nfacet);
    for (j = 0; j < nedge; j++) {
      stat = EG_getBodyTopos(bodies[i], edges[j], NODE, &nnode, &nodes);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_getBodyTopos  = %d for Body %d, edge %d\n", stat, i+1, j+1);
        continue;
      }
      stat = EG_getBodyTopos(bodies[i], edges[j], FACE, &nface, &faces);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_getBodyTopos  = %d for Body %d, Edge %d\n", stat, i+1, j+1);
        EG_free(nodes);
        continue;
      }
      stat = EG_getMassProperties(edges[j], props);
      if (stat != EGADS_SUCCESS) {
        printf(" EG_getMassProperties  = %d for Body %d, Edge %d\n",
               stat, i+1, j+1);
        EG_free(nodes);
        continue;
      }
      printf("   Edge %d: nnodes = %d  nfaces = %d  len = %lf\n",
             j+1, nnode, nface, props[1]);
      printf("       Nodes: ");
      for (k = 0; k < nnode; k++)
        for (m = 0; m < nnodet; m++)
          if (nodet[m] == nodes[k]) {
            printf("%d ", m+1);
            break;
          }
      printf("   Faces: ");
      for (k = 0; k < nface; k++)
        for (m = 0; m < nfacet; m++)
          if (facet[m] == faces[k]) {
            printf("%d ", m+1);
            break;
          }
      printf("\n");
      EG_free(faces);
      EG_free(nodes);
    }
    EG_free(nodet);
    EG_free(facet);
    EG_free(edges);
  }

  printf(" \n");
  printf(" EG_deleteObject  = %d\n", EG_deleteObject(model));
  
  printf(" EG_close         = %d\n", EG_close(context));
  return 0;
}
