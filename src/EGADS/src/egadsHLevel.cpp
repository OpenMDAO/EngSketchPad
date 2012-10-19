/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             High-Level Functions
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
#include <signal.h>
#include <setjmp.h>


#include "egadsTypes.h"
#include "egadsInternals.h"
#include "egadsClasses.h"


  typedef struct {
    int sense;                  /* sense use for the loop construction */
    int index;                  /* index in loop */
    int lIndex;                 /* loop index */
  } loopInfo;


  extern "C" int  EG_destroyTopology( egObject *topo );
    
  extern "C" int  EG_solidBoolean( const egObject *src, const egObject *tool,
                                   int oper, egObject **model );
  extern "C" int  EG_intersection( const egObject *src, const egObject *tool,
                                   int *nedge, /*@null@*/ egObject ***facEdg,
                                   egObject **model );
  extern "C" int  EG_imprintBody( const egObject *src, int nedge, 
                                  const egObject **facEdg, egObject **result );
  extern "C" int  EG_filletBody( const egObject *src, int nedge, 
                                 const egObject **edges, double radius,
                                       egObject **result );
  extern "C" int  EG_chamferBody( const egObject *src, int nedge, 
                                  const egObject **edges, const egObject **faces, 
                                  double dis1, double dis2, egObject **result );
  extern "C" int  EG_hollowBody( const egObject *src, int nface, 
                                 const egObject **faces, double offset, int join,
                                       egObject **result );
  extern "C" int  EG_extrude( const egObject *src, double dist, 
                              const double *dir, egObject **result );
  extern "C" int  EG_rotate( const egObject *src, double angle, 
                             const double *axis, egObject **result );
  extern "C" int  EG_sweep( const egObject *src, const egObject *edge, 
                                  egObject **result );
  extern "C" int  EG_loft( int nsec, const egObject **secs, int opt, 
                                           egObject **result );

  extern     int  EG_traverseBody( egObject *context, int i, egObject *bobj, 
                                   egObject *topObj, egadsBody *body );
  extern     int  EG_attriBodyDup( const egObject *src, egObject *dst );
  extern     void EG_completePCurve( egObject *g, Handle(Geom2d_Curve) &hCurv );
  extern     void EG_completeSurf(   egObject *g, Handle(Geom_Surface) &hSurf );
     

// for trapping SegFaults                                                                 
static jmp_buf jmpenv;

static void
segfault_handler(int x)
{
  longjmp(jmpenv, x);
}


static void
EG_matchMdlFace(BRepAlgoAPI_BooleanOperation& BSO, TopoDS_Shape src,
                int iface, TopoDS_Shape tool, TopoDS_Shape result, 
                int **mapping)
{
  int                        i, j, nf, *map;
  TopoDS_Face                face, genface;
  TopTools_IndexedMapOfShape rmap, smap, tmap;

  *mapping = NULL;
  TopExp::MapShapes(result, TopAbs_FACE, rmap);
  TopExp::MapShapes(src,    TopAbs_FACE, smap);
  nf = rmap.Extent();
  if (nf == 0) return;
  
  map = (int *) EG_alloc(nf*sizeof(int));
  if (map == NULL) return;
  for (i = 0; i < nf; i++) map[i] = 0;
  *mapping = map;

  for (i = 1; i <= smap.Extent(); i++) {
    face = TopoDS::Face(smap(i));
    if (BSO.IsDeleted(face)) continue;
    const TopTools_ListOfShape& listFaces = BSO.Modified(face);
    if (listFaces.Extent() > 0) {
      /* modified faces */
      TopTools_ListIteratorOfListOfShape it(listFaces);
      for (; it.More(); it.Next()) {
        genface = TopoDS::Face(it.Value());
        j = rmap.FindIndex(genface);
        if (j > 0) map[j-1] = i;
      }
    } else {
      j = rmap.FindIndex(face);
      if (j > 0) map[j-1] = i;
    }
  }
  
  if (iface == 0) {
  
    TopExp::MapShapes(tool, TopAbs_FACE, tmap);
    for (i = 1; i <= tmap.Extent(); i++) {
      face = TopoDS::Face(tmap(i));
      if (BSO.IsDeleted(face)) continue;
      const TopTools_ListOfShape& listFaces = BSO.Modified(face);
      if (listFaces.Extent() > 0) {
        /* modified faces */
        TopTools_ListIteratorOfListOfShape it(listFaces);
        for (; it.More(); it.Next()) {
          genface = TopoDS::Face(it.Value());
          j = rmap.FindIndex(genface);
          if (j > 0) map[j-1] = -i;
        }
      }
    }

  } else {

    face = TopoDS::Face(tool);
    if (!BSO.IsDeleted(face)) {
      const TopTools_ListOfShape& listFaces = BSO.Modified(face);
      if (listFaces.Extent() > 0) {
        /* modified faces */
        TopTools_ListIteratorOfListOfShape it(listFaces);
        for (; it.More(); it.Next()) {
          genface = TopoDS::Face(it.Value());
          j = rmap.FindIndex(genface);
          if (j > 0) map[j-1] = -1;
        }
      }
    }

  }

}


static void
EG_matchFaces(BRepAlgoAPI_BooleanOperation& BSO, const egObject *src,
              const egObject *tool, TopoDS_Shape result, int ***mapping)
{
  int             i, j, k, ns, nface, **map;
  egadsBody       *pbods, *pbodt = NULL;
  const egObject  *oface = NULL;
  TopoDS_Shape    solid;
  TopoDS_Face     face, genface;
  TopExp_Explorer Exp;
  
  *mapping = NULL;
  pbods    = (egadsBody *) src->blind;
  if (((tool->oclass == FACE) ||
      ((tool->oclass == BODY) && (tool->mtype == FACEBODY)))) {
    if (tool->oclass == FACE) {
      oface = tool;
    } else {
      egadsBody *pbodf = (egadsBody *) tool->blind;
      oface = pbodf->faces.objs[0];
    }
  } else {
    pbodt = (egadsBody *) tool->blind;
  }

  ns = 0;
  for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) ns++;
  if (ns == 0) return;
  map = (int **) EG_alloc(ns*sizeof(int *));
  if (map == NULL) return;
  for (i = 0; i < ns; i++) map[i] = NULL;

  k = 0;
  for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
    solid = Exp.Current();
    TopTools_IndexedMapOfShape MapF;
    TopExp::MapShapes(solid, TopAbs_FACE, MapF);
    nface = MapF.Extent();
    if (nface > 0) map[k] = (int *) EG_alloc(nface*sizeof(int));
    if (map[k] != NULL)
      for (j = 0; j < nface; j++) map[k][j] = 0;
    k++;
  }

  *mapping = map;

  /* look at source shape */

  for (i = 1; i <= pbods->faces.map.Extent(); i++) {
    face = TopoDS::Face(pbods->faces.map(i));
    if (BSO.IsDeleted(face)) continue;
    const TopTools_ListOfShape& listFaces = BSO.Modified(face);
    if (listFaces.Extent() > 0) {
      /* modified faces */
      TopTools_ListIteratorOfListOfShape it(listFaces);
      for (; it.More(); it.Next()) {
        genface = TopoDS::Face(it.Value());
        k       = 0;
        for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
          solid = Exp.Current();
          TopTools_IndexedMapOfShape MapF;
          TopExp::MapShapes(solid, TopAbs_FACE, MapF);
          if (map[k] != NULL) {
            j = MapF.FindIndex(genface);
            if (j > 0) map[k][j-1] = i;
          }
          k++;
        }
      }
    }
  }

  /* look at tool shape */

  if (oface == NULL) {
    for (i = 1; i <= pbodt->faces.map.Extent(); i++) {
      face = TopoDS::Face(pbodt->faces.map(i));
      if (BSO.IsDeleted(face)) continue;
      const TopTools_ListOfShape& listFaces = BSO.Modified(face);
      if (listFaces.Extent() > 0) {
        /* modified faces */
        TopTools_ListIteratorOfListOfShape it(listFaces);
        for (; it.More(); it.Next()) {
          genface = TopoDS::Face(it.Value());
          k       = 0;
          for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
            solid = Exp.Current();
            TopTools_IndexedMapOfShape MapF;
            TopExp::MapShapes(solid, TopAbs_FACE, MapF);
            if (map[k] != NULL) {
              j = MapF.FindIndex(genface);
              if (j > 0) map[k][j-1] = -i;
            }
            k++;
          }
        }
      }
    }
    
  } else {

    egadsFace *pface = (egadsFace *) oface->blind;
    face = pface->face;
    k    = 0;
    for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
      solid = Exp.Current();
      TopTools_IndexedMapOfShape MapF;
      TopExp::MapShapes(solid, TopAbs_FACE, MapF);
      if (map[k] != NULL) {
        j = MapF.FindIndex(genface);
        if (j > 0) map[k][j-1] = -1;
      }
      k++;
    }
    if (!BSO.IsDeleted(face)) {
      const TopTools_ListOfShape& listFaces = BSO.Modified(face);
      if (listFaces.Extent() > 0) {
        /* modified faces */
        TopTools_ListIteratorOfListOfShape it(listFaces);
        for (; it.More(); it.Next()) {
          genface = TopoDS::Face(it.Value());
          k       = 0;
          for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
            solid = Exp.Current();
            TopTools_IndexedMapOfShape MapF;
            TopExp::MapShapes(solid, TopAbs_FACE, MapF);
            nface = MapF.Extent();
            if (map[k] != NULL) {
              j = MapF.FindIndex(genface);
              if (j > 0) map[k][j-1] = -1;
            }
            k++;
          }
        }
      }
    }
  }

}


