/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             ReBuild Topology
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egads.h"


static void
parseOut(int level, ego object, int sense)
{
  int    i, stat, oclass, mtype, nobjs, periodic, *senses, *ivec;
  ego    prev, next, geom, top, *objs;
  double limits[4], *rvec;
  long unsigned int pointer;
  static char *classType[27] = {"CONTEXT", "TRANSFORM", "TESSELLATION",
                                "NIL", "EMPTY", "REFERENCE", "", "",
                                "", "", "PCURVE", "CURVE", "SURFACE", "", 
                                "", "", "", "", "", "", "NODE",
                                "EGDE", "LOOP", "FACE", "SHELL",
                                "BODY", "MODEL"};
  static char *curvType[9] = {"Line", "Circle", "Ellipse", "Parabola",
                              "Hyperbola", "Trimmed", "Bezier", "BSpline", 
                              "Offset"};
  static char *surfType[11] = {"Plane", "Spherical", "Cylinder", "Revolution",
                               "Toroidal", "Trimmed" , "Bezier", "BSpline", 
                               "Offset", "Conical", "Extrusion"};
  
  pointer = (long unsigned int) object;
  stat = EG_getInfo(object, &oclass, &mtype, &top, &prev, &next);
  if (stat != EGADS_SUCCESS) {
    printf(" parseOut: %d EG_getInfo return = %d\n", level, stat);
    return;
  }
  
  /* geometry */
  if ((oclass >= PCURVE) && (oclass <= SURFACE)) {
    stat = EG_getGeometry(object, &oclass, &mtype, &geom, &ivec, &rvec);
    if (stat != EGADS_SUCCESS) {
      printf(" parseOut: %d EG_getGeometry return = %d\n", level, stat);
      return;
    }
    stat = EG_getRange(object, limits, &periodic);

    /* output most data except the axes info */
    if (oclass != SURFACE) {

      for (i = 0; i < 2*level; i++) printf(" ");
      printf("%s %lx  range = %le %le  per = %d\n", 
             classType[oclass], pointer, limits[0], limits[1], periodic);

      for (i = 0; i < 2*level+2; i++) printf(" ");
      if (oclass == PCURVE) {
        switch (mtype) {
          case CIRCLE:
            printf("%s  radius = %lf\n", curvType[mtype-1], rvec[6]);
            break;
          case ELLIPSE:
            printf("%s  major = %lf, minor = %lf\n", 
                   curvType[mtype-1], rvec[6], rvec[7]);
            break;
          case PARABOLA:
            printf("%s  focus = %lf\n", curvType[mtype-1], rvec[6]);
            break;
          case HYPERBOLA:
            printf("%s  major = %lf, minor = %lf\n", 
                   curvType[mtype-1], rvec[6], rvec[7]);
            break;
          case TRIMMED:
            printf("%s  first = %lf, last = %lf\n", 
                   curvType[mtype-1], rvec[0], rvec[1]);
            break;
          case BEZIER:
            printf("%s  flags = %x, degree = %d, #CPs = %d\n", 
                   curvType[mtype-1], ivec[0], ivec[1], ivec[2]);
            break;       
          case BSPLINE:
            printf("%s  flags = %x, degree = %d, #CPs = %d, #knots = %d\n", 
                   curvType[mtype-1], ivec[0], ivec[1], ivec[2], ivec[3]);
            break;
          case OFFSET:
            printf("%s  offset = %lf\n", curvType[mtype-1], rvec[0]);
            break;
          case 0:
            printf("unknown curve type!\n");
            break;
          default:
            printf("%s\n", curvType[mtype-1]);
        }
      } else {
        switch (mtype) {
          case CIRCLE:
            printf("%s  radius = %lf\n", curvType[mtype-1], rvec[9]);
            break;
          case ELLIPSE:
            printf("%s  major = %lf, minor = %lf\n", 
                   curvType[mtype-1], rvec[9], rvec[10]);
            break;
          case PARABOLA:
            printf("%s  focus = %lf\n", curvType[mtype-1], rvec[9]);
            break;
          case HYPERBOLA:
            printf("%s  major = %lf, minor = %lf\n", 
                   curvType[mtype-1], rvec[9], rvec[10]);
            break;
          case TRIMMED:
            printf("%s  first = %lf, last = %lf\n", 
                   curvType[mtype-1], rvec[0], rvec[1]);
            break;
          case BEZIER:
            printf("%s  flags = %x, degree = %d, #CPs = %d\n", 
                   curvType[mtype-1], ivec[0], ivec[1], ivec[2]);
            break;       
          case BSPLINE:
            printf("%s  flags = %x, degree = %d, #CPs = %d, #knots = %d\n", 
                   curvType[mtype-1], ivec[0], ivec[1], ivec[2], ivec[3]);
            break;
          case OFFSET:
            printf("%s  offset = %lf\n", curvType[mtype-1], rvec[3]);
            break;
          case 0:
            printf("unknown curve type!\n");
            break;
          default:
            printf("%s\n", curvType[mtype-1]);
        }
      }

    } else {
    
      for (i = 0; i < 2*level; i++) printf(" ");
      printf("%s %lx  Urange = %le %le  Vrange = %le %le  per = %d\n", 
             classType[oclass], pointer, limits[0], limits[1], 
                                         limits[2], limits[3], periodic);

      for (i = 0; i < 2*level+2; i++) printf(" ");
      switch (mtype) {
        case SPHERICAL:
          printf("%s  radius = %lf\n", surfType[mtype-1], rvec[9]);
          break;
        case CONICAL:
          printf("%s  angle = %lf, radius = %lf\n", 
                 surfType[mtype-1], rvec[12], rvec[13]);
          break;
        case CYLINDRICAL:
          printf("%s  radius = %lf\n", surfType[mtype-1], rvec[12]);
          break;
        case TOROIDAL:
          printf("%s  major = %lf, minor = %lf\n", 
                 surfType[mtype-1], rvec[12], rvec[13]);
          break;
        case BEZIER:
          printf("%s  flags = %x, U deg = %d #CPs = %d, V deg = %d #CPs = %d\n", 
                 surfType[mtype-1], ivec[0], ivec[1], ivec[2], ivec[3], ivec[4]);
          break;
        case BSPLINE:
          printf("%s  flags = %x, U deg = %d #CPs = %d #knots = %d ",
                 surfType[mtype-1], ivec[0], ivec[1], ivec[2], ivec[3]);
          printf(" V deg = %d #CPs = %d #knots = %d\n",
                 ivec[4], ivec[5], ivec[6]);
          break;
        case TRIMMED:
          printf("%s  U trim = %lf %lf, V trim = %lf %lf\n", 
                 surfType[mtype-1], rvec[0], rvec[1], rvec[2], rvec[3]);
          break;
        case OFFSET:
          printf("%s  offset = %lf\n", surfType[mtype-1], rvec[0]);
          break;
        case 0:
          printf("unknown surface type!\n");
          break;
        default:
          printf("%s\n", surfType[mtype-1]);
      }
    }

    if (ivec != NULL) EG_free(ivec);
    if (rvec != NULL) EG_free(rvec);
    if (geom != NULL) parseOut(level+1, geom, 0);
    return;
  }

  /* output class and pointer data */

  for (i = 0; i < 2*level; i++) printf(" ");
  if (sense == 0) {
    printf("%s %lx %d\n", classType[oclass], pointer, mtype);
  } else {
    printf("%s %lx %d  sense = %d\n", classType[oclass], pointer, mtype, sense);
  }

  /* topology*/
  if ((oclass >= NODE) && (oclass <= MODEL)) {
    stat = EG_getTopology(object, &geom, &oclass, &mtype, limits, &nobjs,
                          &objs, &senses);
    if (stat != EGADS_SUCCESS) {
      printf(" parseOut: %d EG_getTopology return = %d\n", level, stat);
      return;
    }
    if (oclass == NODE) {
      for (i = 0; i < 2*level+2; i++) printf(" ");
      printf("XYZ = %lf %lf %lf\n", limits[0], limits[1], limits[2]);
    } else if (oclass == EDGE) {
      for (i = 0; i < 2*level+2; i++) printf(" ");
      if (mtype == DEGENERATE) {
        printf("tRange = %lf %lf -- Degenerate!\n", limits[0], limits[1]);
      } else {
        printf("tRange = %lf %lf\n", limits[0], limits[1]);
      }
    } else if (oclass == FACE) {
      for (i = 0; i < 2*level+2; i++) printf(" ");
      printf("uRange = %lf %lf, vRange = %lf %lf\n", limits[0], limits[1], 
                                                     limits[2], limits[3]);
    }
    
    if ((geom != NULL) && (mtype != DEGENERATE)) parseOut(level+1, geom, 0);
    if (senses == NULL) {
      for (i = 0; i < nobjs; i++) parseOut(level+1, objs[i], 0);
    } else {
      for (i = 0; i < nobjs; i++) parseOut(level+1, objs[i], senses[i]);
    }
    if ((geom != NULL) && (oclass == LOOP))
      for (i = 0; i < nobjs; i++) parseOut(level+1, objs[i+nobjs], 0);
  }

}


