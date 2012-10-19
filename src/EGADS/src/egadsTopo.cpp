/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Topology Functions
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


  extern "C" int  EG_destroyTopology( egObject *topo );

  extern "C" int  EG_getTolerance( const egObject *topo, double *tol );
  extern "C" int  EG_getTopology( const egObject *topo, egObject **geom,
                                  int *oclass, int *type, 
                                  /*@null@*/ double *limits, int *nChildren, 
                                  egObject ***children, int **senses );
  extern "C" int  EG_makeTopology( egObject *context, /*@null@*/ egObject *geom, 
                                   int oclass, int mtype, /*@null@*/ double *limits, 
                                   int nChildren, /*@null@*/ egObject **children, 
                                   /*@null@*/ int *senses, egObject **topo );
  extern "C" int  EG_makeFace( egObject *object, int mtype, 
                               /*@null@*/ const double *limits, egObject **face );
  extern "C" int  EG_getArea( egObject *object, /*@null@*/ const double *limits, 
                              double *area );
  extern "C" int  EG_getBodyTopos( const egObject *body, /*@null@*/ egObject *src,
                                   int oclass, int *ntopo, egObject ***topos );
  extern "C" int  EG_indexBodyTopo( const egObject *body, const egObject *src );
  extern "C" int  EG_makeSolidBody( egObject *context, int stype, 
                                    const double *rvec, egObject **body );
  extern "C" int  EG_getBoundingBox( const egObject *topo, double *box );
  extern "C" int  EG_getMassProperties( const egObject *topo, double *props );
  extern "C" int  EG_isEquivalent( const egObject *topo1, const egObject *topo2 );
  extern "C" int  EG_inFace( const egObject *face, const double *uv );
  extern "C" int  EG_inTopology( const egObject *topo, const double *xyz );
  extern "C" int  EG_getEdgeUV( const egObject *face, const egObject *edge,
                                int sense, double t, double *result );

  extern     int  EG_attriBodyDup( const egObject *src, egObject *dst );
  extern     int  EG_attriBodyCopy( const egObject *src, egObject *dst );
  extern     void EG_completePCurve( egObject *g, Handle(Geom2d_Curve) &hCurv );
  extern     void EG_completeCurve(  egObject *g, Handle(Geom_Curve)   &hCurv );
  extern     void EG_completeSurf(   egObject *g, Handle(Geom_Surface) &hSurf );


// for trapping SegFaults                                                                 
static jmp_buf jmpenv;

static void
segfault_handler(int x)
{
  longjmp(jmpenv, x);
}


static void
EG_cleanMaps(egadsMap *map)
{
  if (map->objs == NULL) return;
  EG_free(map->objs);
  map->objs = NULL;
}


static void
EG_checkStatus(const Handle_BRepCheck_Result tResult)
{

  const BRepCheck_ListOfStatus& tList = tResult->Status();

/*
  for (int m = 1; m <= tList.Extent(); m++) {
*/
    BRepCheck_Status cStatus = tList.First();
    if (cStatus == BRepCheck_InvalidPointOnCurve) {
      printf("      Fault: Invalid Point On Curve\n");
    } else if (cStatus == BRepCheck_InvalidPointOnCurveOnSurface) {
      printf("      Fault: Invalid Point On Curve On Surface\n");
    } else if (cStatus == BRepCheck_InvalidPointOnSurface) {
      printf("      Fault: Invalid Point On Surface\n");
    } else if (cStatus == BRepCheck_No3DCurve) {
      printf("      Fault: No 3D Curve\n");
    } else if (cStatus == BRepCheck_Multiple3DCurve) {
      printf("      Fault: Multiple 3D Curves\n");
    } else if (cStatus == BRepCheck_Invalid3DCurve) {
      printf("      Fault: Invalid 3D Curve\n");
    } else if (cStatus == BRepCheck_NoCurveOnSurface) {
      printf("      Fault: No Curve On Surface\n");
    } else if (cStatus == BRepCheck_InvalidCurveOnSurface) {
      printf("      Fault: Invalid Curve On Surface\n");
    } else if (cStatus == BRepCheck_InvalidCurveOnClosedSurface) {
      printf("      Fault: Invalid Curve On Closed Surface\n");
    } else if (cStatus == BRepCheck_InvalidSameRangeFlag) {
      printf("      Fault: Invalid SameRange Flag\n");
    } else if (cStatus == BRepCheck_InvalidSameParameterFlag) {
      printf("      Fault: Invalid Same Parameter Flag\n");
    } else if (cStatus == BRepCheck_InvalidDegeneratedFlag) {
      printf("      Fault: Invalid Degenerated Flag\n");
    } else if (cStatus == BRepCheck_FreeEdge) {
      printf("      Fault: Free Edge\n");
    } else if (cStatus == BRepCheck_InvalidMultiConnexity) {
      printf("      Fault: Invalid Multi Connexity\n");
    } else if (cStatus == BRepCheck_InvalidRange) {
      printf("      Fault: Invalid Range\n");
    } else if (cStatus == BRepCheck_EmptyWire) {
      printf("      Fault: Empty Wire\n");
    } else if (cStatus == BRepCheck_RedundantEdge) {
      printf("      Fault: Redundant Edge\n");
    } else if (cStatus == BRepCheck_SelfIntersectingWire) {
      printf("      Fault: Self Intersecting Wire\n");
    } else if (cStatus == BRepCheck_NoSurface) {
      printf("      Fault: No Surface\n");
    } else if (cStatus == BRepCheck_InvalidWire) {
      printf("      Fault: Invalid Wire\n");
    } else if (cStatus == BRepCheck_RedundantWire) {
      printf("      Fault: Redundant Wire\n");
    } else if (cStatus == BRepCheck_IntersectingWires) {
      printf("      Fault: Intersecting Wires\n");
    } else if (cStatus == BRepCheck_InvalidImbricationOfWires) {
      printf("      Fault: Invalid Imbrication Of Wires\n");
    } else if (cStatus == BRepCheck_EmptyShell) {
      printf("      Fault: Empty Shell\n");
    } else if (cStatus == BRepCheck_RedundantFace) {
      printf("      Fault: Redundant Face\n");
    } else if (cStatus == BRepCheck_UnorientableShape) {
      printf("      Fault: Unorientable Shape\n");
    } else if (cStatus == BRepCheck_NotClosed) {
      printf("      Fault: Not Closed\n");
    } else if (cStatus == BRepCheck_NotConnected) {
      printf("      Fault: Not Connected\n");
    } else if (cStatus == BRepCheck_SubshapeNotInShape) {
      printf("      Fault: Subshape Not In Shape\n");
    } else if (cStatus == BRepCheck_BadOrientation) {
      printf("      Fault: Bad Orientation\n");
    } else if (cStatus == BRepCheck_BadOrientationOfSubshape) {
      printf("      Fault: Bad Orientation Of Subshape\n");
    } else if (cStatus == BRepCheck_InvalidToleranceValue) {
      printf("      Fault: Invalid Tolerance Value\n");
    } else if (cStatus == BRepCheck_CheckFail) {
      printf("      Fault: Check Fail\n");
    } else {
      printf("      Unknown Fault = %d\n", cStatus);
    }
/*
  }
*/
}


int 
EG_destroyTopology(egObject *topo)
{
  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  
  if (topo->blind == NULL) return EGADS_SUCCESS;
  
  if (topo->oclass == MODEL) {
    egadsModel *mshape = (egadsModel *) topo->blind;
    if (mshape->bodies != NULL) {
      for (int i = 0; i < mshape->nbody; i++)
        EG_dereferenceObject(mshape->bodies[i], topo);
      delete [] mshape->bodies;
    }
    mshape->shape.Nullify();
    delete mshape;
    
  } else if (topo->oclass == BODY) {
  
    egadsBody *pbody = (egadsBody *) topo->blind;
    if (pbody != NULL) {
      if (topo->mtype == WIREBODY) {
        int nwire = pbody->loops.map.Extent();
        for (int i = 0; i < nwire; i++)
          EG_dereferenceObject(pbody->loops.objs[i], topo);
      } else if (topo->mtype == FACEBODY) {
        int nface = pbody->faces.map.Extent();
        for (int i = 0; i < nface; i++)
          EG_dereferenceObject(pbody->faces.objs[i], topo);
      } else {
        int nshell = pbody->shells.map.Extent();
        for (int i = 0; i < nshell; i++)
          EG_dereferenceObject(pbody->shells.objs[i], topo);
        if (topo->mtype == SOLIDBODY) delete [] pbody->senses;
      }
      EG_cleanMaps(&pbody->shells);
      EG_cleanMaps(&pbody->faces);
      EG_cleanMaps(&pbody->loops);
      EG_cleanMaps(&pbody->edges);
      EG_cleanMaps(&pbody->nodes);
      delete pbody;
    }
    
  } else if (topo->oclass == SHELL) {
  
    egadsShell *pshell = (egadsShell *) topo->blind;
    if (pshell != NULL) {
      if (pshell->topFlg == 0) {
        for (int i = 0; i < pshell->nfaces; i++)
          EG_dereferenceObject(pshell->faces[i], topo);
      } else {
        for (int i = 0; i < pshell->nfaces; i++)
          EG_dereferenceTopObj(pshell->faces[i], topo);
      }
      delete [] pshell->faces;
      delete pshell;
    }

  } else if (topo->oclass == FACE) {

    egadsFace *pface = (egadsFace *) topo->blind;
    if (pface != NULL) {
      if (pface->topFlg == 0) {
        for (int i = 0; i < pface->nloops; i++)
          EG_dereferenceObject(pface->loops[i], topo);
        EG_dereferenceObject(pface->surface, topo);
      } else {
        for (int i = 0; i < pface->nloops; i++)
          EG_dereferenceTopObj(pface->loops[i], topo);
        EG_dereferenceTopObj(pface->surface, topo);
      }
      delete [] pface->senses;
      delete [] pface->loops;
      delete pface;
    }

  } else if (topo->oclass == LOOP) {
  
    egadsLoop *ploop = (egadsLoop *) topo->blind;
    if (ploop != NULL) {
      if (ploop->topFlg == 0) {
        for (int i = 0; i < ploop->nedges; i++) {
          EG_dereferenceObject(ploop->edges[i], topo);
          if (ploop->surface != NULL)
            EG_dereferenceObject(ploop->edges[i+ploop->nedges], topo);
        }
        if (ploop->surface != NULL)
          EG_dereferenceObject(ploop->surface, topo);
      } else {
        for (int i = 0; i < ploop->nedges; i++) {
          EG_dereferenceTopObj(ploop->edges[i], topo);
          if (ploop->surface != NULL)
            EG_dereferenceTopObj(ploop->edges[i+ploop->nedges], topo);
        }
        if (ploop->surface != NULL)
          EG_dereferenceTopObj(ploop->surface, topo);
      }
      delete [] ploop->senses;
      delete [] ploop->edges;
      delete ploop;
    }

  } else if (topo->oclass == EDGE) {

    egadsEdge *pedge = (egadsEdge *) topo->blind;
    if (pedge != NULL) {
      int degen = 0;
      
      if ((pedge->curve == NULL) &&
          (topo->mtype  == DEGENERATE)) degen = 1;
      if (pedge->topFlg == 0) {
        if (degen == 0) EG_dereferenceObject(pedge->curve, topo);
        EG_dereferenceObject(pedge->nodes[0], topo);
        EG_dereferenceObject(pedge->nodes[1], topo);
      } else {
        if (degen == 0) EG_dereferenceTopObj(pedge->curve, topo);
        EG_dereferenceTopObj(pedge->nodes[0], topo);
        EG_dereferenceTopObj(pedge->nodes[1], topo);
      }
      delete pedge;
    }

  } else {
  
    egadsNode *pnode = (egadsNode *) topo->blind;
    if (pnode != NULL) delete pnode;
  
  }

  return EGADS_SUCCESS;
}


void
EG_splitPeriodics(egadsBody *body)
{
  int          hit    = 0;
  TopoDS_Shape bshape = body->shape;
  BRepCheck_Analyzer sCheck(bshape);
  if (!sCheck.IsValid()) {
    printf(" EGADS Warning: Solid is invalid (EG_splitPeriodics)!\n");
    return;
  }

  TopTools_IndexedMapOfShape MapE;
  TopExp::MapShapes(bshape, TopAbs_EDGE, MapE);
  for (int i = 1; i <= MapE.Extent(); i++) {
    TopoDS_Shape shape = MapE(i);
    TopoDS_Edge  Edge  = TopoDS::Edge(shape);
    if (Edge.Closed()) hit++;
  }
  if (hit == 0) {
    TopTools_IndexedMapOfShape MapF;
    TopExp::MapShapes(bshape, TopAbs_FACE, MapF);
    for (int i = 1; i <= MapF.Extent(); i++) {
      TopoDS_Shape shape = MapF(i);
      TopoDS_Face  Face  = TopoDS::Face(shape);
      BRepAdaptor_Surface aSurf(Face, Standard_True);
      if (aSurf.IsUClosed()) hit++;
      if (aSurf.IsVClosed()) hit++;
    }
  }
  if (hit == 0) return;

  // use the OpenCASCADE method ->

  TopoDS_Shape solid = bshape;
  Handle(ShapeBuild_ReShape) reShape = new ShapeBuild_ReShape();
  ShapeUpgrade_ShapeDivideClosed aShape(bshape);
  aShape.SetNbSplitPoints(1);
  aShape.SetContext(reShape);
  if (aShape.Perform(Standard_False)) {
    solid = reShape->Apply(bshape);
    if (solid.IsNull()) {
      printf(" EGADS Warning: Can't Split Periodics!\n");
      solid = bshape;
    } else {
      BRepCheck_Analyzer fCheck(solid);
      if (!fCheck.IsValid()) {
        Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(solid);
        sfs->Perform();
        TopoDS_Shape fixedSolid = sfs->Shape();
        if (fixedSolid.IsNull()) {
          printf(" EGADS Warning: Periodic Split is Invalid!\n");
          solid = bshape;
        } else {
          BRepCheck_Analyzer sfCheck(fixedSolid);
          if (!sfCheck.IsValid()) {
            printf(" EGADS Warning: Periodic Split is InValid!\n");
            solid = bshape;
          } else {
            solid = fixedSolid;
          }
        }
      }
    }
  }

  body->shape = solid;
}


