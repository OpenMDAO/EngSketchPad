/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Base Object Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "egadsTypes.h"
#include "egadsInternals.h"


#define ZERO            1.e-5           /* allow for float-like precision */
#define STRING(a)       #a
#define STR(a)          STRING(a)


  static char *EGADSprop[2] = {STR(EGADSPROP),
                               "\nEGADSprop: Copyright 2011-2012 MIT. All Rights Reserved."};


  extern int EG_destroyGeometry( egObject *geom );
  extern int EG_destroyTopology( egObject *topo );
  extern int EG_copyGeometry( const egObject *geom, /*@null@*/ double *xform,
                              egObject **copy );
  extern int EG_copyTopology( const egObject *topo, /*@null@*/ double *xform,
                              egObject **copy );
  extern int EG_flipGeometry( const egObject *geom, egObject **copy );
  extern int EG_flipTopology( const egObject *topo, egObject **copy );
  extern int EG_getTopology( const egObject *topo, egObject **geom, 
                             int *ocls, int *type, /*@null@*/ double *limits, 
                             int *nobjs, egObject ***objs, int **senses );



void
EG_revision(int *major, int *minor)
{
  *major = EGADSMAJOR;
  *minor = EGADSMINOR;
}


/*@kept@*/ /*@null@*/ egObject *
EG_context(const egObject *obj)
{
  egObject *object, *topObj;

  if (obj == NULL) {
    printf(" EGADS Internal: EG_context called with NULL!\n");
    return NULL;
  }
  if (obj->magicnumber != MAGIC) {
    printf(" EGADS Internal: EG_context Object NOT an ego!\n");
    return NULL;
  }
  if (obj->oclass == CONTXT) return (egObject *) obj;
  
  object = obj->topObj;
  if (object == NULL) {
    printf(" EGADS Internal: EG_context topObj is NULL!\n");
    return NULL;
  }
  if (object->magicnumber != MAGIC) {
    printf(" EGADS Internal: EG_context topObj NOT an ego!\n");
    return NULL;
  }
  if (object->oclass == CONTXT) return object;
  
  topObj = object->topObj;
  if (topObj == NULL) {
    printf(" EGADS Internal: EG_context contents of topObj is NULL!\n");
    return NULL;
  }
  if (topObj->magicnumber != MAGIC) {
    printf(" EGADS Internal: EG_context contents of topObj NOT an ego!\n");
    return NULL;
  }
  if (topObj->oclass == CONTXT) return topObj;

  printf(" EGADS Internal: EG_context contents of topObj NOT context!\n");
  return NULL;
}


int
EG_outLevel(const egObject *obj)
{
  egObject *context;
  egCntxt  *cntxt;

  if (obj == NULL)               return 0;
  if (obj->magicnumber != MAGIC) return 0;
  context = EG_context(obj);
  if (context == NULL)           return 0;
  
  cntxt = (egCntxt *) context->blind;
  return cntxt->outLevel;
}


