/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             FORTRAN Bindings for Memory Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdlib.h>
#include "egadsErrors.h"


  extern /*@null@*/ /*@out@*/ /*@only@*/ void *EG_alloc(int nbytes);
  extern /*@null@*/ /*@only@*/ void *EG_calloc(int nele, int size);
  extern /*@null@*/ /*@only@*/ void *EG_reall(/*@null@*/ /*@only@*/ /*@returned@*/ void *ptr, int nbytes);
  extern void EG_free(/*@null@*/ /*@only@*/ void *ptr);


int
#ifdef WIN32
IG_ALLOC (int *nbytes, void **ptr)
#else
ig_alloc_(int *nbytes, void **ptr)
#endif
{
  void *tptr;
  
  *ptr = NULL;
  tptr = EG_alloc(*nbytes);
  if (tptr == NULL) return EGADS_MALLOC;
  *ptr = tptr;
  return EGADS_SUCCESS;
}


int
#ifdef WIN32
IG_CALLOC (int *nele, int *size, void **ptr)
#else
ig_calloc_(int *nele, int *size, void **ptr)
#endif
{
  void *tptr;
  
  *ptr = NULL;
  tptr = EG_calloc(*nele, *size);
  if (tptr == NULL) return EGADS_MALLOC;
  *ptr = tptr;
  return EGADS_SUCCESS;
}


int
#ifdef WIN32
IG_REALL (void **ptr, int *nbytes)
#else
ig_reall_(void **ptr, int *nbytes)
#endif
{
  void *tptr;

  tptr = EG_reall(*ptr, *nbytes);
  if (tptr == NULL) return EGADS_MALLOC;
  *ptr = tptr;
  return EGADS_SUCCESS;
}


void
#ifdef WIN32
IG_FREE (void **ptr)
#else
ig_free_(void **ptr)
#endif
{
  EG_free(*ptr);
  *ptr = NULL;
}