static int
EG_modelBoolean(const egObject *src, const egObject *tool, int oper, 
                      egObject **model)
{
  int            i, j, k, stat, outLevel, iface, index, *fmap = NULL;
  egObject       *context, *omodel;
  TopoDS_Shape   result;
  const egObject *face = NULL;
  
  if (src->blind == NULL) return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if ((oper != INTERSECTION) && (oper != FUSION)) {
    if (outLevel > 0) 
      printf(" EGADS Error: BAD Operator = %d (EG_solidBoolean)!\n",
             oper);
    return EGADS_RANGERR;
  }
  if (tool == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Tool (EG_solidBoolean)!\n");
    return EGADS_NULLOBJ;
  }
  if (tool->magicnumber != MAGIC) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool is not an EGO (EG_solidBoolean)!\n");
    return EGADS_NOTOBJ;
  }
  if (tool->blind == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool has no data (EG_solidBoolean)!\n");
    return EGADS_NODATA;
  }
  if (EG_context(tool) != context) {
    if (outLevel > 0) 
      printf(" EGADS Error: Context mismatch (EG_solidBoolean)!\n");
    return EGADS_MIXCNTX;
  }
  egadsModel      *pmdl = (egadsModel *) src->blind;
  TopoDS_Compound ssrc  = TopoDS::Compound(pmdl->shape);
  
  if (oper == FUSION) {

    if  ((tool->oclass != FACE) &&
        ((tool->oclass != BODY) || (tool->mtype != FACEBODY))) {
      printf(" EGADS Error: Face Tool is wrong type (EG_solidBoolean)!\n");
      return EGADS_NOTTOPO;
    }
    if (tool->oclass == FACE) {
      face = tool;
    } else {
      egadsBody *pbodf = (egadsBody *) tool->blind;
      face = pbodf->faces.objs[0];
    }
    if (face == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Face Tool (EG_solidBoolean)!\n");
      return EGADS_NULLOBJ;
    }
    if (face->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Tool is not an EGO (EG_solidBoolean)!\n");
      return EGADS_NOTOBJ;
    }
    if (face->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Tool has no data (EG_solidBoolean)!\n");
      return EGADS_NODATA;
    }
    egadsFace    *pface = (egadsFace *) face->blind;
    TopoDS_Shape stool  = pface->face;
    try {
      BRepAlgoAPI_Fuse BSO(ssrc, stool);
      if (!BSO.IsDone()) {
        printf(" EGADS Error: Can't do SBO Fusion (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      result = BSO.Shape();
      stat   = 0;
      TopExp_Explorer Expe;
      for (Expe.Init(result, TopAbs_EDGE); Expe.More(); Expe.Next()) {
        TopoDS_Vertex V1, V2;
        TopoDS_Edge   Edge = TopoDS::Edge(Expe.Current());
        if (BRep_Tool::Degenerated(Edge)) continue;
        TopExp::Vertices(Edge, V2, V1, Standard_True);
        if (V2.IsNull() && V1.IsNull()) stat++;
      }
      if (stat != 0) {
        // extend the tool face and try again
        Handle(Geom_Surface) hSurf = BRep_Tool::Surface(pface->face);
#if CASVER >= 652        
        BRepLib_MakeFace MFace(hSurf, Standard_True);
#else
        BRepLib_MakeFace MFace(hSurf);
#endif
        TopoDS_Face eFace = MFace.Face();
        // get the intersection edge(s)
        BRepAlgoAPI_Section Sec(ssrc, eFace, Standard_False);
        Sec.ComputePCurveOn1(Standard_True);
        Sec.Approximation(Standard_True);
        Sec.Build();
        if (!Sec.IsDone()) {
          if (outLevel > 0) 
            printf(" EGADS Error: Can't Section (EG_solidBoolean)!\n");
          return EGADS_GEOMERR;
        }
        TopoDS_Shape scribe = Sec.Shape();
        // scribe the complete intersection
        BRepFeat_SplitShape Split(ssrc);
        for (Expe.Init(scribe, TopAbs_EDGE); Expe.More(); Expe.Next()) {
          TopoDS_Edge Edge = TopoDS::Edge(Expe.Current());
          TopoDS_Face Face;
          if (!Sec.HasAncestorFaceOn1(Edge, Face)) continue;
          Split.Add(Edge, Face);
        }
        Split.Build();
        if (!Split.IsDone()) {
          if (outLevel > 0) 
            printf(" EGADS Error: Can't Split (EG_solidBoolean)!\n");
          return EGADS_GEOMERR;
        }
        TopoDS_Shape newShape = Split.Shape();
        // map the faces for future union attribution
        TopTools_IndexedMapOfShape splmap, smap;
        TopExp::MapShapes(newShape, TopAbs_FACE, splmap);
        TopExp::MapShapes(ssrc,     TopAbs_FACE, smap);
        int *spltab = NULL;
        if (splmap.Extent() != 0) {
          spltab = (int *) EG_alloc(splmap.Extent()*sizeof(int));
          if (spltab == NULL) {
            if (outLevel > 0) 
              printf(" EGADS Error: Malloc of Split Table (EG_solidBoolean)!\n");
            return EGADS_MALLOC;
          }
          for (i = 0; i < splmap.Extent(); i++) spltab[i] = 0;
          for (i = 1; i <= smap.Extent(); i++) {
            TopoDS_Face dsface = TopoDS::Face(smap(i));
            const TopTools_ListOfShape& listFaces = Split.Modified(dsface);
            if (listFaces.Extent() > 0) {
              /* modified faces */
              TopTools_ListIteratorOfListOfShape it(listFaces);
              for (; it.More(); it.Next()) {
                TopoDS_Face genface = TopoDS::Face(it.Value());
                index = splmap.FindIndex(genface);
                if (index <= 0) continue;
                spltab[index-1] = i;
              }
            } else {
              index = splmap.FindIndex(dsface);
              if (index <= 0) continue;
              spltab[index-1] = i;
            }
          }
          for (i = 0; i < splmap.Extent(); i++)
            if (spltab[i] == 0) {
              printf(" EGADS Error: Mapping Failed (EG_solidBoolean)!\n");
              EG_free(spltab);
              return EGADS_GEOMERR;
            }
        }
        // redo the union
        BRepAlgoAPI_Fuse BSO(newShape, stool);
        if (!BSO.IsDone()) {
          printf(" EGADS Error: Can't do SBO Fusion (EG_solidBoolean)!\n");
          if (spltab != NULL) EG_free(spltab);
          return EGADS_GEOMERR;
        }
        result = BSO.Shape();
        iface  = 1;
        EG_matchMdlFace(BSO, newShape, iface, stool, result, &fmap);
        // patch up the face map
        if (spltab != NULL) {
          TopTools_IndexedMapOfShape rmap;
          TopExp::MapShapes(result, TopAbs_FACE, rmap);
          for (i = 0; i < rmap.Extent(); i++)
            if (fmap[i] > 0) {
              fmap[i] = spltab[fmap[i]-1];
              continue;
            }
//        for (i = 0; i < rmap.Extent(); i++) printf("  map %d = %d\n", i+1, fmap[i]);  
          EG_free(spltab);
        }
      } else {
        iface = 1;
        EG_matchMdlFace(BSO, ssrc, iface, stool, result, &fmap);
      }
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: SBO Fusion Exception (EG_solidBoolean)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: SBO Fusion Exception (EG_solidBoolean)!\n");
      return EGADS_GEOMERR;
    }    
    
  } else {

    if (tool->oclass != BODY) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Body (EG_solidBoolean)!\n");
      return EGADS_NOTBODY;
    }
    if (tool->mtype != SOLIDBODY) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Solid Body (EG_solidBoolean)!\n");
      return EGADS_NOTTOPO;
    }
    egadsBody    *pbods = (egadsBody *) tool->blind;
    TopoDS_Solid stool  = TopoDS::Solid(pbods->shape);
    try {
      BRepAlgoAPI_Common BSO(ssrc, stool);
      if (!BSO.IsDone()) {
        printf(" EGADS Error: Can't do SBO Intersection (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      result = BSO.Shape();
      iface  = 0;
      EG_matchMdlFace(BSO, ssrc, iface, stool, result, &fmap);
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: SBO Inters Exception (EG_solidBoolean)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: SBO Inters Exception (EG_solidBoolean)!\n");
      return EGADS_GEOMERR;
    }

  }
  
  int nWire  = 0;
  int nFace  = 0;
  int nSheet = 0;
  int nSolid = 0;
  TopExp_Explorer Exp;
  for (Exp.Init(result, TopAbs_WIRE,  TopAbs_FACE);
       Exp.More(); Exp.Next()) nWire++;
  for (Exp.Init(result, TopAbs_FACE,  TopAbs_SHELL);
       Exp.More(); Exp.Next()) nFace++;
  for (Exp.Init(result, TopAbs_SHELL, TopAbs_SOLID);
       Exp.More(); Exp.Next()) nSheet++;
  for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) nSolid++;
  if (outLevel > 1)
    printf(" Info: result has %d Solids, %d Sheets, %d Faces and %d Wires\n",
           nSolid, nSheet, nFace, nWire);
         
  int nBody = nWire+nFace+nSheet+nSolid;
  if (nBody == 0) {
    result.Nullify();
    if (outLevel > 0)
      printf(" EGADS Warning: Nothing found in result (EG_solidBoolean)!\n");
    return EGADS_NODATA;
  }
  
  egadsModel *mshape = new egadsModel;
  mshape->shape      = result;
  mshape->nbody      = nBody;
  mshape->bodies     = new egObject*[nBody];
  for (i = 0; i < nBody; i++) {
    stat = EG_makeObject(context, &mshape->bodies[i]);
    if (stat != EGADS_SUCCESS) {
      for (j = 0; j < i; j++) {
        egObject  *obj   = mshape->bodies[j];
        egadsBody *pbody = (egadsBody *) obj->blind;
        delete pbody;
        EG_deleteObject(mshape->bodies[j]);
      }
      delete [] mshape->bodies;
      delete mshape;
      if (fmap != NULL) EG_free(fmap);
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
    result.Nullify();
    for (i = 0; i < nBody; i++) {
      egObject  *obj   = mshape->bodies[i];
      egadsBody *pbody = (egadsBody *) obj->blind;
      delete pbody;
      EG_deleteObject(mshape->bodies[i]);
    }
    delete [] mshape->bodies;
    delete mshape;
    if (fmap != NULL) EG_free(fmap);
    return stat;
  }
  omodel->oclass = MODEL;
  omodel->blind  = mshape;
  EG_referenceObject(omodel, context);
  TopTools_IndexedMapOfShape smap, rmap;
  TopExp::MapShapes(ssrc,   TopAbs_FACE, smap);
  TopExp::MapShapes(result, TopAbs_FACE, rmap);
  
  for (i = 0; i < nBody; i++) {
    egObject  *pobj  = mshape->bodies[i];
    egadsBody *pbody = (egadsBody *) pobj->blind;
    pobj->topObj     = omodel;
    BRepCheck_Analyzer sCheck(pbody->shape);
    stat = EGADS_SUCCESS;
    if (!sCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Error: Result %d/%d is inValid (EG_solidBoolean)!\n",
               i+1, nBody);
      stat = EGADS_GEOMERR;
    }
    if (stat == EGADS_SUCCESS)
      stat = EG_traverseBody(context, i, pobj, omodel, pbody);
    if (stat != EGADS_SUCCESS) {
      mshape->nbody = i;
      EG_destroyTopology(omodel);
      delete [] mshape->bodies;
      delete mshape;
      if (fmap != NULL) EG_free(fmap);
      return stat;
    }
    for (j = 0; j < pmdl->nbody; j++) {
      egObject *bsrc = pmdl->bodies[j];
      EG_attriBodyDup(bsrc, pobj);
    }
    if (iface == 0) EG_attriBodyDup(tool, pobj);
    if (fmap != NULL) {
      // fill in the attributes from cut faces
      for (j = 0; j < pbody->faces.map.Extent(); j++) {
        TopoDS_Face dsface = TopoDS::Face(pbody->faces.map(j+1));
        index = rmap.FindIndex(dsface);
        if (index == 0) continue;
        index = fmap[index-1];
        if (index == 0) continue;
        if (outLevel > 2)
          printf(" %d:  face mapping[%d] = %d\n", i, j, index);
        if (index > 0) {
          for (k = 0; k < pmdl->nbody; k++) {
            egObject  *bsrc  = pmdl->bodies[k];
            egadsBody *pbods = (egadsBody *) bsrc->blind;
            int ind = pbods->faces.map.FindIndex(smap(index));
            if (ind == 0) continue;
            EG_attributeDup(pbods->faces.objs[ind-1], pbody->faces.objs[j]);
/*          printf(" %d:  face mapping[%d] = %d\n", i, j, index);
            EG_attributePrint(pbody->faces.objs[j]);  */
            break;
          }
        } else {
          if (iface == 0) {
            egadsBody *pbodt = (egadsBody *) tool->blind;
            EG_attributeDup(pbodt->faces.objs[-index-1], pbody->faces.objs[j]);
/*          printf(" %d:  face mapping[%d] = %d\n", i, j, index);
            EG_attributePrint(pbody->faces.objs[j]);  */
          } else {
            EG_attributeDup(face, pbody->faces.objs[j]);
          }
        }
      }
    }
  }
  if (fmap != NULL) EG_free(fmap);

  *model = omodel;
  return EGADS_SUCCESS;
}


