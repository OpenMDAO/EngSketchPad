/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Attribute Functions
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
#include "egadsInternals.h"



int
EG_attributePrint(const egObject *obj)
{
  int     i, j;
  egAttrs *attrs;

  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;

  attrs = (egAttrs *) obj->attrs;
  if (attrs == NULL) return EGADS_SUCCESS;
  
  printf("\n Attributes:\n");
  for (i = 0; i < attrs->nattrs; i++) {
    printf("    %s: ", attrs->attrs[i].name);
    if (attrs->attrs[i].type == ATTRINT) {
      if (attrs->attrs[i].length <= 1) {
        printf("%d\n", attrs->attrs[i].vals.integer);
      } else {
        for (j = 0; j < attrs->attrs[i].length; j++)
          printf("%d ", attrs->attrs[i].vals.integers[j]);
        printf("\n");
      }
    } else if (attrs->attrs[i].type == ATTRREAL) {
      if (attrs->attrs[i].length <= 1) {
        printf("%lf\n", attrs->attrs[i].vals.real);
      } else {
        for (j = 0; j < attrs->attrs[i].length; j++)
          printf("%lf ", attrs->attrs[i].vals.reals[j]);
        printf("\n");
      }
    } else {
      printf("%s\n", attrs->attrs[i].vals.string);
    }
  }

  return EGADS_SUCCESS;
}


int
EG_attributeAdd(egObject *obj, const char *name, int atype, int len,
                /*@null@*/ const int  *ints, /*@null@*/ const double *reals,
                /*@null@*/ const char *str)
{
  int     i, length, outLevel, find = -1;
  egAttr  *attr;
  egAttrs *attrs;

  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;
  outLevel = EG_outLevel(obj);

  if (name == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Name (EG_attributeAdd)!\n");
    return EGADS_NONAME;
  }
  length = strlen(name);
  for (i = 0; i < length; i++)
    if (name[i] <= ' ') {
      length = 0;
      break;
    }
  if (length == 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: BAD Name (EG_attributeAdd)!\n");
    return EGADS_INDEXERR;
  }
  if ((atype != ATTRINT) && (atype != ATTRREAL) && (atype != ATTRSTRING)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Bad Attr Type (%d) for %s (EG_attributeAdd)!\n",
             atype, name);
    return EGADS_INDEXERR;
  }
  if (((atype == ATTRINT)    && (ints  == NULL)) ||
      ((atype == ATTRREAL)   && (reals == NULL)) ||
      ((atype == ATTRSTRING) && (str   == NULL))) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL data for %s  type = %d (EG_attributeAdd)!\n",
             name, atype);
    return EGADS_NODATA;
  }
  if ((len <= 0) && (atype != ATTRSTRING)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Bad Attr Length (%d) for %s (EG_attributeAdd)!\n",
             len, name);
    return EGADS_INDEXERR;
  }
  attrs = (egAttrs *) obj->attrs;

  if (attrs != NULL)
    for (i = 0; i < attrs->nattrs; i++)
      if (strcmp(attrs->attrs[i].name,name) == 0) {
        find = i;
        break;
      }

  if ((find != -1) && (attrs != NULL)) {

    /* an existing attribute -- reset the values */

    if (attrs->attrs[find].type == ATTRINT) {
      if (attrs->attrs[find].length != 1) 
        EG_free(attrs->attrs[find].vals.integers);
    } else if (attrs->attrs[find].type == ATTRREAL) {
      if (attrs->attrs[find].length != 1) 
        EG_free(attrs->attrs[find].vals.reals);
    } else {
      EG_free(attrs->attrs[find].vals.string);
    }

  } else {

    if (attrs == NULL) {
      attrs = (egAttrs *) EG_alloc(sizeof(egAttrs));
      if (attrs == NULL) {
        if (outLevel > 0) 
          printf(" EGADS Error: Attrs MALLOC for %s (EG_attributeAdd)!\n",
                 name);
        return EGADS_MALLOC;
      }
      attrs->nattrs = 0;
      attrs->attrs  = NULL;
      obj->attrs    = attrs;
    }
    if (attrs->attrs == NULL) {
      attr = (egAttr *) EG_alloc((attrs->nattrs+1)*sizeof(egAttr));
    } else {
      attr = (egAttr *) EG_reall(attrs->attrs, 
                                 (attrs->nattrs+1)*sizeof(egAttr));
    }
    if (attr == NULL) {
        if (outLevel > 0) 
          printf(" EGADS Error: Attr MALLOC for %s (EG_attributeAdd)!\n",
                 name);
      return EGADS_MALLOC;
    }
    attrs->attrs = attr;
    find = attrs->nattrs;
    attrs->attrs[find].vals.string = NULL;
    attrs->attrs[find].name        = EG_strdup(name);
    if (attrs->attrs[find].name == NULL) return EGADS_MALLOC;
    attrs->nattrs += 1;
  }

  attrs->attrs[find].type   = atype;
  attrs->attrs[find].length = len;
  if (atype == ATTRINT) {
    if (ints != NULL)
      if (len == 1) {
        attrs->attrs[find].vals.integer = *ints;
      } else {
        attrs->attrs[find].vals.integers = (int *) EG_alloc(len*sizeof(int));
        if (attrs->attrs[find].vals.integers == NULL) {
          attrs->attrs[find].length = 0;
        } else {
          for (i = 0; i < len; i++)
            attrs->attrs[find].vals.integers[i] = ints[i];
        }
      }
  } else if (atype == ATTRREAL) {
    if (reals != NULL)
      if (len == 1) {
        attrs->attrs[find].vals.real = *reals;
      } else {
        attrs->attrs[find].vals.reals = (double *) EG_alloc(len*sizeof(double));
        if (attrs->attrs[find].vals.reals == NULL) {
          attrs->attrs[find].length = 0;
        } else {
          for (i = 0; i < len; i++)
            attrs->attrs[find].vals.reals[i] = reals[i];
        }
      }
  } else {
    attrs->attrs[find].length = 0;
    attrs->attrs[find].vals.string = EG_strdup(str);
    if (attrs->attrs[find].vals.string != NULL) 
      attrs->attrs[find].length = strlen(attrs->attrs[find].vals.string);
  }
  return EGADS_SUCCESS;
}


