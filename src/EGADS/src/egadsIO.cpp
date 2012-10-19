/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Load & Save Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "egadsTypes.h"
#include "egadsInternals.h"
#include "egadsClasses.h"


  extern "C" int  EG_destroyTopology( egObject *topo );

  extern "C" int  EG_loadModel( egObject *context, int bflg, const char *name, 
                                egObject **model );
  extern "C" int  EG_saveModel( const egObject *model, const char *name );

  extern     void EG_splitPeriodics( egadsBody *body );
  extern     int  EG_traverseBody( egObject *context, int i, egObject *bobj, 
                                   egObject *topObj, egadsBody *body );


static void
EG_attriBodyTrav(const egObject *obj, egadsBody *pbody)
{
  if (obj->blind == NULL) return;
  
  if (obj->oclass == NODE) {
  
    egadsNode *pnode = (egadsNode *) obj->blind;
    int index = pbody->nodes.map.FindIndex(pnode->node);
    if (index == 0) return;
    EG_attributeDup(obj, pbody->nodes.objs[index-1]);
  
  } else if (obj->oclass == EDGE) {
  
    egadsEdge *pedge = (egadsEdge *) obj->blind;
    int index = pbody->edges.map.FindIndex(pedge->edge);
    if (index != 0)
      EG_attributeDup(obj, pbody->edges.objs[index-1]);
    EG_attriBodyTrav(pedge->nodes[0], pbody);
    if (obj->mtype == TWONODE) EG_attriBodyTrav(pedge->nodes[1], pbody);
  
  } else if (obj->oclass == LOOP) {
  
    egadsLoop *ploop = (egadsLoop *) obj->blind;
    int index = pbody->loops.map.FindIndex(ploop->loop);
    if (index != 0)
      EG_attributeDup(obj, pbody->loops.objs[index-1]);
    for (int i = 0; i < ploop->nedges; i++)
      EG_attriBodyTrav(ploop->edges[i], pbody);
  
  } else if (obj->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) obj->blind;
    int index = pbody->faces.map.FindIndex(pface->face);
    if (index != 0)
      EG_attributeDup(obj, pbody->faces.objs[index-1]);
    for (int i = 0; i < pface->nloops; i++)
      EG_attriBodyTrav(pface->loops[i], pbody);
  
  } else if (obj->oclass == SHELL) {
  
    egadsShell *pshell = (egadsShell *) obj->blind;
    int index = pbody->shells.map.FindIndex(pshell->shell);
    if (index != 0)
      EG_attributeDup(obj, pbody->shells.objs[index-1]);
    for (int i = 0; i < pshell->nfaces; i++)
      EG_attriBodyTrav(pshell->faces[i], pbody);
  
  }
}


int
EG_attriBodyDup(const egObject *src, egObject *dst)
{
  int          i, j, nents, nattr;
  egObject     *aobj, *dobj;
  egAttrs      *attrs;
  TopoDS_Shape shape;
  
  if ((src == NULL) || (dst == NULL)) return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC)     return EGADS_NOTOBJ;
  if  (src->oclass < NODE)            return EGADS_NOTTOPO;
  if  (src->blind == NULL)            return EGADS_NODATA;
  int outLevel = EG_outLevel(src);
  
  if (src->oclass == MODEL) {
    if (outLevel > 0)
      printf(" EGADS Error: src MODEL not supported (EG_attriBodyDup)!\n");
    return EGADS_NOTMODEL;
  }
  if (dst->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: dst not an EGO (EG_attriBodyDup)!\n");
    return EGADS_NOTOBJ;
  }
  if (dst->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: dst not a BODY (EG_attriBodyDup)!\n");
    return EGADS_NOTBODY;
  }
  if (EG_context(src) != EG_context(dst)) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_attriBodyDup)!\n");
    return EGADS_MIXCNTX;
  }
  if (dst->blind == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL dst pointer (EG_attriBodyDup)!\n");
    return EGADS_NODATA;
  }
  egadsBody *pbody = (egadsBody *) dst->blind;
  
  if (src->oclass == BODY) {
  
    // use hashed body data on both ends
    egadsBody *pbods = (egadsBody *) src->blind;
    shape = pbody->shape;
    if (shape.IsSame(pbods->shape)) EG_attributeDup(src, dst);
    
    nents = pbods->shells.map.Extent();
    for (i = 0; i < nents; i++) {
      aobj = pbods->shells.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      shape = pbods->shells.map(i+1);
      j     = pbody->shells.map.FindIndex(shape);
      if (j == 0) continue;                     // not in the dst body
      dobj = pbody->shells.objs[j-1];
      EG_attributeDup(aobj, dobj);
    }

    nents = pbods->faces.map.Extent();
    for (i = 0; i < nents; i++) {
      aobj = pbods->faces.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      shape = pbods->faces.map(i+1);
      j     = pbody->faces.map.FindIndex(shape);
      if (j == 0) continue;                     // not in the dst body
      dobj = pbody->faces.objs[j-1];
      EG_attributeDup(aobj, dobj);
    }
    
    nents = pbods->loops.map.Extent();
    for (i = 0; i < nents; i++) {
      aobj = pbods->loops.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      shape = pbods->loops.map(i+1);
      j     = pbody->loops.map.FindIndex(shape);
      if (j == 0) continue;                     // not in the dst body
      dobj = pbody->loops.objs[j-1];
      EG_attributeDup(aobj, dobj);
    }

    nents = pbods->edges.map.Extent();
    for (i = 0; i < nents; i++) {
      aobj = pbods->edges.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      shape = pbods->edges.map(i+1);
      j     = pbody->edges.map.FindIndex(shape);
      if (j == 0) continue;                     // not in the dst body
      dobj = pbody->edges.objs[j-1];
      EG_attributeDup(aobj, dobj);
    }
  
    nents = pbods->nodes.map.Extent();
    for (i = 0; i < nents; i++) {
      aobj = pbods->nodes.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      shape = pbods->nodes.map(i+1);
      j     = pbody->nodes.map.FindIndex(shape);
      if (j == 0) continue;                     // not in the dst body
      dobj = pbody->nodes.objs[j-1];
      EG_attributeDup(aobj, dobj);
    }

  } else {
  
    // traverse the source to find objects with attributes
    EG_attriBodyTrav(src, pbody);
    
  }
  
  return EGADS_SUCCESS;
}


