/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             FORTRAN Bindings for Base Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INT8 unsigned long long

#include "egadsTypes.h"
#include "egadsInternals.h"


  extern void EG_revision(int *major, int *minor);
  extern int  EG_open(egObject **context);
  extern int  EG_deleteObject(egObject *object);
  extern int  EG_makeTransform(egObject *context, const double *xform, 
                               egObject **oform);
  extern int  EG_getTransformation(const egObject *oform, double *xform);
  extern int  EG_setOutLevel(egObject *context, int outLevel);
  extern int  EG_getContext(egObject *object, egObject **context);
  extern int  EG_getInfo(const egObject *object, int *oclass, int *mtype, 
                         egObject **top, egObject **prev, egObject **next);
  extern int  EG_copyObject(const egObject *object, const egObject *oform, 
                            egObject **copy);
  extern int  EG_flipObject(const egObject *object, egObject **copy);
  extern int  EG_close(egObject *context);



void EG_c2f(/*@null@*/ const char *string, char *name, int nameLen)
{
  int  i, len;

  for (i = 0; i < nameLen; i++) name[i] = ' ';
  if (string == NULL) return;
  len = strlen(string);
  if (len > nameLen) len = nameLen;
  for (i = 0; i < len; i++) name[i] = string[i];
}


/*@null@*/ char *EG_f2c(const char *name, int nameLen)
{
  char *string;
  int  i, len;

  for (len = nameLen; len >= 1; len--) if (name[len-1] != ' ') break;
  string = (char *) EG_alloc((len+1)*sizeof(char));
  if (string != NULL) {
    for (i = 0; i < len; i++) string[i] = name[i];
    string[len] = 0;
  }
  return string;
}


void
#ifdef WIN32
IG_REVISION (int *major, int *minor)
#else
ig_revision_(int *major, int *minor)
#endif
{
  EG_revision(major, minor);
}


int
#ifdef WIN32
IG_OPEN (INT8 *cntxt)
#else
ig_open_(INT8 *cntxt)
#endif
{
  int      stat;
  egObject *context;

  *cntxt = 0;
  stat   = EG_open(&context);
  if (stat == EGADS_SUCCESS) *cntxt = (INT8) context;
  return stat;
}


int
#ifdef WIN32
IG_DELETEOBJECT (INT8 *obj)
#else
ig_deleteobject_(INT8 *obj)
#endif
{
  egObject *object;

  object = (egObject *) *obj;
  return EG_deleteObject(object);
}


int
#ifdef WIN32
IG_MAKETRANSFORM (INT8 *cntxt, double *xform, INT8 *ofrm)
#else
ig_maketransform_(INT8 *cntxt, double *xform, INT8 *ofrm)
#endif
{
  int      stat;
  egObject *context, *oform;

  *ofrm   = 0;
  context = (egObject *) *cntxt;
  stat    = EG_makeTransform(context, xform, &oform);
  if (stat == EGADS_SUCCESS) *ofrm = (INT8) oform;
  return stat;
}


int
#ifdef WIN32
IG_GETTRANSFORM (INT8 *ofrm, double *xform)
#else
ig_gettransform_(INT8 *ofrm, double *xform)
#endif
{
  egObject *oform;

  oform = (egObject *) *ofrm;
  return EG_getTransformation(oform, xform);
}


int
#ifdef WIN32
IG_SETOUTLEVEL (INT8 *cntxt, int *out)
#else
ig_setoutlevel_(INT8 *cntxt, int *out)
#endif
{
  egObject *context;

  context = (egObject *) *cntxt;
  return EG_setOutLevel(context, *out);
}


int
#ifdef WIN32
IG_GETCONTEXT (INT8 *obj, INT8 *cntxt)
#else
ig_getcontext_(INT8 *obj, INT8 *cntxt)
#endif
{
  int      stat;
  egObject *object, *context;
  
  *cntxt = 0;
  object = (egObject *) *obj;
  stat   = EG_getContext(object, &context);
  if (stat == EGADS_SUCCESS) *cntxt = (INT8) context;
  return stat;
}


int
#ifdef WIN32
IG_GETINFO (INT8 *obj, int *oclass, int *mtype, INT8 *top, INT8 *prv, INT8 *nxt)
#else
ig_getinfo_(INT8 *obj, int *oclass, int *mtype, INT8 *top, INT8 *prv, INT8 *nxt)
#endif
{
  int      stat;
  egObject *object, *topObj, *prev, *next;
  
  *prv   = *nxt = 0;
  object = (egObject *) *obj;
  stat   = EG_getInfo(object, oclass, mtype, &topObj, &prev, &next);
  if (stat == EGADS_SUCCESS) {
    *top = (INT8) topObj;
    *prv = (INT8) prev;
    *nxt = (INT8) next;
  }
  return stat;
}


int
#ifdef WIN32
IG_COPYOBJECT (INT8 *obj, INT8 *ofrm, INT8 *cp)
#else
ig_copyobject_(INT8 *obj, INT8 *ofrm, INT8 *cp)
#endif
{
  int      stat;
  egObject *object, *oform, *copy;

  *cp    = 0;
  object = (egObject *) *obj;
  oform  = (egObject *) *ofrm;
  stat   = EG_copyObject(object, oform, &copy);
  if (stat == EGADS_SUCCESS) *cp = (INT8) copy;
  return stat;
}


int
#ifdef WIN32
IG_FLIPOBJECT (INT8 *obj, INT8 *cp)
#else
ig_flipobject_(INT8 *obj, INT8 *cp)
#endif
{
  int      stat;
  egObject *object, *copy;

  *cp    = 0;
  object = (egObject *) *obj;
  stat   = EG_flipObject(object, &copy);
  if (stat == EGADS_SUCCESS) *cp = (INT8) copy;
  return stat;
}


int
#ifdef WIN32
IG_CLOSE (INT8 *obj)
#else
ig_close_(INT8 *obj)
#endif
{
  egObject *object;

  object = (egObject *) *obj;
  return EG_close(object);
}