int
EG_attributeDel(egObject *obj, /*@null@*/ const char *name)
{
  int     i, outLevel, find = -1;
  egAttrs *attrs;

  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;
  outLevel = EG_outLevel(obj);

  attrs = (egAttrs *) obj->attrs;
  if (attrs == NULL) return EGADS_SUCCESS;

  if (name == NULL) {

    /* delete all attributes associated with the object */
    obj->attrs = NULL;
    for (i = 0; i < attrs->nattrs; i++) {
      EG_free(attrs->attrs[i].name);
      if (attrs->attrs[i].type == ATTRINT) {
        if (attrs->attrs[i].length > 1) EG_free(attrs->attrs[i].vals.integers);
      } else if (attrs->attrs[i].type == ATTRREAL) {
        if (attrs->attrs[i].length > 1) EG_free(attrs->attrs[i].vals.reals);
      } else {
        EG_free(attrs->attrs[i].vals.string);
      }
    }
    EG_free(attrs->attrs);
    EG_free(attrs);

  } else {

    /* delete the named attribute */
    for (i = 0; i < attrs->nattrs; i++)
      if (strcmp(attrs->attrs[i].name,name) == 0) {
        find = i;
        break;
      }

    if (find == -1) {
      if (outLevel > 0) 
        printf(" EGADS Error: No Attribute -> %s (EG_attributeDel)!\n",
               name);
      return EGADS_NOTFOUND;
    }
    EG_free(attrs->attrs[find].name);
    if (attrs->attrs[find].type == ATTRINT) {
      if (attrs->attrs[find].length > 1) 
        EG_free(attrs->attrs[find].vals.integers);
    } else if (attrs->attrs[find].type == ATTRREAL) {
      if (attrs->attrs[find].length > 1) 
        EG_free(attrs->attrs[find].vals.reals);
    } else {
      EG_free(attrs->attrs[find].vals.string);
    }
    for (i = find+1; i < attrs->nattrs; i++)
      attrs->attrs[i-1] = attrs->attrs[i];
    attrs->nattrs -= 1;

  }

  return EGADS_SUCCESS;
}