int
EG_attriBodyCopy(const egObject *src, egObject *dst)
{
  int      i, nents, nattr;
  egObject *aobj, *dobj;
  egAttrs  *attrs;
  
  if ((src == NULL) || (dst == NULL)) return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC)     return EGADS_NOTOBJ;
  if  (src->oclass < NODE)            return EGADS_NOTTOPO;
  if  (src->blind == NULL)            return EGADS_NODATA;
  int outLevel = EG_outLevel(src);
  
  if (src->oclass == MODEL) {
    if (outLevel > 0)
      printf(" EGADS Error: src MODEL not supported (EG_attriBodyCopy)!\n");
    return EGADS_NOTMODEL;
  }
  if (dst->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: dst not an EGO (EG_attriBodyCopy)!\n");
    return EGADS_NOTOBJ;
  }
  if (src->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: src not a BODY (EG_attriBodyCopy)!\n");
    return EGADS_NOTBODY;
  }
  if (dst->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: dst not a BODY (EG_attriBodyCopy)!\n");
    return EGADS_NOTBODY;
  }
  if (EG_context(src) != EG_context(dst)) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_attriBodyCopy)!\n");
    return EGADS_MIXCNTX;
  }
  if (src->blind == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL src pointer (EG_attriBodyCopy)!\n");
    return EGADS_NODATA;
  }
  if (dst->blind == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL dst pointer (EG_attriBodyCopy)!\n");
    return EGADS_NODATA;
  }
  EG_attributeDup(src, dst);
  egadsBody *pbods = (egadsBody *) src->blind;
  egadsBody *pbody = (egadsBody *) dst->blind;
  
  nents = pbods->shells.map.Extent();
  for (i = 0; i < nents; i++) {
    aobj = pbods->shells.objs[i];
    if (aobj->attrs == NULL) continue;
    attrs = (egAttrs *) aobj->attrs;
    nattr = attrs->nattrs;
    if (nattr <= 0) continue;
    dobj = pbody->shells.objs[i];
    EG_attributeDup(aobj, dobj);
  }

  nents = pbods->faces.map.Extent();
  for (i = 0; i < nents; i++) {
    aobj = pbods->faces.objs[i];
    if (aobj->attrs == NULL) continue;
    attrs = (egAttrs *) aobj->attrs;
    nattr = attrs->nattrs;
    if (nattr <= 0) continue;
    dobj = pbody->faces.objs[i];
    EG_attributeDup(aobj, dobj);
  }
    
  nents = pbods->loops.map.Extent();
  for (i = 0; i < nents; i++) {
    aobj = pbods->loops.objs[i];
    if (aobj->attrs == NULL) continue;
    attrs = (egAttrs *) aobj->attrs;
    nattr = attrs->nattrs;
    if (nattr <= 0) continue;
    dobj = pbody->loops.objs[i];
    EG_attributeDup(aobj, dobj);
  }

  nents = pbods->edges.map.Extent();
  for (i = 0; i < nents; i++) {
    aobj = pbods->edges.objs[i];
    if (aobj->attrs == NULL) continue;
    attrs = (egAttrs *) aobj->attrs;
    nattr = attrs->nattrs;
    if (nattr <= 0) continue;
    dobj = pbody->edges.objs[i];
    EG_attributeDup(aobj, dobj);
  }
  
  nents = pbods->nodes.map.Extent();
  for (i = 0; i < nents; i++) {
    aobj = pbods->nodes.objs[i];
    if (aobj->attrs == NULL) continue;
    attrs = (egAttrs *) aobj->attrs;
    nattr = attrs->nattrs;
    if (nattr <= 0) continue;
    dobj = pbody->nodes.objs[i];
    EG_attributeDup(aobj, dobj);
  }
  
  return EGADS_SUCCESS;
}