void
EG_fillPCurves(TopoDS_Face face, egObject *surfo, egObject *loopo,
                                 egObject *topObj)
{
  int           i = 0;
  egObject      *geom;
  Standard_Real f, l;

  egadsLoop *ploop = (egadsLoop *) loopo->blind;
  if (ploop->surface == NULL) return;
  if (ploop->surface != surfo) {
    printf(" EGADS Internal: Loop/Face mismatch on Surface!\n");
    return;
  }

  TopoDS_Wire wire = ploop->loop;
  BRepTools_WireExplorer ExpWE;
  for (ExpWE.Init(wire); ExpWE.More(); ExpWE.Next()) {
    if (ploop->edges[ploop->nedges+i] != NULL) {
      printf(" EGADS Internal: PCurve already Filled!\n");
      return;
    }
    TopoDS_Shape shape = ExpWE.Current();
    TopoDS_Edge  edge  = TopoDS::Edge(shape);
    if (EG_makeObject(EG_context(surfo), 
          &ploop->edges[ploop->nedges+i]) == EGADS_SUCCESS) {
      geom = ploop->edges[ploop->nedges+i];
      BRep_Tool::Range(edge, f, l);
      Handle(Geom2d_Curve) hCurve = BRep_Tool::
                                    CurveOnSurface(edge, face, f, l);
      geom->topObj = topObj;
      EG_completePCurve(geom,  hCurve);
      EG_referenceObject(geom, loopo);
    }
    i++;
  }

}


int
EG_shellClosure(egadsShell *pshell, int mtype)
{
  int ret, i, *hits;

  TopoDS_Shell Shell = pshell->shell;
  TopTools_IndexedMapOfShape MapE;
  TopExp::MapShapes(Shell, TopAbs_EDGE, MapE);
  if (MapE.Extent() == 0) return CLOSED;

  hits = new int[MapE.Extent()];
  for (i = 0; i < MapE.Extent(); i++) hits[i] = 0;

  TopExp_Explorer ExpW;
  for (ExpW.Init(Shell, TopAbs_EDGE); ExpW.More(); ExpW.Next()) {
    TopoDS_Shape shape = ExpW.Current();
    TopoDS_Edge  edge  = TopoDS::Edge(shape);  
    if (BRep_Tool::Degenerated(edge)) continue;
    i = MapE.FindIndex(edge);
    if (i == 0) {
      printf(" EGADS Internal: Edge not found (EG_shellClosure)!\n");
      continue;
    }
    hits[i-1]++;
  }

  ret = CLOSED;
  for (i = 0; i < MapE.Extent(); i++)
    if ((hits[i] != 2) && (hits[i] != 0)) ret = OPEN;
  if ((mtype == DEGENERATE) && (ret == OPEN))
    for (i = 0; i < MapE.Extent(); i++)
      printf(" EGADS Info: Edge %d: hits = %d\n", i+1, hits[i]);
  
  delete [] hits;
  return ret;
}


static void
EG_fillTopoObjs(egObject *object, egObject *topObj)
{
  int           outLevel, stat, degen = 0;
  egObject      *context;
  Standard_Real t1, t2;
  TopoDS_Vertex V1, V2;
  
  outLevel = EG_outLevel(object);
  context  = EG_context(object);
  
  if (object->oclass == EDGE) {
  
    egObject *geom   = NULL;
    egObject *pn1    = NULL;
    egObject *pn2    = NULL;
    egadsEdge *pedge = (egadsEdge *) object->blind;
    TopoDS_Edge Edge = pedge->edge;
    if (BRep_Tool::Degenerated(Edge)) {
      degen = 1;
    } else {
      Handle(Geom_Curve) hCurve = BRep_Tool::Curve(Edge, t1, t2);
      stat = EG_makeObject(context, &geom);
      if (stat == EGADS_SUCCESS) {
        geom->topObj = topObj;
        EG_completeCurve(geom, hCurve);
      }
    }
    
    TopExp::Vertices(Edge, V2, V1, Standard_True);
    EG_makeObject(context, &pn1);
    if (pn1 != NULL) {
      egadsNode *pnode = new egadsNode;
      gp_Pnt pv        = BRep_Tool::Pnt(V1);
      pnode->node      = V1;
      pnode->xyz[0]    = pv.X();
      pnode->xyz[1]    = pv.Y();
      pnode->xyz[2]    = pv.Z();
      pn1->oclass      = NODE;
      pn1->blind       = pnode;
      pn1->topObj      = topObj;
      BRepCheck_Analyzer v1Check(V1);
      if(!v1Check.IsValid())
        if (outLevel > 0)
        printf(" EGADS Info: Node1 may be invalid (EG_fillTopoObjs)!\n");
    }
    if (V1.IsSame(V2)) {
      object->mtype = ONENODE;
      pn2           = pn1;
    } else {
      object->mtype = TWONODE;
      EG_makeObject(context, &pn2);
      if (pn2 != NULL) {
        egadsNode *pnode = new egadsNode;
        gp_Pnt pv        = BRep_Tool::Pnt(V2);
        pnode->node      = V2;
        pnode->xyz[0]    = pv.X();
        pnode->xyz[1]    = pv.Y();
        pnode->xyz[2]    = pv.Z();
        pn2->oclass      = NODE;
        pn2->blind       = pnode;
        pn2->topObj      = topObj;
        BRepCheck_Analyzer v2Check(V2);
        if(!v2Check.IsValid())
          if (outLevel > 0)
          printf(" EGADS Info: Node2 may be invalid (EG_fillTopoObjs)!\n");
      }
    }
    if (Edge.Orientation() != TopAbs_REVERSED) {
      pedge->nodes[0] = pn2;
      pedge->nodes[1] = pn1; 
    } else {
      pedge->nodes[0] = pn1;
      pedge->nodes[1] = pn2;
    }

    pedge->curve   = geom;
    pedge->topFlg  = 0;
    object->topObj = topObj;
    if (degen == 1) {
      object->mtype = DEGENERATE;
    } else {
      EG_referenceObject(geom, object);
    }
    EG_referenceObject(pn1,  object);
    EG_referenceObject(pn2,  object);
    BRepCheck_Analyzer eCheck(Edge);
    if (!eCheck.IsValid())
      if (outLevel > 0)
        printf(" EGADS Info: Edge may be invalid (EG_fillTopoObjs)!\n");

  } else if (object->oclass == LOOP) {

    int      *senses = NULL;
    egObject **edgeo = NULL;
    egadsLoop *ploop = (egadsLoop *) object->blind;
    TopoDS_Wire Wire = ploop->loop;
    int            n = 1;
    int           ne = 0;
    int       closed = 0;
    if (Wire.Closed()) closed = 1;
    BRepTools_WireExplorer ExpWE;
    for (ExpWE.Init(Wire); ExpWE.More(); ExpWE.Next()) ne++;
    if (ploop->surface != NULL) n = 2;

    if (ne > 0) {
      edgeo  = new egObject*[n*ne];
      senses = new int[ne];
    }
    int k = 0;
    for (ExpWE.Init(Wire); ExpWE.More(); ExpWE.Next()) {
      TopoDS_Shape shapW = ExpWE.Current();
      TopoDS_Edge  Edge  = TopoDS::Edge(shapW);
      edgeo[k]           = NULL;
      senses[k]          = 1;
      if (n == 2) edgeo[k+ne] = NULL;
      if (shapW.Orientation() == TopAbs_REVERSED) senses[k] = -1;
      for (int j = 0; j < k; j++) {
        egadsEdge *pedg = (egadsEdge *) edgeo[j]->blind;
        if (Edge.IsSame(pedg->edge)) {
          edgeo[k] = edgeo[j];
          break;
        }
      }
      if (edgeo[k] == NULL) {
        stat = EG_makeObject(context, &edgeo[k]);
        if (stat != EGADS_SUCCESS) continue;
        edgeo[k]->oclass = EDGE;
        egadsEdge *pedge = new egadsEdge;
        pedge->edge      = Edge;
        pedge->curve     = NULL;
        pedge->nodes[0]  = NULL;
        pedge->nodes[1]  = NULL;
        edgeo[k]->blind  = pedge;
        EG_fillTopoObjs(edgeo[k], topObj);
      }
      EG_referenceObject(edgeo[k], object);
      k++;
    }

    ploop->nedges  = ne;
    ploop->edges   = edgeo;
    ploop->senses  = senses;
    ploop->topFlg  = 0;
    object->topObj = topObj;
    object->mtype  = OPEN;
    if (closed == 1) object->mtype = CLOSED;
    BRepCheck_Analyzer wCheck(Wire);
    if (!wCheck.IsValid())
      if (outLevel > 0)
        printf(" EGADS Info: Loop may be invalid (EG_fillTopoObjs)!\n");

  } else {
  
    printf(" EGADS Internal: Not Implemented (EG_fillTopoObjs)!\n");

  }

}