int
EG_solidBoolean(const egObject *src, const egObject *tool, int oper, 
                      egObject **model)
{
  int             i, j, outLevel, index, stat, rev = 0, **fmap = NULL;
  egObject        *context, *omodel;
  const egObject  *face  = NULL;
  egadsBody       *pbodt = NULL;
  TopoDS_Shape    stool, result;
  TopExp_Explorer Exp;
  
  *model = NULL;
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->oclass == MODEL)      return EG_modelBoolean(src, tool, oper, model);
  if (src->oclass != BODY)       return EGADS_NOTBODY;
  if (src->mtype != SOLIDBODY)   return EGADS_NOTTOPO;
  if (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);

  if ((oper != SUBTRACTION) && (oper != INTERSECTION) && 
      (oper != FUSION)) {
    if (outLevel > 0) 
      printf(" EGADS Error: BAD Operator = %d (EG_solidBoolean)!\n",
             oper);
    return EGADS_RANGERR;
  }
  if (tool == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Tool (EG_solidBoolean)!\n");
    return EGADS_NULLOBJ;
  }
  if (tool->magicnumber != MAGIC) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool is not an EGO (EG_solidBoolean)!\n");
    return EGADS_NOTOBJ;
  }
  if (tool->blind == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool has no data (EG_solidBoolean)!\n");
    return EGADS_NODATA;
  }
  if (EG_context(tool) != context) {
    if (outLevel > 0) 
      printf(" EGADS Error: Context mismatch (EG_solidBoolean)!\n");
    return EGADS_MIXCNTX;
  }
  if ((oper == SUBTRACTION) && ((tool->oclass == FACE) ||
      ((tool->oclass == BODY) && (tool->mtype == FACEBODY)))) {
    if (tool->oclass == FACE) {
      face = tool;
    } else {
      egadsBody *pbodf = (egadsBody *) tool->blind;
      face = pbodf->faces.objs[0];
    }
    if (face == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Face Tool (EG_solidBoolean)!\n");
      return EGADS_NULLOBJ;
    }
    if (face->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Tool is not an EGO (EG_solidBoolean)!\n");
      return EGADS_NOTOBJ;
    }
    if (face->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Tool has no data (EG_solidBoolean)!\n");
      return EGADS_NODATA;
    }
  } else {
    if (tool->oclass != BODY) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Body (EG_solidBoolean)!\n");
      return EGADS_NOTBODY;
    }
    if (tool->mtype != SOLIDBODY) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Solid Body (EG_solidBoolean)!\n");
      return EGADS_NOTTOPO;
    }
  }

  egadsBody   *pbods = (egadsBody *) src->blind;
  TopoDS_Solid ssrc  = TopoDS::Solid(pbods->shape);
  if (face == NULL) {
    pbodt = (egadsBody *) tool->blind;
    stool = pbodt->shape;
  } else {
    egadsFace *pface = (egadsFace *) face->blind;
    stool = pface->face;
/*  BRep_Builder builder3D;
    TopoDS_Shell shell;
    builder3D.MakeShell(shell);
    builder3D.Add(shell, pface->face);
    stool = shell; */
  }
  
  if (oper == INTERSECTION) {

    try {
      BRepAlgoAPI_Common BSO(ssrc, stool);
      if (!BSO.IsDone()) {
        printf(" EGADS Error: Can't do SBO Intersection (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      result = BSO.Shape();
      for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
        TopoDS_Solid solid = TopoDS::Solid(Exp.Current());
        BRepCheck_Analyzer sCheck(solid);
        if (!sCheck.IsValid()) {
          if (outLevel > 1)
            printf(" EGADS Info: Inters Failed -- try reverse (EG_solidBoolean)!\n");
           // intersection is associtive -- see if it works the other way
          rev = 1;
          BRepAlgoAPI_Common BSO(stool, ssrc);
          if (!BSO.IsDone()) {
            printf(" EGADS Error: Can't do SBO Inters (EG_solidBoolean)!\n");
            return EGADS_GEOMERR;
          }
          result = BSO.Shape();
          EG_matchFaces(BSO, src, tool, result, &fmap);
          break;
        }
      }
      if (rev == 0) EG_matchFaces(BSO, src, tool, result, &fmap);
    }
    catch (...) {
      if (rev == 1) {
        printf(" EGADS Error: SBO Inters Exception (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      rev = -1;
    }
    
    if (rev == -1) {
      rev = 1;
      if (outLevel > 1)
        printf(" EGADS Info: Inters Aborted -- try reverse (EG_solidBoolean)!\n");
      try {
        BRepAlgoAPI_Common BSO(stool, ssrc);
        if (!BSO.IsDone()) {
          printf(" EGADS Error: Can't do SBO Inters (EG_solidBoolean)!\n");
          return EGADS_GEOMERR;
        }
        result = BSO.Shape();
        EG_matchFaces(BSO, src, tool, result, &fmap);
      }
      catch (Standard_Failure) {
        printf(" EGADS Error: SBO Inters Exception (EG_solidBoolean)!\n");
        Handle_Standard_Failure e = Standard_Failure::Caught();
        printf("              %s\n", e->GetMessageString());
        return EGADS_GEOMERR;
      }
      catch (...) {
        printf(" EGADS Error: SBO Inters Exception (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
    }

  } else if (oper == SUBTRACTION) {

    try {
      BRepAlgoAPI_Cut BSO(ssrc, stool);
      if (!BSO.IsDone()) {
        printf(" EGADS Error: Can't do SBO Subtraction (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      result = BSO.Shape();
      EG_matchFaces(BSO, src, tool, result, &fmap);
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: SBO Subraction Exception (EG_solidBoolean)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: SBO Subraction Exception (EG_solidBoolean)!\n");
      return EGADS_GEOMERR;
    }

  } else {

    try {
      BRepAlgoAPI_Fuse BSO(ssrc, stool);
      if (!BSO.IsDone()) {
        printf(" EGADS Error: Can't do SBO Fusion (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      result = BSO.Shape();
      i = 0;
      for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) i++;
      if (i != 1) 
        printf(" EGADS Warning: SBO Union has %d Bodies (EG_solidBoolean)!\n",
               i);
      for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
        TopoDS_Solid solid = TopoDS::Solid(Exp.Current());
        BRepCheck_Analyzer sCheck(solid);
        if (!sCheck.IsValid()) {
          if (outLevel > 1)
            printf(" EGADS Info: Union Failed -- try reverse (EG_solidBoolean)!\n");
          // union is associtive -- see if it works the other way
          rev = 1;
          BRepAlgoAPI_Fuse BSO(stool, ssrc);
          if (!BSO.IsDone()) {
            printf(" EGADS Error: Can't do SBO Union (EG_solidBoolean)!\n");
            return EGADS_GEOMERR;
          }
          result = BSO.Shape();
          EG_matchFaces(BSO, src, tool, result, &fmap);
          break;
        }
      }
      if (rev == 0) EG_matchFaces(BSO, src, tool, result, &fmap);
    }
    catch (...) {
      if (rev == 1) {
        printf(" EGADS Error: SBO Fusion Exception (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
      rev = -1;
    }
    
    if (rev == -1) {
      rev = 1;
      if (outLevel > 1)
        printf(" EGADS Info: Fusion Aborted -- try reverse (EG_solidBoolean)!\n");
      try {
        BRepAlgoAPI_Fuse BSO(stool, ssrc);
        if (!BSO.IsDone()) {
          printf(" EGADS Error: Can't do SBO Fusion (EG_solidBoolean)!\n");
          return EGADS_GEOMERR;
        }
        result = BSO.Shape();
        EG_matchFaces(BSO, src, tool, result, &fmap);
      }
      catch (Standard_Failure) {
        printf(" EGADS Error: SBO Fusion Exception (EG_solidBoolean)!\n");
        Handle_Standard_Failure e = Standard_Failure::Caught();
        printf("              %s\n", e->GetMessageString());
        return EGADS_GEOMERR;
      }
      catch (...) {
        printf(" EGADS Error: SBO Fusion Exception (EG_solidBoolean)!\n");
        return EGADS_GEOMERR;
      }
    } 
    
  }
  
  int nBody = 0;
  for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) nBody++;
  i = 0;
  for (Exp.Init(result, TopAbs_SOLID); Exp.More(); Exp.Next()) {
    TopoDS_Solid solid = TopoDS::Solid(Exp.Current());
    BRepCheck_Analyzer sCheck(solid);
    if (!sCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Warning: Solid %d/%d is invalid (EG_solidBoolean)!\n",
               i+1, nBody);
      if (fmap != NULL) {
        for (j = 0; j < nBody; j++) EG_free(fmap[j]);
        EG_free(fmap);
      }
      return EGADS_CONSTERR;
    }
    i++;
  }
  if (outLevel > 1)
    printf("   Boolean Solid Oper result has #%d solids!\n", nBody);
  if (nBody == 0) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL SBO Result (EG_solidBoolean)!\n");
    return EGADS_NOTFOUND;
  }
  
  egadsModel *mshape = new egadsModel;
  mshape->shape      = result;
  mshape->nbody      = nBody;
  mshape->bodies     = new egObject*[nBody];
  for (i = 0; i < nBody; i++) {
    stat = EG_makeObject(context, &mshape->bodies[i]);
    if (stat != EGADS_SUCCESS) {
      for (j = 0; j < i; j++) {
        egObject  *obj   = mshape->bodies[j];
        egadsBody *pbody = (egadsBody *) obj->blind;
        delete pbody;
        EG_deleteObject(mshape->bodies[j]);
      }
      delete [] mshape->bodies;
      delete mshape;
      if (fmap != NULL) {
        for (j = 0; j < nBody; j++) EG_free(fmap[j]);
        EG_free(fmap);
      }
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
  for (Exp.Init(mshape->shape, TopAbs_SOLID); Exp.More(); Exp.Next()) {
    egObject  *obj   = mshape->bodies[i++];
    egadsBody *pbody = (egadsBody *) obj->blind;
    pbody->shape     = Exp.Current();
  }

  stat = EG_makeObject(context, &omodel);
  if (stat != EGADS_SUCCESS) {
    result.Nullify();
    for (i = 0; i < nBody; i++) {
      egObject  *obj   = mshape->bodies[i];
      egadsBody *pbody = (egadsBody *) obj->blind;
      delete pbody;
      EG_deleteObject(mshape->bodies[i]);
    }
    delete [] mshape->bodies;
    delete mshape;
    if (fmap != NULL) {
      for (j = 0; j < nBody; j++) EG_free(fmap[j]);
      EG_free(fmap);
    }
    return stat;
  }
  omodel->oclass = MODEL;
  omodel->blind  = mshape;
  EG_referenceObject(omodel, context);
  
  for (i = 0; i < nBody; i++) {
    egObject  *pobj  = mshape->bodies[i];
    egadsBody *pbody = (egadsBody *) pobj->blind;
    pobj->topObj     = omodel;
    stat = EG_traverseBody(context, i, pobj, omodel, pbody);
    if (stat != EGADS_SUCCESS) {
      mshape->nbody = i;
      EG_destroyTopology(omodel);
      delete [] mshape->bodies;
      delete mshape;
      if (fmap != NULL) {
        for (j = 0; j < nBody; j++) EG_free(fmap[j]);
        EG_free(fmap);
      }
      return stat;
    }
    EG_attriBodyDup(src,  pobj);
    if (face == NULL) EG_attriBodyDup(tool, pobj);
    if (fmap != NULL) {
      // fill in the attributes from cut faces
      for (j = 0; j < pbody->faces.map.Extent(); j++) {
        index = fmap[i][j];
        if (index == 0) continue;
        if (outLevel > 2)
          printf(" %d:  face mapping[%d] = %d\n", i, j, index);
        if (index > 0) {
          EG_attributeDup(pbods->faces.objs[index-1], pbody->faces.objs[j]);
        } else {
          if (face == NULL) {
            EG_attributeDup(pbodt->faces.objs[-index-1], pbody->faces.objs[j]);
          } else {
            EG_attributeDup(face, pbody->faces.objs[j]);
          }
        }
      }
      EG_free(fmap[i]);
    }
  }
  if (fmap != NULL) EG_free(fmap);

  *model = omodel;
  return EGADS_SUCCESS;
}


int
EG_intersection(const egObject *src, const egObject *tool, int *nEdge, 
                /*@null@*/ egObject ***facEdg, egObject **model)
{
  int             i, j, n, stat, outLevel, nloop, sense, index, found;
  int             plane = 0;
  egObject        *context, *omodel, *geom, **list;
  TopoDS_Shape    s1, s2, result;
  TopoDS_Vertex   V1, V2, Vs, lV1, lV2;
  TopExp_Explorer Exp;
  
  *nEdge = 0;
  *model = NULL;
  if (facEdg != NULL) *facEdg = NULL;
  if  (src == NULL)               return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if  (src->oclass != BODY)       return EGADS_NOTBODY;
  if ((src->mtype != SOLIDBODY) && (src->mtype != SHEETBODY) &&
      (src->mtype != FACEBODY))   return EGADS_NOTTOPO;
  if  (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);

  if (tool == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Tool (EG_intersection)!\n");
    return EGADS_NULLOBJ;
  }
  if (tool->magicnumber != MAGIC) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool is not an EGO (EG_intersection)!\n");
    return EGADS_NOTOBJ;
  }
  if (tool->blind == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool has no data (EG_intersection)!\n");
    return EGADS_NODATA;
  }
  if (EG_context(tool) != context) {
    if (outLevel > 0) 
      printf(" EGADS Error: Context mismatch (EG_intersection)!\n");
    return EGADS_MIXCNTX;
  }
  const egObject *face;
  if (tool->oclass == BODY) {
    if (tool->mtype != FACEBODY) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Face Body (EG_intersection)!\n");
      return EGADS_NOTTOPO;
    }
    egadsBody *pbodf = (egadsBody *) tool->blind;
    face = pbodf->faces.objs[0];
  } else {
    if (tool->oclass != FACE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Tool is not a Face (EG_intersection)!\n");
      return EGADS_NOTBODY;
    }
    face = tool;
  }
  egadsFace *pface = (egadsFace *) face->blind;
  egadsBody *pbody = (egadsBody *) src->blind;
  geom = pface->surface;
  if (geom->blind == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: Tool Surface is NULL (EG_intersection)!\n");
    return EGADS_NOTGEOM;
  }
  if (geom->mtype == PLANE) plane = 1;

  s1 = pface->face;
  s2 = pbody->shape;
  BRepAlgoAPI_Section Sec(s1, s2, Standard_False);
  Sec.ComputePCurveOn1(Standard_True);
  Sec.Approximation(Standard_True);
  Sec.Build();
  if (!Sec.IsDone()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Can't Section (EG_intersection)!\n");
    return EGADS_GEOMERR;
  }
  result = Sec.Shape();

  TopTools_IndexedMapOfShape MapE;
  TopExp::MapShapes(result, TopAbs_EDGE, MapE);
  int nedge = MapE.Extent();
  if (nedge == 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Intersection (EG_intersection)!\n");
    return EGADS_CONSTERR;
  }
  
  // find the loops
  loopInfo *info = new loopInfo[nedge];
  for (i = 0; i <  nedge; i++) info[i].lIndex = info[i].sense = 0;
  for (nloop = i = 1; i <= nedge; i++) {
    if (info[i-1].lIndex != 0) continue;
    TopoDS_Shape shape = MapE(i);
    TopoDS_Edge  Edge  = TopoDS::Edge(shape);
    TopExp::Vertices(Edge, V2, V1, Standard_True);
    sense = -1;
    if (Edge.Orientation() != TopAbs_REVERSED) {
      sense = 1;
      Vs    = V2;
      V2    = V1;
      V1    = Vs;
    }
    Vs    = V1;
    index = 0;
    info[i-1].lIndex = nloop;
    info[i-1].index  = index;
    info[i-1].sense  = sense;
    while (!Vs.IsSame(V2)) {
      for (j = 1; j <= nedge; j++) {
        if (info[j-1].lIndex != 0) continue;
        TopoDS_Edge lEdge = TopoDS::Edge(MapE(j));
        TopExp::Vertices(lEdge, lV1, lV2, Standard_True);
        if (V2.IsSame(lV1)) {
          index++;
          sense = 1;
          if (Edge.Orientation() == TopAbs_REVERSED) sense = -1;
          info[j-1].lIndex = nloop;
          info[j-1].index  = index;
          info[j-1].sense  = sense;
          V2 = lV2;
          break;
        } else if (V2.IsSame(lV2)) {
          index++;
          sense = -1;
          if (Edge.Orientation() == TopAbs_REVERSED) sense = 1;
          info[j-1].lIndex = nloop;
          info[j-1].index  = index;
          info[j-1].sense  = sense;
          V2 = lV1;
          break;
        }      
      }
      if (j > nedge) {
        /* we are open -- check the other direction */
        TopExp::Vertices(Edge, V2, V1, Standard_True);
        sense = -1;
        if (Edge.Orientation() != TopAbs_FORWARD) {
          sense = 1;
          Vs    = V2;
          V2    = V1;
          V1    = Vs;
        }
        j = 1;
        while (j <= nedge) {
          for (j = 1; j <= nedge; j++) {
            if (info[j-1].lIndex != 0) continue;
            TopoDS_Edge lEdge = TopoDS::Edge(MapE(j));
            TopExp::Vertices(lEdge, lV1, lV2, Standard_True);
            if (V2.IsSame(lV1)) {
              index++;
              sense = 1;
              if (Edge.Orientation() == TopAbs_FORWARD) sense = -1;
              info[j-1].lIndex = nloop;
              info[j-1].index  = index;
              info[j-1].sense  = sense;
              V2 = lV2;
              break;
            } else if (V2.IsSame(lV2)) {
              index++;
              sense = -1;
              if (Edge.Orientation() == TopAbs_FORWARD) sense = 1;
              info[j-1].lIndex = nloop;
              info[j-1].index  = index;
              info[j-1].sense  = sense;
              V2 = lV1;
              break;
            }      
          }
        }
        break;
      }
    }
    nloop++;
  }
  nloop--;
  
  // create the EGADS objects for the WireBodies
  egObject **wireo = new egObject*[nloop];
  for (i = 0; i < nloop; i++) {
    stat = EG_makeObject(context, &wireo[i]);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Loop object (EG_intersection)!\n");
      for (j = 0; j < i; j++) EG_deleteObject(wireo[j]);
      delete [] info;
      return stat;
    }
  }

  // make the OCC Wires and then the WireBodies
  TopoDS_Compound compound;
  BRep_Builder    builder3D;
  builder3D.MakeCompound(compound);
  for (i = 0; i < nloop; i++) { 
    BRepBuilderAPI_MakeWire MW;
    index = 0;
    do {
      for (found = j = 0; j < nedge; j++)
        if (info[j].index == index)
          if (info[j].lIndex == i+1) {
            found = j+1;
            break;
          }
      if (found == 0) continue;
      TopoDS_Shape shape = MapE(found);
      TopoDS_Edge  Edge  = TopoDS::Edge(shape);
      if (Edge.Orientation() == TopAbs_REVERSED) {
        if (info[found-1].sense ==  1) Edge.Orientation(TopAbs_FORWARD);
      } else {
        if (info[found-1].sense == -1) Edge.Orientation(TopAbs_REVERSED);
      }
      MW.Add(Edge);
      if (MW.Error()) {
        if (outLevel > 0)
          printf(" EGADS Error: Problem with Edge %d (EG_intersection)!\n",
                 sense);
        for (j = 0; j < nloop; j++) EG_deleteObject(wireo[j]);
        delete [] info;
        return EGADS_NODATA;
      }
      index++;
    } while (found != 0);
    if (!MW.IsDone()) {
      if (outLevel > 0)
        printf(" EGADS Error: Problem with Loop (EG_intersection)!\n");
      for (j = 0; j < nloop; j++) EG_deleteObject(wireo[j]);
      delete [] info;
      return EGADS_NODATA;
    }
    TopoDS_Wire Wire = MW.Wire();
    builder3D.Add(compound, Wire);
    if (outLevel > 1)
      printf(" Wire %d made with %d edges!\n", i+1, index);

    egadsBody *pbodw   = new egadsBody;
    wireo[i]->oclass   = BODY;
    wireo[i]->mtype    = WIREBODY;
    pbodw->nodes.objs  = NULL;
    pbodw->edges.objs  = NULL;
    pbodw->loops.objs  = NULL;
    pbodw->faces.objs  = NULL;
    pbodw->shells.objs = NULL;
    pbodw->senses      = NULL;
    pbodw->shape       = Wire;
    wireo[i]->blind    = pbodw;
    stat = EG_traverseBody(context, i, wireo[i], wireo[i], pbodw);
    if (stat != EGADS_SUCCESS) {
      delete pbodw;
      for (j = 0; j < nloop; j++) EG_deleteObject(wireo[j]);
      delete [] info;
      return stat;
    }
  }
  delete [] info;
  
  // fix up the WireBodies for PCurves
  
  if (plane == 0) {
    egadsSurface *psurf = (egadsSurface *) geom->blind;
    Handle(Geom_Surface) hSurface = psurf->handle;
    for (i = 0; i < nloop; i++) {
      egObject  *bobj  = wireo[i];
      egadsBody *pbodw = (egadsBody *) bobj->blind;
      egObject  *lobj  = pbodw->loops.objs[0];
      egadsLoop *ploop = (egadsLoop *) lobj->blind;
      egObject  *sobj  = NULL;
      egObject **edgeo = new egObject*[2*ploop->nedges];
      for (j = 0; j < ploop->nedges; j++) {
        edgeo[j] = ploop->edges[j];
        edgeo[j+ploop->nedges] = NULL;
      }
      delete [] ploop->edges;
      ploop->edges = edgeo; 
      stat = EG_makeObject(context, &sobj);
      if (stat != EGADS_SUCCESS) continue;
      sobj->topObj = bobj;
      EG_completeSurf(sobj, hSurface);
      EG_referenceObject(sobj, lobj);
      ploop->surface = sobj;
      for (j = 0; j < ploop->nedges; j++) {
        egObject  *eobj  = ploop->edges[j];
        egadsEdge *pedge = (egadsEdge *) eobj->blind;
        TopoDS_Edge Edge = pedge->edge;
        Handle(Geom2d_Curve) hPCurv = Sec.PCurveOn1(Edge);
        stat = EG_makeObject(context, &edgeo[j+ploop->nedges]);
        if (stat != EGADS_SUCCESS) continue;
        edgeo[j+ploop->nedges]->topObj = bobj;
        EG_completePCurve(edgeo[j+ploop->nedges],  hPCurv);
        EG_referenceObject(edgeo[j+ploop->nedges], lobj);
      }
    }
  }
  
  // Attach the Attributes

  for (i = 0; i < nloop; i++) {
    egObject  *bobj  = wireo[i];
    egadsBody *pbodw = (egadsBody *) bobj->blind;
    for (j = 0; j < pbodw->edges.map.Extent(); j++) {
      TopoDS_Edge Edge = TopoDS::Edge(pbodw->edges.map(j+1));
      TopoDS_Face Face;
      if (Sec.HasAncestorFaceOn2(Edge, Face)) {
        index = pbody->faces.map.FindIndex(Face);
        if (index <= 0) continue;
        EG_attributeDup(pbody->faces.objs[index-1], pbodw->edges.objs[j]);
      }
    }
  }
  
  // fill in the Face/Edge pairs (if requested)
  if (facEdg != NULL) {
    *facEdg = list = (egObject **) EG_alloc(2*nedge*sizeof(egObject *));
    if (list != NULL)
      for (n = i = 0; i < nloop; i++) {
        egObject  *bobj  = wireo[i];
        egadsBody *pbodw = (egadsBody *) bobj->blind;
        for (j = 0; j < pbodw->edges.map.Extent(); j++) {
          TopoDS_Edge Edge = TopoDS::Edge(pbodw->edges.map(j+1));
          TopoDS_Face Face;
          if (Sec.HasAncestorFaceOn2(Edge, Face)) {
            index = pbody->faces.map.FindIndex(Face);
            if (index <= 0) continue;
            list[n  ] = pbody->faces.objs[index-1];
            list[n+1] = pbodw->edges.objs[j];
            n += 2;
          }
        }
      }
    *nEdge = n/2;
  }
  
  // make the EGADS model

  egadsModel *mshape = new egadsModel;
  mshape->shape      = compound;
  mshape->nbody      = nloop;
  mshape->bodies     = wireo;
  stat = EG_makeObject(context, &omodel);
  if (stat != EGADS_SUCCESS) {
    compound.Nullify();
    for (i = 0; i < nloop; i++) {
      egObject  *obj   = mshape->bodies[i];
      egadsBody *pbodw = (egadsBody *) obj->blind;
      delete pbodw;
      EG_deleteObject(mshape->bodies[i]);
    }
    delete [] mshape->bodies;
    delete mshape;
    return stat;
  }
  omodel->oclass = MODEL;
  omodel->blind  = mshape;
  for (i = 0; i < nloop; i++) {
    EG_referenceObject(mshape->bodies[i], omodel);
    EG_removeCntxtRef(mshape->bodies[i]);
  }
  EG_referenceObject(omodel, context);

  *model = omodel;
  return EGADS_SUCCESS;
}


int
EG_imprintBody(const egObject *src, int nedge, const egObject **facEdg, 
                      egObject **result)
{
  int      i, outLevel, stat;
  egObject *context, *obj;
  
  *result = NULL;
  if  (src == NULL)               return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if  (src->oclass != BODY)       return EGADS_NOTBODY;
  if ((src->mtype != SOLIDBODY) && (src->mtype != SHEETBODY) &&
      (src->mtype != FACEBODY))   return EGADS_NOTTOPO;
  if  (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if (nedge <= 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Edges (EG_imprintBody)!\n");
    return EGADS_NODATA;
  }
  if (facEdg == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL facEdg Pointer (EG_imprintBody)!\n");
    return EGADS_NULLOBJ;
  }
  for (i = 0; i < 2*nedge; i++) {
    if (facEdg[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Object %d (EG_imprintBody)!\n", i/2+1);
      return EGADS_NULLOBJ;
    }
    if (facEdg[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not an EGO (EG_imprintBody)!\n",
               i/2+1);
      return EGADS_NOTOBJ;
    }
    if (facEdg[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d has no data (EG_imprintBody)!\n",
               i/2+1);
      return EGADS_NODATA;
    }
  }
  egadsBody *pbody = (egadsBody *) src->blind;
  for (i = 0; i < nedge; i++) {
    if (facEdg[2*i  ]->oclass != FACE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not FACE (EG_imprintBody)!\n", i);
      return EGADS_NOTTOPO;
    }
    egadsFace *pface = (egadsFace *) facEdg[2*i]->blind;
    if (pbody->faces.map.FindIndex(pface->face) <= 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face %d is not in Body (EG_imprintBody)!\n", i);
      return EGADS_NOTBODY;
    }
    if (facEdg[2*i+1]->oclass != EDGE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not EDGE (EG_imprintBody)!\n", i);
      return EGADS_NOTTOPO;
    }
    egadsEdge *pedge = (egadsEdge *) facEdg[2*i+1]->blind;
    if (pbody->edges.map.FindIndex(pedge->edge) > 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge %d is in Body (EG_imprintBody)!\n", i);
      return EGADS_NOTBODY;
    }
  }
  
  TopoDS_Shape newShape;
  BRepFeat_SplitShape Split(pbody->shape);
  try {
    for (i = 0; i < nedge; i++) {
      egadsFace *pface = (egadsFace *) facEdg[2*i  ]->blind;
      egadsEdge *pedge = (egadsEdge *) facEdg[2*i+1]->blind;
      Split.Add(pedge->edge, pface->face);
    }
    Split.Build();
    if (!Split.IsDone()) {
      if (outLevel > 0) 
        printf(" EGADS Error: Can't Split (EG_imprintBody)!\n");
      return EGADS_GEOMERR;
    }
    newShape = Split.Shape();
  }
  catch (Standard_Failure) {
    printf(" EGADS Warning: Split Construction Error (EG_imprintBody)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("                %s\n", e->GetMessageString());
    return EGADS_CONSTERR;
  }
  catch (...) {
    printf(" EGADS Warning: Split Construction Error (EG_imprintBody)!\n");
    return EGADS_CONSTERR;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_imprintBody)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = src->mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // map the Attributes
  EG_attriBodyDup(src, obj);
  for (i = 0; i < nedge; i++) {
    egadsFace *pface = (egadsFace *) facEdg[2*i]->blind;
    const TopTools_ListOfShape& listFaces = Split.Modified(pface->face);
    if (listFaces.Extent() > 0) {
      /* modified faces */
      TopTools_ListIteratorOfListOfShape it(listFaces);
      for (; it.More(); it.Next()) {
        TopoDS_Face genface = TopoDS::Face(it.Value());
        int index = pbods->faces.map.FindIndex(genface);
        if (index <= 0) continue;
        EG_attributeDup(facEdg[2*i], pbods->faces.objs[index-1]);
      }
    }
  }

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_filletBody(const egObject *src, int nedge, const egObject **edges, 
              double radius, egObject **result)
{
  int      i, k, outLevel, stat;
  egObject *context, *obj, *face;
  
  *result = NULL;
  if  (src == NULL)               return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if  (src->oclass != BODY)       return EGADS_NOTBODY;
  if ((src->mtype != SOLIDBODY) && 
      (src->mtype != SHEETBODY))  return EGADS_NOTTOPO;
  if  (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if (nedge <= 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Edges (EG_filletBody)!\n");
    return EGADS_NODATA;
  }
  if (edges == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Edge Pointer (EG_filletBody)!\n");
    return EGADS_NULLOBJ;
  }
  egadsBody *pbody = (egadsBody *) src->blind;
  for (k = i = 0; i < nedge; i++) {
    if (edges[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Edge Object %d (EG_filletBody)!\n", i+1);
      return EGADS_NULLOBJ;
    }
    if (edges[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge Object %d is not an EGO (EG_filletBody)!\n",
               i+1);
      return EGADS_NOTOBJ;
    }
    if (edges[i]->oclass != EDGE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not EDGE (EG_filletBody)!\n", i+1);
      return EGADS_NOTTOPO;
    }
    if (edges[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge Object %d has no data (EG_filletBody)!\n",
               i+1);
      return EGADS_NODATA;
    }
    egadsEdge *pedge = (egadsEdge *) edges[i]->blind;
    if (pbody->edges.map.FindIndex(pedge->edge) == 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge %d is NOT in Body (EG_filletBody)!\n", 
               i+1);
      return EGADS_NOTBODY;
    }
    if (edges[i]->mtype != DEGENERATE) k++;
  }
  if (k == 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No nonDegenerate Edges (EG_filletBody)!\n");
    return EGADS_NODATA;
  }
  BRepCheck_Analyzer check(pbody->shape);
  if (!check.IsValid()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid Input Body (EG_filletBody)!\n");
    return EGADS_TOPOERR;
  }
  
  // fillet the body
  BRepFilletAPI_MakeFillet fillet(pbody->shape);
  for (i = 0; i < nedge; i++) {
    if (edges[i]->mtype == DEGENERATE) continue;
    egadsEdge *pedge = (egadsEdge *) edges[i]->blind;
    fillet.Add(radius, pedge->edge);
  }
  signal(SIGSEGV, segfault_handler);
  switch (stat = setjmp(jmpenv)) {
  case 0:
    try {
      fillet.Build();
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: Fillet Exception (EG_filletBody)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: Fillet Exception (EG_filletBody)!\n");
      return EGADS_GEOMERR;
    }
    break;
  default:
    printf(" EGADS Fatal Error: OCC SegFault %d (EG_filletBody)!\n",
           stat);
    signal(SIGSEGV, SIG_DFL);
    return EGADS_OCSEGFLT;
  }
  signal(SIGSEGV, SIG_DFL);
  if (!fillet.IsDone()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Can't Fillet (EG_filletBody)!\n");
    return EGADS_GEOMERR;
  }
  TopoDS_Shape newShape = fillet.Shape();
  BRepCheck_Analyzer fCheck(newShape);
  if (!fCheck.IsValid()) {
    Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
    sfs->Perform();
    TopoDS_Shape fixedShape = sfs->Shape();
    if (fixedShape.IsNull()) {
      if (outLevel > 0) 
        printf(" EGADS Error: Filleted Body is invalid (EG_filletBody)!\n");
      return EGADS_GEOMERR;
    } else {
      BRepCheck_Analyzer sfCheck(fixedShape);
      if (!sfCheck.IsValid()) {
        printf(" EGADS Error: Fixed Body is invalid (EG_filletBody)!\n");
        return EGADS_GEOMERR;
      } else {
        newShape = fixedShape;
      }
    }
  }
  
  // make sure we have the correct result!
  if (newShape.ShapeType() == TopAbs_COMPOUND) {
    int nshell = 0, nsolid = 0;
    TopExp_Explorer Exp;
    for (Exp.Init(newShape, TopAbs_SHELL, TopAbs_SOLID);
         Exp.More(); Exp.Next()) nshell++;
    for (Exp.Init(newShape, TopAbs_SOLID); Exp.More(); Exp.Next()) nsolid++;
    if (nshell+nsolid != 1) {
      if (outLevel > 0) 
        printf(" EGADS Error: Number of Results = %d (EG_filletBody)!\n",
               nshell+nsolid);
      return EGADS_CONSTERR;
    }
    if (nshell == 1) {
      Exp.Init(newShape, TopAbs_SHELL, TopAbs_SOLID);
      newShape = Exp.Current();
      }
    if (nsolid == 1) {
      Exp.Init(newShape, TopAbs_SOLID);
      newShape = Exp.Current();
    }
  }
  if ((newShape.ShapeType() != TopAbs_SOLID) &&
      (newShape.ShapeType() != TopAbs_SHELL)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Incorrect Result (EG_filletBody)!\n");
    return EGADS_CONSTERR;
  }
  if ((src->mtype == SOLIDBODY) && 
      (newShape.ShapeType() != TopAbs_SOLID)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Result Not a Solid (EG_filletBody)!\n");
    return EGADS_CONSTERR;
  }
  if ((src->mtype == SHEETBODY) && 
      (newShape.ShapeType() != TopAbs_SHELL)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Result Not a Sheet (EG_filletBody)!\n");
    return EGADS_CONSTERR;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_filletBody)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = src->mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // map the Attributes
  EG_attriBodyDup(src, obj);
  for (i = 0; i < pbody->faces.map.Extent(); i++) {
    face = pbody->faces.objs[i];
    egadsFace *pface = (egadsFace *) face->blind;
    const TopTools_ListOfShape& listFaces = fillet.Modified(pface->face);
    if (listFaces.Extent() > 0) {
      /* modified faces */
      TopTools_ListIteratorOfListOfShape it(listFaces);
      for (; it.More(); it.Next()) {
        TopoDS_Face genface = TopoDS::Face(it.Value());
        int index = pbods->faces.map.FindIndex(genface);
        if (index > 0) EG_attributeDup(face, pbods->faces.objs[index-1]);
      }
    }
  }

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_chamferBody(const egObject *src, int nedge, const egObject **edges, 
               const egObject **faces, double dis1, double dis2, 
               egObject **result)
{
  int      i, k, outLevel, stat;
  egObject *context, *obj, *face;
  
  *result = NULL;
  if  (src == NULL)               return EGADS_NULLOBJ;
  if  (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if  (src->oclass != BODY)       return EGADS_NOTBODY;
  if ((src->mtype != SOLIDBODY) && 
      (src->mtype != SHEETBODY))  return EGADS_NOTTOPO;
  if  (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if (nedge <= 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Edges (EG_chamferBody)!\n");
    return EGADS_NODATA;
  }
  if (edges == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Edge Pointer (EG_chamferBody)!\n");
    return EGADS_NULLOBJ;
  }
  if (faces == NULL) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Face Pointer (EG_chamferBody)!\n");
    return EGADS_NULLOBJ;
  }
  egadsBody *pbody = (egadsBody *) src->blind;
  for (k = i = 0; i < nedge; i++) {

    if (edges[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Edge Object %d (EG_chamferBody)!\n", i+1);
      return EGADS_NULLOBJ;
    }
    if (edges[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge Object %d is not an EGO (EG_chamferBody)!\n",
               i+1);
      return EGADS_NOTOBJ;
    }
    if (edges[i]->oclass != EDGE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not EDGE (EG_chamferBody)!\n", i+1);
      return EGADS_NOTTOPO;
    }
    if (edges[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge Object %d has no data (EG_chamferBody)!\n",
               i+1);
      return EGADS_NODATA;
    }
    egadsEdge *pedge = (egadsEdge *) edges[i]->blind;
    if (pbody->edges.map.FindIndex(pedge->edge) == 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Edge %d is NOT in Body (EG_chamferBody)!\n", 
               i+1);
      return EGADS_NOTBODY;
    }
    if (edges[i]->mtype != DEGENERATE) k++;

    if (faces[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Face Object %d (EG_chamferBody)!\n", i+1);
      return EGADS_NULLOBJ;
    }
    if (faces[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Object %d is not an EGO (EG_chamferBody)!\n",
               i+1);
      return EGADS_NOTOBJ;
    }
    if (faces[i]->oclass != FACE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not FACE (EG_chamferBody)!\n", i+1);
      return EGADS_NOTTOPO;
    }
    if (faces[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Object %d has no data (EG_chamferBody)!\n",
               i+1);
      return EGADS_NODATA;
    }
    egadsFace *pface = (egadsFace *) faces[i]->blind;
    if (pbody->faces.map.FindIndex(pface->face) == 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face %d is NOT in Body (EG_chamferBody)!\n", 
               i+1);
      return EGADS_NOTBODY;
    }
  }
  if (k == 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No nonDegenerate Edges (EG_chamferBody)!\n");
    return EGADS_NODATA;
  }
  BRepCheck_Analyzer check(pbody->shape);
  if (!check.IsValid()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid Input Body (EG_chamferBody)!\n");
    return EGADS_TOPOERR;
  }
  
  // chamfer the body
  BRepFilletAPI_MakeChamfer chamfer(pbody->shape);
  for (i = 0; i < nedge; i++) {
    if (edges[i]->mtype == DEGENERATE) continue;
    egadsEdge *pedge = (egadsEdge *) edges[i]->blind;
    egadsFace *pface = (egadsFace *) faces[i]->blind;
    chamfer.Add(dis1, dis2, pedge->edge, pface->face);
  }
  signal(SIGSEGV, segfault_handler);
  switch (stat = setjmp(jmpenv)) {
  case 0:
    try {
      chamfer.Build();
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: Chamfer Exception (EG_chamferBody)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: Chamfer Exception (EG_chamferBody)!\n");
      return EGADS_GEOMERR;
    }
    break;
  default:
    printf(" EGADS Fatal Error: OCC SegFault %d (EG_chamferBody)!\n",
           stat);
    signal(SIGSEGV, SIG_DFL);
    return EGADS_OCSEGFLT;
  }
  signal(SIGSEGV, SIG_DFL);
  if (!chamfer.IsDone()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Can't Chamfer (EG_chamferBody)!\n");
    return EGADS_GEOMERR;
  }
  TopoDS_Shape newShape = chamfer.Shape();
  BRepCheck_Analyzer fCheck(newShape);
  if (!fCheck.IsValid()) {
    Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
    sfs->Perform();
    TopoDS_Shape fixedShape = sfs->Shape();
    if (fixedShape.IsNull()) {
      if (outLevel > 0) 
        printf(" EGADS Error: Chamfered Body is invalid (EG_chamferBody)!\n");
      return EGADS_GEOMERR;
    } else {
      BRepCheck_Analyzer sfCheck(fixedShape);
      if (!sfCheck.IsValid()) {
        printf(" EGADS Error: Fixed Body is invalid (EG_chamferBody)!\n");
        return EGADS_GEOMERR;
      } else {
        newShape = fixedShape;
      }
    }
  }
  
  // make sure we have the correct result!
  if (newShape.ShapeType() == TopAbs_COMPOUND) {
    int nshell = 0, nsolid = 0;
    TopExp_Explorer Exp;
    for (Exp.Init(newShape, TopAbs_SHELL, TopAbs_SOLID);
         Exp.More(); Exp.Next()) nshell++;
    for (Exp.Init(newShape, TopAbs_SOLID); Exp.More(); Exp.Next()) nsolid++;
    if (nshell+nsolid != 1) {
      if (outLevel > 0) 
        printf(" EGADS Error: Number of Results = %d (EG_chamferBody)!\n",
               nshell+nsolid);
      return EGADS_CONSTERR;
    }
    if (nshell == 1) {
      Exp.Init(newShape, TopAbs_SHELL, TopAbs_SOLID);
      newShape = Exp.Current();
      }
    if (nsolid == 1) {
      Exp.Init(newShape, TopAbs_SOLID);
      newShape = Exp.Current();
    }
  }
  if ((newShape.ShapeType() != TopAbs_SOLID) &&
      (newShape.ShapeType() != TopAbs_SHELL)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Incorrect Result (EG_chamferBody)!\n");
    return EGADS_CONSTERR;
  }
  if ((src->mtype == SOLIDBODY) && 
      (newShape.ShapeType() != TopAbs_SOLID)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Result Not a Solid (EG_chamferBody)!\n");
    return EGADS_CONSTERR;
  }
  if ((src->mtype == SHEETBODY) && 
      (newShape.ShapeType() != TopAbs_SHELL)) {
    if (outLevel > 0) 
      printf(" EGADS Error: Result Not a Sheet (EG_chamferBody)!\n");
    return EGADS_CONSTERR;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_chamferBody)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = src->mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // map the Attributes
  EG_attriBodyDup(src, obj);
  for (i = 0; i < pbody->faces.map.Extent(); i++) {
    face = pbody->faces.objs[i];
    egadsFace *pface = (egadsFace *) face->blind;
    const TopTools_ListOfShape& listFaces = chamfer.Modified(pface->face);
    if (listFaces.Extent() > 0) {
      /* modified faces */
      TopTools_ListIteratorOfListOfShape it(listFaces);
      for (; it.More(); it.Next()) {
        TopoDS_Face genface = TopoDS::Face(it.Value());
        int index = pbods->faces.map.FindIndex(genface);
        if (index > 0) EG_attributeDup(face, pbods->faces.objs[index-1]);
      }
    }
  }

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_hollowBody(const egObject *src, int nface, const egObject **faces, 
              double offset, int joined, egObject **result)
{
  int      i, outLevel, stat;
  double   tol = Precision::Confusion();
  egObject *context, *obj;
  
  *result = NULL;
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->oclass != BODY)       return EGADS_NOTBODY;
  if (src->mtype != SOLIDBODY)   return EGADS_NOTTOPO;
  if (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if (nface < 0) {
    if (outLevel > 0) 
      printf(" EGADS Error: No Faces (EG_hollowBody)!\n");
    return EGADS_NODATA;
  }
  if ((faces == NULL) && (nface > 0)) {
    if (outLevel > 0) 
      printf(" EGADS Error: NULL Face Pointer (EG_hollowBody)!\n");
    return EGADS_NULLOBJ;
  }
  TopTools_ListOfShape aList;
  egadsBody *pbody = (egadsBody *) src->blind;
  for (i = 0; i < nface; i++) {
    if (faces[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Face Object %d (EG_hollowBody)!\n", i+1);
      return EGADS_NULLOBJ;
    }
    if (faces[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Object %d is not an EGO (EG_hollowBody)!\n",
               i+1);
      return EGADS_NOTOBJ;
    }
    if (faces[i]->oclass != FACE) {
      if (outLevel > 0) 
        printf(" EGADS Error: Object %d is not FACE (EG_hollowBody)!\n", i+1);
      return EGADS_NOTTOPO;
    }
    if (faces[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face Object %d has no data (EG_hollowBody)!\n",
               i+1);
      return EGADS_NODATA;
    }
    egadsFace *pface = (egadsFace *) faces[i]->blind;
    if (pbody->faces.map.FindIndex(pface->face) == 0) {
      if (outLevel > 0) 
        printf(" EGADS Error: Face %d is NOT in Body (EG_hollowBody)!\n", 
               i+1);
      return EGADS_NOTBODY;
    }
    aList.Append(pface->face);
    if (tol < BRep_Tool::Tolerance(pface->face))
      tol = BRep_Tool::Tolerance(pface->face);
  }
  BRepCheck_Analyzer check(pbody->shape);
  if (!check.IsValid()) {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid Input Body (EG_hollowBody)!\n");
    return EGADS_TOPOERR;
  }
  
  GeomAbs_JoinType join = GeomAbs_Arc;
  if (joined == 1) join = GeomAbs_Intersection;
  
  if (nface == 0) {
    // offset the body
    TopoDS_Shape newShape;
    try {
      newShape = BRepOffsetAPI_MakeOffsetShape(pbody->shape, offset, tol,
                                               BRepOffset_Skin, Standard_False, 
                                               Standard_False, join);
    }
    catch (Standard_Failure) {
      printf(" EGADS Error: MakeOffsetShape Exception (EG_hollowBody)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("              %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...) {
      printf(" EGADS Error: MakeOffsetShape Exception (EG_hollowBody)!\n");
      return EGADS_GEOMERR;
    }
    BRepCheck_Analyzer fCheck(newShape);
    if (!fCheck.IsValid()) {
      Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
      sfs->Perform();
      TopoDS_Shape fixedShape = sfs->Shape();
      if (fixedShape.IsNull()) {
        if (outLevel > 0) 
          printf(" EGADS Error: Offset Body is invalid (EG_hollowBody)!\n");
        return EGADS_GEOMERR;
      } else {
        BRepCheck_Analyzer sfCheck(fixedShape);
        if (!sfCheck.IsValid()) {
          printf(" EGADS Error: Offset Fixed Body is invalid (EG_hollowBody)!\n");
          return EGADS_GEOMERR;
        } else {
          newShape = fixedShape;
        }
      }
    }

    if (newShape.ShapeType() == TopAbs_COMPOUND) {
      int nsolid = 0;
      TopExp_Explorer Exp;
      for (Exp.Init(newShape, TopAbs_SOLID); Exp.More(); Exp.Next()) nsolid++;
      if (nsolid == 1) {
        Exp.Init(newShape, TopAbs_SOLID);
        newShape = Exp.Current();
      }
    }
    if (newShape.ShapeType() != TopAbs_SOLID) {
      if (outLevel > 0) 
        printf(" EGADS Error: Offset Result Not a Solid (EG_hollowBody)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot Make Body object (EG_hollowBody)!\n");
      return stat;
    }
    egadsBody *pbods   = new egadsBody;
    obj->oclass        = BODY;
    obj->mtype         = SOLIDBODY;
    pbods->nodes.objs  = NULL;
    pbods->edges.objs  = NULL;
    pbods->loops.objs  = NULL;
    pbods->faces.objs  = NULL;
    pbods->shells.objs = NULL;
    pbods->senses      = NULL;
    pbods->shape       = newShape;
    obj->blind         = pbods;
    stat = EG_traverseBody(context, 0, obj, obj, pbods);
    if (stat != EGADS_SUCCESS) {
      delete pbods;
      return stat;
    }
                                                  
    EG_referenceObject(obj, context);
    *result = obj;
    return EGADS_SUCCESS;
  }
  
  // hollow the body
  try {
    BRepOffsetAPI_MakeThickSolid hollow(pbody->shape, aList, -offset, tol,
                                        BRepOffset_Skin, Standard_False, 
                                        Standard_False, join);
    hollow.Build();
    TopoDS_Shape newShape = hollow.Shape();
    BRepCheck_Analyzer fCheck(newShape);
    if (!fCheck.IsValid()) {
      Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
      sfs->Perform();
      TopoDS_Shape fixedShape = sfs->Shape();
      if (fixedShape.IsNull()) {
        if (outLevel > 0) 
          printf(" EGADS Error: Hollowed Body is invalid (EG_hollowBody)!\n");
        return EGADS_GEOMERR;
      } else {
        BRepCheck_Analyzer sfCheck(fixedShape);
        if (!sfCheck.IsValid()) {
          printf(" EGADS Error: Fixed Body is invalid (EG_hollowBody)!\n");
          return EGADS_GEOMERR;
        } else {
          newShape = fixedShape;
        }
      }
    }
  
    // make sure we have the correct result!
    if (newShape.ShapeType() == TopAbs_COMPOUND) {
      int nsolid = 0;
      TopExp_Explorer Exp;
      for (Exp.Init(newShape, TopAbs_SOLID); Exp.More(); Exp.Next()) nsolid++;
      if (nsolid == 1) {
        Exp.Init(newShape, TopAbs_SOLID);
        newShape = Exp.Current();
      }
    }
    if (newShape.ShapeType() != TopAbs_SOLID) {
      if (outLevel > 0) 
        printf(" EGADS Error: Result Not a Solid (EG_hollowBody)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Body object (EG_hollowBody)!\n");
      return stat;
    }
    egadsBody *pbods   = new egadsBody;
    obj->oclass        = BODY;
    obj->mtype         = SOLIDBODY;
    pbods->nodes.objs  = NULL;
    pbods->edges.objs  = NULL;
    pbods->loops.objs  = NULL;
    pbods->faces.objs  = NULL;
    pbods->shells.objs = NULL;
    pbods->senses      = NULL;
    pbods->shape       = newShape;
    obj->blind         = pbods;
    stat = EG_traverseBody(context, 0, obj, obj, pbods);
    if (stat != EGADS_SUCCESS) {
      delete pbods;
      return stat;
    }
  
    // map the Attributes
    EG_attriBodyDup(src, obj);
    for (i = 0; i < pbody->faces.map.Extent(); i++) {
      ego face = pbody->faces.objs[i];
      egadsFace *pface = (egadsFace *) face->blind;
      const TopTools_ListOfShape& listFaces = hollow.Modified(pface->face);
      if (listFaces.Extent() > 0) {
        /* modified faces */
        TopTools_ListIteratorOfListOfShape it(listFaces);
        for (; it.More(); it.Next()) {
          TopoDS_Face genface = TopoDS::Face(it.Value());
          int index = pbods->faces.map.FindIndex(genface);
          if (index > 0) EG_attributeDup(face, pbods->faces.objs[index-1]);
        }
      }
    }
  }
  catch (Standard_Failure) {
    printf(" EGADS Error: MakeThickSolid Exception (EG_hollowBody)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("              %s\n", e->GetMessageString());
    return EGADS_GEOMERR;
  }
  catch (...) {
    printf(" EGADS Error: MakeThickSolid Exception (EG_hollowBody)!\n");
    return EGADS_GEOMERR;
  }

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_extrude(const egObject *src, double dist, const double *dir, 
                 egObject **result)
{
  int          outLevel, stat, mtype;
  double       d, vec[3];
  egObject     *context, *obj;
  TopoDS_Shape shape, newShape;
  
  *result = NULL;
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  mtype = SOLIDBODY;
  if  (src->oclass == BODY) {
    if ((src->mtype == WIREBODY) || (src->mtype == FACEBODY)) {
      egadsBody *pbody = (egadsBody *) src->blind;
      shape = pbody->shape;
      if (src->mtype == WIREBODY) mtype = SHEETBODY;
    } else {
      if (outLevel > 0) 
        printf(" EGADS Error: Body src must be Wire or Face (EG_extrude)!\n");
      return EGADS_NOTTOPO;
    }
  } else if (src->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) src->blind;
    shape = ploop->loop;
    mtype = SHEETBODY;
  } else if (src->oclass == FACE) {
    egadsFace *pface = (egadsFace *) src->blind;
    shape = pface->face;
  } else {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid src type (EG_extrude)!\n");
    return EGADS_NOTTOPO;
  }
  
  d = sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
  if (d == 0.0) {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid Direction (EG_extrude)!\n");
    return EGADS_GEOMERR;
  }
  vec[0] = dist*dir[0]/d;
  vec[1] = dist*dir[1]/d;
  vec[2] = dist*dir[2]/d;
  try {
    newShape = BRepPrimAPI_MakePrism(shape, gp_Vec(vec[0],vec[1],vec[2]));
  }
  catch (Standard_Failure) {
    printf(" EGADS Error: MakePrism Exception (EG_extrude)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("              %s\n", e->GetMessageString());
    return EGADS_GEOMERR;
  }
  catch (...) {
    printf(" EGADS Error: MakePrism Exception (EG_extrude)!\n");
    return EGADS_GEOMERR;
  }
 
  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_extrude)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // do we want to do anything with attributes?

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_rotate(const egObject *src, double angle, const double *axis, 
                egObject **result)
{
  int          outLevel, stat, mtype;
  egObject     *context, *obj;
  TopoDS_Shape shape, newShape;
  
  *result = NULL;
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  mtype = SOLIDBODY;
  if  (src->oclass == BODY) {
    if ((src->mtype == WIREBODY) || (src->mtype == FACEBODY)) {
      egadsBody *pbody = (egadsBody *) src->blind;
      shape = pbody->shape;
      if (src->mtype == WIREBODY) mtype = SHEETBODY;
    } else {
      if (outLevel > 0) 
        printf(" EGADS Error: Body src must be Wire or Face (EG_rotate)!\n");
      return EGADS_NOTTOPO;
    }
  } else if (src->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) src->blind;
    shape = ploop->loop;
    mtype = SHEETBODY;
  } else if (src->oclass == FACE) {
    egadsFace *pface = (egadsFace *) src->blind;
    shape = pface->face;
  } else {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid src type (EG_rotate)!\n");
    return EGADS_NOTTOPO;
  }

  gp_Pnt pnt(axis[0], axis[1], axis[2]);
  gp_Dir dir(axis[3], axis[4], axis[5]);
  gp_Ax1 axi(pnt, dir);
  try {
    if ((angle > 0.0) && (angle < 360.0)) {
      newShape = BRepPrimAPI_MakeRevol(shape, axi, angle*PI/180.0);
    } else {
      newShape = BRepPrimAPI_MakeRevol(shape, axi);
    }
  }
  catch (Standard_Failure) {
    printf(" EGADS Error: MakeRevol Exception (EG_rotate)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("              %s\n", e->GetMessageString());
    return EGADS_GEOMERR;
  }
  catch (...) {
    printf(" EGADS Error: MakeRevol Exception (EG_rotate)!\n");
    return EGADS_GEOMERR;
  } 

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_rotate)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // do we want to do anything with attributes?

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_sweep(const egObject *src, const egObject *edge, 
               egObject **result)
{
  int          outLevel, stat, mtype;
  egObject     *context, *obj;
  TopoDS_Shape shape, newShape;
  
  *result = NULL;
  if (src == NULL)               return EGADS_NULLOBJ;
  if (src->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (src->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(src);
  context  = EG_context(src);
  
  if (edge == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL edge Reference (EG_sweep)!\n");
    return EGADS_NULLOBJ;
  }
  if (edge->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: edge not an EGO (EG_sweep)!\n");
    return EGADS_NOTOBJ;
  }
  if (edge->oclass != EDGE) {
    if (outLevel > 0)
      printf(" EGADS Error: Not an Edge (EG_sweep!\n");
    return EGADS_NOTTOPO;
  }
  if (context != EG_context(edge)) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_sweep)!\n");
    return EGADS_MIXCNTX;
  }
  BRepBuilderAPI_MakeWire MW;
  egadsEdge *pedge = (egadsEdge *) edge->blind;
  TopoDS_Edge edg  = pedge->edge;
  edg.Orientation(TopAbs_FORWARD);
  MW.Add(edg);
  if (MW.Error()) {
    if (outLevel > 0)
      printf(" EGADS Error: Problem adding Edge (EG_sweep)!\n");
    return EGADS_NODATA;
  }
  if (!MW.IsDone()) {
    if (outLevel > 0)
      printf(" EGADS Error: Problem with Loop (EG_sweep)!\n");
    return EGADS_NODATA;
  }
  TopoDS_Wire wire = MW.Wire();

  mtype = SOLIDBODY;
  if  (src->oclass == BODY) {
    if ((src->mtype == WIREBODY) || (src->mtype == FACEBODY)) {
      egadsBody *pbody = (egadsBody *) src->blind;
      shape = pbody->shape;
      if (src->mtype == WIREBODY) mtype = SHEETBODY;
    } else {
      if (outLevel > 0) 
        printf(" EGADS Error: Body src must be Wire or Face (EG_sweep)!\n");
      return EGADS_NOTTOPO;
    }
  } else if (src->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) src->blind;
    shape = ploop->loop;
    mtype = SHEETBODY;
  } else if (src->oclass == FACE) {
    egadsFace *pface = (egadsFace *) src->blind;
    shape = pface->face;
  } else {
    if (outLevel > 0) 
      printf(" EGADS Error: Invalid src type (EG_sweep)!\n");
    return EGADS_NOTTOPO;
  }

  try {
    newShape = BRepOffsetAPI_MakePipe(wire, shape);
  }
  catch (Standard_Failure) {
    printf(" EGADS Error: BRepOffsetAPI_MakePipe Exception (EG_sweep)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("              %s\n", e->GetMessageString());
    return EGADS_GEOMERR;
  }
  catch (...) {
    printf(" EGADS Error: BRepOffsetAPI_MakePipe Exception (EG_sweep)!\n");
    return EGADS_GEOMERR;
  }
  if (mtype == SOLIDBODY) {
    if (newShape.ShapeType() != TopAbs_SOLID) {
      if (outLevel > 0) 
        printf(" EGADS Error: Sweep Result Not a Solid (EG_sweep)!\n");
      return EGADS_CONSTERR;
    }
  } else {
    if (newShape.ShapeType() != TopAbs_SHELL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Sweep Result Not a Shell (EG_sweep)!\n");
      return EGADS_CONSTERR;
    }
  }

  // are we OK?
  BRepCheck_Analyzer check(newShape);
  if (!check.IsValid()) {
    // try to fix the fault
    Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
    sfs->Perform();
    TopoDS_Shape fixedShape = sfs->Shape();
    if (fixedShape.IsNull()) {
      if (outLevel > 0)
        printf(" EGADS Info: Invalid Shape w/ NULL Fix (EG_sweep)!\n");
      return EGADS_CONSTERR;
    }
    BRepCheck_Analyzer fxCheck(fixedShape);
    if (!fxCheck.IsValid()) {
      if (outLevel > 0) 
        printf(" EGADS Info: Result is invalid (EG_sweep)!\n");
      return EGADS_CONSTERR;
    }
    newShape = fixedShape;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_sweep)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = mtype;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // do we want to do anything with attributes?
  //       can get end-caps

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}


int
EG_loft(int nsec, const egObject **secs, int opt, egObject **result)
{
  int      i, outLevel, stat;
  egObject *context, *obj;
  
  *result = NULL;
  if (nsec <= 1)                     return EGADS_EMPTY;
  if (secs == NULL)                  return EGADS_NULLOBJ;
  if (secs[0] == NULL)               return EGADS_NULLOBJ;
  if (secs[0]->magicnumber != MAGIC) return EGADS_NOTOBJ;
  outLevel = EG_outLevel(secs[0]);
  context  = EG_context(secs[0]);

  for (i = 0; i < nsec; i++) {
    if (secs[i] == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: NULL Section Object %d (EG_loft)!\n", i+1);
      return EGADS_NULLOBJ;
    }
    if (secs[i]->magicnumber != MAGIC) {
      if (outLevel > 0) 
        printf(" EGADS Error: Section %d is not an EGO (EG_loft)!\n", i+1);
      return EGADS_NOTOBJ;
    }
    if (secs[i]->blind == NULL) {
      if (outLevel > 0) 
        printf(" EGADS Error: Section %d has no data (EG_loft)!\n", i+1);
      return EGADS_NODATA;
    }
    if (secs[i]->oclass == NODE) {
      if ((i != 0) && (i != nsec-1)) {
        if (outLevel > 0) 
          printf(" EGADS Error: Section %d is Node and not Bound (EG_loft)!\n",
                 i+1);
        return EGADS_NOTTOPO;
      }
    } else if (secs[i]->oclass == BODY) {
      if (secs[i]->mtype != WIREBODY) {
        if (outLevel > 0) 
          printf(" EGADS Error: Section %d is Not a WireBody (EG_loft)!\n", 
                 i+1);
        return EGADS_NOTTOPO;
      }
    } else if (secs[i]->oclass != LOOP) {
      if (outLevel > 0) 
        printf(" EGADS Error: Section %d is Not a Loop (EG_loft)!\n", i+1);
      return EGADS_NOTTOPO;
    }
  }
  
  Standard_Boolean isSolid = ((opt&1) == 1);
  Standard_Boolean isRuled = ((opt&2) == 2);
  TopoDS_Shape     newShape;
  try {
    BRepOffsetAPI_ThruSections loft(isSolid, isRuled);
    for (i = 0; i < nsec; i++)
      if (secs[i]->oclass == NODE) {
        egadsNode *pnode = (egadsNode *) secs[i]->blind;
        loft.AddVertex(pnode->node);
      } else if (secs[i]->oclass == BODY) {
        egadsBody *pbody = (egadsBody *) secs[i]->blind;
        TopoDS_Shape bshape = pbody->shape;
        TopoDS_Wire wire = TopoDS::Wire(bshape);
        loft.AddWire(wire);
      } else {
        egadsLoop *ploop = (egadsLoop *) secs[i]->blind;
        loft.AddWire(ploop->loop);
      }  
    loft.Build();
    if (!loft.IsDone()) {
      if (outLevel > 0) 
        printf(" EGADS Error: Can't Loft (EG_loft)!\n");
      return EGADS_GEOMERR;
    }
    newShape = loft.Shape();
  }
  catch (Standard_Failure) {
    printf(" EGADS Error: ThruSections Exception (EG_loft)!\n");
    Handle_Standard_Failure e = Standard_Failure::Caught();
    printf("              %s\n", e->GetMessageString());
    return EGADS_GEOMERR;
  }
  catch (...) {
    printf(" EGADS Error: ThruSections Exception (EG_loft)!\n");
    return EGADS_GEOMERR;
  }
  
  // are we OK?
  BRepCheck_Analyzer check(newShape);
  if (!check.IsValid()) {
    // try to fix the fault
    Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(newShape);
    sfs->Perform();
    TopoDS_Shape fixedShape = sfs->Shape();
    if (fixedShape.IsNull()) {
      if (outLevel > 0)
        printf(" EGADS Info: Invalid Shape w/ NULL Fix (EG_loft)!\n");
      return EGADS_CONSTERR;
    }
    BRepCheck_Analyzer fxCheck(fixedShape);
    if (!fxCheck.IsValid()) {
      if (outLevel > 0) 
        printf(" EGADS Info: Result is invalid (EG_loft)!\n");
      return EGADS_CONSTERR;
    }
    newShape = fixedShape;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_loft)!\n");
    return stat;
  }
  egadsBody *pbods   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = SHEETBODY;
  pbods->nodes.objs  = NULL;
  pbods->edges.objs  = NULL;
  pbods->loops.objs  = NULL;
  pbods->faces.objs  = NULL;
  pbods->shells.objs = NULL;
  pbods->senses      = NULL;
  pbods->shape       = newShape;
  obj->blind         = pbods;
  if (isSolid) obj->mtype = SOLIDBODY;
  stat = EG_traverseBody(context, 0, obj, obj, pbods);
  if (stat != EGADS_SUCCESS) {
    delete pbods;
    return stat;
  }
  
  // do we want to do anything with attributes?

  EG_referenceObject(obj, context);
  *result = obj;
  return EGADS_SUCCESS;
}