static void
EG_readAttrs(egObject *obj, int nattr, FILE *fp)
{
  int     i, j, n, type, namlen, len, *ivec, ival;
  char    *name, *string, cval;
  double  *rvec, rval;
  egAttrs *attrs = NULL;
  egAttr  *attr  = NULL;
  
  attr = (egAttr *) EG_alloc(nattr*sizeof(egAttr));
  if (attr != NULL) {
    attrs = (egAttrs *) EG_alloc(sizeof(egAttrs));
    if (attrs == NULL) {
      EG_free(attr);
      attr = NULL;
    }
  }
  
  for (n = i = 0; i < nattr; i++) {
    j = fscanf(fp, "%d %d %d", &type, &namlen, &len);
    if (j != 3) break;
    name = NULL;
    if ((attrs != NULL) && (namlen != 0))
      name = (char *) EG_alloc((namlen+1)*sizeof(char));
    if (name != NULL) {
      fscanf(fp, "%s", name);
    } else {
      for (j = 0; j < namlen; j++) fscanf(fp, "%c", &cval);
    }
    if (type == ATTRINT) {
      if (len == 1) {
        fscanf(fp, "%d", &ival);
      } else {
        ivec = NULL;
        if ((name != NULL) && (len != 0))
          ivec = (int *) EG_alloc(len*sizeof(int));
        if (ivec == NULL) {
          for (j = 0; j < len; j++) fscanf(fp, "%d", &ival);
          if (name != NULL) {
            EG_free(name);
            name = NULL;
          }
        } else {
          for (j = 0; j < len; j++) fscanf(fp, "%d", &ivec[j]);
        }
      }
    } else if (type == ATTRREAL) {
      if (len == 1) {
        fscanf(fp, "%lf", &rval);
      } else {
        rvec = NULL;
        if ((name != NULL) && (len != 0))
          rvec = (double *) EG_alloc(len*sizeof(double));
        if (rvec == NULL) {
          for (j = 0; j < len; j++) fscanf(fp, "%lf", &rval);
          if (name != NULL) {
            EG_free(name);
            name = NULL;
          }
        } else {
          for (j = 0; j < len; j++) fscanf(fp, "%lf", &rvec[j]);
        }
      } 
    } else {
      do {
        j = fscanf(fp, "%c", &cval);
        if (j == 0) break;
      } while (cval != '#');
      string = NULL;
      if ((name != NULL) && (len != 0))
        string = (char *) EG_alloc((len+1)*sizeof(char));
      if (string != NULL) {
        for (j = 0; j < len; j++) fscanf(fp, "%c", &string[j]);
        string[len] = 0;
      } else {
        for (j = 0; j < len; j++) fscanf(fp, "%c", &cval);
        if (name != NULL) {
          EG_free(name);
          name = NULL;
        }
      }
    }

    if (name != NULL) {
      attr[n].name   = name;
      attr[n].type   = type;
      attr[n].length = len;
      if (type == ATTRINT) {
        if (len == 1) {
          attr[n].vals.integer  = ival;
        } else {
          attr[n].vals.integers = ivec;
        }
      } else if (type == ATTRREAL) {
        if (len == 1) {
          attr[n].vals.real  = rval;
        } else {
          attr[n].vals.reals = rvec;
        }
      } else {
        attr[n].vals.string = string;
      }
      n++;
    }
  }
  
  if (attrs != NULL) {
    attrs->nattrs = n;
    attrs->attrs  = attr;
    obj->attrs    = attrs;
  }
}