int
EG_setOutLevel(egObject *context, int outLevel)
{
  int     old;
  egCntxt *cntx;

  if  (context == NULL)                 return EGADS_NULLOBJ;
  if  (context->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if  (context->oclass != CONTXT)       return EGADS_NOTCNTX;
  if ((outLevel < 0) || (outLevel > 3)) return EGADS_RANGERR;
  cntx = (egCntxt *) context->blind;
  if  (cntx == NULL)                    return EGADS_NODATA;
  old = cntx->outLevel;
  cntx->outLevel = outLevel;
  
  return old;
}


int
EG_makeObject(/*@null@*/ egObject *context, egObject **obj)
{
  int      outLevel;
  egObject *object, *prev;
  egCntxt  *cntx;

  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  cntx = (egCntxt *) context->blind;
  if (cntx == NULL)                  return EGADS_NODATA;
  outLevel = cntx->outLevel;

  /* any objects in the pool? */
  object = cntx->pool;
  if (object == NULL) {
    object = (egObject *) EG_alloc(sizeof(egObject));
    if (object == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Malloc on Object (EG_makeObject)!\n");
      return EGADS_MALLOC;
    }
  } else {
    cntx->pool   = object->next;
    object->prev = NULL;
  }
  
  prev                = cntx->last;
  object->magicnumber = MAGIC;
  object->oclass      = NIL;
  object->mtype       = 0;
  object->tref        = NULL;
  object->attrs       = NULL;
  object->blind       = NULL;
  object->topObj      = context;
  object->prev        = prev;
  object->next        = NULL;
  prev->next          = object;

  *obj = object;
  cntx->last = *obj;
  return EGADS_SUCCESS;
}


int
EG_open(egObject **context)
{
  egObject *object;
  egCntxt  *cntx;

  cntx   = (egCntxt *) EG_alloc(sizeof(egCntxt));
  if (cntx == NULL) return EGADS_MALLOC;
  object = (egObject *) EG_alloc(sizeof(egObject));
  if (object == NULL) {
    EG_free(cntx);
    return EGADS_MALLOC;
  }
  cntx->outLevel  = 1;
  cntx->signature = EGADSprop;
  cntx->pool      = NULL;
  cntx->last      = object;
  
  object->magicnumber = MAGIC;
  object->oclass      = CONTXT;
  object->mtype       = 0;
  object->tref        = NULL;
  object->attrs       = NULL;
  object->blind       = cntx;
  object->topObj      = NULL;
  object->prev        = NULL;
  object->next        = NULL;

  *context = object;
  return EGADS_SUCCESS;
}


int
EG_referenceObject(egObject *object, /*@null@*/ const egObject *ref)
{
  int      cnt, stat, outLevel;
  egObject *next, *last, *obj, *ocontext, *rcontext;
  
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  outLevel = EG_outLevel(object);

  if (ref == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Reference (EG_referenceObject)!\n");
    return EGADS_NULLOBJ;
  }
  if (ref->magicnumber != MAGIC) {
    if (outLevel > 0) 
      printf(" EGADS Error: Reference not an EGO (EG_referenceObject)!\n");
    return EGADS_NOTOBJ;
  }
  if ((ref->oclass == EMPTY) || (ref->oclass == NIL)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Reference is Empty (EG_refreneceObject)!\n");
    return EGADS_EMPTY;
  }
  ocontext = EG_context(object);
  rcontext = EG_context(ref);
  if (rcontext != ocontext) {
    if (outLevel > 0) 
      printf(" EGADS Error: Context mismatch (EG_referenceObject)!\n");
    return EGADS_MIXCNTX;
  }

  cnt = 1;
  obj = NULL;
  if (object->tref == NULL) {
    stat = EG_makeObject(ocontext, &obj);
    if (outLevel > 2) 
      printf(" 0 makeRef oclass %d for rclass %d = %d\n", 
             object->oclass, ref->oclass, stat);
    if (stat != EGADS_SUCCESS) return stat;
    if (obj != NULL) {
      obj->oclass  = REFERENCE;
      obj->attrs   = (void *) ref;
      object->tref = obj;
    }
  } else {
    next = object->tref;
    while (next != NULL) {
    if (outLevel > 2) {
      if (next->magicnumber != MAGIC)
        printf(" %d: Thread not an EGO!\n", cnt);
      if (next->oclass != REFERENCE)
        printf(" %d: Not a Reference - class = %d!\n", cnt, next->oclass);
      }
      obj = (egObject *) next->attrs;
      if (outLevel > 2) {
        if (obj == NULL) {
          printf(" %d: Reference is NULL!\n", cnt);
        } else {
          if (obj->magicnumber != MAGIC)
            printf(" %d: Reference not an EGO!\n", cnt);
          if ((obj->oclass == EMPTY) || (obj->oclass == NIL))
            printf(" %d: Reference is Empty!\n", cnt);
        }
      }
/*    single node edges do double reference!
      if (obj == ref) return EGADS_SUCCESS;  */
      last = next;
      next = (egObject *) last->blind;          /* next reference */
      cnt++;
    }
    stat = EG_makeObject(ocontext, &obj);
    if (outLevel > 2) 
      printf(" %d makeRef oclass %d for rclass %d = %d\n", 
             cnt, object->oclass, ref->oclass, stat);
    if (stat != EGADS_SUCCESS) return stat;
    if (obj != NULL) {
      obj->oclass = REFERENCE;
      obj->attrs  = (void *) ref;
      last->blind = obj;
    }
  }

  return cnt;
}


int
EG_referenceTopObj(egObject *object, /*@null@*/ const egObject *ref)
{
  egObject *context, *obj;
  
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  context = EG_context(object);
  obj     = object;
  if (object->topObj != context) obj = object->topObj;
  
  return EG_referenceObject(obj, ref);
}


static int
EG_derefObj(egObject *object, /*@null@*/ const egObject *refx, int flg)
{
  int      i, j, stat, outLevel;
  long     ptr1, ptr2;
  egObject *pobj, *nobj, *obj, *context;
  egCntxt  *cntx;
  egTessel *tess;
  const egObject *ref;

  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  context = EG_context(object);
  if (context == NULL)              return EGADS_NOTCNTX;
  cntx = (egCntxt *) context->blind;
  if (cntx == NULL)                 return EGADS_NODATA;
  outLevel = cntx->outLevel;
  ref      = refx;

  /* context is an attempt to delete */
  
  if ((ref == context) && (object->tref != NULL)) {
    nobj = object->tref;
    pobj = NULL;
    i = 0;
    while (nobj != NULL) {
      obj = (egObject *) nobj->attrs;
      if (obj != ref) i++;
      pobj = nobj;
      nobj = (egObject *) pobj->blind;          /* next reference */
    }
    if (object->topObj == context)
      if (i > 0) {
        if (outLevel > 0)
          printf(" EGADS Info: dereference with %d active objects!\n", i); 
        return i;
      }
  }
  if (ref == NULL) ref = context;
  
  /* we should never see a NULL reference! */
  if (object->tref != NULL) {
    nobj = object->tref;
    pobj = NULL;
    while (nobj != NULL) {
      obj = (egObject *) nobj->attrs;
      if (obj == ref) break;
      pobj = nobj;
      nobj = (egObject *) pobj->blind;          /* next reference */
    }
    if (nobj == NULL) {
      if (refx != NULL) {
        ptr1 = (long) object;
        ptr2 = (long) ref;
        printf(" EGADS Internal: Ref Not Found (EG_dereferenceObject)!\n");
        printf("                 Object %lx = %d/%d,  ref %lx = %d/%d\n",
               ptr1, object->oclass, object->mtype,
               ptr2, ref->oclass, ref->mtype);
      }
      return EGADS_NOTFOUND;
    }
    if (pobj == NULL) {
      object->tref = (egObject *) nobj->blind;
    } else {
      pobj->blind  = nobj->blind;
    }
    obj  = nobj;
    pobj = obj->prev;
    nobj = obj->next;
    if (nobj != NULL) {
      nobj->prev = pobj;
    } else {
      cntx->last = pobj;
    }
    pobj->next   = nobj;
    obj->mtype   = REFERENCE;
    obj->oclass  = EMPTY;
    obj->blind   = NULL;
    obj->topObj  = context;
    obj->prev    = NULL;
    obj->next    = cntx->pool;
    cntx->pool   = obj;  
  }
  if (object->tref != NULL) return EGADS_SUCCESS;

  stat = EG_attributeDel(object, NULL);
  if (stat != EGADS_SUCCESS)
    if (outLevel > 0)
      printf(" EGADS Warning: Del Attributes = %d (EG_destroyObject)!\n",
             stat);

  stat = EGADS_SUCCESS;
  if (object->oclass == TRANSFORM) {
  
    EG_free(object->blind);

  } else if (object->oclass == TESSELLATION) {

    tess = (egTessel *) object->blind;
    if (tess != NULL) {
      EG_dereferenceTopObj(tess->src, object);
      if (tess->xyzs != NULL) EG_free(tess->xyzs);
      if (tess->tess1d != NULL) {
        for (i = 0; i < tess->nEdge; i++) {
          if (tess->tess1d[i].faces[0].faces != NULL)
            EG_free(tess->tess1d[i].faces[0].faces);
          if (tess->tess1d[i].faces[1].faces != NULL)
            EG_free(tess->tess1d[i].faces[1].faces);
          if (tess->tess1d[i].faces[0].tric  != NULL)
            EG_free(tess->tess1d[i].faces[0].tric);
          if (tess->tess1d[i].faces[1].tric  != NULL)
            EG_free(tess->tess1d[i].faces[1].tric);
          if (tess->tess1d[i].xyz != NULL) 
            EG_free(tess->tess1d[i].xyz);
          if (tess->tess1d[i].t   != NULL) 
            EG_free(tess->tess1d[i].t);
        }
        EG_free(tess->tess1d);
      }
      if (tess->tess2d != NULL) {
        for (i = 0; i < 2*tess->nFace; i++) {
          if (tess->tess2d[i].xyz    != NULL) 
            EG_free(tess->tess2d[i].xyz);
          if (tess->tess2d[i].uv     != NULL) 
            EG_free(tess->tess2d[i].uv);
          if (tess->tess2d[i].ptype  != NULL) 
            EG_free(tess->tess2d[i].ptype);
          if (tess->tess2d[i].pindex != NULL) 
            EG_free(tess->tess2d[i].pindex);
          if (tess->tess2d[i].tris   != NULL) 
            EG_free(tess->tess2d[i].tris);
          if (tess->tess2d[i].tric   != NULL) 
            EG_free(tess->tess2d[i].tric);
          if (tess->tess2d[i].patch  != NULL) {
            for (j = 0; j < tess->tess2d[i].npatch; j++) {
              if (tess->tess2d[i].patch[j].ipts != NULL) 
                EG_free(tess->tess2d[i].patch[j].ipts);
              if (tess->tess2d[i].patch[j].bounds != NULL) 
                EG_free(tess->tess2d[i].patch[j].bounds);
            }
            EG_free(tess->tess2d[i].patch);
          }
        }
        EG_free(tess->tess2d);
      }
      EG_free(tess);
    }

  } else if (object->oclass <= SURFACE) {
  
    if ((object->oclass != NIL) && (flg == 0))
      stat = EG_destroyGeometry(object);
  
  } else {
  
    if (flg == 0) stat = EG_destroyTopology(object);

  }
  object->mtype  = object->oclass;
  object->oclass = EMPTY;
  object->blind  = NULL;
  
  /* patch up the lists & put the object in the pool */

  pobj = object->prev;          /* always have a previous -- context! */
  nobj = object->next;
  if (nobj == NULL) {
    if (object != cntx->last)
      printf(" EGADS Info: Context Last NOT Object Next w/ NULL!\n");
    cntx->last = pobj;
  } else {
    nobj->prev = pobj;
  }
  if (pobj == NULL) {
    printf(" EGADS Info: PrevObj is NULL (EG_destroyObject)!\n");
  } else {
    pobj->next = nobj;
  }
  object->prev = NULL;
  object->next = cntx->pool;
  cntx->pool   = object;

  return stat;
}


int
EG_dereferenceTopObj(egObject *object, /*@null@*/ const egObject *ref)
{
  egObject *context, *obj;
  
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  context = EG_context(object);
  obj     = object;
  if (object->topObj != context) obj = object->topObj;
  
  return EG_derefObj(obj, ref, 1);
}


int
EG_dereferenceObject(egObject *object, /*@null@*/ const egObject *ref)
{
  return EG_derefObj(object, ref, 0);
}


int
EG_deleteObject(egObject *object)
{
  int      outLevel, total, cnt, nref, stat, oclass, mtype, nbody;
  int      i, *senses;
  egObject *context, *obj, *next, *pobj, **bodies;
  egCntxt  *cntx;

  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  if (object->oclass != CONTXT) {
    /* normal dereference */
    context = EG_context(object);
    if (context == NULL)            return EGADS_NOTCNTX;
    outLevel = EG_outLevel(object);
    
    /* special check for body references in models */
    if (object->oclass == MODEL) {
      stat = EG_getTopology(object, &next, &oclass, &mtype, NULL, 
                            &nbody, &bodies, &senses);
      if (stat != EGADS_SUCCESS) return stat;
      for (cnt = i = 0; i < nbody; i++) {
        if (bodies[i]->tref == NULL) continue;
        next = bodies[i]->tref;
        pobj = NULL;
        while (next != NULL) {
          obj = (egObject *) next->attrs;
          if (obj != object) cnt++;
          pobj = next;
          next = (egObject *) pobj->blind;      /* next reference */
        }
      }
      if (cnt > 0) {
        if (outLevel > 0)
          printf(" EGADS Info: Model delete w/ %d active Body Refs!\n", 
                 cnt); 
        return cnt;
      }
    }
  
    return EG_dereferenceObject(object, context);
  }
  
  /* delete all non-body attached topology and geometry */
 
  context  = object; 
  cntx     = (egCntxt *) context->blind;
  if (cntx == NULL) return EGADS_NODATA;
  outLevel = cntx->outLevel;
  
  nref = 0;
  obj  = context->next;
  while (obj != NULL) {
    next = obj->next;
    if (obj->oclass == REFERENCE) nref++;
    obj  = next;
  }

  cntx->outLevel = total = 0;
  do {
    cnt = 0;
    obj = context->next;
    while (obj != NULL) {
      next = obj->next;
      if ((obj->oclass >= PCURVE) && (obj->oclass <= SHELL) &&
          (obj->topObj == context)) {
        stat = EG_dereferenceObject(obj, context);
        if (stat == EGADS_SUCCESS) {
          cnt++;
          break;
        }
      }
      obj = next;
    }
    total += cnt;
  } while (cnt != 0);
  cntx->outLevel = outLevel;
  
  cnt = 0;
  obj = context->next;
  while (obj != NULL) {
    next = obj->next;
    if (obj->oclass == REFERENCE) cnt++;
    obj  = next;
  }
  
  if ((outLevel > 0) && (total != 0))
    printf(" EGADS Info: %d unattached Objects (%d References) removed!\n",
           total, nref-cnt);

  return EGADS_SUCCESS;
}


int
EG_removeCntxtRef(egObject *object)
{
  egObject *context, *nobj, *pobj, *obj;
  egCntxt  *cntx;
  
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  if (object->tref   == NULL)       return EGADS_SUCCESS;
  context = EG_context(object);
  if (context == NULL)              return EGADS_NULLOBJ;
  cntx    = (egCntxt *) context->blind;
  if (cntx == NULL)                 return EGADS_NODATA;

  nobj = object->tref;
  pobj = NULL;
  while (nobj != NULL) {
    obj = (egObject *) nobj->attrs;
    if (obj == context) break;
    pobj = nobj;
    nobj = (egObject *) pobj->blind;          /* next reference */
  }
  if (nobj == NULL) return EGADS_NOTFOUND;
  if (pobj == NULL) {
    object->tref = (egObject *) nobj->blind;
  } else {
    pobj->blind  = nobj->blind;
  }
  obj  = nobj;
  pobj = obj->prev;
  nobj = obj->next;
  if (nobj != NULL) {
    nobj->prev = pobj;
  } else {
    cntx->last = pobj;
  }
  pobj->next   = nobj;
  obj->mtype   = REFERENCE;
  obj->oclass  = EMPTY;
  obj->blind   = NULL;
  obj->topObj  = context;
  obj->prev    = NULL;
  obj->next    = cntx->pool;
  /*@ignore@*/ 
  cntx->pool   = obj;
  /*@end@*/

  return EGADS_SUCCESS;
}


int
EG_makeTransform(egObject *context, const double *xform, egObject **oform)
{
  int      i, stat, outLevel;
  double   *reals, dotXY, dotXZ, dotYZ, dotXX, dotYY, dotZZ;
  egObject *object;

  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  if (xform == NULL)                 return EGADS_NODATA;
  outLevel = EG_outLevel(context);

  /* check for "scaled" orthonormality */

  dotXX = xform[0]*xform[0] + xform[1]*xform[1] + xform[ 2]*xform[ 2];
  dotXY = xform[0]*xform[4] + xform[1]*xform[5] + xform[ 2]*xform[ 6];
  dotXZ = xform[0]*xform[8] + xform[1]*xform[9] + xform[ 2]*xform[10];

  dotYY = xform[4]*xform[4] + xform[5]*xform[5] + xform[ 6]*xform[ 6];
  dotYZ = xform[4]*xform[8] + xform[5]*xform[9] + xform[ 6]*xform[10];

  dotZZ = xform[8]*xform[8] + xform[9]*xform[9] + xform[10]*xform[10];

  if (sqrt(dotXX) < ZERO) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Length on Transform  (EG_makeTransform)!\n");
    return EGADS_DEGEN;
  }
  if ((fabs((dotXX-dotYY)/dotXX) > ZERO) ||
      (fabs((dotXX-dotZZ)/dotXX) > ZERO)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Skew Scaling in Transform  (EG_makeTransform)!\n");
    return EGADS_BADSCALE;
  }
  if ((fabs(dotXY/dotXX) > ZERO) || (fabs(dotXZ/dotXX) > ZERO) ||
      (fabs(dotYZ/dotXX) > ZERO)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Transform not Orthogonal (EG_makeTransform)!\n");
    return EGADS_NOTORTHO;
  }

  reals = (double *) EG_alloc(12*sizeof(double));
  if (reals == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Malloc on transform (EG_makeTransform)!\n");
    return EGADS_MALLOC;
  }
  stat = EG_makeObject(context, &object);
  if (stat != EGADS_SUCCESS) {
    EG_free(reals);
    return stat;
  }
  
  object->oclass = TRANSFORM;
  object->blind  = reals;
  for (i = 0; i < 12; i++) reals[i] = xform[i];
  
  *oform = object;

  return EGADS_SUCCESS;
}