static ego
LookUp(ego target, int len, ego *list, ego *newList)
{
  int i;
  
  for (i = 0; i < len; i++)
    if (target == list[i]) return newList[i];

  printf(" Error: EGO not found!\n");
  return NULL;
}


int main(int argc, char *argv[])
{
  int    i, j, k, n, *senses, stat, oclass, mtype, stype;
  int    nbody, nshell, nface, nloop, nedge, nnode;
  double data[7], limit[4];
  ego    context, top, obj, ref, *bodies, *dummy, nds[2], *dum, prev, next;
  ego    *shells, *faces, *loops, *edges, *nodes, body;
  ego    *shelln, *facen, *loopn, *edgen, *noden, bodyn, model;
  
  if (argc != 2) {
    printf("\n Usage: rebuild filename\n\n");
    return 1;
  }

  /* create an EGADS context */
  stat = EG_open(&context);
  if (stat != EGADS_SUCCESS) {
    printf(" EG_open return = %d\n", stat);
    return 1;
  }

  if (strcmp(argv[1],"box") == 0) {
    data[0] = data[1] = data[2] = 0.0;
    data[3] = data[4] = data[5] = 1.0;
    stat = EG_makeSolidBody(context, 1, data, &top);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_makeSolidBody box return = %d\n", stat);
      return 1;
    }
    body = top;
  } else if ((strcmp(argv[1],  "cylinder") == 0) ||
             (strcmp(argv[1], "-cylinder") == 0)) {
    data[0] = data[1] = data[2] = 0.0;
    data[3] = data[4] = data[5] = 0.0;
    data[4] = data[6] = 1.0;
    stype = 4;
    if (strcmp(argv[1], "-cylinder") == 0) stype = -4;
    stat = EG_makeSolidBody(context, stype, data, &top);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_makeSolidBody cylinder return = %d\n", stat);
      return 1;
    }
    body = top;
  } else if ((strcmp(argv[1],  "cone") == 0) ||
             (strcmp(argv[1], "-cone") == 0)) {
    data[0] = data[1] = data[2] = 0.0;
    data[3] = data[4] = data[5] = 0.0;
    data[4] = data[6] = 1.0;
    stype = 3;
    if (strcmp(argv[1], "-cone") == 0) stype = -3;
    stat = EG_makeSolidBody(context, stype, data, &top);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_makeSolidBody cone return = %d\n", stat);
      return 1;
    }
    body = top;
  } else if ((strcmp(argv[1],  "sphere") == 0) ||
             (strcmp(argv[1], "-sphere") == 0)) {
    data[0] = data[1] = data[2] = 0.0;
    data[3] = 1.0;
    stype = 2;
    if (strcmp(argv[1], "-sphere") == 0) stype = -2;
    stat = EG_makeSolidBody(context, stype, data, &top);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_makeSolidBody sphere return = %d\n", stat);
      return 1;
    }
    body = top;
  } else {
    /* load a model from disk */
    stat = EG_loadModel(context, 0, argv[1], &top);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_loadModel return = %d\n", stat);
      return 1;
    }
    stat = EG_getTopology(top, &ref, &oclass, &mtype, NULL,
                          &nbody, &bodies, &senses);
    if (stat != EGADS_SUCCESS) {
      printf(" EG_getTopology return = %d\n", stat);
      return 1;
    }
    body = bodies[0];
  }
  
  parseOut(0, top, 0);
  printf(" \n");

  /* copy and rebuild */

  EG_getBodyTopos(body, NULL, NODE,  &nnode,  &nodes);
  EG_getBodyTopos(body, NULL, EDGE,  &nedge,  &edges);
  EG_getBodyTopos(body, NULL, LOOP,  &nloop,  &loops);
  EG_getBodyTopos(body, NULL, FACE,  &nface,  &faces);
  EG_getBodyTopos(body, NULL, SHELL, &nshell, &shells);

  printf("\n building %d Nodes!\n", nnode);
  noden = (ego *) malloc(nnode*sizeof(ego));
  for (i = 0; i < nnode; i++) {
    EG_getTopology(nodes[i], &ref, &oclass, &mtype, limit, &j, &dummy, &senses);
    stat = EG_makeTopology(context, ref, oclass, mtype, limit,
                           j, dummy, senses, &noden[i]);
    printf(" Node %d: EG_makeTopology = %d\n", i+1, stat);
  }

  printf("\n building %d Edges!\n", nedge);
  edgen = (ego *) malloc(nedge*sizeof(ego));
  for (i = 0; i < nedge; i++) {
    EG_getTopology(edges[i], &ref, &oclass, &mtype, limit, &j, &dummy, &senses);
    nds[0] = LookUp(dummy[0], nnode, nodes, noden);
    if (j == 2) nds[1] = LookUp(dummy[1], nnode, nodes, noden);
    stat = EG_makeTopology(context, ref, oclass, mtype, limit,
                           j, nds, senses, &edgen[i]);
    printf(" Edge %d: EG_makeTopology = %d  %d\n", i+1, stat, mtype);
  } 
  
  printf("\n building %d Loops!\n", nloop);
  loopn = (ego *) malloc(nloop*sizeof(ego));
  for (i = 0; i < nloop; i++) {
    EG_getTopology(loops[i], &ref, &oclass, &mtype, limit, &j, &dummy, &senses);
    n = 1;
    if (ref != NULL) n = 2;
    dum = (ego *) malloc(n*j*sizeof(ego));
    for (k = 0; k < j; k++) 
      dum[k] = LookUp(dummy[k], nedge, edges, edgen);
    if (n == 2)
      for (k = 0; k < j; k++) dum[j+k] = dummy[j+k];
    stat = EG_makeTopology(context, ref, oclass, CLOSED, limit,
                           j, dum, senses, &loopn[i]);
    printf(" Loop %d: EG_makeTopology = %d  %d\n", i+1, stat, j);
    free(dum);
  }
  
  printf("\n building %d Faces!\n", nface);
  facen = (ego *) malloc(nface*sizeof(ego));
  for (i = 0; i < nface; i++) {
    EG_getTopology(faces[i], &ref, &oclass, &mtype, limit, &j, &dummy, &senses);
    dum = (ego *) malloc(j*sizeof(ego));
    for (k = 0; k < j; k++) 
      dum[k] = LookUp(dummy[k], nloop, loops, loopn);
    stat = EG_makeTopology(context, ref, oclass, mtype, limit,
                           j, dum, senses, &facen[i]);
    EG_getInfo(ref, &oclass, &mtype, &obj, &prev, &next);
    printf(" Face %d: EG_makeTopology = %d, surf = %d\n", i+1, stat, mtype);
    free(dum);
  }
  
  printf("\n building %d Shell(s)!\n", nshell);
  shelln = (ego *) malloc(nshell*sizeof(ego));
  for (i = 0; i < nshell; i++) {
    EG_getTopology(shells[i], &ref, &oclass, &mtype, limit, &j, &dummy,
                   &senses);
    dum = (ego *) malloc(j*sizeof(ego));
    for (k = 0; k < j; k++) 
      dum[k] = LookUp(dummy[k], nface, faces, facen);
    stat = EG_makeTopology(context, ref, oclass, mtype, limit,
                           j, dum, senses, &shelln[i]);
    printf(" Shell %d: EG_makeTopology = %d\n", i+1, stat);
    free(dum);
  }

  printf(" \n");