int
EG_attributeNum(const egObject *obj, int *num)
{
  egAttrs *attrs;

  *num = 0;
  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;

  attrs = (egAttrs *) obj->attrs;
  if (attrs == NULL) return EGADS_SUCCESS;

  *num = attrs->nattrs;
  return EGADS_SUCCESS;
}


int
EG_attributeGet(const egObject *obj, int index, const char **name, 
                int *atype, int *len, /*@null@*/ const int **ints, 
                /*@null@*/ const double **reals, /*@null@*/ const char **str)
{
  int     outLevel;
  egAttrs *attrs;

  *name  = NULL;
  *atype = 0;
  if (ints  != NULL) *ints  = NULL;
  if (reals != NULL) *reals = NULL;
  if (str   != NULL) *str   = NULL;
  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;
  outLevel = EG_outLevel(obj);

  attrs = (egAttrs *) obj->attrs;
  if (attrs == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Attributes (EG_attributeGet)!\n");
    return EGADS_INDEXERR;
  }
  if ((index < 1) || (index > attrs->nattrs)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Index Error %d [1-%d] (EG_attributeGet)!\n",
             index, attrs->nattrs);
    return EGADS_INDEXERR;
  }

  *name  = attrs->attrs[index-1].name;
  *atype = attrs->attrs[index-1].type;
  *len   = attrs->attrs[index-1].length;
  if (*atype == ATTRINT) {
    if (ints != NULL) 
      if (*len <= 1) {
        *ints = &attrs->attrs[index-1].vals.integer;
      } else {
        *ints =  attrs->attrs[index-1].vals.integers;
      }
  } else if (*atype == ATTRREAL) {
    if (reals != NULL)
      if (*len <= 1) {
        *reals = &attrs->attrs[index-1].vals.real;
      } else {
        *reals =  attrs->attrs[index-1].vals.reals;
      }
  } else {
    if (str != NULL) *str = attrs->attrs[index-1].vals.string;
  }
  return EGADS_SUCCESS;
}


