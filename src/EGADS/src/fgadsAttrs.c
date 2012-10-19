/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             FORTRAN Bindings for Attributes
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
  extern void EG_c2f(/*@null@*/ const char *string, char *name, int nameLen);

  extern int EG_attributeAdd(egObject *obj, const char *name, int atype, 
                             int len, /*@null@*/ const int *ints, 
                             /*@null@*/ const double *reals,
                             /*@null@*/ const char *str);
  extern int EG_attributeNum(const egObject *obj, int *num);
  extern int EG_attributeGet(const egObject *obj, int index, const char **name, 
                             int *atype, int *len, /*@null@*/ const int **ints, 
                             /*@null@*/ const double **reals,
                             /*@null@*/ const char **str);
  extern int EG_attributeRet(const egObject *obj, const char *name, int *atype, 
                             int *len, /*@null@*/ const int **ints, 
                                       /*@null@*/ const double **reals,
                                       /*@null@*/ const char **str);


int
#ifdef WIN32
IG_ATTRIBUTEADD (INT8 *obj, const char *name, int *atype, int *len, int *ints,
                 double *reals, char *str, int nameLen, int strLen)
#else
ig_attributeadd_(INT8 *obj, const char *name, int *atype, int *len, int *ints,
                 double *reals, char *str, int nameLen, int strLen)
#endif
{
  int      stat;
  char     *fname, *fstr;
  egObject *object;
  
  object = (egObject *) *obj;
  fname  = EG_f2c(name, nameLen);
  if (fname == NULL) return EGADS_NONAME;
  fstr   = EG_f2c(str,  strLen);
  stat   = EG_attributeAdd(object, fname, *atype, *len, ints, reals, fstr);
  EG_free(fstr);
  EG_free(fname);
  return stat;
}


int
#ifdef WIN32
IG_ATTRIBUTEDEL (INT8 *obj, const char *name, int nameLen)
#else
ig_attributedel_(INT8 *obj, const char *name, int nameLen)
#endif
{
  int      stat;
  char     *fname;
  egObject *object;
  
  object = (egObject *) *obj;
  fname  = EG_f2c(name, nameLen);
  stat   = EG_attributeDel(object, fname);
  EG_free(fname);
  return stat;
}


int
#ifdef WIN32
IG_ATTRIBUTENUM (INT8 *obj, int *num)
#else
ig_attributeNUM_(INT8 *obj, int *num)
#endif
{
  egObject *object;
  
  object = (egObject *) *obj;
  return EG_attributeNum(object, num);
}


int
#ifdef WIN32
IG_ATTRIBUTEGET (INT8 *obj, int *ind, char *name, int *atype, int *len,
                 const int **ints, const double **reals, char *str,
                 int nameLen, int strLen)
#else
ig_attributeget_(INT8 *obj, int *ind, char *name, int *atype, int *len, 
                 const int **ints, const double **reals, char *str, 
                 int nameLen, int strLen)
#endif
{
  int        stat;
  const char *fname, *fstr;
  egObject   *object;

  *ints  = NULL;
  *reals = NULL;  
  object = (egObject *) *obj;
  stat   = EG_attributeGet(object, *ind, &fname, atype, len, 
                           ints, reals, &fstr);
  EG_c2f(fname, name, nameLen);
  EG_c2f(fstr,  str,  strLen);
  return stat;
}


int
#ifdef WIN32
IG_ATTRIBUTERET (INT8 *obj, char *name, int *atype, int *len,
                 const int **ints, const double **reals, char *str,
                 int nameLen, int strLen)
#else
ig_attributeret_(INT8 *obj, char *name, int *atype, int *len, 
                 const int **ints, const double **reals, char *str, 
                 int nameLen, int strLen)
#endif
{
  int        stat;
  char       *fname;
  const char *fstr;
  egObject   *object;

  *ints  = NULL;
  *reals = NULL;  
  object = (egObject *) *obj;
  fname  = EG_f2c(name, nameLen);
  if (fname == NULL) return EGADS_NONAME;
  stat   = EG_attributeRet(object, fname, atype, len, 
                           ints, reals, &fstr);
  EG_c2f(fstr, str, strLen);
  EG_free(fname);
  return stat;
}


int
#ifdef WIN32
IG_ATTRIBUTEDUP (INT8 *src, INT8 *dst)
#else
ig_attributeDUP_(INT8 *src, INT8 *dst)
#endif
{
  egObject *source, *dest;
  
  source = (egObject *) *src;
  dest   = (egObject *) *dst;
  return EG_attributeDup(source, dest);
}
