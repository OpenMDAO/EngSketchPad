/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Hollow out an Existing Model
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
  int       i, j, stat, mtype, oclass, nbody, nface, atype, len, *senses;
  double    size, box[6];
  ego       context, model, newModel, geom, body, *bodies, *faces, *hfaces;
  const int *id;
  
  if (argc < 3) {
    printf("\n Usage: fillet filename relOffset [face# ... face#]\n\n");
    return 1;
  }
  
  sscanf(argv[2], "%lf", &size);
  if (argc == 3) {
    printf("\n offset: Using Relative Offset = %lf\n\n", size);
  } else {
    printf("\n hollow: Using Relative Offset = %lf\n", size);
  }

  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel      = %d\n", EG_loadModel(context, 0, argv[1], &model));
  if (model == NULL) return 1;

  /* get all bodies */
  printf(" EG_getTopology    = %d\n", EG_getTopology(model, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies, &senses));
  /* get all faces in the first body */
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(bodies[0], NULL, FACE,
                                                      &nface, &faces));
  /* mark every face */
  if (argc != 3)
    for (i = 0; i < nface; i++) {
      j = i+1;
      EG_attributeAdd(faces[i], "Face#", ATTRINT, 1, &j, NULL, NULL);
    }

  printf(" EG_getBoundingBox = %d\n", EG_getBoundingBox(bodies[0], box));
  printf("       BoundingBox = %lf %lf %lf\n", box[0], box[1], box[2]);
  printf("                     %lf %lf %lf\n", box[3], box[4], box[5]);

  size *= sqrt((box[0]-box[3])*(box[0]-box[3]) + (box[1]-box[4])*(box[1]-box[4]) +
               (box[2]-box[5])*(box[2]-box[5]));
  
  hfaces = NULL;
  if (argc != 3) {
    hfaces = (ego *) malloc((argc-3)*sizeof(ego));
    if (hfaces == NULL) {
      printf(" MALLOC ERROR!\n");
      EG_free(faces);
      printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
      printf(" EG_close          = %d\n", EG_close(context));
      return 1;
    }
    printf("\n hollow: Using Faces = ");
    for (i = 0; i < argc-3; i++) {
      sscanf(argv[i+3], "%d", &j);
      if ((j < 0) || (j > nface)) {
        printf(" ERROR: Argument %d is %d [1-%d]!\n", i+3, j, nface);
        free(hfaces);
        EG_free(faces);
        printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
        printf(" EG_close          = %d\n", EG_close(context));
        return 1;
      }
      hfaces[i] = faces[j-1];
      printf(" %d", j);
    }
  }
  EG_free(faces);
  printf("\n\n");
  
  /* do the hollow */
  printf(" EG_hollow         = %d\n", EG_hollowBody(bodies[0], argc-3, hfaces, size,
                                                    1, &body));
  if (hfaces != NULL) free(hfaces);
  printf(" EG_getBodyTopos   = %d\n", EG_getBodyTopos(body, NULL, FACE,
                                                      &nface, &faces));
  /* show new face markings */
  if (argc != 3)
    for (i = 0; i < nface; i++) {
      stat = EG_attributeRet(faces[i], "Face#", &atype, &len, &id, NULL, NULL);
      if (stat != EGADS_SUCCESS) continue;
      printf("  Face %d/%d:  Old ID = %d\n", i+1, nface, id[0]);
    }

  /* make a model and write it out */
  if (body != NULL) {
    printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                        NULL, 1, &body, NULL, &newModel));                                               
    printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "hollow.egads"));
    printf("\n");
    printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  }
                                                
  EG_free(faces);
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