int
EG_loadModel(egObject *context, int bflg, const char *name, 
             egObject **model)
{
  int          i, j, stat, outLevel, len, nattr, egads = 0;
  egObject     *omodel, *aobj;
  TopoDS_Shape source;
  egadsModel   *mshape = NULL;
  FILE         *fp;
  
  *model = NULL;
  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  outLevel = EG_outLevel(context);
  
  if (name == NULL) {
    if (outLevel > 0)
      printf(" EGADS Warning: NULL Filename (EG_loadModel)!\n");
    return EGADS_NONAME;
  }
  
  /* does file exist? */

  fp = fopen(name, "r");
  if (fp == NULL) {
    if (outLevel > 0)
      printf(" EGADS Warning: File %s Not Found (EG_loadModel)!\n", name);
    return EGADS_NOTFOUND;
  }
  fclose(fp);
  
  /* find extension */
  
  len = strlen(name);
  for (i = len-1; i > 0; i--)
    if (name[i] == '.') break;
  if (i == 0) {
    if (outLevel > 0)
      printf(" EGADS Warning: No Extension in %s (EG_loadModel)!\n", name);
    return EGADS_NODATA;
  }
  
  if ((strcasecmp(&name[i],".step") == 0) || 
      (strcasecmp(&name[i],".stp") == 0)) {

    /* STEP files */

    STEPControl_Reader aReader;
    IFSelect_ReturnStatus status = aReader.ReadFile(name);
    if (status != IFSelect_RetDone) {
      if (outLevel > 0)
        printf(" EGADS Error: STEP Read of %s = %d (EG_loadModel)!\n", 
               name, status);
      return EGADS_NOLOAD;
    }

    // inspect the root transfers
    if (outLevel > 2)
      aReader.PrintCheckLoad(Standard_False, IFSelect_ItemsByEntity);

    int nroot = aReader.NbRootsForTransfer();
    if (outLevel > 1)
      printf(" EGADS Info: %s Entries = %d\n", name, nroot);

    for (i = 1; i <= nroot; i++) {
      Standard_Boolean ok = aReader.TransferRoot(i);
      if ((!ok) && (outLevel > 0))
        printf(" EGADS Warning: Transfer %d/%d is not OK!\n", i, nroot);
    }

    int nbs = aReader.NbShapes();
    if (nbs <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: %s has No Shapes (EG_loadModel)!\n", 
               name);
      return EGADS_NOLOAD;
    }
    if (outLevel > 1)    
      printf(" EGADS Info: %s has %d Shape(s)\n", name, nbs);

    TopoDS_Compound compound;
    BRep_Builder    builder3D;
    builder3D.MakeCompound(compound);
    for (i = 1; i <= nbs; i++) {
      TopoDS_Shape aShape = aReader.Shape(i);
      builder3D.Add(compound, aShape);
    }
    source = compound;
    
  } else if ((strcasecmp(&name[i],".iges") == 0) || 
             (strcasecmp(&name[i],".igs") == 0)) {
             
    /* IGES files */
    
    IGESControl_Reader iReader;
    Standard_Integer stats = iReader.ReadFile(name);
    if (stats != IFSelect_RetDone) {
      if (outLevel > 0)
        printf(" EGADS Error: IGES Read of %s = %d (EG_loadModel)!\n", 
               name, stats);
      return EGADS_NOLOAD;
    }
    iReader.TransferRoots();

    int nbs = iReader.NbShapes();
    if (nbs <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: %s has No Shapes (EG_loadModel)!\n", 
               name);
      return EGADS_NOLOAD;
    }
    if (outLevel > 1)    
      printf(" EGADS Info: %s has %d Shape(s)\n", name, nbs);

    TopoDS_Compound compound;
    BRep_Builder    builder3D;
    builder3D.MakeCompound(compound);
    for (i = 1; i <= nbs; i++) {
      TopoDS_Shape aShape = iReader.Shape(i);
      builder3D.Add(compound, aShape);
    }
    source = compound;

  } else if ((strcasecmp(&name[i],".brep") == 0) ||
             (strcasecmp(&name[i],".egads") == 0)) {
  
    /* Native OCC file */
    if (strcasecmp(&name[i],".egads") == 0) egads = 1;

    BRep_Builder builder;
    if (!BRepTools::Read(source, name, builder)) {
      if (outLevel > 0)
        printf(" EGADS Warning: Read Error on %s (EG_loadModel)!\n", name);
      return EGADS_NOLOAD;
    }

  } else {
    if (outLevel > 0)
      printf(" EGADS Warning: Extension in %s Not Supported (EG_loadModel)!\n",
             name);
    return EGADS_NODATA;
  }
  
  int nWire  = 0;
  int nFace  = 0;
  int nSheet = 0;
  int nSolid = 0;
  
  TopExp_Explorer Exp;
  for (Exp.Init(source, TopAbs_WIRE,  TopAbs_FACE);
       Exp.More(); Exp.Next()) nWire++;
  for (Exp.Init(source, TopAbs_FACE,  TopAbs_SHELL);
       Exp.More(); Exp.Next()) nFace++;
  for (Exp.Init(source, TopAbs_SHELL, TopAbs_SOLID);
       Exp.More(); Exp.Next()) nSheet++;
  for (Exp.Init(source, TopAbs_SOLID); Exp.More(); Exp.Next()) nSolid++;

  if (outLevel > 1)
    printf("\n EGADS Info: %s has %d Solids, %d Sheets, %d Faces and %d Wires\n",
           name, nSolid, nSheet, nFace, nWire);
           
  int nBody = nWire+nFace+nSheet+nSolid;
  if (nBody == 0) {
    source.Nullify();
    if (outLevel > 0)
      printf(" EGADS Warning: Nothing found in %s (EG_loadModel)!\n", name);
    return EGADS_NODATA;
  }
  
  mshape = new egadsModel;
  mshape->shape  = source;
  mshape->nbody  = nBody;
  mshape->bodies = new egObject*[nBody];
  for (i = 0; i < nBody; i++) {
    stat = EG_makeObject(context, &mshape->bodies[i]);
    if (stat != EGADS_SUCCESS) {
      for (int j = 0; j < i; j++) {
        egObject  *obj   = mshape->bodies[j];
        egadsBody *pbody = (egadsBody *) obj->blind;
        delete pbody;
        EG_deleteObject(mshape->bodies[j]);
      }
      delete [] mshape->bodies;
      delete mshape;
      return stat;
    }
    egObject  *pobj    = mshape->bodies[i];
    egadsBody *pbody   = new egadsBody;
    pbody->nodes.objs  = NULL;
    pbody->edges.objs  = NULL;
    pbody->loops.objs  = NULL;
    pbody->faces.objs  = NULL;
    pbody->shells.objs = NULL;
    pbody->senses      = NULL;
    pobj->blind        = pbody;
  }
  
  i = 0;
  for (Exp.Init(mshape->shape, TopAbs_WIRE,  TopAbs_FACE); 
       Exp.More(); Exp.Next()) {
    egObject  *obj   = mshape->bodies[i++];
    egadsBody *pbody = (egadsBody *) obj->blind;
    pbody->shape     = Exp.Current();
  }
  for (Exp.Init(mshape->shape, TopAbs_FACE,  TopAbs_SHELL);
       Exp.More(); Exp.Next()) {
    egObject  *obj   = mshape->bodies[i++];
    egadsBody *pbody = (egadsBody *) obj->blind;
    pbody->shape     = Exp.Current();
  }
  for (Exp.Init(mshape->shape, TopAbs_SHELL, TopAbs_SOLID);
       Exp.More(); Exp.Next()) {
    egObject  *obj   = mshape->bodies[i++];
    egadsBody *pbody = (egadsBody *) obj->blind;
    pbody->shape     = Exp.Current();
  }
  for (Exp.Init(mshape->shape, TopAbs_SOLID); Exp.More(); Exp.Next()) {
    egObject  *obj   = mshape->bodies[i++];
    egadsBody *pbody = (egadsBody *) obj->blind;
    pbody->shape     = Exp.Current();
  }
  
  stat = EG_makeObject(context, &omodel);
  if (stat != EGADS_SUCCESS) {
    source.Nullify();
    for (i = 0; i < nBody; i++) {
      egObject  *obj   = mshape->bodies[i];
      egadsBody *pbody = (egadsBody *) obj->blind;
      delete pbody;
      EG_deleteObject(mshape->bodies[i]);
    }
    delete [] mshape->bodies;
    delete mshape;
    return stat;
  }
  omodel->oclass = MODEL;
  omodel->blind  = mshape;
  EG_referenceObject(omodel, context);
  
  for (i = 0; i < nBody; i++) {
    egObject  *pobj  = mshape->bodies[i];
    egadsBody *pbody = (egadsBody *) pobj->blind;
    pobj->topObj     = omodel;
    if (((bflg&1) == 0) && (egads == 0)) EG_splitPeriodics(pbody);
    stat = EG_traverseBody(context, i, pobj, omodel, pbody);
    if (stat != EGADS_SUCCESS) {
      mshape->nbody = i;
      EG_destroyTopology(omodel);
      delete [] mshape->bodies;
      delete mshape;
      return stat;
    }
  }

  *model = omodel;
  if (egads == 0) return EGADS_SUCCESS;

  /* get the attributes from the EGADS files */
  
  fp = fopen(name, "r");
  if (fp == NULL) {
    printf(" EGADS Info: Cannot reOpen %s (EG_loadModel)!\n", name);
    return EGADS_SUCCESS;
  }
  char line[81];
  for (;;) {
    line[0] = line[1] = ' ';
    if (fgets(line, 81, fp) == NULL) break;
    if ((line[0] == '#') && (line[1] == '#')) break;
  }
  
  // got the header
  if ((line[0] == '#') && (line[1] == '#')) {
    if (outLevel > 1) printf(" Header = %s\n", line);
    // get number of model attributes
    fscanf(fp, "%d", &nattr);
    if (nattr != 0) EG_readAttrs(omodel, nattr, fp);
    for (i = 0; i < nBody; i++) {
      int otype,  oindex;
      int rsolid, rshell, rface, rloop, redge, rnode;
      int nsolid, nshell, nface, nloop, nedge, nnode;

      fscanf(fp, " %d %d %d %d %d %d %d", &rsolid, &rshell, 
             &rface, &rloop, &redge, &rnode, &nattr);
      if (outLevel > 2)
        printf(" read = %d %d %d %d %d %d %d\n", rsolid, rshell, 
               rface, rloop, redge, rnode, nattr);
      egObject  *pobj  = mshape->bodies[i];
      egadsBody *pbody = (egadsBody *) pobj->blind;
      nnode  = pbody->nodes.map.Extent();
      nedge  = pbody->edges.map.Extent();
      nloop  = pbody->loops.map.Extent();
      nface  = pbody->faces.map.Extent();
      nshell = pbody->shells.map.Extent();
      nsolid = 0;
      if (pobj->mtype == SOLIDBODY) nsolid = 1;
      if ((nnode != rnode) || (nedge  != redge)  || (nloop  != rloop) ||
          (nface != rface) || (nshell != rshell) || (nsolid != rsolid)) {
        printf(" EGADS Info: %d %d, %d %d, %d %d, %d %d, %d %d, %d %d",
               nnode, rnode, nedge,  redge,  nloop,  rloop,
               nface, rface, nshell, rshell, nsolid, rsolid);
        printf("  MisMatch on Attributes (EG_loadModel)!\n");
        fclose(fp);
        return EGADS_SUCCESS;
      }
      // got the correct body -- transfer the attributes
      if (nattr != 0) EG_readAttrs(pobj, nattr, fp);
      for (;;)  {
        j = fscanf(fp, "%d %d %d\n", &otype, &oindex, &nattr);
        if (outLevel > 2)
          printf(" %d:  attr header = %d %d %d\n", 
                 j, otype, oindex, nattr);
        if (j     != 3) break;
        if (otype == 0) break;
        if (otype == 1) {
          aobj = pbody->shells.objs[oindex];
        } else if (otype == 2) {
          aobj = pbody->faces.objs[oindex];
        } else if (otype == 3) {
          aobj = pbody->loops.objs[oindex];
        } else if (otype == 4) {
          aobj = pbody->edges.objs[oindex];
        } else {
          aobj = pbody->nodes.objs[oindex];
        }
        EG_readAttrs(aobj, nattr, fp);
      }
    }
  } else {
    printf(" EGADS Info: EGADS Header not found in %s (EG_loadModel)!\n", 
           name);
    return EGADS_SUCCESS;
  }
  
  fclose(fp);
  return EGADS_SUCCESS;
}