int
EG_getTransformation(const egObject *oform, double *xform)
{
  int    i;
  double *reals;

  for (i = 1; i < 12; i++) xform[i] = 0.0;
  xform[0] = xform[5] = xform[10]   = 1.0;
  if (oform == NULL)               return EGADS_NULLOBJ;
  if (oform->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (oform->oclass != TRANSFORM)  return EGADS_NOTXFORM;
  reals = (double *) oform->blind;
  if (reals == NULL)               return EGADS_NOTFOUND;
  
  for (i = 0; i < 12; i++) xform[i] = reals[i];
  
  return EGADS_SUCCESS;
}


int
EG_getContext(egObject *object, egObject **context)
{
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  
  *context = EG_context(object);
  return EGADS_SUCCESS;
}


int
EG_getInfo(const egObject *object, int *oclass, int *mtype, egObject **top,
           egObject **prev, egObject **next)
{
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;

  *oclass = object->oclass;
  *mtype  = object->mtype;
  *top    = object->topObj;
  *prev   = object->prev;
  *next   = object->next;

  return EGADS_SUCCESS;
}


int
EG_copyObject(const egObject *object, /*@null@*/ const egObject *oform,
              egObject **copy)
{
  int      stat, outLevel;
  double   *xform = NULL;
  egObject *ocontext, *xcontext, *obj   = NULL;

  *copy = NULL;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == TRANSFORM)  return EGADS_NOTXFORM;
  outLevel = EG_outLevel(object);

  if (oform != NULL) {
    if (oform->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: 2nd argument not an EGO (EG_copyObject)!\n");
      return EGADS_NOTOBJ;
    }
    if (oform->oclass != TRANSFORM) {
      if (outLevel > 0) 
        printf(" EGADS Error: 2nd argument not an XForm (EG_copyObject)!\n");
      return EGADS_NOTXFORM;
    }
    ocontext = EG_context(object);
    xcontext = EG_context(oform);
    if (xcontext != ocontext) {
      if (outLevel > 0) 
        printf(" EGADS Error: Context mismatch (EG_copyObject)!\n");
      return EGADS_MIXCNTX;
    }
    xform = (double *) oform->blind;
  }
  
  stat = EGADS_SUCCESS;
  if (object->oclass == TESSELLATION) {
  
    /* what do we do here? */
    stat = EGADS_NOTTESS;
    
  } else if (object->oclass == PCURVE) {
  
    /* this does not make sense -- 2D! */
    if (outLevel > 0) 
      printf(" EGADS Error: PCurve is 2D (EG_copyObject)!\n");
    stat = EGADS_CONSTERR;

  } else if (object->oclass <= SURFACE) {
  
    stat = EG_copyGeometry(object, xform, &obj);
  
  } else {
  
    stat = EG_copyTopology(object, xform, &obj);

  }

  if (obj != NULL) {
    stat  = EG_attributeDup(object, obj);
    *copy = obj;
  }
  return stat;
}


