/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             FORTRAN Bindings for Topological Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>

#define INT8 unsigned long long

#include "egadsTypes.h"
#include "egadsInternals.h"


  extern /*@null@*/ char *EG_f2c(const char *name, int nameLen);

  extern int EG_getTopology(const egObject *topo, egObject **geom,
                             int *oclass, int *mtype, 
                             /*@null@*/ double *limits, int *nChildren, 
                             egObject ***children, int **senses);
  extern int EG_makeTopology(egObject *context, /*@null@*/ egObject *geom, 
                             int oclass, int mtype, /*@null@*/ double *limits, 
                             int nChildren, /*@null@*/ egObject **children, 
                             /*@null@*/ int *senses, egObject **topo);
  extern int EG_makeFace( egObject *object, int mtype, 
                          /*@null@*/ const double *limits, egObject **face );
  extern int EG_getArea( egObject *object, /*@null@*/ const double *limits, 
                         double *area );
  extern int EG_getBodyTopos(const egObject *body, /*@null@*/ egObject *src, 
                             int oclass, int *ntopo, egObject ***topos);
  extern int EG_makeSolidBody(egObject *context, int stype, const double *data,
                              egObject **body);
  extern int EG_getBoundingBox(const egObject *topo, double *box);
  extern int EG_getMassProperties(const egObject *topo, double *box);
  extern int EG_isEquivalent(const egObject *topo1, const egObject *topo2);
  extern int EG_loadModel(egObject *context, int bflg, const char *name, 
                          egObject **model);
  extern int EG_saveModel(const egObject *model, const char *name);
  extern int EG_getEdgeUV(const egObject *face, const egObject *topo, int sense,
                          double t, double *uv);
  extern int EG_inTopology(const egObject *topo, const double *xyz);
  extern int EG_inFace(const egObject *face, const double *uv);


int
#ifdef WIN32
IG_GETTOPOLOGY (INT8 *topo, INT8 *igeom, int *oclass, int *mtype, 
                double *limits, int *nchildren, INT8 **children, 
                int **senses)
#else
ig_gettopology_(INT8 *topo, INT8 *igeom, int *oclass, int *mtype, 
                double *limits, int *nchildren, INT8 **children, 
                int **senses)
#endif
{
  int      i, stat, nobj, n = 1;
  INT8     *cobjs;
  egObject *object, *geom, **objs;

  *igeom     = 0;
  *oclass    = *mtype = 0;
  *nchildren = 0;
  *children  = NULL;
  *senses    = NULL;
  object     = (egObject *) *topo;
  stat       = EG_getTopology(object, &geom, oclass, mtype, 
                              limits, &nobj, &objs, senses);
  if (stat == EGADS_SUCCESS) {
    if (nobj != 0) {
      if ((*oclass == LOOP) && (geom != NULL)) n = 2;
      *children = cobjs = (INT8 *) EG_alloc(n*nobj*sizeof(INT8));
      if (cobjs == NULL) return EGADS_MALLOC;
      for (i = 0; i < n*nobj; i++) cobjs[i] = (INT8) objs[i];
    }
    *nchildren = nobj;
    *igeom     = (INT8) geom;
  }
  return stat;
}


int
#ifdef WIN32
IG_MAKETOPOLOGY (INT8 *cntxt, INT8 *igeom, int *oclass, int *mtype, 
                double *limits, int *nchildren, INT8 *children, 
                int *senses, INT8 *topo)
#else
ig_maketopology_(INT8 *cntxt, INT8 *igeom, int *oclass, int *mtype, 
                double *limits, int *nchildren, INT8 *children, 
                int *senses, INT8 *topo)
#endif
{
  int      i, stat, n = 1;
  egObject *context, *object, *geom, **objs = NULL;
  
  *topo   = 0;
  context = (egObject *) *cntxt;
  geom    = (egObject *) *igeom;
  if ((geom != NULL) && (*oclass == LOOP)) n = 2;
  if (*nchildren != 0) {
    objs = (egObject **) EG_alloc(*nchildren*n*sizeof(egObject *));
    if (objs == NULL) return EGADS_MALLOC;
    for (i = 0; i < *nchildren*n; i++)
      objs[i] = (egObject *) children[i];
  }
  stat = EG_makeTopology(context, geom, *oclass, *mtype, limits, 
                         *nchildren, objs, senses, &object);
  if (objs != NULL) EG_free(objs);
  if (stat == EGADS_SUCCESS) *topo = (INT8) object;
  return stat;
}


int
#ifdef WIN32
IG_MAKEFACE (INT8 *iobj, int *mtype, double *limits, INT8 *iface)
#else
ig_makeface_(INT8 *iobj, int *mtype, double *limits, INT8 *iface)
#endif
{
  int      stat;
  egObject *object, *face;

  *iface = 0;
  object = (egObject *) *iobj;
  stat   = EG_makeFace(object, *mtype, limits, &face);
  if (stat == EGADS_SUCCESS) *iface = (INT8) face;
  return stat;
}


int
#ifdef WIN32
IG_GETAREA (INT8 *iobj, double *limits, double *area)
#else
ig_getarea_(INT8 *iobj, double *limits, double *area)
#endif
{
  egObject *object;

  object = (egObject *) *iobj;
  return EG_getArea(object, limits, area);
}