int
EG_traverseBody(egObject *context, int i, egObject *bobj, 
                egObject *topObj, egadsBody *body)
{
  int          j, k, outLevel, stat, solid = 0;
  TopoDS_Shape shape;
  egObject     *obj, *geom;
  
  outLevel = EG_outLevel(context);
  if (body->shape.ShapeType() == TopAbs_SOLID) solid = 1;
  
  TopExp_Explorer Exp;
  TopExp::MapShapes(body->shape, TopAbs_VERTEX, body->nodes.map);
  TopExp::MapShapes(body->shape, TopAbs_EDGE,   body->edges.map);
  TopExp::MapShapes(body->shape, TopAbs_WIRE,   body->loops.map);
  TopExp::MapShapes(body->shape, TopAbs_FACE,   body->faces.map);
  TopExp::MapShapes(body->shape, TopAbs_SHELL,  body->shells.map);
  int nNode  = body->nodes.map.Extent();
  int nEdge  = body->edges.map.Extent();
  int nLoop  = body->loops.map.Extent();
  int nFace  = body->faces.map.Extent();
  int nShell = body->shells.map.Extent();
  bobj->oclass = BODY;
  bobj->mtype  = WIREBODY;
  if (nFace > 0) {
    bobj->mtype = FACEBODY;
    if (nShell > 0) {
      bobj->mtype = SHEETBODY;
      if (solid == 1) bobj->mtype = SOLIDBODY;
    }
  }
  
  if (outLevel > 1)
    printf(" EGADS Info: Shape %d has %d Nodes, %d Edges, %d Loops, %d Faces and %d Shells\n",
           i+1, nNode, nEdge, nLoop, nFace, nShell);
  
  // allocate ego storage
  
  if (nNode > 0) {
    body->nodes.objs = (egObject **) EG_alloc(nNode*sizeof(egObject *));
    if (body->nodes.objs == NULL) return EGADS_MALLOC;
    for (j = 0; j < nNode; j++) {
      stat = EG_makeObject(context, &body->nodes.objs[j]);
      if (stat != EGADS_SUCCESS) {
        EG_cleanMaps(&body->nodes);
        return stat;
      }
    }
  }
  if (nEdge > 0) {
    body->edges.objs = (egObject **) EG_alloc(2*nEdge*sizeof(egObject *));
    if (body->edges.objs == NULL) {
      EG_cleanMaps(&body->nodes);
      return EGADS_MALLOC;
    }
    for (j = 0; j < 2*nEdge; j++) {
      stat = EG_makeObject(context, &body->edges.objs[j]);
      if (stat != EGADS_SUCCESS) {
        EG_cleanMaps(&body->edges);
        EG_cleanMaps(&body->nodes);
        return stat;
      }
    }
  }
  if (nLoop > 0) {
    body->loops.objs = (egObject **) EG_alloc(nLoop*sizeof(egObject *));
    if (body->loops.objs == NULL) {
      EG_cleanMaps(&body->edges);
      EG_cleanMaps(&body->nodes);
      return EGADS_MALLOC;
    }
    for (j = 0; j < nLoop; j++) {
      stat = EG_makeObject(context, &body->loops.objs[j]);
      if (stat != EGADS_SUCCESS) {
        EG_cleanMaps(&body->loops);
        EG_cleanMaps(&body->edges);
        EG_cleanMaps(&body->nodes);
        return stat;
      }
    }
  }
  if (nFace > 0) {
    body->faces.objs = (egObject **) EG_alloc(2*nFace*sizeof(egObject *));
    if (body->faces.objs == NULL) {
      EG_cleanMaps(&body->loops);
      EG_cleanMaps(&body->edges);
      EG_cleanMaps(&body->nodes);
      return EGADS_MALLOC;
    }
    for (j = 0; j < 2*nFace; j++) {
      stat = EG_makeObject(context, &body->faces.objs[j]);
      if (stat != EGADS_SUCCESS) {
        EG_cleanMaps(&body->faces);
        EG_cleanMaps(&body->loops);
        EG_cleanMaps(&body->edges);
        EG_cleanMaps(&body->nodes);
        return stat;
      }
    }
  }
  if (nShell > 0) {
    body->shells.objs = (egObject **) EG_alloc(nShell*sizeof(egObject *));
    if (body->shells.objs == NULL) {
      EG_cleanMaps(&body->faces);
      EG_cleanMaps(&body->loops);
      EG_cleanMaps(&body->edges);
      EG_cleanMaps(&body->nodes);
      return EGADS_MALLOC;
    }
    for (j = 0; j < nShell; j++) {
      stat = EG_makeObject(context, &body->shells.objs[j]);
      if (stat != EGADS_SUCCESS) {
        EG_cleanMaps(&body->shells);
        EG_cleanMaps(&body->faces);
        EG_cleanMaps(&body->loops);
        EG_cleanMaps(&body->edges);
        EG_cleanMaps(&body->nodes);
        return stat;
      }
    }
  }

  // fill our stuff

  for (j = 0; j < nNode; j++) {
    egadsNode *pnode   = new egadsNode;
    obj                = body->nodes.objs[j];
    shape              = body->nodes.map(j+1);
    TopoDS_Vertex Vert = TopoDS::Vertex(shape);
    gp_Pnt pv          = BRep_Tool::Pnt(Vert);
    pnode->node        = Vert;
    pnode->xyz[0]      = pv.X();
    pnode->xyz[1]      = pv.Y();
    pnode->xyz[2]      = pv.Z();
    obj->oclass        = NODE;
    obj->blind         = pnode;
    obj->topObj        = topObj;
  }
  
  for (j = 0; j < nEdge; j++) {
    int           n1, n2, degen = 0;
    TopoDS_Vertex V1, V2;
    Standard_Real t1, t2;

    egadsEdge *pedge = new egadsEdge;
    obj              = body->edges.objs[j];
    geom             = body->edges.objs[j+nEdge];
    shape            = body->edges.map(j+1);
    geom->topObj     = topObj;
    TopoDS_Edge Edge = TopoDS::Edge(shape);
    if (BRep_Tool::Degenerated(Edge)) {
      degen        = 1;
      geom->oclass = CURVE;
      geom->mtype  = DEGENERATE;
      geom->blind  = NULL;
    } else {
      Handle(Geom_Curve) hCurve = BRep_Tool::Curve(Edge, t1, t2);
      EG_completeCurve(geom, hCurve);
    }
    
    TopExp::Vertices(Edge, V2, V1, Standard_True);
    if (Edge.Orientation() != TopAbs_REVERSED) {
      n1 = body->nodes.map.FindIndex(V2);
      n2 = body->nodes.map.FindIndex(V1);    
    } else {
      n1 = body->nodes.map.FindIndex(V1);
      n2 = body->nodes.map.FindIndex(V2);
    }
    if (outLevel > 2)
      printf(" Edge %d:  nodes = %d %d  degen = %d (%lf, %lf)\n", 
             j+1, n1, n2, degen, t1, t2);
    egObject *pn1 = NULL;
    egObject *pn2 = NULL;
    if ((n1 == 0) || (n2 == 0))
      printf(" EGADS Warning: Node(s) not found for Edge!\n");
    if (n1 != 0) pn1 = body->nodes.objs[n1-1];
    if (n2 != 0) pn2 = body->nodes.objs[n2-1];

    pedge->edge     = Edge;
    pedge->curve    = geom;
    pedge->nodes[0] = pn1;
    pedge->nodes[1] = pn2;
    pedge->topFlg   = 0;
    obj->oclass     = EDGE;
    obj->blind      = pedge;
    obj->topObj     = topObj;
    obj->mtype      = TWONODE;
    if (n1 == n2) obj->mtype = ONENODE;
    if (degen == 1) {
      obj->mtype = DEGENERATE;
    } else {
      EG_referenceObject(geom, obj);
    }
    EG_referenceObject(pn1, obj);
    EG_referenceObject(pn2, obj);
  }
  
  for (j = 0; j < nLoop; j++) {
    int      *senses = NULL, closed = 0, ne = 0;
    egObject **edgeo = NULL;

    egadsLoop *ploop = new egadsLoop;
    obj              = body->loops.objs[j];
    shape            = body->loops.map(j+1);
    obj->oclass      = LOOP;
    if (shape.Closed()) closed = 1;
    TopoDS_Wire Wire = TopoDS::Wire(shape);
    BRepTools_WireExplorer ExpWE;
    for (ExpWE.Init(Wire); ExpWE.More(); ExpWE.Next()) ne++;
    if (outLevel > 2)
      printf(" Loop %d: # edges = %d, closed = %d\n", j+1, ne, closed);

    // find the Face
    TopoDS_Face Face;
    int         hit;
    for (hit = k = 0; k < nFace; k++) {
      TopoDS_Shape shapf = body->faces.map(k+1);
      Face = TopoDS::Face(shapf);
      TopExp_Explorer ExpW;
      for (ExpW.Init(shapf, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
        TopoDS_Shape shapw = ExpW.Current();
        TopoDS_Wire  fwire = TopoDS::Wire(shapw);
        if (fwire.IsSame(Wire)) {
          hit++;
          break;
        }
      }
      if (hit != 0) break;
    }
    if ((hit == 0) && (outLevel > 0) && (nFace != 0))
      printf(" EGADS Internal: Loop without a Face!\n");
    if (hit != 0) {
      geom = body->faces.objs[k+nFace];
      if (geom->oclass != SURFACE) {
        Handle(Geom_Surface) hSurface = BRep_Tool::Surface(Face);
        geom->topObj = topObj;
        EG_completeSurf(geom, hSurface);
      }
      hit = 2;
      if (geom->mtype == PLANE) hit = 1;
    } else {
      hit = 1;
    }
    if (hit == 1) {
      geom = NULL;
    } else {
      EG_referenceObject(geom, obj);
    }

    if (ne > 0) {
      edgeo  = new egObject*[hit*ne];
      senses = new int[ne];
    }
    k = 0;
#ifdef FACEWIRE
    if (hit != 0) {
      for (ExpWE.Init(Wire, Face); ExpWE.More(); ExpWE.Next()) {
        TopoDS_Shape shapW = ExpWE.Current();
        TopoDS_Edge  Edge  = TopoDS::Edge(shapW);
        int          ed    = body->edges.map.FindIndex(Edge);
        edgeo[k]           = NULL;
        senses[k]          = 1;
        if (shapW.Orientation() == TopAbs_REVERSED) senses[k] = -1;
        if (ed != 0) {
          egObject *eobj = body->edges.objs[ed-1];
          edgeo[k]       = eobj;
          if (hit == 2) edgeo[k+ne] = NULL;
          EG_referenceObject(eobj, obj);
        } else {
          printf(" EGADS Warning: Edge not found for Loop!\n");
        }
        if (outLevel > 2)
          printf("        %d  edge = %d   sense = %d\n", k, ed, senses[k]);
        k++;
      }
    } else {
#endif
      for (ExpWE.Init(Wire); ExpWE.More(); ExpWE.Next()) {
        TopoDS_Shape shapW = ExpWE.Current();
        TopoDS_Edge  Edge  = TopoDS::Edge(shapW);
        int          ed    = body->edges.map.FindIndex(Edge);
        edgeo[k]           = NULL;
        senses[k]          = 1;
        if (shapW.Orientation() == TopAbs_REVERSED) senses[k] = -1;
        if (ed != 0) {
          egObject *eobj = body->edges.objs[ed-1];
          edgeo[k]       = eobj;
          if (hit == 2) edgeo[k+ne] = NULL;
          EG_referenceObject(eobj, obj);
        } else {
          printf(" EGADS Warning: Edge not found for Loop!\n");
        }
        if (outLevel > 2)
          printf("        %d  edge = %d   sense = %d\n", k, ed, senses[k]);
        k++;
      }
#ifdef FACEWIRE
    }
#endif
    ploop->loop    = Wire;
    ploop->surface = geom;
    ploop->nedges  = ne;
    ploop->edges   = edgeo;
    ploop->senses  = senses;
    ploop->topFlg  = 0;
    obj->blind     = ploop;
    obj->topObj    = topObj;
    obj->mtype     = OPEN;
    if (closed == 1) obj->mtype = CLOSED;
    if (bobj->mtype == WIREBODY) EG_referenceObject(obj, bobj);
  }

  for (j = 0; j < nFace; j++) {
    int      *senses = NULL;
    egObject **loopo = NULL;
    
    egadsFace *pface = new egadsFace;
    obj              = body->faces.objs[j];
    geom             = body->faces.objs[j+nFace];
    shape            = body->faces.map(j+1);
    obj->oclass      = FACE;
    TopoDS_Face Face = TopoDS::Face(shape);
    if (geom->oclass != SURFACE) {
      Handle(Geom_Surface) hSurface = BRep_Tool::Surface(Face);
      geom->topObj = topObj;
      EG_completeSurf(geom, hSurface);
    }
    EG_referenceObject(geom, obj);

    int nl = 0;
    TopExp_Explorer ExpW;
    for (ExpW.Init(shape, TopAbs_WIRE); ExpW.More(); ExpW.Next()) nl++;
    if (outLevel > 2)
      printf(" Face %d: # loops = %d\n", j+1, nl);
    TopoDS_Wire oWire = BRepTools::OuterWire(Face);

    if (nl > 0) {
      loopo  = new egObject*[nl];
      senses = new int[nl];
    }
    k = 0;
    for (ExpW.Init(shape, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
      TopoDS_Shape shapw = ExpW.Current();
      TopoDS_Wire  Wire  = TopoDS::Wire(shapw);
      loopo[k]           = NULL;
      senses[k]          = -1;
      if (Wire.IsSame(oWire)) senses[k] = 1;
      int lp = body->loops.map.FindIndex(Wire);
      if (lp != 0) {
        loopo[k] = body->loops.objs[lp-1];
        EG_fillPCurves(Face, geom, loopo[k], topObj);
        EG_referenceObject(loopo[k], obj);
      } else {
        printf(" EGADS Warning: Loop not found for Face!\n");
      }
      if (outLevel > 2)
        printf("        %d  loop = %d     outer = %d\n", k, lp, senses[k]);
      k++;
    }
    pface->face    = Face;
    pface->surface = geom;
    pface->nloops  = nl;
    pface->loops   = loopo;
    pface->senses  = senses;
    pface->topFlg  = 0;
    obj->blind     = pface;
    obj->topObj    = topObj;
    obj->mtype     = SFORWARD;
    if (Face.Orientation() == TopAbs_REVERSED) obj->mtype = SREVERSE;
    if (bobj->mtype == FACEBODY) EG_referenceObject(obj, bobj);
  }
  
  if (nShell > 0) {
    TopoDS_Shell oShell;
    if (solid == 1) {
      TopoDS_Solid Solid = TopoDS::Solid(body->shape);
      oShell = BRepTools::OuterShell(Solid);
      body->senses = new int[nShell];
    }

    for (j = 0; j < nShell; j++) {
      egObject   **faceo = NULL;
      egadsShell *pshell = new egadsShell;
      obj                = body->shells.objs[j];
      shape              = body->shells.map(j+1);
      obj->oclass        = SHELL;
      TopoDS_Shell Shell = TopoDS::Shell(shape);
      if (solid == 1) {
        body->senses[j] = -1;
        if (Shell.IsSame(oShell)) body->senses[j] = 1;
      }

      int nf = 0;
      TopExp_Explorer ExpF;
      for (ExpF.Init(shape, TopAbs_FACE); ExpF.More(); ExpF.Next()) nf++;
      
      if (nf > 0) faceo = new egObject*[nf];

      k = 0;
      for (ExpF.Init(shape, TopAbs_FACE); ExpF.More(); ExpF.Next()) {
        TopoDS_Shape shapf = ExpF.Current();
        TopoDS_Face  Face  = TopoDS::Face(shapf);
        faceo[k]           = NULL;
        int fa = body->faces.map.FindIndex(Face);
        if (fa != 0) {
          faceo[k] = body->faces.objs[fa-1];
          EG_referenceObject(faceo[k], obj);
        } else {
          printf(" EGADS Warning: Face not found for Shell!\n");
        }
        if (outLevel > 2)
          printf(" Shell %d/%d: Face = %d\n", k, j+1, fa);
        k++;
      }
      pshell->shell  = Shell;
      pshell->nfaces = nf;
      pshell->faces  = faceo;
      pshell->topFlg = 0;
      obj->blind     = pshell;
      obj->topObj    = topObj;
      obj->mtype     = EG_shellClosure(pshell, 0);
      if (bobj->mtype >= SHEETBODY) EG_referenceObject(obj, bobj);
    }
  }

  return EGADS_SUCCESS;
}


int
EG_getTolerance(const egObject *topo, double *tol)
{
  *tol = 0.0;
  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (topo->oclass <  NODE)       return EGADS_NOTTOPO;
  if (topo->oclass >= MODEL)      return EGADS_NOTTOPO;
  
  if (topo->oclass == NODE) {
    egadsNode *pnode = (egadsNode *) topo->blind;
    if (pnode != NULL) *tol = BRep_Tool::Tolerance(pnode->node);
  } else if (topo->oclass == EDGE) {
    egadsEdge *pedge = (egadsEdge *) topo->blind;
    if (pedge != NULL) *tol = BRep_Tool::Tolerance(pedge->edge);
  } else if (topo->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) topo->blind;
    if (ploop != NULL)
      for (int i = 0; i < ploop->nedges; i++) {
        egadsEdge *pedge = (egadsEdge *) ploop->edges[i]->blind;
        if (pedge == NULL) continue;
        double toler = BRep_Tool::Tolerance(pedge->edge);
        if (toler > *tol) *tol = toler;
      }
  } else if (topo->oclass == FACE) {
    egadsFace *pface = (egadsFace *) topo->blind;
    if (pface != NULL) *tol = BRep_Tool::Tolerance(pface->face);
  } else if (topo->oclass == SHELL) {
    egadsShell *pshell = (egadsShell *) topo->blind;
    if (pshell != NULL)
      for (int i = 0; i < pshell->nfaces; i++) {
        egadsFace *pface = (egadsFace *) pshell->faces[i]->blind;
        if (pface == NULL) continue;
        double toler = BRep_Tool::Tolerance(pface->face);
        if (toler > *tol) *tol = toler;
      }
  } else {
    egadsBody *pbody = (egadsBody *) topo->blind;
    if (pbody != NULL)
      if (topo->mtype == WIREBODY) {
        int nedge = pbody->edges.map.Extent();
        for (int i = 0; i < nedge; i++) {
          TopoDS_Edge Edge = TopoDS::Edge(pbody->edges.map(i+1));
          double toler     = BRep_Tool::Tolerance(Edge);
          if (toler > *tol) *tol = toler;
        }
      } else {
        int nface = pbody->faces.map.Extent();
        for (int i = 0; i < nface; i++) {
          TopoDS_Face Face = TopoDS::Face(pbody->faces.map(i+1));
          double toler     = BRep_Tool::Tolerance(Face);
          if (toler > *tol) *tol = toler;
        }
      }
  }
  
  return EGADS_SUCCESS;
}


int
EG_getTopology(const egObject *topo, egObject **geom, int *oclass, 
               int *type, /*@null@*/ double *limits, int *nChildren, 
               egObject ***children, int **senses)
{
  *geom      = NULL;
  *oclass    = *type = 0;
  *nChildren = 0;
  *children  = NULL;
  *senses    = NULL;
  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (topo->oclass < NODE)        return EGADS_NOTTOPO;
  *oclass = topo->oclass;
  *type   = topo->mtype;
  
  if (topo->oclass == NODE) {
  
    egadsNode *pnode = (egadsNode *) topo->blind;
    if ((limits != NULL) && (pnode != NULL)) {
      limits[0] = pnode->xyz[0];
      limits[1] = pnode->xyz[1];
      limits[2] = pnode->xyz[2];
    }

  } else if (topo->oclass == EDGE) {
  
    egadsEdge *pedge = (egadsEdge *) topo->blind;
    if (pedge != NULL) {
      *geom      = pedge->curve;
      *nChildren = 1;
      if (topo->mtype == TWONODE) *nChildren = 2;
      *children = pedge->nodes;
      if (limits != NULL)
        BRep_Tool::Range(pedge->edge, limits[0], limits[1]);
    }

  } else if (topo->oclass == LOOP) {
  
    egadsLoop *ploop = (egadsLoop *) topo->blind;
    if (ploop != NULL) {
      *geom      = ploop->surface;
      *nChildren = ploop->nedges;
      *children  = ploop->edges;
      *senses    = ploop->senses;
    }
  
  } else if (topo->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) topo->blind;
    if (pface != NULL) {
      *geom      = pface->surface;
      *nChildren = pface->nloops;
      *children  = pface->loops;
      *senses    = pface->senses;
      if (limits != NULL) {
        Standard_Real umin, umax, vmin, vmax;

        BRepTools::UVBounds(pface->face, umin, umax, vmin, vmax);
        limits[0] = umin;
        limits[1] = umax;
        limits[2] = vmin;
        limits[3] = vmax;
      }
    }
    
  } else if (topo->oclass == SHELL) {
  
    egadsShell *pshell = (egadsShell *) topo->blind;
    if (pshell != NULL) {
      *nChildren = pshell->nfaces;
      *children  = pshell->faces;
    }
    
  } else if (topo->oclass == BODY) {
  
    egadsBody *pbody = (egadsBody *) topo->blind;
    if (pbody != NULL)
      if (topo->mtype == WIREBODY) {
        *nChildren = pbody->loops.map.Extent();
        *children  = pbody->loops.objs;
      } else if (topo->mtype == FACEBODY) {
        *nChildren = pbody->faces.map.Extent();
        *children  = pbody->faces.objs;
      } else {
        *nChildren = pbody->shells.map.Extent();
        *children  = pbody->shells.objs;
        if (topo->mtype == SOLIDBODY) *senses = pbody->senses;
      }
    
  } else {
  
    egadsModel *pmodel = (egadsModel *) topo->blind;
    if (pmodel != NULL) {
      *nChildren = pmodel->nbody;
      *children  = pmodel->bodies;
    }

  }
  
  return EGADS_SUCCESS;
}


