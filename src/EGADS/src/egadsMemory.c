/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Memory Handling Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "egadsTypes.h"


/*@null@*/ /*@out@*/ /*@only@*/ void *
EG_alloc(int nbytes)
{
  if (nbytes <= 0) return NULL;
  return malloc(nbytes);
}


/*@null@*/ /*@only@*/ void *
EG_calloc(int nele, int size)
{
  if (nele*size <= 0) return NULL;
  return calloc(nele, size);
}


/*@null@*/ /*@only@*/ void *
EG_reall(/*@null@*/ /*@only@*/ /*@returned@*/ void *ptr, int nbytes)
{
  if (nbytes <= 0) return NULL;
  return realloc(ptr, nbytes);
}


void
EG_free(/*@null@*/ /*@only@*/ void *ptr)
{
  if (ptr == NULL) return;
  free(ptr);
}


/*@null@*/ /*@only@*/ char *
EG_strdup(/*@null@*/ const char *str)
{
  int  i, len;
  char *dup;

  if (str == NULL) return NULL;

  len = strlen(str) + 1;
  dup = (char *) EG_alloc(len*sizeof(char));
  if (dup != NULL)
    for (i = 0; i < len; i++) dup[i] = str[i];

  return dup;
}