static void
EG_writeAttr(egAttrs *attrs, FILE *fp)
{
  int namln;
  
  int    nattr = attrs->nattrs;
  egAttr *attr = attrs->attrs;
  for (int i = 0; i < nattr; i++) {
    namln = 0;
    if (attr[i].name != NULL) namln = strlen(attr[i].name);
    fprintf(fp, "%d %d %d\n", attr[i].type, namln, attr[i].length);
    if (namln != 0) fprintf(fp, "%s\n", attr[i].name);
    if (attr[i].type == ATTRINT) {
      if (attr[i].length == 1) {
        fprintf(fp, "%d\n", attr[i].vals.integer);
      } else {
        for (int j = 0; j < attr[i].length; j++)
          fprintf(fp, "%d ", attr[i].vals.integers[j]);
        fprintf(fp, "\n");
      }
    } else if (attr[i].type == ATTRREAL) {
      if (attr[i].length == 1) {
        fprintf(fp, "%19.12e\n", attr[i].vals.real);
      } else {
        for (int j = 0; j < attr[i].length; j++)
          fprintf(fp, "%19.12e ", attr[i].vals.reals[j]);
        fprintf(fp, "\n");
      }    
    } else {
      if (attr[i].length != 0)
        fprintf(fp, "#%s\n", attr[i].vals.string);
    }
  }
}