static void
EG_makePCurves(TopoDS_Face face, egObject *surfo, egObject *loopo,
               Standard_Real prec, int flag)
{
  int      i = 0;
  egObject *geom;

  egadsLoop *ploop = (egadsLoop *) loopo->blind;
  if (ploop->surface == NULL) return;
  if (ploop->surface != surfo) {
    printf(" EGADS Internal: Loop/Face mismatch on Surface (EG_makePCurves)!\n");
    return;
  }

  int outLevel     = EG_outLevel(surfo);
  TopoDS_Wire wire = ploop->loop;
  BRep_Builder           Builder;
  BRepTools_WireExplorer ExpWE;
  for (ExpWE.Init(wire); ExpWE.More(); ExpWE.Next(), i++) {
    geom                = ploop->edges[ploop->nedges+i];
    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    TopoDS_Shape shape  = ExpWE.Current();
    TopoDS_Edge  edge   = TopoDS::Edge(shape);
    if (ppcurv == NULL) continue;
    Handle(Geom2d_Curve) hCurv2d = ppcurv->handle;

    if ((flag == 0) || (outLevel > 2)) {
      TopoDS_Vertex V1, V2;
      Standard_Real t, t1, t2, delta, mdelta;
      gp_Pnt        pnt, pnte, pv1, pv2;
      gp_Pnt2d      uv;

      egadsSurface *psurf = (egadsSurface *) surfo->blind;
      Handle(Geom_Surface) hSurface = psurf->handle;
      if (edge.Orientation() == TopAbs_REVERSED) {
        TopExp::Vertices(edge, V2, V1, Standard_True);
      } else {
        TopExp::Vertices(edge, V1, V2, Standard_True);
      }
      Handle(Geom_Curve) hCurve = BRep_Tool::Curve(edge, t1, t2);
      pv1 = BRep_Tool::Pnt(V1);
      pv2 = BRep_Tool::Pnt(V2);
      if (outLevel > 2)
        printf(" PCurve #%d: Limits = %lf %lf    prec = %le\n",
               i, t1, t2, prec);
      hCurv2d->D0(t1, uv);
      hSurface->D0(uv.X(), uv.Y(), pnt);
      mdelta = sqrt((pnt.X()-pv1.X())*(pnt.X()-pv1.X()) +
                    (pnt.Y()-pv1.Y())*(pnt.Y()-pv1.Y()) +
                    (pnt.Z()-pv1.Z())*(pnt.Z()-pv1.Z()));
      if (outLevel > 2)
        printf("            delta for 1st Node     = %le  %lf %lf %lf\n", 
               mdelta, pv1.X(), pv1.Y(), pv1.Z());
      delta = 0.0;
      for (int j = 1; j < 36; j++) {
        t = t1 + j*(t2-t1)/36.0;
        hCurv2d->D0(t, uv);
        hSurface->D0(uv.X(), uv.Y(), pnt);
        if (BRep_Tool::Degenerated(edge)) {
          pnte = pv1;
        } else {
          hCurve->D0(t, pnte);
        }
        delta += sqrt((pnt.X()-pnte.X())*(pnt.X()-pnte.X()) +
                      (pnt.Y()-pnte.Y())*(pnt.Y()-pnte.Y()) +
                      (pnt.Z()-pnte.Z())*(pnt.Z()-pnte.Z()));
      }
      delta /= 35;
      if (outLevel > 2)
        printf("            ave delta against Edge = %le\n", delta);
      if (delta > mdelta) mdelta = delta;
      hCurv2d->D0(t2, uv);
      hSurface->D0(uv.X(), uv.Y(), pnt);
      delta = sqrt((pnt.X()-pv2.X())*(pnt.X()-pv2.X()) +
                   (pnt.Y()-pv2.Y())*(pnt.Y()-pv2.Y()) +
                   (pnt.Z()-pv2.Z())*(pnt.Z()-pv2.Z()));
      if (outLevel > 2)
        printf("            delta for 2nd Node     = %le  %lf %lf %lf\n", 
               delta, pv2.X(), pv2.Y(), pv2.Z());
      if (delta > mdelta) mdelta = delta;
      if ((flag == 0) && (mdelta*1.001 > prec)) 
        Builder.SameParameter(edge, Standard_False);
    }

    Builder.UpdateEdge(edge, hCurv2d, face, prec);
  }

}