int
#ifdef WIN32
IG_GETBODYTOPOS (INT8 *ibody, INT8 *source, int *oclass, int *ntopo, 
                 INT8 **topos)
#else
ig_getbodytopos_(INT8 *ibody, INT8 *source, int *oclass, int *ntopo, 
                 INT8 **topos)
#endif
{
  int      i, stat, nobj;
  INT8     *cobjs;
  egObject *object, *src, **objs;

  *ntopo = 0;
  *topos = NULL;
  object = (egObject *) *ibody;
  if (*source != 0) {
    src  = (egObject *) *source;
    stat = EG_getBodyTopos(object, src,  *oclass, &nobj, &objs);
  } else {
    stat = EG_getBodyTopos(object, NULL, *oclass, &nobj, &objs);
  }
  if (stat != EGADS_SUCCESS) return stat;
  if (nobj != 0) {
    *topos = cobjs = (INT8 *) EG_alloc(nobj*sizeof(INT8));
    if (cobjs == NULL) {
      EG_free(objs);
      return EGADS_MALLOC;
    }
    for (i = 0; i < nobj; i++) cobjs[i] = (INT8) objs[i];
    EG_free(objs);
  }
  *ntopo = nobj;
  return EGADS_SUCCESS;  
}


int
#ifdef WIN32
IG_MAKESOLIDBODY (INT8 *cntxt, int *stype, const double *data, INT8 *ibdy)
#else
ig_makesolidbody_(INT8 *cntxt, int *stype, const double *data, INT8 *ibdy)
#endif
{
  int      stat;
  egObject *object, *context;

  *ibdy = 0;
  context = (egObject *) *cntxt;
  stat = EG_makeSolidBody(context, *stype, data, &object);
  if (stat == EGADS_SUCCESS) *ibdy = (INT8) object;
  return stat;
}


int
#ifdef WIN32
IG_GETBOUNDINGBOX (INT8 *topo, double *box)
#else
ig_getboundingbox_(INT8 *topo, double *box)
#endif
{
  egObject *object;
  
  object = (egObject *) *topo;
  return EG_getBoundingBox(object, box);
}


int
#ifdef WIN32
IG_GETMASSPROPERTIES (INT8 *topo, double *props)
#else
ig_getmassproperties_(INT8 *topo, double *props)
#endif
{
  egObject *object;
  
  object = (egObject *) *topo;
  return EG_getMassProperties(object, props);
}


int
#ifdef WIN32
IG_ISEQUIVALENT (INT8 *itopo1, INT8 *itopo2)
#else
ig_isequivalent_(INT8 *itopo1, INT8 *itopo2)
#endif
{
  egObject *topo1, *topo2;
  
  topo1 = (egObject *) *itopo1;
  topo2 = (egObject *) *itopo2;
  return EG_isEquivalent(topo1, topo2);
}


int
#ifdef WIN32
IG_LOADMODEL (INT8 *cntxt, int *bflg, const char *name, INT8 *model,
              int nameLen)
#else
ig_loadmodel_(INT8 *cntxt, int *bflg, const char *name, INT8 *model, 
              int nameLen)
#endif
{
  int      stat;
  char     *fname;
  egObject *object, *context;

  *model  = 0;
  context = (egObject *) *cntxt;
  fname   = EG_f2c(name, nameLen);
  if (fname == NULL) return EGADS_NONAME;
  stat = EG_loadModel(context, *bflg, fname, &object);
  EG_free(fname);
  if (stat == EGADS_SUCCESS) *model = (INT8) object;
  return stat;
}


int
#ifdef WIN32
IG_SAVEMODEL (INT8 *model, const char *name, int nameLen)
#else
ig_savemodel_(INT8 *model, const char *name, int nameLen)
#endif
{
  int      stat;
  char     *fname;
  egObject *object;

  object = (egObject *) *model;
  fname  = EG_f2c(name, nameLen);
  if (fname == NULL) return EGADS_NONAME;
  stat = EG_saveModel(object, fname);
  EG_free(fname);
  return stat;
}


int
#ifdef WIN32
IG_GETEDGEUV (INT8 *iface, INT8 *itopo, int *sense, double *t, double *uv)
#else
ig_getedgeuv_(INT8 *iface, INT8 *itopo, int *sense, double *t, double *uv)
#endif
{
  egObject *face, *topo;
  
  face = (egObject *) *iface;
  topo = (egObject *) *itopo;
  
  return EG_getEdgeUV(face, topo, *sense, *t, uv);
}


int
#ifdef WIN32
IG_INTOPOLOGY (INT8 *itopo, const double *xyz)
#else
ig_intopology_(INT8 *itopo, const double *xyz)
#endif
{
  egObject *topo;
  
  topo = (egObject *) *itopo;
  return EG_inTopology(topo, xyz);
}


int
#ifdef WIN32
IG_INFACE (INT8 *iface, const double *uv)
#else
ig_inface_(INT8 *iface, const double *uv)
#endif
{
  egObject *face;
  
  face = (egObject *) *iface;
  return EG_inFace(face, uv);
}