int
EG_flipObject(const egObject *object, egObject **copy)
{
  int      stat;
  egObject *obj = NULL;

  *copy = NULL;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass == EMPTY)      return EGADS_EMPTY;
  if (object->oclass == NIL)        return EGADS_EMPTY;
  if (object->oclass == REFERENCE)  return EGADS_REFERCE;
  if (object->oclass == CONTXT)     return EGADS_NOTCNTX;
  if (object->oclass == TRANSFORM)  return EGADS_NOTXFORM;
  
  stat = EGADS_SUCCESS;
  if (object->oclass == TESSELLATION) {
  
    /* what do we do here? */
    stat = EGADS_NOTTESS;
    
  } else if (object->oclass <= SURFACE) {
  
    stat = EG_flipGeometry(object, &obj);
  
  } else {
  
    stat = EG_flipTopology(object, &obj);

  }

  if (obj != NULL) {
    stat  = EG_attributeDup(object, obj);
    *copy = obj;
  }
  return stat;
}


int
EG_close(egObject *context)
{
  int      outLevel, cnt, ref, total, stat;
  egObject *obj, *next, *last;
  egCntxt  *cntx;

  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  cntx = (egCntxt *) context->blind;
  if (cntx == NULL)                  return EGADS_NODATA;
  outLevel = cntx->outLevel;

  /* count all active objects */
  
  cnt = ref = 0;
  obj = context->next;
  while (obj != NULL) {
    if (obj->magicnumber != MAGIC) {
      printf(" EGADS Info: Found BAD Object in cleanup (EG_close)!\n");
      printf("             Class = %d\n", obj->oclass);
      return EGADS_NOTFOUND;
    }
    if (obj->oclass == REFERENCE) {
      ref++;
    } else {
      if (outLevel > 2)
        printf(" EGADS Info: Object oclass = %d, mtype = %d Found!\n",
               obj->oclass, obj->mtype);
      cnt++;
    }
    obj = obj->next;
  }
  total = ref+cnt;
  obj   = cntx->pool;
  while (obj != NULL) {
    next = obj->next;
    obj = next;
    total++;
  }
  if (outLevel > 0)
    printf(" EGADS Info: %d Objects, %d Reference in Use (of %d) at Close!\n",
           cnt, ref, total);

  /* delete unattached geometry and topology objects */
  
  EG_deleteObject(context);
  
  /* delete tessellation objects */

  obj  = context->next;
  last = NULL;
  while (obj != NULL) {
    next = obj->next;
    if (obj->oclass == TESSELLATION)
      if (EG_deleteObject(obj) == EGADS_SUCCESS) {
        obj = last;
        if (obj == NULL) {
          next = context->next;
        } else {
          next = obj->next;
        }
      }
    last = obj;
    obj  = next;
  }

  /* delete all models */

  obj  = context->next;
  last = NULL;
  while (obj != NULL) {
    next = obj->next;
    if (obj->oclass == MODEL)
      if (EG_deleteObject(obj) == EGADS_SUCCESS) {
        obj = last;
        if (obj == NULL) {
          next = context->next;
        } else {
          next = obj->next;
        }
      }
    last = obj;
    obj  = next;
  }
  
  /* delete all bodies that are left */
  
  obj  = context->next;
  last = NULL;
  while (obj != NULL) {
    next = obj->next;
    if (obj->oclass == BODY)
      if (EG_deleteObject(obj) == EGADS_SUCCESS) {
        obj = last;
        if (obj == NULL) {
          next = context->next;
        } else {
          next = obj->next;
        }
      }
    last = obj;
    obj  = next;
  }
  
  /* dereference until nothing is left (which should be the case) */

  do {
    cnt = 0;
    obj = context->next;
    while (obj != NULL) {
      next = obj->next;
      if (obj->oclass != REFERENCE) {
        stat = EG_dereferenceTopObj(obj, NULL);
        if (stat == EGADS_SUCCESS) {
          cnt++;
          break;
        }
      }
      obj = next;
    }
    if (cnt != 0) continue;
    obj = context->next;
    while (obj != NULL) {
      next = obj->next;
      if (obj->oclass != REFERENCE) {
        stat = EG_dereferenceObject(obj, NULL);
        if (stat == EGADS_SUCCESS) {
          cnt++;
          break;
        }
      }
      obj = next;
    }    
  } while (cnt != 0);

  ref = cnt = 0;
  obj = context->next;
  while (obj != NULL) {
    if (cnt == 0)
      if (outLevel > 1)
        printf(" EGADS Info: Undeleted Object(s) in cleanup (EG_close):\n");
    if (obj->oclass == REFERENCE) {
      ref++;
    } else {
      if (outLevel > 1)
        printf("             %d: Class = %d, Type = %d\n", 
               cnt, obj->oclass, obj->mtype);
    }
    obj = obj->next;
    cnt++;
  }
  if (outLevel > 1)
    if ((cnt != 0) && (ref != 0))
      printf("             In Addition to %d Refereces\n", ref);  

  /* clean up the pool */
  
  obj = cntx->pool;
  if (obj == NULL) return EGADS_SUCCESS;
  if (obj->magicnumber != MAGIC) {
    printf(" EGADS Info: Found bad Object in Cleanup (EG_close)!\n");
    printf("             Class = %d\n", obj->oclass);
    return EGADS_SUCCESS;
  }
  while (obj != NULL) {
    if (obj->magicnumber != MAGIC) {
      printf(" EGADS Info: Found BAD Object in Cleanup (EG_close)!\n");
      printf("             Class = %d\n", obj->oclass);
      break;
    }
    next = obj->next;
    EG_free(obj);
    obj = next;
  }
  EG_attributeDel(context, NULL);
  EG_free(context);
  EG_free(cntx);
    
  return EGADS_SUCCESS;
}