/*
  stat = EG_makeTopology(context, NULL, BODY, SHEETBODY, limit,
                         nshell, shelln, NULL, &bodyn);
*/
  stat = EG_makeTopology(context, NULL, BODY, SOLIDBODY, limit,
                         nshell, shelln, NULL, &bodyn);
  printf(" Body: EG_makeTopology = %d\n", stat);
  if (stat == 0) {
    stat = EG_makeTopology(context, NULL, MODEL, 0, NULL, 1, &bodyn,
                           NULL, &model);
    printf(" EG_makeTopology M = %d\n", stat);
    printf(" EG_saveModel      = %d\n", EG_saveModel(model, "rebuild.BRep"));
    printf(" \n");
    printf(" EG_deleteObject M = %d\n", EG_deleteObject(model));
    } else {
      printf(" \n");
    }  
/*
  parseOut(0, bodyn, 0);
  printf(" \n");
*/
  for (i = 0; i < nshell; i++) EG_deleteObject(shelln[i]);
  free(shelln);
  for (i = 0; i < nface; i++) EG_deleteObject(facen[i]);
  free(facen);
  for (i = 0; i < nloop; i++) EG_deleteObject(loopn[i]);
  free(loopn);
  for (i = 0; i < nedge; i++) EG_deleteObject(edgen[i]);
  free(edgen);
  for (i = 0; i < nnode; i++) EG_deleteObject(noden[i]);
  free(noden);
  if (shells != NULL) EG_free(shells);
  if (faces  != NULL) EG_free(faces);
  if (loops  != NULL) EG_free(loops);
  if (edges  != NULL) EG_free(edges);
  if (nodes  != NULL) EG_free(nodes);
  printf(" EG_deleteObject top  = %d\n", EG_deleteObject(top));
  printf(" EG_close the context = %d\n", EG_close(context));
  return 0;
}