static void
EG_writeAttrs(const egObject *obj, FILE *fp)
{
  int     i, nsolid, nshell, nface, nloop, nedge, nnode, nattr = 0;
  egAttrs *attrs;
  
  attrs = (egAttrs *) obj->attrs;
  if (attrs != NULL) nattr = attrs->nattrs;
  
  if (obj->oclass == MODEL) {
  
    fprintf(fp, "%d\n", nattr);
    if (nattr != 0) EG_writeAttr(attrs, fp);
    
  } else {

    egadsBody *pbody = (egadsBody *) obj->blind;
    nnode  = pbody->nodes.map.Extent();
    nedge  = pbody->edges.map.Extent();
    nloop  = pbody->loops.map.Extent();
    nface  = pbody->faces.map.Extent();
    nshell = pbody->shells.map.Extent();
    nsolid = 0;
    if (obj->mtype == SOLIDBODY) nsolid = 1;
    fprintf(fp, "  %d  %d  %d  %d  %d  %d  %d\n", nsolid, nshell, nface,  
            nloop, nedge, nnode, nattr);
    if (nattr != 0) EG_writeAttr(attrs, fp);
    
    for (i = 0; i < nshell; i++) {
      egObject *aobj = pbody->shells.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      fprintf(fp, "    1 %d %d\n", i, nattr);
      EG_writeAttr(attrs, fp);
    }

    for (i = 0; i < nface; i++) {
      egObject *aobj = pbody->faces.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      fprintf(fp, "    2 %d %d\n", i, nattr);
      EG_writeAttr(attrs, fp);
    }
    
    for (i = 0; i < nloop; i++) {
      egObject *aobj = pbody->loops.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      fprintf(fp, "    3 %d %d\n", i, nattr);
      EG_writeAttr(attrs, fp);
    }
        
    for (i = 0; i < nedge; i++) {
      egObject *aobj = pbody->edges.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      fprintf(fp, "    4 %d %d\n", i, nattr);
      EG_writeAttr(attrs, fp);
    }
        
    for (int i = 0; i < nnode; i++) {
      egObject *aobj = pbody->nodes.objs[i];
      if (aobj->attrs == NULL) continue;
      attrs = (egAttrs *) aobj->attrs;
      nattr = attrs->nattrs;
      if (nattr <= 0) continue;
      fprintf(fp, "    5 %d %d\n", i, nattr);
      EG_writeAttr(attrs, fp);
    }
    fprintf(fp, "    0 0 0\n");

  }

}


