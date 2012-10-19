/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             General Build Test
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


int main(/*@unused@*/int argc, char *argv[])
{
  int    mtype, oclass, nbody, nedge, *senses;
  double box[6], data[9], limits[4];
  ego    context, model, face, newModel, wModel, geom, *bodies;
  ego    body[2], *facEdge;
  
  /* initialize */
  printf(" EG_open           = %d\n", EG_open(&context));
  printf(" EG_loadModel      = %d\n", EG_loadModel(context, 0, argv[1], &model));
  printf(" EG_getBoundingBox = %d\n", EG_getBoundingBox(model, box));
  printf("       BoundingBox = %lf %lf %lf\n", box[0], box[1], box[2]);
  printf("                     %lf %lf %lf\n", box[3], box[4], box[5]);
  /* get all bodies */
  printf(" EG_getTopology    = %d\n", EG_getTopology(model, &geom, &oclass, 
                                                     &mtype, NULL, &nbody,
                                                     &bodies, &senses));
  printf(" \n");
  
  /* make a z-plane centered at the middle of the bounding box */
  data[0] = 0.5*(box[0] + box[3]);
  data[1] = 0.5*(box[1] + box[4]);
  data[2] = 0.5*(box[2] + box[5]);
  data[3] = 1.0;
  data[4] = data[5] = 0.0;
  data[7] = 1.0;
  data[6] = data[8] = 0.0;
  printf(" EG_makeGeometry   = %d\n", EG_makeGeometry(context, SURFACE, PLANE, 
                                                      NULL, NULL, data, &geom));
  /* make a face out of the surface */
  limits[0] = -0.75*(box[3] - box[0]) - 1.0;
  limits[1] =  0.75*(box[3] - box[0]) + 1.0;
  limits[2] = -0.75*(box[4] - box[1]) - 1.0;
  limits[3] =  0.75*(box[4] - box[1]) + 1.0;
  printf(" EG_makeFace       = %d\n", EG_makeFace(geom, SFORWARD, limits, &face));

/*
  data[0] = box[0] - 1.0;
  data[1] = box[1] - 1.0;
  data[2] = 0.5*(box[2] + box[5]) - 0.1;
  data[3] = box[3] + 1.0;
  data[4] = box[4] + 1.0;
  data[5] = 0.5*(box[2] + box[5]) + 0.1; 
  printf(" EG_makeSolidBody  = %d\n", EG_makeSolidBody(context, 1, data, &face));
*/
/*
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, BODY, FACEBODY,
                                                      NULL, 1, &face, NULL, &body[0]));
  printf(" EG_copyObject     = %d\n", EG_copyObject(bodies[0], NULL, &body[1]));
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                      NULL, 2, body, NULL, &newModel));
*/
                                                    
  /* cut the first body in the model 
  printf(" EG_solidBoolean   = %d\n", EG_solidBoolean(bodies[0], face, SUBTRACTION,
                                                      &newModel)); */
  printf(" EG_intersection   = %d\n", EG_intersection(bodies[0], face, &nedge,
                                                      &facEdge, &wModel));
  printf(" EG_imprintBody    = %d\n", EG_imprintBody(bodies[0], nedge, facEdge, 
                                                     &body[0]));
  printf(" EG_saveModel      = %d\n", EG_saveModel(wModel, "wModel.egads"));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(wModel));
  EG_free(facEdge);
  printf(" EG_makeTopology   = %d\n", EG_makeTopology(context, NULL, MODEL, 0,
                                                      NULL, 1, body, NULL, &newModel));                                               
  printf(" EG_saveModel      = %d\n", EG_saveModel(newModel, "newModel.egads"));
                                                
  printf("\n");
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(newModel));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(face));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(geom));
  printf(" EG_deleteObject   = %d\n", EG_deleteObject(model));
  printf(" EG_close          = %d\n", EG_close(context));
  return 0;
}