int
EG_attributeRet(const egObject *obj, const char *name, int *atype, 
                int *len, /*@null@*/ const int **ints, 
                          /*@null@*/ const double **reals, 
                          /*@null@*/ const char **str)
{
  int     i, outLevel, index;
  egAttrs *attrs;

  *atype = 0;
  *len   = 0;
  if (ints  != NULL) *ints  = NULL;
  if (reals != NULL) *reals = NULL;
  if (str   != NULL) *str   = NULL;
  if (obj == NULL)               return EGADS_NULLOBJ;
  if (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (obj->oclass == EMPTY)      return EGADS_EMPTY;
  if (obj->oclass == NIL)        return EGADS_EMPTY;
  if (obj->oclass == REFERENCE)  return EGADS_REFERCE;
  outLevel = EG_outLevel(obj);

  if (name == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Name (EG_attributeAdd)!\n");
    return EGADS_NONAME;
  }
  attrs = (egAttrs *) obj->attrs;
  if (attrs == NULL) return EGADS_NOTFOUND;

  index = -1;
  for (i = 0; i < attrs->nattrs; i++)
    if (strcmp(attrs->attrs[i].name, name) == 0) {
      index = i;
      break;
    }
  if (index == -1) return EGADS_NOTFOUND;

  *atype = attrs->attrs[index].type;
  *len   = attrs->attrs[index].length;
  if (*atype == ATTRINT) {
    if (ints != NULL) 
      if (*len <= 1) {
        *ints = &attrs->attrs[index].vals.integer;
      } else {
        *ints =  attrs->attrs[index].vals.integers;
      }
  } else if (*atype == ATTRREAL) {
    if (reals != NULL)
      if (*len <= 1) {
        *reals = &attrs->attrs[index].vals.real;
      } else {
        *reals =  attrs->attrs[index].vals.reals;
      }
  } else {
    if (str != NULL) *str = attrs->attrs[index].vals.string;
  }
  return EGADS_SUCCESS;
}


int
EG_attributeDup(const egObject *src, egObject *dst)
{
  int     i, j, n, stat, outLevel;
  egAttr  *attr;
  egAttrs *sattrs, *dattrs;
  
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->oclass == EMPTY)      return EGADS_EMPTY;
  if (src->oclass == NIL)        return EGADS_EMPTY;
  if (src->oclass == REFERENCE)  return EGADS_REFERCE;
  
  outLevel = EG_outLevel(src);
  if (dst == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL dst (EG_attributeDup)!\n");
    return EGADS_NULLOBJ;
  }
  if (dst->magicnumber != MAGIC) {
    if (outLevel > 0) 
      printf(" EGADS Error: dst not an EGO (EG_attributeDup)!\n");
    return EGADS_NOTOBJ;
  }
  if (EG_context(src) != EG_context(dst)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Context mismatch (EG_attributeDup)!\n");
    return EGADS_MIXCNTX;
  }

  /* remove any current attributes */
  stat = EG_attributeDel(dst, NULL);
  if (stat != EGADS_SUCCESS) return stat;
  sattrs = src->attrs;
  if (sattrs == NULL) return EGADS_SUCCESS;
  n = sattrs->nattrs;
  if (n == 0) return EGADS_SUCCESS;

  /* copy the attributes */
  dattrs = (egAttrs *) EG_alloc(sizeof(egAttrs));
  if (dattrs == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: dst Malloc (EG_attributeDup)!\n");
    return EGADS_MALLOC;
  }
  dattrs->nattrs = 0;
  dattrs->attrs  = NULL;
  dst->attrs     = dattrs;
  attr           = (egAttr *) EG_alloc(n*sizeof(egAttr));
  if (attr == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: dst attr Malloc (EG_attributeDup)!\n");
    return EGADS_MALLOC;
  }
  for (i = 0; i < n; i++) {
    attr[i].name   = EG_strdup(sattrs->attrs[i].name);
    attr[i].length = sattrs->attrs[i].length;
    attr[i].type   = sattrs->attrs[i].type;
    if (attr[i].type == ATTRINT) {
      if (attr[i].length <= 1) {
        attr[i].vals.integer = sattrs->attrs[i].vals.integer;
      } else {
        attr[i].vals.integers = (int *) EG_alloc(attr[i].length*sizeof(int));
        if (attr[i].vals.integers == NULL) {
          attr[i].length = 0;
        } else {
          for (j = 0; j < attr[i].length; j++)
            attr[i].vals.integers[j] = sattrs->attrs[i].vals.integers[j];
        }
      }
    } else if (attr[i].type == ATTRREAL) {
      if (attr[i].length <= 1) {
        attr[i].vals.real = sattrs->attrs[i].vals.real;
      } else {
        attr[i].vals.reals = (double *) EG_alloc(attr[i].length*sizeof(double));
        if (attr[i].vals.reals == NULL) {
          attr[i].length = 0;
        } else {
          for (j = 0; j < attr[i].length; j++)
            attr[i].vals.reals[j] = sattrs->attrs[i].vals.reals[j];
        }
      }
    } else {
      attr[i].vals.string = EG_strdup(sattrs->attrs[i].vals.string);
    }
  }
  dattrs->nattrs = n;
  dattrs->attrs  = attr;
  
  return EGADS_SUCCESS;
}