int
EG_saveModel(const egObject *model, const char *name)
{
  int         i, len, outLevel;
  egadsModel *mshape;
  FILE        *fp;
  
  if (model == NULL)               return EGADS_NULLOBJ;
  if (model->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (model->oclass != MODEL)      return EGADS_NOTMODEL;
  outLevel = EG_outLevel(model);

  if (name == NULL) {
    if (outLevel > 0)
      printf(" EGADS Warning: NULL Filename (EG_saveModel)!\n");
    return EGADS_NONAME;
  }
  
  /* does file exist? */

  fp = fopen(name, "r");
  if (fp != NULL) {
    if (outLevel > 0)
      printf(" EGADS Warning: File %s Exists (EG_saveModel)!\n", name);
    fclose(fp);
    return EGADS_NOTFOUND;
  }
  
  /* find extension */
  
  len = strlen(name);
  for (i = len-1; i > 0; i--)
    if (name[i] == '.') break;
  if (i == 0) {
    if (outLevel > 0)
      printf(" EGADS Warning: No Extension in %s (EG_saveModel)!\n", name);
    return EGADS_NODATA;
  }
  
  mshape = (egadsModel *) model->blind;
  
  if ((strcasecmp(&name[i],".step") == 0) || 
      (strcasecmp(&name[i],".stp") == 0)) {

    /* STEP files */
    
    STEPControl_Writer aWriter;
    TopExp_Explorer Exp;
    const STEPControl_StepModelType aVal = STEPControl_AsIs;
    for (Exp.Init(mshape->shape, TopAbs_WIRE,  TopAbs_FACE);
         Exp.More(); Exp.Next()) aWriter.Transfer(Exp.Current(), aVal);
    for (Exp.Init(mshape->shape, TopAbs_FACE,  TopAbs_SHELL);
         Exp.More(); Exp.Next()) aWriter.Transfer(Exp.Current(), aVal);
    for (Exp.Init(mshape->shape, TopAbs_SHELL, TopAbs_SOLID);
         Exp.More(); Exp.Next()) aWriter.Transfer(Exp.Current(), aVal);
    for (Exp.Init(mshape->shape, TopAbs_SOLID); 
         Exp.More(); Exp.Next()) aWriter.Transfer(Exp.Current(), aVal);	
  
    if (!aWriter.Write(name)) {
      printf(" EGADS Warning: STEP Write Error (EG_saveModel)!\n");
      return EGADS_WRITERR;
    }
    
  } else if ((strcasecmp(&name[i],".iges") == 0) || 
             (strcasecmp(&name[i],".igs") == 0)) {
             
    /* IGES files */

    try {
      IGESControl_Controller::Init();
      IGESControl_Writer iWrite;
      TopExp_Explorer Exp;
      for (Exp.Init(mshape->shape, TopAbs_WIRE,  TopAbs_FACE);
           Exp.More(); Exp.Next()) iWrite.AddShape(Exp.Current());
      for (Exp.Init(mshape->shape, TopAbs_FACE,  TopAbs_SHELL);
           Exp.More(); Exp.Next()) iWrite.AddShape(Exp.Current());
      for (Exp.Init(mshape->shape, TopAbs_SHELL, TopAbs_SOLID);
           Exp.More(); Exp.Next()) iWrite.AddShape(Exp.Current());
      for (Exp.Init(mshape->shape, TopAbs_SOLID); 
           Exp.More(); Exp.Next()) iWrite.AddShape(Exp.Current());

      iWrite.ComputeModel();
      if (!iWrite.Write(name)) {
        printf(" EGADS Warning: IGES Write Error (EG_saveModel)!\n");
        return EGADS_WRITERR;
      }
    }
    catch (...)
    {
      printf(" EGADS Warning: Internal IGES Write Error (EG_saveModel)!\n");
      return EGADS_WRITERR;
    }

  } else if ((strcasecmp(&name[i],".brep") == 0) ||
             (strcasecmp(&name[i],".egads") == 0)) {
  
    /* Native OCC file or our filetype */

    if (!BRepTools::Write(mshape->shape, name)) {
      printf(" EGADS Warning: OCC Write Error (EG_saveModel)!\n");
      return EGADS_WRITERR;
    }
    if (strcasecmp(&name[i],".brep") == 0) return EGADS_SUCCESS;
    
    /* append the attributes -- output in the read order */
    
    FILE *fp = fopen(name, "a");
    if (fp == NULL) {
      printf(" EGADS Warning: EGADS Open Error (EG_saveModel)!\n");
      return EGADS_WRITERR;
    }
    fprintf(fp, "\n##EGADS HEADER FILE-REV 1 ##\n");
    /* write model attributes */
    EG_writeAttrs(model, fp);
    TopExp_Explorer Exp;
    for (Exp.Init(mshape->shape, TopAbs_WIRE,  TopAbs_FACE); 
         Exp.More(); Exp.Next()) {
      TopoDS_Shape shape = Exp.Current();
      for (i = 0; i < mshape->nbody; i++) {
        egObject  *obj   = mshape->bodies[i];
        egadsBody *pbody = (egadsBody *) obj->blind;
        if (shape.IsSame(pbody->shape)) {
          EG_writeAttrs(obj, fp);
          break;
        }
      }
    }
    for (Exp.Init(mshape->shape, TopAbs_FACE,  TopAbs_SHELL);
         Exp.More(); Exp.Next()) {
      TopoDS_Shape shape = Exp.Current();
      for (i = 0; i < mshape->nbody; i++) {
        egObject  *obj   = mshape->bodies[i];
        egadsBody *pbody = (egadsBody *) obj->blind;
        if (shape.IsSame(pbody->shape)) {
          EG_writeAttrs(obj, fp);
          break;
        }
      }
    }
    for (Exp.Init(mshape->shape, TopAbs_SHELL, TopAbs_SOLID);
         Exp.More(); Exp.Next()) {
      TopoDS_Shape shape = Exp.Current();
      for (i = 0; i < mshape->nbody; i++) {
        egObject  *obj   = mshape->bodies[i];
        egadsBody *pbody = (egadsBody *) obj->blind;
        if (shape.IsSame(pbody->shape)) {
          EG_writeAttrs(obj, fp);
          break;
        }
      }
    }
    for (Exp.Init(mshape->shape, TopAbs_SOLID); Exp.More(); Exp.Next()) {
      TopoDS_Shape shape = Exp.Current();
      for (i = 0; i < mshape->nbody; i++) {
        egObject  *obj   = mshape->bodies[i];
        egadsBody *pbody = (egadsBody *) obj->blind;
        if (shape.IsSame(pbody->shape)) {
          EG_writeAttrs(obj, fp);
          break;
        }
      }
    }
   
   fclose(fp);

  } else {
    if (outLevel > 0)
      printf(" EGADS Warning: Extension in %s Not Supported (EG_saveModel)!\n",
             name);
    return EGADS_NODATA;
  }

  return EGADS_SUCCESS;
}