int
EG_makeTopology(egObject *context, /*@null@*/ egObject *geom, 
                int oclass, int mtype, /*@null@*/ double *limits, 
                int nChildren, /*@null@*/ egObject **children, 
                /*@null@*/ int *senses, egObject **topo)
{
  int      i, n, stat, outLevel;
  egCntxt  *cntx;
  egObject *obj;

  *topo = NULL;
  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  cntx = (egCntxt *) context->blind;
  if (cntx == NULL)                  return EGADS_NODATA;
  outLevel = cntx->outLevel;

  if ((oclass < NODE) || (oclass > MODEL)) {
    if (outLevel > 0)
      printf(" EGADS Error: oclass = %d (EG_makeTopology)!\n", oclass);
    return EGADS_NOTTOPO;
  }
  
  if (oclass == NODE) {

    if (limits == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Node with no Data (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }

    gp_Pnt pnt(limits[0], limits[1], limits[2]);
    TopoDS_Vertex vert = BRepBuilderAPI_MakeVertex(pnt);
    BRepCheck_Analyzer vCheck(vert);
    if (!vCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Info: Node is invalid (EG_makeTopology)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Node object (EG_makeTopology)!\n");
      return stat;
    }        
    egadsNode *pnode   = new egadsNode;
    pnode->node        = vert;
    pnode->xyz[0]      = limits[0];
    pnode->xyz[1]      = limits[1];
    pnode->xyz[2]      = limits[2];
    obj->oclass        = NODE;
    obj->blind         = pnode;
    obj->topObj        = context;
    EG_referenceObject(obj, context);

  } else if (oclass == EDGE) {
    TopoDS_Vertex V1, V2;
    Standard_Real P1, P2;

    if (limits == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Limits is NULL (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    if (limits[0] >= limits[1]) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge Tmin (%lf) >= Tmax (%lf) (EG_makeTopology)!\n",
               limits[0], limits[1]);
      return EGADS_RANGERR;
    }

    if (mtype == DEGENERATE) {
      if (nChildren != 1) {
        if (outLevel > 0)
          printf(" EGADS Error: Degen Edge with %d Verts (EG_makeTopology)!\n",
                 nChildren);
        return EGADS_TOPOERR;
      }
      if (children[0] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Degen Edge with Vert NULL (EG_makeTopology)!\n");
        return EGADS_NULLOBJ;
      }
      if (children[0]->oclass != NODE) {
        if (outLevel > 0)
          printf(" EGADS Error: Degen Edge with nonNode Child (EG_makeTopology)!\n");
        return EGADS_NOTTOPO;
      }
      if (children[0]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Degen Edge with NULL Node Child (EG_makeTopology)!\n");
        return EGADS_NODATA;
      }

      // make a degenerate circle
      egadsNode *pnode = (egadsNode *) children[0]->blind;
      V1 = pnode->node;
      P1 = limits[0];
      P2 = limits[1];
      gp_Pnt pv = BRep_Tool::Pnt(V1);
      gp_Ax2 axi2(pv, gp_Dir(1.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
      Handle(Geom_Curve) hCurve = new Geom_Circle(axi2, 0.0);
      BRepBuilderAPI_MakeEdge MEdge(hCurve, V1, V1, P1, P2);
      TopoDS_Edge Edge = MEdge.Edge();
      BRep_Builder Builder;
      Builder.Degenerated(Edge, Standard_True);
      if (!BRep_Tool::Degenerated(Edge))
        printf(" EGADS Info: Degenerate Edge NOT Degenerate!\n");
      BRepCheck_Analyzer eCheck(Edge);
      if (!eCheck.IsValid()) {
        if (outLevel > 0)
          printf(" EGADS Info: Degen Edge is invalid (EG_makeTopology)!\n");
        return EGADS_CONSTERR;
      }

      stat = EG_makeObject(context, &obj);
      if (stat != EGADS_SUCCESS) {
        if (outLevel > 0)
          printf(" EGADS Error: Cannot make Degen Edge object (EG_makeTopology)!\n");
        return stat;
      }
      egadsEdge *pedge = new egadsEdge;
      pedge->edge      = Edge;
      pedge->curve     = NULL;
      pedge->nodes[0]  = children[0];
      pedge->nodes[1]  = children[0];
      pedge->topFlg    = 1;
      obj->oclass      = EDGE;
      obj->blind       = pedge;
      obj->topObj      = context;
      obj->mtype       = DEGENERATE;
      EG_referenceTopObj(pedge->nodes[0],  obj);
      EG_referenceTopObj(pedge->nodes[1],  obj);
      EG_referenceObject(obj,          context);
        
      *topo = obj;
      return EGADS_SUCCESS;
    }

    if (geom == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with NULL Geom (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (geom->blind == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with No Geom (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    if (geom->oclass != CURVE) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge Geom not CURVE (EG_makeTopology)!\n");
      return EGADS_NOTGEOM;
    }
    if (children == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with NULL Children (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if ((nChildren != 1) && (nChildren != 2)) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with %d Verts (EG_makeTopology)!\n",
               nChildren);
      return EGADS_TOPOERR;
    }
    if (children[0] == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with Vert[0] NULL (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (children[0]->oclass != NODE) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with nonNode Child[0] (EG_makeTopology)!\n");
      return EGADS_NOTTOPO;
    }
    if (children[0]->blind == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge with NULL Node Child[0] (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    if (nChildren == 2) {
      if (children[1] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Edge with Vert[1] NULL (EG_makeTopology)!\n");
        return EGADS_NULLOBJ;
      }
      if (children[1]->oclass != NODE) {
        if (outLevel > 0)
          printf(" EGADS Error: Edge with nonNode Child[1] (EG_makeTopology)!\n");
        return EGADS_NOTTOPO;
      }
      if (children[1]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Edge with NULL Node Child[1] (EG_makeTopology)!\n");
        return EGADS_NODATA;
      }
    }
    egadsCurve *pcurve = (egadsCurve *) geom->blind;
    egadsNode  *pnode1 = (egadsNode *)  children[0]->blind;
    egadsNode  *pnode2 = (egadsNode *)  children[0]->blind;
    if (nChildren == 2) pnode2 = (egadsNode *) children[1]->blind;

    P1 = limits[0];
    P2 = limits[1];
    V1 = pnode1->node;
    V2 = pnode2->node;
    Handle(Geom_Curve) hCurve = pcurve->handle;
    gp_Pnt pnt1, pnt2, pv1, pv2;
    hCurve->D0(P1, pnt1);
    hCurve->D0(P2, pnt2);
    pv1 = BRep_Tool::Pnt(V1);
    pv2 = BRep_Tool::Pnt(V2);
    if (outLevel > 2) {
      printf(" P1 = %lf %lf %lf  %lf %lf %lf\n", pnt1.X(), pnt1.Y(), pnt1.Z(),
                pnode1->xyz[0], pnode1->xyz[1], pnode1->xyz[2]);
      printf("      vert = %lf %lf %lf\n", pv1.X(), pv1.Y(), pv1.Z());
      printf(" P2 = %lf %lf %lf  %lf %lf %lf\n", pnt2.X(), pnt2.Y(), pnt2.Z(),
              pnode2->xyz[0], pnode2->xyz[1], pnode2->xyz[2]);
      printf("      vert = %lf %lf %lf\n", pv2.X(), pv2.Y(), pv2.Z());
    }
    double delta1 = sqrt((pnt1.X()-pv1.X())*(pnt1.X()-pv1.X()) +
                         (pnt1.Y()-pv1.Y())*(pnt1.Y()-pv1.Y()) +
                         (pnt1.Z()-pv1.Z())*(pnt1.Z()-pv1.Z()));
    double delta2 = sqrt((pnt2.X()-pv2.X())*(pnt2.X()-pv2.X()) +
                         (pnt2.Y()-pv2.Y())*(pnt2.Y()-pv2.Y()) +
                         (pnt2.Z()-pv2.Z())*(pnt2.Z()-pv2.Z()));

    Standard_Real old  = BRepBuilderAPI::Precision();
    Standard_Real prec = old;
    if (outLevel > 1)
      printf("   Limits = %f %lf, Tol = %le %le   %le\n",
             P1, P2, delta1, delta2, old);
    if (delta1*1.001 > prec) prec = 1.001*delta1;
    if (delta2*1.001 > prec) prec = 1.001*delta2;
    BRepBuilderAPI::Precision(prec);
    BRepBuilderAPI_MakeEdge MEdge;
    MEdge.Init(hCurve, V1, V2, P1, P2);
    BRepBuilderAPI::Precision(old);
    if (!MEdge.IsDone()) {
      if (outLevel > 0)
        printf(" EGADS Error: Problem with the Edge (EG_makeTopology)!\n");
       return EGADS_NODATA;
    }
    TopoDS_Edge Edge = MEdge.Edge();
    BRepCheck_Analyzer eCheck(Edge);
    if (!eCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Info: Edge is invalid (EG_makeTopology)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Edge object (EG_makeTopology)!\n");
      return stat;
    }
    egadsEdge *pedge = new egadsEdge;
    pedge->edge      = Edge;
    pedge->curve     = geom;
    pedge->nodes[0]  = children[0];
    pedge->nodes[1]  = children[0];
    pedge->topFlg    = 1;
    obj->oclass      = EDGE;
    obj->blind       = pedge;
    obj->topObj      = context;
    obj->mtype       = ONENODE;
    if (nChildren == 2) {
      obj->mtype      = TWONODE;
      pedge->nodes[1] = children[1];
    }
    EG_referenceTopObj(geom,             obj);
    EG_referenceTopObj(pedge->nodes[0],  obj);
    EG_referenceTopObj(pedge->nodes[1],  obj);
    EG_referenceObject(obj,          context);

  } else if (oclass == LOOP) {

    if ((children == NULL) || (senses == NULL)) {
      if (outLevel > 0)
        printf(" EGADS Error: Loop with NULL Input (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (nChildren <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Loop with %d Edges (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    n = 1;
    if (geom != NULL) {
      if (geom->oclass != SURFACE) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop Geom not SURFACE (EG_makeTopology)!\n");
        return EGADS_NOTGEOM;
      }
      if (geom->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop with No Geom Data (EG_makeTopology)!\n");
        return EGADS_NODATA;
      }
      n = 2;
    }
    for (i = 0; i < nChildren; i++) {
      if (children[i] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop with Edge[%d] NULL (EG_makeTopology)!\n", i);
        return EGADS_NULLOBJ;
      }
      if (children[i]->oclass != EDGE) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop with nonEdge Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop with NULL Edge Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NODATA;
      }
      if (geom != NULL) {
        if (children[i+nChildren] == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Loop with PCurve[%d] NULL (EG_makeTopology)!\n", i);
          return EGADS_NULLOBJ;
        }
        if (children[i+nChildren]->oclass != PCURVE) {
          if (outLevel > 0)
            printf(" EGADS Error: Loop with nonPCurve Child[%d] (EG_makeTopology)!\n",
                   i);
          return EGADS_NOTTOPO;
        }
        if (children[i+nChildren]->blind == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Loop with NULL PCurve Child[%d] (EG_makeTopology)!\n",
                   i);
          return EGADS_NODATA;
        }
      }
    }
    BRepBuilderAPI_MakeWire MW;
    for (i = 0; i < nChildren; i++) {
      egadsEdge *pedge = (egadsEdge *) children[i]->blind;
      TopoDS_Edge edge = pedge->edge;
      // may only be required for the first Edge, must be in order!
      if (edge.Orientation() == TopAbs_REVERSED) {
        if (senses[i] ==  1) edge.Orientation(TopAbs_FORWARD);
      } else {
        if (senses[i] == -1) edge.Orientation(TopAbs_REVERSED);
      }
/*
      if (edge.Orientation() == TopAbs_REVERSED) {
        if (senses[i] ==  1) {
          TopoDS_Shape shape = edge.Oriented(TopAbs_FORWARD);
          edge = TopoDS::Edge(shape);
        }
      } else {
        if (senses[i] == -1) {
          TopoDS_Shape shape = edge.Oriented(TopAbs_REVERSED);
          edge = TopoDS::Edge(shape);
        }
      }
*/
      MW.Add(edge);
      if (MW.Error()) {
        if (outLevel > 0)
          printf(" EGADS Error: Problem with Edge %d (EG_makeTopology)!\n",
                 i+1);
        return EGADS_NODATA;
      }
    }
    if (!MW.IsDone()) {
      if (outLevel > 0)
        printf(" EGADS Error: Problem with Loop (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    TopoDS_Wire wire = MW.Wire();
    
    // validate against the senses
    if (outLevel > 2) {
      i = 0;
      BRepTools_WireExplorer ExpWE;
      for (ExpWE.Init(wire); ExpWE.More(); ExpWE.Next()) {
        TopoDS_Shape shape = ExpWE.Current();
        TopoDS_Edge  Edge  = TopoDS::Edge(shape);
        int sense = 1;
        if (shape.Orientation() == TopAbs_REVERSED) sense = -1;
        egadsEdge *pedge = (egadsEdge *) children[i]->blind;
        if (Edge.IsSame(pedge->edge)) {
          printf("  %d: Edges same senses = %d %d\n", 
                 i, senses[i], sense);
        } else {
          printf("  %d: Edges NOT the same senses = %d %d\n",
                 i, senses[i], sense);
        }
        i++;
      }
    }
    
    BRepCheck_Analyzer wCheck(wire);
    if (!wCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Info: Wire is invalid (EG_makeTopology)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Loop object (EG_makeTopology)!\n");
      return stat;
    }
    obj->oclass = LOOP;

    egadsLoop *ploop  = new egadsLoop;
    egObject  **edgeo = new egObject*[n*nChildren];
    int       *esense = new int[nChildren];
    int       closed  = 0;
    if (wire.Closed()) closed = 1;
    for (i = 0; i < nChildren; i++) {
      edgeo[i]  = children[i];
      esense[i] = senses[i];
      EG_referenceTopObj(children[i], obj);
      if (n == 1) continue;
      edgeo[i+nChildren] = children[i+nChildren];
      EG_referenceTopObj(children[i+nChildren], obj);
    }
    ploop->loop    = wire;
    ploop->surface = geom;
    ploop->nedges  = nChildren;
    ploop->edges   = edgeo;
    ploop->senses  = esense;
    ploop->topFlg  = 1;
    obj->blind     = ploop;
    obj->topObj    = context;
    obj->mtype     = OPEN;
    if (closed == 1) obj->mtype = CLOSED;
    EG_referenceObject(obj, context);
    if (geom != NULL)
      if (geom->mtype == PLANE) {
        ploop->surface = NULL;
      } else {
        EG_referenceTopObj(geom, obj);
      }
    if (mtype == CLOSED)
      if ((outLevel > 0) && (closed == 0))
        printf(" EGADS Info: Wire is Open (EG_makeTopology)!\n");
    if (mtype == OPEN)
      if ((outLevel > 0) && (closed == 1))
        printf(" EGADS Info: Wire is Closed (EG_makeTopology)!\n");
    
  } else if (oclass == FACE) {

    if ((mtype != SFORWARD) && (mtype != SREVERSE)) {
      if (outLevel > 0)
        printf(" EGADS Error: Face with MType = %d (EG_makeTopology)!\n",
               mtype);
      return EGADS_RANGERR;
    }
    if (geom == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Face with NULL Geom (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (geom->oclass != SURFACE) {
      if (outLevel > 0)
        printf(" EGADS Error: Face Geom not SURFACE (EG_makeTopology)!\n");
      return EGADS_NOTGEOM;
    }
    if (geom->blind == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Face with No Geom (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    egadsSurface *psurf = (egadsSurface *) geom->blind;
    if (children == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Face with NULL Children (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (nChildren <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Face with %d Loops (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    for (i = 0; i < nChildren; i++) {
      if (children[i] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Face with Loop[%d] NULL (EG_makeTopology)!\n", i);
        return EGADS_NULLOBJ;
      }
      if (children[i]->oclass != LOOP) {
        if (outLevel > 0)
          printf(" EGADS Error: Face with nonLoop Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->mtype != CLOSED) {
        if (outLevel > 0)
          printf(" EGADS Error: Face with OPEN Loop[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Face with NULL Loop Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NODATA;
      }
      egadsLoop *ploop = (egadsLoop *) children[i]->blind;
      if ((ploop->surface != geom) && (geom->mtype != PLANE)) {
        if (outLevel > 0)
          printf(" EGADS Error: Face/Loop[%d] Geom Mismatch (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTGEOM;
      }
    }
    
    TopoDS_Face face;
    BRepBuilderAPI_MakeFace MFace;
    Standard_Real old = BRepBuilderAPI::Precision();
    Standard_Real prc = old;
    int nTrys = 5;              // # of tol attempts before setting SameParam = F
    for (int itry = 0; itry < nTrys; itry++) {
      if (itry != 0) face.Nullify();
      if (itry == nTrys-1) {    // last attempt -- set tol back
        prc = old;
        BRepBuilderAPI::Precision(old);
      }
#if CASVER >= 652
      MFace.Init(psurf->handle, Standard_False, prc);
#else
      MFace.Init(psurf->handle, Standard_False);
#endif
      for (i = 0; i < nChildren; i++) {
        egadsLoop *ploop = (egadsLoop *) children[i]->blind;
        TopoDS_Wire wire = ploop->loop;
        if (mtype == SREVERSE) wire.Reverse();
/*      if (mtype == SREVERSE) {
          TopoDS_Shape shape = wire.Reversed();
          wire = TopoDS::Wire(shape);
        }  */
        MFace.Add(wire);
        if (MFace.Error()) {
          if (outLevel > 0)
            printf(" EGADS Error: Problem with Loop %d (EG_makeTopology)!\n",
                   i+1);
          return EGADS_NODATA;
        }
      }
      if (MFace.IsDone()) {
        face = MFace.Face();
        if (mtype == SREVERSE) {
          face.Orientation(TopAbs_REVERSED);
        } else {
          face.Orientation(TopAbs_FORWARD);
        }
        for (i = 0; i < nChildren; i++)
          EG_makePCurves(face, geom, children[i], prc, nTrys-itry-1);
        BRepLib::SameParameter(face);
/*      BRepTools::Update(face);
        BRepTools::UpdateFaceUVPoints(face);  */
        BRepCheck_Analyzer oCheck(face);
        if (oCheck.IsValid()) break;
      }
      prc *= 10.0;
      BRepBuilderAPI::Precision(prc);
      if (outLevel > 1)
        printf(" EGADS Info: Adjusting Precision for Face - itry = %d  prec = %lf\n", 
               itry, prc);
    }
    BRepBuilderAPI::Precision(old);
    if (!MFace.IsDone()) {
      if (outLevel > 0)
        printf(" EGADS Error: Problem with the Face (EG_makeTopology)!\n");
      return EGADS_NODATA;
    }
    BRepCheck_Analyzer fCheck(face);
    if (!fCheck.IsValid()) {
      // try to fix the fault
      Handle_ShapeFix_Shape sfs = new ShapeFix_Shape(face);
      sfs->Perform();
      TopoDS_Shape fixedFace = sfs->Shape();
      if (fixedFace.IsNull()) {
        if (outLevel > 0) {
          printf(" EGADS Info: Invalid Face w/ NULL Fix (EG_makeTopology)!\n");
          EG_checkStatus(fCheck.Result(face));
        }
        return  EGADS_CONSTERR;
      }
      BRepCheck_Analyzer fxCheck(fixedFace);
      if (!fxCheck.IsValid()) {
        if (outLevel > 0) {
          printf(" EGADS Info: Face is invalid (EG_makeTopology)!\n");
          EG_checkStatus(fxCheck.Result(fixedFace));
        }
        return  EGADS_CONSTERR;
      }
      face = TopoDS::Face(fixedFace);
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Face object (EG_makeTopology)!\n");
      return stat;
    }
    obj->oclass = FACE;

    egadsFace *pface = new egadsFace;
    egObject **loopo = new egObject*[nChildren];
    int      *lsense = new int[nChildren];
    for (i = 0; i < nChildren; i++) {
      loopo[i]  = children[i];
      lsense[i] = senses[i];
      EG_referenceTopObj(children[i], obj);
    }
    pface->face    = face;
    pface->surface = geom;
    pface->nloops  = nChildren;
    pface->loops   = loopo;
    pface->senses  = lsense;
    pface->topFlg  = 1;
    obj->blind     = pface;
    obj->topObj    = context;
    EG_referenceTopObj(geom, obj);
    EG_referenceObject(obj,  context);

  } else if (oclass == SHELL) {

    if (children == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Shell with NULL Input (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (nChildren <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Shell with %d Faces (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    for (i = 0; i < nChildren; i++) {
      if (children[i] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Shell with Face[%d] NULL (EG_makeTopology)!\n", i);
        return EGADS_NULLOBJ;
      }
      if (children[i]->oclass != FACE) {
        if (outLevel > 0)
          printf(" EGADS Error: Shell with nonFace Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Shell with NULL Face Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NODATA;
      }
    }
    BRep_Builder builder3D;
    TopoDS_Shell shell;
    builder3D.MakeShell(shell);
    for (i = 0; i < nChildren; i++) {
      egadsFace *pface = (egadsFace *) children[i]->blind;
      builder3D.Add(shell, pface->face);
    }
    BRepLib::SameParameter(shell);
    BRepCheck_Analyzer shCheck(shell);
    if (!shCheck.IsValid()) {
      if (outLevel > 0) {
        printf(" EGADS Info: Shell is invalid (EG_makeTopology)!\n");
        EG_checkStatus(shCheck.Result(shell));
      }
      if (mtype != DEGENERATE) return EGADS_CONSTERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Shell object (EG_makeTopology)!\n");
      return stat;
    }
   obj->oclass = SHELL;

    egadsShell *pshell = new egadsShell;
    egObject   **faceo = new egObject*[nChildren];
    for (i = 0; i < nChildren; i++) {
      faceo[i] = children[i];
      EG_referenceTopObj(children[i], obj);
    }
    pshell->shell  = shell;
    pshell->nfaces = nChildren;
    pshell->faces  = faceo;
    pshell->topFlg = 1;
    obj->blind     = pshell;
    obj->topObj    = context;
    obj->mtype     = EG_shellClosure(pshell, mtype);
    EG_referenceObject(obj, context);
    if (mtype == CLOSED)
      if ((outLevel > 0) && (obj->mtype == OPEN))
        printf(" EGADS Info: Shell is Open (EG_makeTopology)!\n");
    if (mtype == OPEN)
      if ((outLevel > 0) && (obj->mtype == CLOSED))
        printf(" EGADS Info: Shell is Closed (EG_makeTopology)!\n");
    if (mtype == DEGENERATE)
      if (obj->mtype == OPEN) {
        printf(" EGADS Info: Shell is Open (EG_makeTopology)!\n");
      } else {
        printf(" EGADS Info: Shell is Closed (EG_makeTopology)!\n");
      }

  } else if (oclass == BODY) {
  
    if (children == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Body with NULL Children (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (nChildren <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Body with %d Children (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    if ((mtype < WIREBODY) || (mtype > SOLIDBODY)) {
      if (outLevel > 0)
        printf(" EGADS Error: Body with mtype = %d (EG_makeTopology)!\n",
               mtype);
      return EGADS_RANGERR;
    }
    int                    cclass = SHELL;
    if (mtype == FACEBODY) cclass = FACE;
    if (mtype == WIREBODY) cclass = LOOP;
    if ((mtype != SOLIDBODY) && (nChildren != 1)) {
      if (outLevel > 0)
        printf(" EGADS Error: non SolidBody w/ %d children (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    for (i = 0; i < nChildren; i++) {
      if (children[i] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Body with child[%d] NULL (EG_makeTopology)!\n", i);
        return EGADS_NULLOBJ;
      }
      if (children[i]->oclass != cclass) {
        if (outLevel > 0)
          printf(" EGADS Error: Body with invalid Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Body with NULL Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NODATA;
      }
      if ((children[i]->mtype != CLOSED) && (mtype == SOLIDBODY)) {
        if (outLevel > 0)
          printf(" EGADS Error: Solid w/ nonClosed Shell[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_RANGERR;
      }
    }
    TopoDS_Shape shape;
    if (mtype == WIREBODY) {
      egadsLoop *ploop = (egadsLoop *) children[0]->blind;
      shape = ploop->loop;
    } else if (mtype == FACEBODY) {
      egadsFace *pface = (egadsFace *) children[0]->blind;
      shape = pface->face;
    } else if (mtype == SHEETBODY) {
      egadsShell *pshell = (egadsShell *) children[0]->blind;
      shape = pshell->shell;
    } else {
      BRep_Builder builder3D;
      TopoDS_Solid solid;
      builder3D.MakeSolid(solid);
      for (i = 0; i < nChildren; i++) {
        egadsShell *pshell = (egadsShell *) children[i]->blind;
        builder3D.Add(solid, pshell->shell);
      }
      try {
        BRepLib::OrientClosedSolid(solid);
      }
      catch (Standard_Failure)
      {
        printf(" EGADS Warning: Cannot Orient Solid (EG_makeTopology)!\n");
        Handle_Standard_Failure e = Standard_Failure::Caught();
        printf("                %s\n", e->GetMessageString());
        return EGADS_TOPOERR;
      }
      catch (...) {
        printf(" EGADS Warning: Cannot Orient Solid (EG_makeTopology)!\n");
        return EGADS_TOPOERR;
      }
      BRepCheck_Analyzer sCheck(solid);
      if (!sCheck.IsValid()) {
        if (outLevel > 0)
          printf(" EGADS Warning: Solid is invalid (EG_makeTopology)!\n");
        solid.Nullify();
        return EGADS_CONSTERR;
      }
      shape = solid;
    }
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Body object (EG_makeTopology)!\n");
      return stat;
    }
    obj->oclass        = oclass;
    obj->mtype         = mtype;
    egadsBody *pbody   = new egadsBody;
    pbody->nodes.objs  = NULL;
    pbody->edges.objs  = NULL;
    pbody->loops.objs  = NULL;
    pbody->faces.objs  = NULL;
    pbody->shells.objs = NULL;
    pbody->senses      = NULL;
    pbody->shape       = shape;
    obj->blind         = pbody;
    stat = EG_traverseBody(context, 0, obj, obj, pbody);
    if (stat != EGADS_SUCCESS) {
      delete pbody;
      return stat;
    }
    for (i = 0; i < nChildren; i++) 
      EG_attriBodyDup(children[i], obj);
    EG_referenceObject(obj, context);
 
  } else {
  
    if (children == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Model with NULL Children (EG_makeTopology)!\n");
      return EGADS_NULLOBJ;
    }
    if (nChildren <= 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Model with %d Bodies (EG_makeTopology)!\n",
               nChildren);
      return EGADS_RANGERR;
    }
    for (i = 0; i < nChildren; i++) {
      if (children[i] == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Model with Body[%d] NULL (EG_makeTopology)!\n", i);
        return EGADS_NULLOBJ;
      }
      if (children[i]->oclass != BODY) {
        if (outLevel > 0)
          printf(" EGADS Error: Model with nonBody Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NOTTOPO;
      }
      if (children[i]->topObj != context) {
        if (outLevel > 0)
          printf(" EGADS Error: Model with body[%d] reference (EG_makeTopology)!\n",
                 i);
        return EGADS_REFERCE;
      }
      if (children[i]->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Model with NULL Body Child[%d] (EG_makeTopology)!\n",
                 i);
        return EGADS_NODATA;
      }
    }
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Model object (EG_makeTopology)!\n");
      return stat;
    }
    egadsModel *pmodel = new egadsModel;
    pmodel->bodies     = new egObject*[nChildren];
    pmodel->nbody      = nChildren;
    obj->oclass = MODEL;
    obj->blind  = pmodel;
    TopoDS_Compound compound;
    BRep_Builder    builder3D;
    builder3D.MakeCompound(compound);
    for (i = 0; i < nChildren; i++) {
      pmodel->bodies[i] = children[i];
      egadsBody *pbody  = (egadsBody *) children[i]->blind;
      builder3D.Add(compound, pbody->shape);
      EG_referenceObject(children[i], obj);
      EG_removeCntxtRef(children[i]);
    }
    pmodel->shape = compound;
    EG_referenceObject(obj, context);

  }
  
  *topo = obj;
  return EGADS_SUCCESS;
}


int
EG_getArea(egObject *object, /*@null@*/ const double *limits, 
           double *area)
{
  double      sense = 1.0;
  TopoDS_Face Face;

  *area = 0.0;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((object->oclass != SURFACE) && (object->oclass != LOOP) &&
      (object->oclass != FACE))     return EGADS_GEOMERR;
  if (object->blind == NULL)        return EGADS_NODATA;
  int outLevel = EG_outLevel(object);

  if (object->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) object->blind;
    Face = pface->face;

  } else if (object->oclass == SURFACE) {
  
    if (limits == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Surface with NULL Limits (EG_getArea)!\n");
      return EGADS_NODATA;      
    }
    egadsSurface *psurf = (egadsSurface *) object->blind;
#if CASVER >= 652
    Standard_Real tol = BRepLib::Precision();
    BRepLib_MakeFace MFace(psurf->handle, limits[0], limits[1],
                                          limits[2], limits[3], tol);
#else
    BRepLib_MakeFace MFace(psurf->handle, limits[0], limits[1],
                                          limits[2], limits[3]);
#endif
    Face = MFace.Face();
    
  } else {
  
    egadsLoop *ploop  = (egadsLoop *) object->blind;
    if (ploop->surface == NULL) {

      // try to fit a plane
      Standard_Real tol = Precision::Confusion();
      Handle(Geom_Surface) hSurface = NULL;
      int nTrys = 4;              // # of tol attempts
      for (int itry = 0; itry < nTrys; itry++) {    
        BRepBuilderAPI_FindPlane FPlane(ploop->loop, tol);
        if (FPlane.Found()) {
          hSurface = FPlane.Plane();
          break;
        }
        tol *= 10.0;
        if (outLevel > 1)
          printf(" EGADS Info: Adjusting Prec for makeFace - itry = %d  prec = %le\n", 
                 itry, tol);
      }
      if (hSurface == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Cannot make Planar Surface (EG_getArea)!\n");
        return EGADS_GEOMERR;
      }
      BRepLib_MakeFace MFace(hSurface, ploop->loop);
      Face = MFace.Face();
      
      // did making the Face flip the Loop?
      TopExp_Explorer ExpW;
      for (ExpW.Init(Face, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
        TopoDS_Shape shapw = ExpW.Current();
        TopoDS_Wire  wire  = TopoDS::Wire(shapw);
        if (!wire.IsSame(ploop->loop)) continue;
        if (!wire.IsEqual(ploop->loop)) sense = -1.0;
      }

    } else {

      // make a standard Face
      egObject *geom = ploop->surface;
      if (geom->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Loop had NULL Ref Surface (EG_getArea)!\n");
        return EGADS_NOTGEOM;   
      }
      egadsSurface *psurf = (egadsSurface *) geom->blind;
      BRepBuilderAPI_MakeFace MFace;
      Standard_Real old = BRepBuilderAPI::Precision();
      Standard_Real prc = old;
      int nTrys = 5;              // # of tol attempts before setting SameParam = F
      for (int itry = 0; itry < nTrys; itry++) {
        if (itry != 0) Face.Nullify();
        if (itry == nTrys-1) {    // last attempt -- set tol back
          prc = old;
          BRepBuilderAPI::Precision(old);
        }
#if CASVER >= 652
        MFace.Init(psurf->handle, Standard_False, prc);
#else
        MFace.Init(psurf->handle, Standard_False);
#endif
        MFace.Add(ploop->loop);
        if (MFace.Error()) {
          if (outLevel > 0)
            printf(" EGADS Error: Problem with Loop (EG_getArea)!\n");
          return EGADS_NODATA;
        }
        if (MFace.IsDone()) {
          Face = MFace.Face();
          EG_makePCurves(Face, ploop->surface, object, prc, nTrys-itry-1);
          BRepLib::SameParameter(Face);
          BRepCheck_Analyzer oCheck(Face);
          if (oCheck.IsValid()) break;
        }
        prc *= 10.0;
        BRepBuilderAPI::Precision(prc);
        if (outLevel > 1)
          printf(" EGADS Info: Adjusting Precision for Face - itry = %d  prec = %lf\n", 
                 itry, prc);
      }
      BRepBuilderAPI::Precision(old);
      if (!MFace.IsDone()) {
        if (outLevel > 0)
          printf(" EGADS Error: Problem making the Face (EG_getArea)!\n");
        return EGADS_NODATA;
      }

    }

  }
  
  BRepGProp    BProps;
  GProp_GProps SProps;
  BProps.SurfaceProperties(Face, SProps);
  *area = sense*SProps.Mass();

  return EGADS_SUCCESS;
}


int
EG_makeFace(egObject *object, int mtype, 
            /*@null@*/ const double *limits, egObject **face)
{
  int      stat, outLevel, nl = 1;
  egObject *obj, *context, *loop = NULL, *geom = NULL;
  
  *face = NULL;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((object->oclass != SURFACE) && (object->oclass != LOOP))
                                    return EGADS_GEOMERR;
  if (object->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(object);
  context  = EG_context(object);
  if ((mtype != SFORWARD) && (mtype != SREVERSE)) {
    if (outLevel > 0)
      printf(" EGADS Error: Mtype = %d (EG_makeFace)!\n", mtype);
    return EGADS_TOPOERR;
  }
  if (object->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) object->blind;
    if (ploop->surface != NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Loop had Ref Surface (EG_makeFace)!\n");
      return EGADS_NOTGEOM;     
    }
  } else {
    if (limits == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Surface with NULL Limits (EG_makeFace)!\n");
      return EGADS_NODATA;      
    }
  }

  TopoDS_Face Face;
  if (object->oclass == SURFACE) {
  
    egadsSurface *psurf = (egadsSurface *) object->blind;
    Handle(Geom_Surface) hSurf = psurf->handle;
#if CASVER >= 652
    Standard_Real tol = BRepLib::Precision();
    BRepLib_MakeFace MFace(hSurf, limits[0], limits[1],
                                  limits[2], limits[3], tol);
#else
    BRepLib_MakeFace MFace(hSurf, limits[0], limits[1],
                                  limits[2], limits[3]);
#endif
    Face = MFace.Face();
    if (mtype == SREVERSE) {
      Face.Orientation(TopAbs_REVERSED);
    } else {
      Face.Orientation(TopAbs_FORWARD);
    }
    BRepLib::SameParameter(Face);
    BRepCheck_Analyzer fCheck(Face);
    if (!fCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Info: Face may be invalid (EG_makeFace)!\n");
      return EGADS_CONSTERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Face object (EG_makeFace)!\n");
      return stat;
    }
    obj->oclass = FACE;
    geom        = object;
    TopExp_Explorer ExpW;
    for (ExpW.Init(Face, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
      TopoDS_Shape shapw = ExpW.Current();
      TopoDS_Wire  Wire  = TopoDS::Wire(shapw);
      stat = EG_makeObject(context, &loop);
      if (stat != EGADS_SUCCESS) {
        if (outLevel > 0)
          printf(" EGADS Error: Cannot make Surface object (EG_makeFace)!\n");
        obj->oclass = NIL;
        EG_deleteObject(obj);
        return stat;
      }
      loop->oclass      = LOOP;
      egadsLoop *ploop  = new egadsLoop;
      loop->blind       = ploop;
      ploop->loop       = Wire;
      ploop->nedges     = 0;
      ploop->edges      = NULL;
      ploop->senses     = NULL;
      ploop->topFlg     = 0;
      ploop->surface    = NULL;
      if (object->mtype != PLANE) {
        ploop->surface = geom;
        EG_referenceObject(geom, loop);
      }
      EG_fillTopoObjs(loop, obj);
      EG_fillPCurves(Face, geom, loop, obj);
      break;
    }
    
  } else {
  
    egadsLoop *ploop  = (egadsLoop *) object->blind;
    Standard_Real tol = Precision::Confusion();
    Handle(Geom_Surface) hSurface = NULL;
    int nTrys = 4;              // # of tol attempts
    for (int itry = 0; itry < nTrys; itry++) {    
      BRepBuilderAPI_FindPlane FPlane(ploop->loop, tol);
      if (FPlane.Found()) {
        hSurface = FPlane.Plane();
        break;
      }
      tol *= 10.0;
      if (outLevel > 1)
        printf(" EGADS Info: Adjusting Prec for makeFace - itry = %d  prec = %le\n", 
               itry, tol);
    }
    if (hSurface == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Planar Surface (EG_makeFace)!\n");
      return EGADS_GEOMERR;
    }

    signal(SIGSEGV, segfault_handler);
    switch (stat = setjmp(jmpenv)) {
    case 0:
      {
        BRepLib_MakeFace MFace(hSurface, ploop->loop);
        Face = MFace.Face();
      }
      break;
    default:
      printf(" EGADS Fatal Error: OCC SegFault %d (EG_makeFace)!\n", stat);
      signal(SIGSEGV, SIG_DFL);
      return EGADS_OCSEGFLT;
    }
    signal(SIGSEGV, SIG_DFL);
  
    if (mtype == SREVERSE) {
      Face.Orientation(TopAbs_REVERSED);
    } else {
      Face.Orientation(TopAbs_FORWARD);
    }
    BRepLib::SameParameter(Face);
    BRepCheck_Analyzer fCheck(Face);
    if (!fCheck.IsValid()) {
      if (outLevel > 0)
        printf(" EGADS Info: Face may be invalid (EG_makeFace)!\n");
      return EGADS_CONSTERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Face object (EG_makeFace)!\n");
      return stat;
    }
    obj->oclass = FACE;
  
    loop = object;
    stat = EG_makeObject(context, &geom);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot make Surface object (EG_makeFace)!\n");
      obj->oclass = NIL;
      EG_deleteObject(obj);
      return stat;
    }
    geom->topObj = obj;
    EG_completeSurf(geom, hSurface);

  }
  
  egadsFace *pface = new egadsFace;
  egObject **loopo = new egObject*[nl];
  int *senses      = new int[nl];
  loopo[0]         = loop;
  senses[0]        = 1;
  pface->face      = Face;
  pface->surface   = geom;
  pface->nloops    = nl;
  pface->loops     = loopo;
  pface->senses    = senses;
  pface->topFlg    = 0;
  obj->blind       = pface;
  obj->mtype       = mtype;

  EG_referenceObject(geom, obj);
  EG_referenceObject(loop, obj);
  EG_referenceObject(obj,  context);

  *face = obj;
  return EGADS_SUCCESS;
}


int
EG_getBodyTopos(const egObject *body, /*@null@*/ egObject *src,
                int oclass, int *ntopo, egObject ***topos)
{
  int      outLevel, i, n, index;
  egadsMap *map;
  egObject **objs;

  *ntopo = 0;
  *topos = NULL;
  if (body == NULL)               return EGADS_NULLOBJ;
  if (body->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (body->oclass != BODY)       return EGADS_NOTBODY;
  if (body->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(body);
  
  if ((oclass < NODE) || (oclass > SHELL)) {
    if (outLevel > 0)
      printf(" EGADS Error: oclass = %d (EG_getBodyTopos)!\n", 
             oclass);
    return EGADS_NOTTOPO;
  }

  egadsBody *pbody = (egadsBody *) body->blind;
  if (oclass == NODE) {
    map = &pbody->nodes;
  } else if (oclass == EDGE) {
    map = &pbody->edges;
  } else if (oclass == LOOP) {
    map = &pbody->loops;
  } else if (oclass == FACE) {
    map = &pbody->faces;
  } else {
    map = &pbody->shells;
  }
  
  if (src == NULL) {

    n = map->map.Extent();
    if (n == 0) return EGADS_SUCCESS;
    objs = (egObject **) EG_alloc(n*sizeof(egObject *));
    if (objs == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Malloc oclass = %d, n = %d (EG_getBodyTopos)!\n", 
               oclass, n);
      return EGADS_MALLOC;
    }
    for (i = 0; i < n; i++) objs[i] = map->objs[i];

  } else {

    if (src->magicnumber != MAGIC) {
      if (outLevel > 0)
        printf(" EGADS Error: src not an EGO (EG_getBodyTopos)!\n");
      return EGADS_NOTOBJ;
    }
    if ((src->oclass < NODE) || (src->oclass > SHELL)) {
      if (outLevel > 0)
        printf(" EGADS Error: src not a Topo (EG_getBodyTopos)!\n");
      return EGADS_NOTTOPO;
    }
    if (src->oclass == oclass) {
      if (outLevel > 0)
        printf(" EGADS Error: src Topo is oclass (EG_getBodyTopos)!\n");
      return EGADS_TOPOERR;
    }
    if (EG_context(body) != EG_context(src)) {
      if (outLevel > 0)
        printf(" EGADS Error: Context mismatch (EG_getBodyTopos)!\n");
      return EGADS_MIXCNTX;
    }
    if (src->blind == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: NULL src pointer (EG_getBodyTopos)!\n");
      return EGADS_NODATA;
    }

    TopoDS_Shape     shape;
    TopAbs_ShapeEnum senum;
    if (src->oclass == NODE) {
      egadsNode *pnode = (egadsNode *) src->blind;
      shape = pnode->node;
      senum = TopAbs_VERTEX;
    } else if (src->oclass == EDGE) {
      egadsEdge *pedge = (egadsEdge *) src->blind;
      shape = pedge->edge;
      senum = TopAbs_EDGE;
    } else if (src->oclass == LOOP) {
      egadsLoop *ploop = (egadsLoop *) src->blind;
      shape = ploop->loop;
      senum = TopAbs_WIRE;
    } else if (src->oclass == FACE) {
      egadsFace *pface = (egadsFace *) src->blind;
      shape = pface->face;
      senum = TopAbs_FACE;
    } else {
      egadsShell *pshell = (egadsShell *) src->blind;
      shape = pshell->shell;
      senum = TopAbs_SHELL;
    }

    if (src->oclass > oclass) {
    
      // look down the tree (get sub-shapes)
      if (oclass == NODE) {
        senum = TopAbs_VERTEX;
      } else if (oclass == EDGE) {
        senum = TopAbs_EDGE;
      } else if (oclass == LOOP) {
        senum = TopAbs_WIRE;
      } else if (oclass == FACE) {
        senum = TopAbs_FACE;
      } else {
        senum = TopAbs_SHELL;
      }
      TopTools_IndexedMapOfShape smap;
      TopExp::MapShapes(shape, senum, smap);
      n = smap.Extent();
      if (n == 0) return EGADS_SUCCESS;
      objs = (egObject **) EG_alloc(n*sizeof(egObject *));
      if (objs == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Malloc oclass = %d, n = %d (EG_getBodyTopos)!\n", 
                 oclass, n);
        return EGADS_MALLOC;
      }
      for (i = 0; i < n; i++) {
        objs[i] = NULL;
        TopoDS_Shape shapo = smap(i+1);
        index = map->map.FindIndex(shapo);
        if (index == 0) {
          if (outLevel > 0)
            printf(" EGADS Warning: %d/%d NotFound oclass = %d (EG_getBodyTopos)!\n", 
                   i+1, n, oclass);
        } else {
          objs[i] = map->objs[index-1];
        }
      }
      
    } else {
  
      // look up (get super-shapes)
      for (n = i = 0; i < map->map.Extent(); i++) {
        TopoDS_Shape shapo = map->map(i+1);
        TopTools_IndexedMapOfShape smap;
        TopExp::MapShapes(shapo, senum, smap);
        if (smap.FindIndex(shape) != 0) n++;
      }
      if (n == 0) return EGADS_SUCCESS;
      objs = (egObject **) EG_alloc(n*sizeof(egObject *));
      if (objs == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Malloc oclass = %d, N = %d (EG_getBodyTopos)!\n", 
                 oclass, n);
        return EGADS_MALLOC;
      }
      for (n = i = 0; i < map->map.Extent(); i++) {
        TopoDS_Shape shapo = map->map(i+1);
        TopTools_IndexedMapOfShape smap;
        TopExp::MapShapes(shapo, senum, smap);
        if (smap.FindIndex(shape) != 0) {
          objs[n] = map->objs[i];
          n++;
        }
      }
    }    
  }
  
  *ntopo = n;
  *topos = objs;
    
  return EGADS_SUCCESS;
}


int
EG_indexBodyTopo(const egObject *body, const egObject *src)
{
  int outLevel, index;

  if (body == NULL)               return EGADS_NULLOBJ;
  if (body->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (body->oclass != BODY)       return EGADS_NOTBODY;
  if (body->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(body);
  
  if (src->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: src not an EGO (EG_indexBodyTopo)!\n");
    return EGADS_NOTOBJ;
  }
  if ((src->oclass < NODE) || (src->oclass > SHELL)) {
    if (outLevel > 0)
      printf(" EGADS Error: src not a Topo (EG_indexBodyTopo)!\n");
    return EGADS_NOTTOPO;
  }
  if (EG_context(body) != EG_context(src)) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_indexBodyTopo)!\n");
    return EGADS_MIXCNTX;
  }
  if (src->blind == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL src pointer (EG_indexBodyTopo)!\n");
    return EGADS_NODATA;
  }

  egadsBody *pbody = (egadsBody *) body->blind;
  if (src->oclass == NODE) {
    egadsNode *pnode = (egadsNode *) src->blind;
    index = pbody->nodes.map.FindIndex(pnode->node);
  } else if (src->oclass == EDGE) {
    egadsEdge *pedge = (egadsEdge *) src->blind;
    index = pbody->edges.map.FindIndex(pedge->edge);
  } else if (src->oclass == LOOP) {
    egadsLoop *ploop = (egadsLoop *) src->blind;
    index = pbody->loops.map.FindIndex(ploop->loop);
  } else if (src->oclass == FACE) {
    egadsFace *pface = (egadsFace *) src->blind;
    index = pbody->faces.map.FindIndex(pface->face);
  } else {
    egadsShell *pshell = (egadsShell *) src->blind;
    index = pbody->shells.map.FindIndex(pshell->shell);
  }
 
  if (index == 0) index = EGADS_NOTFOUND;
  return index; 
}


int
EG_makeSolidBody(egObject *context, int stypx, const double *data,
                 egObject **body)
{
  int           outLevel, stype, stat;
  egObject      *obj;
  TopoDS_Shape  solid;
  Standard_Real height;

  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  outLevel = EG_outLevel(context);
  stype    = abs(stypx);

  if ((stype < BOX) || (stype > TORUS)) {
    if (outLevel > 0)
      printf(" EGADS Error: stype = %d (EG_makeSolidBody)!\n", stype);
    return EGADS_RANGERR;
  }
  
  switch (stype) {

  /* box=1 */
  case BOX:
    solid = (TopoDS_Solid) 
            BRepPrimAPI_MakeBox(gp_Pnt(data[0], data[1], data[2]),
                                data[3], data[4], data[5]);
    break;

  /* sphere=2 */
  case SPHERE:
    solid = (TopoDS_Solid)
            BRepPrimAPI_MakeSphere(gp_Pnt(data[0], data[1], data[2]), 
                                   data[3]);
    break;

  /* cone=3 */
  case CONE:
    height = sqrt( (data[3]-data[0])*(data[3]-data[0]) +
                   (data[4]-data[1])*(data[4]-data[1]) +
                   (data[5]-data[2])*(data[5]-data[2]) );
    solid = (TopoDS_Solid)
            BRepPrimAPI_MakeCone(gp_Ax2(gp_Pnt(data[0], data[1], data[2]),
                                 gp_Dir(data[3]-data[0], data[4]-data[1],
                                        data[5]-data[2])),
                                 0.0, data[6], height);
    break;
    
  /* cylinder=4 */
  case CYLINDER:
    height = sqrt( (data[3]-data[0])*(data[3]-data[0]) +
                   (data[4]-data[1])*(data[4]-data[1]) +
                   (data[5]-data[2])*(data[5]-data[2]) );
    solid = (TopoDS_Solid)
            BRepPrimAPI_MakeCylinder(gp_Ax2(gp_Pnt(data[0], data[1], data[2]),
                                     gp_Dir(data[3]-data[0], data[4]-data[1],
                                            data[5]-data[2])),
                                     data[6], height);
    break;

  /* torus=5 */
  case TORUS:
    solid = (TopoDS_Solid)
            BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(data[0], data[1], data[2]),
                                  gp_Dir(data[3], data[4], data[5])),
                                  data[6], data[7]);
    break;
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: Cannot make Body object (EG_makeSolidBody)!\n");
    return stat;
  }
  egadsBody *pbody   = new egadsBody;
  obj->oclass        = BODY;
  obj->mtype         = SOLIDBODY;
  pbody->nodes.objs  = NULL;
  pbody->edges.objs  = NULL;
  pbody->loops.objs  = NULL;
  pbody->faces.objs  = NULL;
  pbody->shells.objs = NULL;
  pbody->senses      = NULL;
  pbody->shape       = solid;
  obj->blind         = pbody;
  if (stypx > 0) EG_splitPeriodics(pbody);
  stat = EG_traverseBody(context, 0, obj, obj, pbody);
  if (stat != EGADS_SUCCESS) {
    delete pbody;
    return stat;
  }

  EG_referenceObject(obj, context);
  *body = obj;
  return EGADS_SUCCESS;
}


int
EG_getBoundingBox(const egObject *topo, double *bbox)
{
  int      i, n;
  egObject *obj;
  Bnd_Box  Box;

  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (topo->oclass < NODE)        return EGADS_NOTTOPO;
  if (topo->blind == NULL)        return EGADS_NODATA;

  if (topo->oclass == NODE) {
  
    egadsNode *pnode = (egadsNode *) topo->blind;
    BRepBndLib::Add(pnode->node, Box);

  } else if (topo->oclass == EDGE) {
  
    egadsEdge *pedge = (egadsEdge *) topo->blind;
    BRepBndLib::Add(pedge->edge, Box);

  } else if (topo->oclass == LOOP) {
  
    egadsLoop *ploop = (egadsLoop *) topo->blind;
    BRepBndLib::Add(ploop->loop, Box);
  
  } else if (topo->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) topo->blind;
    BRepBndLib::Add(pface->face, Box);
    
  } else if (topo->oclass == SHELL) {
  
    egadsShell *pshell = (egadsShell *) topo->blind;
    BRepBndLib::Add(pshell->shell, Box);
    
  } else if (topo->oclass == BODY) {
  
    egadsBody *pbody = (egadsBody *) topo->blind;
    BRepBndLib::Add(pbody->shape, Box);
    
  } else {
  
    egadsModel *pmodel = (egadsModel *) topo->blind;
    if (pmodel != NULL) {
      n = pmodel->nbody;
      for (i = 0; i < n; i++) {
        obj = pmodel->bodies[i];
        if (obj == NULL) continue;
        egadsBody *pbody = (egadsBody *) obj->blind;
        if (pbody == NULL) continue;
        BRepBndLib::Add(pbody->shape, Box);
      }
    }

  }

  Box.Get(bbox[0],bbox[1],bbox[2],bbox[3],bbox[4],bbox[5]);

  return EGADS_SUCCESS;
}


int
EG_getMassProperties(const egObject *topo, double *data)
{
  BRepGProp    BProps;
  GProp_GProps SProps, VProps;

  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (topo->oclass < EDGE)        return EGADS_NOTTOPO;
  if (topo->blind == NULL)        return EGADS_NODATA;

  if (topo->oclass == EDGE) {
  
    egadsEdge *pedge = (egadsEdge *) topo->blind;
    BProps.LinearProperties(pedge->edge, SProps);
    BProps.VolumeProperties(pedge->edge, VProps);

  } else if (topo->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) topo->blind;
    BProps.SurfaceProperties(pface->face, SProps);
    BProps.VolumeProperties( pface->face, VProps);
    
  } else if (topo->oclass == SHELL) {
  
    egadsShell *pshell = (egadsShell *) topo->blind;
    BProps.SurfaceProperties(pshell->shell, SProps);
    BProps.VolumeProperties( pshell->shell, VProps);
    
  } else if (topo->oclass == BODY) {
  
    egadsBody *pbody = (egadsBody *) topo->blind;
    BProps.SurfaceProperties(pbody->shape, SProps);
    BProps.VolumeProperties( pbody->shape, VProps);
    
  } else {
  
    egadsModel *pmodel = (egadsModel *) topo->blind;
    BProps.SurfaceProperties(pmodel->shape, SProps);
    BProps.VolumeProperties( pmodel->shape, VProps);
  }
  
  gp_Pnt CofG  = VProps.CentreOfMass();
  gp_Mat Inert = VProps.MatrixOfInertia();
  data[ 0] = VProps.Mass();
  data[ 1] = SProps.Mass();
  data[ 2] = CofG.X();
  data[ 3] = CofG.Y();
  data[ 4] = CofG.Z();
  data[ 5] = Inert.Value(1,1);
  data[ 6] = Inert.Value(1,2);
  data[ 7] = Inert.Value(1,3);
  data[ 8] = Inert.Value(2,1);
  data[ 9] = Inert.Value(2,2);
  data[10] = Inert.Value(2,3);
  data[11] = Inert.Value(3,1);
  data[12] = Inert.Value(3,2);
  data[13] = Inert.Value(3,3);

  return EGADS_SUCCESS;
}


int
EG_isEquivalent(const egObject *topo1, const egObject *topo2)
{
  TopoDS_Shape shape1, shape2;

  if (topo1 == topo2)                 return EGADS_SUCCESS;
  if (topo1 == NULL)                  return EGADS_NULLOBJ;
  if (topo1->magicnumber != MAGIC)    return EGADS_NOTOBJ;
  if (topo1->oclass < NODE)           return EGADS_NOTTOPO;
  if (topo1->blind == NULL)           return EGADS_NODATA;
  if (topo2 == NULL)                  return EGADS_NULLOBJ;
  if (topo2->magicnumber != MAGIC)    return EGADS_NOTOBJ;
  if (topo2->oclass != topo1->oclass) return EGADS_NOTTOPO;
  if (topo2->blind == NULL)           return EGADS_NODATA;

  if (topo1->oclass == NODE) {
  
    egadsNode *pnode1 = (egadsNode *) topo1->blind;
    egadsNode *pnode2 = (egadsNode *) topo2->blind;
    shape1            = pnode1->node;
    shape2            = pnode2->node;

  } else if (topo1->oclass == EDGE) {
  
    egadsEdge *pedge1 = (egadsEdge *) topo1->blind;
    egadsEdge *pedge2 = (egadsEdge *) topo2->blind;
    shape1            = pedge1->edge;
    shape2            = pedge2->edge;

  } else if (topo1->oclass == LOOP) {
  
    egadsLoop *ploop1 = (egadsLoop *) topo1->blind;
    egadsLoop *ploop2 = (egadsLoop *) topo2->blind;
    shape1            = ploop1->loop;
    shape2            = ploop2->loop;
  
  } else if (topo1->oclass == FACE) {
  
    egadsFace *pface1 = (egadsFace *) topo1->blind;
    egadsFace *pface2 = (egadsFace *) topo2->blind;
    shape1            = pface1->face;
    shape2            = pface2->face;
    
  } else if (topo1->oclass == SHELL) {
  
    egadsShell *pshell1 = (egadsShell *) topo1->blind;
    egadsShell *pshell2 = (egadsShell *) topo2->blind;
    shape1              = pshell1->shell;
    shape2              = pshell2->shell;
    
  } else if (topo1->oclass == BODY) {
  
    egadsBody *pbody1 = (egadsBody *) topo1->blind;
    egadsBody *pbody2 = (egadsBody *) topo2->blind;
    shape1            = pbody1->shape;
    shape2            = pbody2->shape;
    
  } else {
  
    egadsModel *pmodel1 = (egadsModel *) topo1->blind;
    egadsModel *pmodel2 = (egadsModel *) topo2->blind;
    shape1              = pmodel1->shape;
    shape2              = pmodel2->shape;
  }
  
  if (shape1.IsSame(shape2)) return EGADS_SUCCESS;
  return EGADS_OUTSIDE;
}


int 
EG_getEdgeUV(const egObject *face, const egObject *topo, int sense, double t,
             double *uv)
{
  int      outLevel;
  gp_Pnt2d P2d;
  
  if (face == NULL)               return EGADS_NULLOBJ;
  if (face->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (face->oclass != FACE)       return EGADS_NOTTOPO;
  if (face->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(face);
  
  if (topo == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Reference (EG_getEdgeUV)!\n");
    return EGADS_NULLOBJ;
  }
  if (topo->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: topo not an EGO (EG_getEdgeUV)!\n");
    return EGADS_NOTOBJ;
  }
  if (topo->oclass != EDGE) {
    if (outLevel > 0)
      printf(" EGADS Error: Not an Edge (EG_getEdgeUV)!\n");
    return EGADS_NOTTOPO;
  }
  if ((sense < -1) || (sense > 1)) {
    if (outLevel > 0)
      printf(" EGADS Error: Sense = %d (EG_getEdgeUV)!\n", sense);
    return EGADS_RANGERR;
  }
  if (EG_context(face) != EG_context(topo)) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_getEdgeUV)!\n");
    return EGADS_MIXCNTX;
  }
  egadsFace *pface = (egadsFace *) face->blind;
  egadsEdge *pedge = (egadsEdge *) topo->blind;
  if ((pface == NULL) || (pedge == NULL)) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL pointer(s) (EG_getEdgeUV)!\n");
    return EGADS_NODATA;
  }
  TopoDS_Edge edge = pedge->edge;

  // is edge in the face?
  
  int found = 0;
  TopExp_Explorer ExpW;
  for (ExpW.Init(pface->face, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
    TopoDS_Shape shapw = ExpW.Current();
    TopoDS_Wire  wire  = TopoDS::Wire(shapw);
    BRepTools_WireExplorer ExpWE;
    for (ExpWE.Init(wire); ExpWE.More(); ExpWE.Next()) {
      TopoDS_Shape shape = ExpWE.Current();
      TopoDS_Edge  wedge = TopoDS::Edge(shape);
      if (!wedge.IsSame(edge)) continue;
      if (sense == 0) {
        found++;
      } else if (sense == -1) {
        if (shape.Orientation() == TopAbs_REVERSED) found++;
      } else {
        if (shape.Orientation() != TopAbs_REVERSED) found++;
      }
      if (found == 0) continue;
      edge = wedge;
      break;
    }
    if (found != 0) break;
  }
  if (found == 0) {
    if (outLevel > 0)
      printf(" EGADS Error: Edge/Sense not in Face (EG_getEdgeUV)!\n");
    return EGADS_NOTFOUND;
  }
  if (!BRep_Tool::SameRange(edge)) {
    if (outLevel > 0)
      printf(" EGADS Error: Edge & PCurve not SameRange (EG_getEdgeUV)!\n");
    return EGADS_GEOMERR;
  }
/*  
  if (!BRep_Tool::SameParameter(edge)) {
    printf(" EGADS Info: Edge has SameParameter NOT set!\n");
    printf(" Edge tol = %le\n", BRep_Tool::Tolerance(edge));
  }
 */

  // get and evaluate the pcurve

  BRepAdaptor_Curve2d Curve2d(edge, pface->face);
  Curve2d.D0(t, P2d);
  uv[0] = P2d.X();
  uv[1] = P2d.Y();
  
  return EGADS_SUCCESS;
}


int
EG_inTopology(const egObject *topo, const double *xyz)
{
  int           outLevel;
  Standard_Real tol, t, u, v, range[2];

  if (topo == NULL)               return EGADS_NULLOBJ;
  if (topo->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (topo->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(topo);

  gp_Pnt pnt(xyz[0], xyz[1], xyz[2]);
  
  if (topo->oclass == EDGE) {
    
    egadsEdge *pedge = (egadsEdge *) topo->blind;
    egObject  *curv  = pedge->curve;
    if (curv == NULL) {
      if (outLevel > 0)
        printf(" EGADS Warning: No curve Object for Edge (EG_inTopology)!\n");
      return EGADS_NULLOBJ;
    }
    egadsCurve *pcurve = (egadsCurve *) curv->blind;
    if (pcurve == NULL) {
      if (outLevel > 0)
        printf(" EGADS Warning: No curve Data for Edge (EG_inTopology)!\n");
      return EGADS_NODATA;
    }
    Handle(Geom_Curve) hCurve = pcurve->handle;
    GeomAPI_ProjectPointOnCurve projPnt(pnt, hCurve);
    if (projPnt.NbPoints() == 0) {
      if (outLevel > 0)
        printf(" EGADS Warning: No projection on Curve (EG_inTopology)!\n");
      return EGADS_NOTFOUND;
    }
    tol = BRep_Tool::Tolerance(pedge->edge);
    if (projPnt.LowerDistance() > tol) return EGADS_OUTSIDE;
    t = projPnt.LowerDistanceParameter();
    BRep_Tool::Range(pedge->edge, range[0],range[1]);
    if ((t < range[0]) || (t > range[1])) return EGADS_OUTSIDE;
    return EGADS_SUCCESS;

  } else if (topo->oclass == FACE) {
  
    egadsFace *pface = (egadsFace *) topo->blind;
    egObject  *surf  = pface->surface;
    if (surf == NULL) {
      if (outLevel > 0)
        printf(" EGADS Warning: No Surf Object for Face (EG_inTopology)!\n");
      return EGADS_NULLOBJ;
    }
    egadsSurface *psurf = (egadsSurface *) surf->blind;
    if (psurf == NULL) {
      if (outLevel > 0)
        printf(" EGADS Warning: No Surf Data for Face (EG_inTopology)!\n");
      return EGADS_NODATA;
    }
    GeomAPI_ProjectPointOnSurf projPnt(pnt, psurf->handle);
    if (!projPnt.IsDone()) {
      printf(" EGADS Warning: GeomAPI_ProjectPointOnSurf (EG_inTopology)!\n");
      return EGADS_GEOMERR;
    }
    tol = BRep_Tool::Tolerance(pface->face);
    if (projPnt.LowerDistance() > tol) return EGADS_OUTSIDE;
    projPnt.LowerDistanceParameters(u, v);
    gp_Pnt2d pnt2d(u, v);
    TopOpeBRep_PointClassifier pClass;
    pClass.Load(pface->face);
    if (pClass.Classify(pface->face, pnt2d, tol) == TopAbs_OUT) 
      return EGADS_OUTSIDE;
    return EGADS_SUCCESS;

  } else if ((topo->oclass == SHELL) && (topo->mtype == CLOSED)) {
  
    egadsShell *pshell = (egadsShell *) topo->blind;
    TopOpeBRepTool_SolidClassifier sClass;
    sClass.LoadShell(pshell->shell);
    if (sClass.Classify(pshell->shell, pnt, 
          Precision::Confusion()) == TopAbs_OUT) return EGADS_OUTSIDE;
    return EGADS_SUCCESS;

  } else if ((topo->oclass == BODY) && (topo->mtype == SOLIDBODY)) {

    egadsBody   *pbody = (egadsBody *) topo->blind;
    TopoDS_Solid solid = TopoDS::Solid(pbody->shape);
    TopOpeBRepTool_SolidClassifier sClass;
    sClass.LoadSolid(solid);
    if (sClass.Classify(solid, pnt, Precision::Confusion()) == TopAbs_OUT)
      return EGADS_OUTSIDE;
    return EGADS_SUCCESS;

  }
  
  return EGADS_NOTTOPO;
}


int 
EG_inFace(const egObject *face, const double *uv)
{
  if (face == NULL)               return EGADS_NULLOBJ;
  if (face->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (face->oclass != FACE)       return EGADS_NOTTOPO;
  if (face->blind == NULL)        return EGADS_NODATA;

  egadsFace  *pface = (egadsFace *) face->blind;
  Standard_Real tol = BRep_Tool::Tolerance(pface->face);
  gp_Pnt2d pnt2d(uv[0], uv[1]);
  TopOpeBRep_PointClassifier pClass;
  pClass.Load(pface->face);
  if (pClass.Classify(pface->face, pnt2d, tol) == TopAbs_OUT) 
    return EGADS_OUTSIDE;

  return EGADS_SUCCESS;
}
