/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Geometry Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include "egadsTypes.h"
#include "egadsInternals.h"
#include "egadsClasses.h"

#define PARAMACC 1.0e-4         // parameter accuracy
#define KNACC    1.0e-12	// knot accuracy


  extern "C" int EG_destroyGeometry( egObject *geom );
  extern "C" int EG_copyGeometry( const egObject *geom,
                                  /*@null@*/ double *xform, egObject **copy );
  extern "C" int EG_flipGeometry( const egObject *geom, egObject **copy );

  extern "C" int EG_getGeometry( const egObject *geom, int *oclass, int *type, 
                                 egObject **rGeom, int **ivec, double **rvec );
  extern "C" int EG_makeGeometry( egObject *context, int oclass, int mtype,
                                  /*@null@*/ egObject *refGeom, const int *ivec, 
                                  const double *rvec, egObject **geom );
  extern "C" int EG_getRange( const egObject *geom, double *range, int *pflag );
  extern "C" int EG_evaluate( const egObject *geom, const double *param, 
                              double *result );
  extern "C" int EG_invEvaluate( const egObject *geom, double *xyz, 
                                 double *param, double *result );
  extern "C" int EG_approximate( egObject *context, int maxdeg, double tol,
                                 const int *sizes, const double *xyzs,
                                 egObject **bspline );
  extern "C" int EG_otherCurve( const egObject *surface, const egObject *curve, 
                                double tol, egObject **newcurve );
  extern "C" int EG_isoCline( const egObject *surface, int UV, double value, 
                              egObject **newcurve );
  extern "C" int EG_convertToBSpline( egObject *geom, egObject **bspline ); 



int 
EG_destroyGeometry(egObject *geom)
{
  egObject *obj = NULL;
  
  if (geom->oclass == PCURVE) {
  
    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    if (ppcurv != NULL) obj = ppcurv->basis;
    if (obj    != NULL)
      if (ppcurv->topFlg == 0) {
        EG_dereferenceObject(obj, geom);
      } else {
        EG_dereferenceTopObj(obj, geom);
      }
    if (ppcurv != NULL) delete ppcurv;  
  
  } else if (geom->oclass == CURVE) {

    egadsCurve *pcurve = (egadsCurve *) geom->blind;
    if (pcurve != NULL) obj = pcurve->basis;
    if (obj    != NULL)
      if (pcurve->topFlg == 0) {
        EG_dereferenceObject(obj, geom);
      } else {
        EG_dereferenceTopObj(obj, geom);
      }
    if (pcurve != NULL) delete pcurve;

  } else {
  
    egadsSurface *psurf = (egadsSurface *) geom->blind;
    if (psurf != NULL) obj = psurf->basis;
    if (obj   != NULL)
      if (psurf->topFlg == 0) {
        EG_dereferenceObject(obj, geom);
      } else {
        EG_dereferenceTopObj(obj, geom);
      }
    if (psurf != NULL) delete psurf;

  }
  return EGADS_SUCCESS;
}


static int
EG_getPCurveType(Handle(Geom2d_Curve) &hCurve)
{

  Handle(Geom2d_Line) hLine = Handle(Geom2d_Line)::DownCast(hCurve);
  if (!hLine.IsNull()) return LINE;

  Handle(Geom2d_Circle) hCirc = Handle(Geom2d_Circle)::DownCast(hCurve);
  if (!hCirc.IsNull()) return CIRCLE;

  Handle(Geom2d_Ellipse) hEllip = Handle(Geom2d_Ellipse)::DownCast(hCurve);
  if (!hEllip.IsNull()) return ELLIPSE;

  Handle(Geom2d_Parabola) hParab = Handle(Geom2d_Parabola)::DownCast(hCurve);
  if (!hParab.IsNull()) return PARABOLA;
 
  Handle(Geom2d_Hyperbola) hHypr = Handle(Geom2d_Hyperbola)::DownCast(hCurve);
  if (!hHypr.IsNull()) return HYPERBOLA;

  Handle(Geom2d_BezierCurve) hBezier = Handle(Geom2d_BezierCurve)::DownCast(hCurve);
  if (!hBezier.IsNull()) return BEZIER;

  Handle(Geom2d_BSplineCurve) hBSpline = Handle(Geom2d_BSplineCurve)::DownCast(hCurve);
  if (!hBSpline.IsNull()) return BSPLINE;
  
  Handle(Geom2d_TrimmedCurve) hTrim = Handle(Geom2d_TrimmedCurve)::DownCast(hCurve);
  if (!hTrim.IsNull()) return TRIMMED;

  Handle(Geom2d_OffsetCurve) hOffst = Handle(Geom2d_OffsetCurve)::DownCast(hCurve);
  if (!hOffst.IsNull()) return OFFSET;

  return 0;
}


void 
EG_completePCurve(egObject *geom, Handle(Geom2d_Curve) &hCurve)
{
  int                  outLevel, stat;
  egObject             *obj;
  Handle(Geom2d_Curve) base;

  outLevel            = EG_outLevel(geom);
  geom->oclass        = PCURVE;
  egadsPCurve *ppcurv = new egadsPCurve;
  ppcurv->handle      = hCurve;
  ppcurv->basis       = NULL;
  ppcurv->topFlg      = 0;
  geom->blind         = ppcurv;

  // stand alone geometry
  Handle(Geom2d_Line) hLine = Handle(Geom2d_Line)::DownCast(hCurve);
  if (!hLine.IsNull()) {
    geom->mtype = LINE;
    return;
  }
  Handle(Geom2d_Circle) hCirc = Handle(Geom2d_Circle)::DownCast(hCurve);
  if (!hCirc.IsNull()) {
    geom->mtype = CIRCLE;
    return;
  }
  Handle(Geom2d_Ellipse) hEllip = Handle(Geom2d_Ellipse)::DownCast(hCurve);
  if (!hEllip.IsNull()) {
    geom->mtype = ELLIPSE;
    return;
  }
  Handle(Geom2d_Parabola) hParab = Handle(Geom2d_Parabola)::DownCast(hCurve);
  if (!hParab.IsNull()) {
    geom->mtype = PARABOLA;
    return;
  }
  Handle(Geom2d_Hyperbola) hHypr = Handle(Geom2d_Hyperbola)::DownCast(hCurve);
  if (!hHypr.IsNull()) {
    geom->mtype = HYPERBOLA;
    return;
  }
  Handle(Geom2d_BezierCurve) hBezier = Handle(Geom2d_BezierCurve)::DownCast(hCurve);
  if (!hBezier.IsNull()) {
    geom->mtype = BEZIER;
    return;
  }
  Handle(Geom2d_BSplineCurve) hBSpline = Handle(Geom2d_BSplineCurve)::DownCast(hCurve);
  if (!hBSpline.IsNull()) {
    geom->mtype = BSPLINE;
    return;
  }
  
  // referencing geometry
  Handle(Geom2d_TrimmedCurve) hTrim = Handle(Geom2d_TrimmedCurve)::DownCast(hCurve);
  if (!hTrim.IsNull()) {
    geom->mtype = TRIMMED;
    base = hTrim->BasisCurve();
  }
  Handle(Geom2d_OffsetCurve) hOffst = Handle(Geom2d_OffsetCurve)::DownCast(hCurve);
  if (!hOffst.IsNull()) {
    geom->mtype = OFFSET;
    base = hOffst->BasisCurve();
  }  
  if (geom->mtype == 0) {
    printf(" EGADS Error: Unknown PCurve Type!\n");
    return;
  }
  
  // make the reference curve
  stat = EG_makeObject(EG_context(geom), &obj);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: make Object = %d (EG_completePCurve)!\n", stat);
    return;
  }
  ppcurv->basis = obj;
  if (geom->topObj == EG_context(geom)) {
    obj->topObj = geom;
  } else {
    obj->topObj = geom->topObj;
  }
  EG_completePCurve(obj,  base);
  EG_referenceObject(obj, geom);
}


void 
EG_completeCurve(egObject *geom, Handle(Geom_Curve) &hCurve)
{
  int                outLevel, stat;
  egObject           *obj;
  Handle(Geom_Curve) base;

  outLevel           = EG_outLevel(geom);
  geom->oclass       = CURVE;
  egadsCurve *pcurve = new egadsCurve;
  pcurve->handle     = hCurve;
  pcurve->basis      = NULL;
  pcurve->topFlg     = 0;
  geom->blind        = pcurve;

  // stand alone geometry
  Handle(Geom_Line) hLine = Handle(Geom_Line)::DownCast(hCurve);
  if (!hLine.IsNull()) {
    geom->mtype = LINE;
    return;
  }
  Handle(Geom_Circle) hCirc = Handle(Geom_Circle)::DownCast(hCurve);
  if (!hCirc.IsNull()) {
    geom->mtype = CIRCLE;
    return;
  }
  Handle(Geom_Ellipse) hEllip = Handle(Geom_Ellipse)::DownCast(hCurve);
  if (!hEllip.IsNull()) {
    geom->mtype = ELLIPSE;
    return;
  }
  Handle(Geom_Parabola) hParab = Handle(Geom_Parabola)::DownCast(hCurve);
  if (!hParab.IsNull()) {
    geom->mtype = PARABOLA;
    return;
  }
  Handle(Geom_Hyperbola) hHypr = Handle(Geom_Hyperbola)::DownCast(hCurve);
  if (!hHypr.IsNull()) {
    geom->mtype = HYPERBOLA;
    return;
  }
  Handle(Geom_BezierCurve) hBezier = Handle(Geom_BezierCurve)::DownCast(hCurve);
  if (!hBezier.IsNull()) {
    geom->mtype = BEZIER;
    return;
  }
  Handle(Geom_BSplineCurve) hBSpline = Handle(Geom_BSplineCurve)::DownCast(hCurve);
  if (!hBSpline.IsNull()) {
    geom->mtype = BSPLINE;
    return;
  }
  
  // referencing geometry
  Handle(Geom_TrimmedCurve) hTrim = Handle(Geom_TrimmedCurve)::DownCast(hCurve);
  if (!hTrim.IsNull()) {
    geom->mtype = TRIMMED;
    base = hTrim->BasisCurve();
  }
  Handle(Geom_OffsetCurve) hOffst = Handle(Geom_OffsetCurve)::DownCast(hCurve);
  if (!hOffst.IsNull()) {
    geom->mtype = OFFSET;
    base = hOffst->BasisCurve();
  }  
  if (geom->mtype == 0) {
    printf(" EGADS Error: Unknown Curve Type!\n");
    return;
  }
  
  // make the reference curve
  stat = EG_makeObject(EG_context(geom), &obj);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: make Object = %d (EG_completeCurve)!\n", stat);
    return;
  }
  pcurve->basis = obj;
  if (geom->topObj == EG_context(geom)) {
    obj->topObj = geom;
  } else {
    obj->topObj = geom->topObj;
  }
  EG_completeCurve(obj, base);
  EG_referenceObject(obj, geom);
}


void EG_completeSurf(egObject *geom, Handle(Geom_Surface) &hSurf)
{
  int                  outLevel, stat;
  egObject             *obj;
  Handle(Geom_Surface) base;
  Handle(Geom_Curve)   curve;

  outLevel            = EG_outLevel(geom);
  geom->oclass        = SURFACE;
  egadsSurface *psurf = new egadsSurface;
  psurf->handle       = hSurf;
  psurf->basis        = NULL;
  psurf->topFlg       = 0;
  geom->blind         = psurf;
  
  // stand alone geometry
  Handle(Geom_Plane) hPlane = Handle(Geom_Plane)::DownCast(hSurf);
  if (!hPlane.IsNull()) {
    geom->mtype = PLANE;
    return;
  }
  Handle(Geom_SphericalSurface) hSphere = 
      Handle(Geom_SphericalSurface)::DownCast(hSurf);
  if (!hSphere.IsNull()) {
    geom->mtype = SPHERICAL;
    return;
  }
  Handle(Geom_ConicalSurface) hCone = 
    Handle(Geom_ConicalSurface)::DownCast(hSurf);
  if (!hCone.IsNull()) {
    geom->mtype = CONICAL;
    return;
  }
  Handle(Geom_CylindricalSurface) hCyl =
    Handle(Geom_CylindricalSurface)::DownCast(hSurf);
  if (!hCyl.IsNull()) {
    geom->mtype = CYLINDRICAL;
    return;
  }
  Handle(Geom_ToroidalSurface) hTorus =
    Handle(Geom_ToroidalSurface)::DownCast(hSurf);
  if (!hTorus.IsNull()) {
    geom->mtype = TOROIDAL;
    return;
  }
  Handle(Geom_BezierSurface) hBezier = 
    Handle(Geom_BezierSurface)::DownCast(hSurf);
  if (!hBezier.IsNull()) {
    geom->mtype = BEZIER;
    return;
  }
  Handle(Geom_BSplineSurface) hBSpline = 
    Handle(Geom_BSplineSurface)::DownCast(hSurf);
  if (!hBSpline.IsNull()) {
    geom->mtype = BSPLINE;
    return;
  }

  // referencing geometry -- surface
  Handle(Geom_OffsetSurface) hOffst = 
    Handle(Geom_OffsetSurface)::DownCast(hSurf);
  if (!hOffst.IsNull()) {
    geom->mtype = OFFSET;
    base = hOffst->BasisSurface();
    stat = EG_makeObject(EG_context(geom), &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Object = %d (EG_completeSurface)!\n", stat);
      return;
    }
    psurf->basis = obj;
    if (geom->topObj == EG_context(geom)) {
      obj->topObj = geom;
    } else {
      obj->topObj = geom->topObj;
    }
    EG_completeSurf(obj, base);
    EG_referenceObject(obj, geom);
    return;
  } 
  Handle(Geom_RectangularTrimmedSurface) hTrim = 
    Handle(Geom_RectangularTrimmedSurface)::DownCast(hSurf);
  if (!hTrim.IsNull()) {
    geom->mtype = TRIMMED;
    base = hTrim->BasisSurface();
    stat = EG_makeObject(EG_context(geom), &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Object = %d (EG_completeSurface)!\n", stat);
      return;
    }
    psurf->basis = obj;
    if (geom->topObj == EG_context(geom)) {
      obj->topObj = geom;
    } else {
      obj->topObj = geom->topObj;
    }
    EG_completeSurf(obj, base);
    EG_referenceObject(obj, geom);
    return;
  } 
  
  // referencing geometry -- curve
  Handle(Geom_SurfaceOfLinearExtrusion) hSLExtr = 
    Handle(Geom_SurfaceOfLinearExtrusion)::DownCast(hSurf);
  if (!hSLExtr.IsNull()) {
    geom->mtype = EXTRUSION;
    curve       = hSLExtr->BasisCurve();
  }
  Handle(Geom_SurfaceOfRevolution) hSORev = 
    Handle(Geom_SurfaceOfRevolution)::DownCast(hSurf);
  if (!hSORev.IsNull()) {
    geom->mtype = REVOLUTION;
    curve       = hSORev->BasisCurve();
  }
  if (geom->mtype == 0) {
    printf(" EGADS Error: Unknown Surface Type!\n");
    return;
  }
  
  // make the reference curve
  stat = EG_makeObject(EG_context(geom), &obj);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: make Curve = %d (EG_completeSurface)!\n", stat);
    return;
  }
  psurf->basis = obj;
  if (geom->topObj == EG_context(geom)) {
    obj->topObj = geom;
  } else {
    obj->topObj = geom->topObj;
  }
  EG_completeCurve(obj, curve);
  EG_referenceObject(obj, geom);  
}


int
EG_copyGeometry(const egObject *geom, /*@null@*/ double *xform, egObject **copy)
{
  int      stat, outLevel;
  egObject *obj, *context;
  
  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass != CURVE) && (geom->oclass != SURFACE))
                                   return EGADS_NOTGEOM;
  if  (geom->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(geom);
  context  = EG_context(geom);
          
  gp_Trsf form = gp_Trsf();
  if (xform != NULL)
    form.SetValues(xform[ 0], xform[ 1], xform[ 2], xform[ 3],
                   xform[ 4], xform[ 5], xform[ 6], xform[ 7],
                   xform[ 8], xform[ 9], xform[10], xform[11],
                   Precision::Confusion(), Precision::Angular());
                   
  if (geom->oclass == CURVE) {

    egadsCurve           *pcurve = (egadsCurve *) geom->blind;
    Handle(Geom_Curve)    hCurve = pcurve->handle;
    Handle(Geom_Geometry) nGeom  = hCurve->Transformed(form);
    Handle(Geom_Curve)    nCurve = Handle(Geom_Curve)::DownCast(nGeom);
    if (nCurve.IsNull()) {
      if (outLevel > 0) 
        printf(" EGADS Error: XForm Curve Failed (EG_copyGeometry)!\n");
      return EGADS_CONSTERR;
    }
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0) 
        printf(" EGADS Error: makeObject = %d (EG_copyGeometry)!\n", stat);
      return EGADS_CONSTERR;
    }
    EG_completeCurve(obj, nCurve);
    
  } else {
  
    egadsSurface         *psurf = (egadsSurface *) geom->blind;
    Handle(Geom_Surface)  hSurf = psurf->handle;
    Handle(Geom_Geometry) nGeom = hSurf->Transformed(form);
    Handle(Geom_Surface)  nSurf = Handle(Geom_Surface)::DownCast(nGeom);
    if (nSurf.IsNull()) {
      if (outLevel > 0) 
        printf(" EGADS Error: XForm Surface Failed (EG_copyGeometry)!\n");
      return EGADS_CONSTERR;
    }
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0) 
        printf(" EGADS Error: makeObject = %d (EG_copyGeometry)!\n", stat);
      return EGADS_CONSTERR;
    }
    EG_completeSurf(obj, nSurf);
    
  }

  EG_referenceObject(obj, context);
  *copy = obj;
  return EGADS_SUCCESS;
}


int
EG_flipGeometry(const egObject *geom, egObject **copy)
{
  int      stat, outLevel;
  egObject *obj, *context;
  
  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass != CURVE) && (geom->oclass != SURFACE))
                                   return EGADS_NOTGEOM;
  if  (geom->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(geom);
  context  = EG_context(geom);
    
  if (geom->oclass == PCURVE) {
  
    egadsPCurve          *ppcurv = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve)  hPCurv = ppcurv->handle;
    Handle(Geom2d_Curve)  nPCurv = hPCurv->Reversed();

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0) 
        printf(" EGADS Error: makeObject = %d (EG_flipGeometry)!\n", stat);
      return EGADS_CONSTERR;
    }
    EG_completePCurve(obj, nPCurv);    
    
  } else if (geom->oclass == CURVE) {

    egadsCurve         *pcurve = (egadsCurve *) geom->blind;
    Handle(Geom_Curve)  hCurve = pcurve->handle;
    Handle(Geom_Curve)  nCurve = hCurve->Reversed();

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0) 
        printf(" EGADS Error: makeObject = %d (EG_flipGeometry)!\n", stat);
      return EGADS_CONSTERR;
    }
    EG_completeCurve(obj, nCurve);
    
  } else {
  
    egadsSurface         *psurf = (egadsSurface *) geom->blind;
    Handle(Geom_Surface)  hSurf = psurf->handle;
    Handle(Geom_Surface)  nSurf = hSurf->UReversed();

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0) 
        printf(" EGADS Error: makeObject = %d (EG_flipGeometry)!\n", stat);
      return EGADS_CONSTERR;
    }
    EG_completeSurf(obj, nSurf);
    
  }

  EG_referenceObject(obj, context);
  *copy = obj;
  return EGADS_SUCCESS;
}


int
EG_getGeometry(const egObject     *geom, int *oclass, int *type, 
                     egObject **refGeom, int **ivec,  double **rvec)
{
  int    *ints = NULL, i, j, len, outLevel;
  double *data = NULL;

  *ivec    = NULL;
  *rvec    = NULL;
  *refGeom = NULL;
  *oclass  = *type = 0;
  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass < PCURVE) || (geom->oclass > SURFACE))
                                   return EGADS_NOTGEOM;
  if (geom->blind == NULL)         return EGADS_NODATA;
  outLevel = EG_outLevel(geom);
  *oclass = geom->oclass;
  *type   = geom->mtype;

  if (geom->oclass == PCURVE) {

    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    *refGeom = ppcurv->basis;

    switch (geom->mtype) {

      case LINE:
        data = (double *) EG_alloc(4*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PLine (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_Line) hLine = Handle(Geom2d_Line)::DownCast(hCurve);
          gp_Dir2d direct = hLine->Direction();
          gp_Pnt2d locat  = hLine->Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = direct.X();
          data[3] = direct.Y();
          *rvec   = data;
        }
        break;
      
      case CIRCLE:
        data = (double *) EG_alloc(7*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PCircle (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_Circle) hCirc = Handle(Geom2d_Circle)::DownCast(hCurve);
          gp_Circ2d circ  = hCirc->Circ2d();
          gp_Ax2d   xaxis = circ.XAxis();
          gp_Ax2d   yaxis = circ.YAxis();
          gp_Pnt2d  locat = circ.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = xaxis.Direction().X();
          data[3] = xaxis.Direction().Y();
          data[4] = yaxis.Direction().X();
          data[5] = yaxis.Direction().Y();
          data[6] = circ.Radius();
          *rvec   = data;
        }
        break;
      
      case ELLIPSE:
        data = (double *) EG_alloc(8*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PEllipse (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_Ellipse) hEllip = Handle(Geom2d_Ellipse)::DownCast(hCurve);
          gp_Elips2d elips = hEllip->Elips2d();
          gp_Ax2d    xaxis = elips.XAxis();
          gp_Ax2d    yaxis = elips.YAxis();
          gp_Pnt2d   locat = elips.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = xaxis.Direction().X();
          data[3] = xaxis.Direction().Y();
          data[4] = yaxis.Direction().X();
          data[5] = yaxis.Direction().Y();
          data[6] = elips.MajorRadius();
          data[7] = elips.MinorRadius();
          *rvec   = data;
        }
        break;
      
      case PARABOLA:
        data = (double *) EG_alloc(7*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PParabola (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_Parabola) hParab=Handle(Geom2d_Parabola)::DownCast(hCurve);
          gp_Parab2d parab = hParab->Parab2d();
          gp_Ax22d   axes  = parab.Axis();
          gp_Ax2d    xaxis = axes.XAxis();
          gp_Ax2d    yaxis = axes.YAxis();
          gp_Pnt2d   locat = parab.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = xaxis.Direction().X();
          data[3] = xaxis.Direction().Y();
          data[4] = yaxis.Direction().X();
          data[5] = yaxis.Direction().Y();
          data[6] = parab.Focal();
          *rvec   = data;
        }
        break;
      
      case HYPERBOLA:
        data = (double *) EG_alloc(8*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PHyperbola (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_Hyperbola) hHypr = 
            Handle(Geom2d_Hyperbola)::DownCast(hCurve);
          gp_Hypr2d hypr  = hHypr->Hypr2d();
          gp_Ax2d   xaxis = hypr.XAxis();
          gp_Ax2d   yaxis = hypr.YAxis();
          gp_Pnt2d  locat = hypr.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = xaxis.Direction().X();
          data[3] = xaxis.Direction().Y();
          data[4] = yaxis.Direction().X();
          data[5] = yaxis.Direction().Y();
          data[6] = hypr.MajorRadius();
          data[7] = hypr.MinorRadius();
          *rvec   = data;
        }
        break;
      
      case TRIMMED:
        data = (double *) EG_alloc(2*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PTrimmed Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_TrimmedCurve) hTrim = 
            Handle(Geom2d_TrimmedCurve)::DownCast(hCurve);
          data[0] = hTrim->FirstParameter();
          data[1] = hTrim->LastParameter();
          *rvec   = data;
        }
        break;
      
      case BEZIER:
        ints = (int *) EG_alloc(3*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PBezier Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_BezierCurve) hBezier = 
            Handle(Geom2d_BezierCurve)::DownCast(hCurve);
          int rational = 0;
          if (hBezier->IsRational()) rational = 1;
          ints[0] = rational*2;
          if (hBezier->IsPeriodic()) ints[0] |= 4;
          ints[1] = hBezier->Degree();
          ints[2] = hBezier->NbPoles();
          len = ints[2]*2;
          if (rational == 1) len += ints[2];
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc on PBezier Data (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (i = 1; i <= ints[2]; i++, len+=2) {
            gp_Pnt2d P  = hBezier->Pole(i);
            data[len  ] = P.X();
            data[len+1] = P.Y();
          }
          if (rational == 1)
            for (i = 1; i <= ints[2]; i++,len++) data[len] = hBezier->Weight(i);
          *ivec = ints;
          *rvec = data;
        }
        break;
      
      case BSPLINE:
        ints = (int *) EG_alloc(4*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on PBSpline Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_BSplineCurve) hBSpline = 
            Handle(Geom2d_BSplineCurve)::DownCast(hCurve);
          int rational = 0;
          if (hBSpline->IsRational()) rational = 1;
          ints[0] = rational*2;
          if (hBSpline->IsPeriodic()) ints[0] |= 4;
          ints[1] = hBSpline->Degree();
          ints[2] = hBSpline->NbPoles();
          ints[3] = 0;
          for (i = 1; i <= hBSpline->NbKnots(); i++)
            ints[3] += hBSpline->Multiplicity(i);
          len = ints[3] + ints[2]*2;
          if (rational == 1) len += ints[2];
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc PBSpline Data (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (i = 1; i <= hBSpline->NbKnots(); i++) {
            int km = hBSpline->Multiplicity(i);
            for (j = 1; j <= km; j++, len++) data[len] = hBSpline->Knot(i);
          }
          j = len;
          for (i = 1; i <= ints[2]; i++, len+=2) {
            gp_Pnt2d P  = hBSpline->Pole(i);
            data[len  ] = P.X();
            data[len+1] = P.Y();
          }
          if (rational == 1)
            for (i = 1; i <= ints[2]; i++,len++) 
              data[len] = hBSpline->Weight(i);
          *ivec = ints;
          *rvec = data;
        }
        break;
      
      case OFFSET:
        data = (double *) EG_alloc(1*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on POffset Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom2d_OffsetCurve) hOffst = 
            Handle(Geom2d_OffsetCurve)::DownCast(hCurve);
          data[0] = hOffst->Offset();
          *rvec   = data;
        }
        break;
    }  
  
  } else if (geom->oclass == CURVE) {
    
    egadsCurve *pcurve = (egadsCurve *) geom->blind;
    Handle(Geom_Curve) hCurve = pcurve->handle;
    *refGeom = pcurve->basis;

    switch (geom->mtype) {

      case LINE:
        data = (double *) EG_alloc(6*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Line (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Line) hLine = Handle(Geom_Line)::DownCast(hCurve);
          gp_Lin line   = hLine->Lin();
          gp_Dir direct = line.Direction();
          gp_Pnt locat  = line.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = direct.X();
          data[4] = direct.Y();
          data[5] = direct.Z();
          *rvec   = data;
        }
        break;
      
      case CIRCLE:
        data = (double *) EG_alloc(10*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Circle (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Circle) hCirc = Handle(Geom_Circle)::DownCast(hCurve);
          gp_Circ circ  = hCirc->Circ();
          gp_Ax1  xaxis = circ.XAxis();
          gp_Ax1  yaxis = circ.YAxis();
          gp_Pnt  locat = circ.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = xaxis.Direction().X();
          data[4] = xaxis.Direction().Y();
          data[5] = xaxis.Direction().Z();
          data[6] = yaxis.Direction().X();
          data[7] = yaxis.Direction().Y();
          data[8] = yaxis.Direction().Z();
          data[9] = circ.Radius();
          *rvec   = data;
        }
        break;
      
      case ELLIPSE:
        data = (double *) EG_alloc(11*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Ellipse (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Ellipse) hEllip = Handle(Geom_Ellipse)::DownCast(hCurve);
          gp_Elips elips = hEllip->Elips();
          gp_Ax1   xaxis = elips.XAxis();
          gp_Ax1   yaxis = elips.YAxis();
          gp_Pnt   locat = elips.Location();
          data[ 0] = locat.X();
          data[ 1] = locat.Y();
          data[ 2] = locat.Z();
          data[ 3] = xaxis.Direction().X();
          data[ 4] = xaxis.Direction().Y();
          data[ 5] = xaxis.Direction().Z();
          data[ 6] = yaxis.Direction().X();
          data[ 7] = yaxis.Direction().Y();
          data[ 8] = yaxis.Direction().Z();
          data[ 9] = elips.MajorRadius();
          data[10] = elips.MinorRadius();
          *rvec    = data;
        }
        break;
      
      case PARABOLA:
        data = (double *) EG_alloc(10*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Parabola (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Parabola) hParab=Handle(Geom_Parabola)::DownCast(hCurve);
          gp_Parab parab = hParab->Parab();
          gp_Ax1   xaxis = parab.XAxis();
          gp_Ax1   yaxis = parab.YAxis();
          gp_Pnt   locat = parab.Location();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = xaxis.Direction().X();
          data[4] = xaxis.Direction().Y();
          data[5] = xaxis.Direction().Z();
          data[6] = yaxis.Direction().X();
          data[7] = yaxis.Direction().Y();
          data[8] = yaxis.Direction().Z();
          data[9] = parab.Focal();
          *rvec   = data;
        }
        break;
      
      case HYPERBOLA:
        data = (double *) EG_alloc(11*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Hyperbola (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Hyperbola) hHypr = 
            Handle(Geom_Hyperbola)::DownCast(hCurve);
          gp_Hypr hypr  = hHypr->Hypr();
          gp_Ax1  xaxis = hypr.XAxis();
          gp_Ax1  yaxis = hypr.YAxis();
          gp_Pnt  locat = hypr.Location();
          data[ 0] = locat.X();
          data[ 1] = locat.Y();
          data[ 2] = locat.Z();
          data[ 3] = xaxis.Direction().X();
          data[ 4] = xaxis.Direction().Y();
          data[ 5] = xaxis.Direction().Z();
          data[ 6] = yaxis.Direction().X();
          data[ 7] = yaxis.Direction().Y();
          data[ 8] = yaxis.Direction().Z();
          data[ 9] = hypr.MajorRadius();
          data[10] = hypr.MinorRadius();
          *rvec    = data;
        }
        break;
      
      case TRIMMED:
        data = (double *) EG_alloc(2*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Trimmed Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_TrimmedCurve) hTrim = 
            Handle(Geom_TrimmedCurve)::DownCast(hCurve);
          data[0] = hTrim->FirstParameter();
          data[1] = hTrim->LastParameter();
          *rvec   = data;
        }
        break;
      
      case BEZIER:
        ints = (int *) EG_alloc(3*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Bezier Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_BezierCurve) hBezier = 
            Handle(Geom_BezierCurve)::DownCast(hCurve);
          int rational = 0;
          if (hBezier->IsRational()) rational = 1;
          ints[0] = rational*2;
          if (hBezier->IsPeriodic()) ints[0] |= 4;
          ints[1] = hBezier->Degree();
          ints[2] = hBezier->NbPoles();
          len = ints[2]*3;
          if (rational == 1) len += ints[2];
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc on Bezier Data (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (i = 1; i <= ints[2]; i++, len+=3) {
            gp_Pnt P    = hBezier->Pole(i);
            data[len  ] = P.X();
            data[len+1] = P.Y();
            data[len+2] = P.Z();
          }
          if (rational == 1)
            for (i = 1; i <= ints[2]; i++,len++) data[len] = hBezier->Weight(i);
          *ivec = ints;
          *rvec = data;
        }
        break;
      
      case BSPLINE:
        ints = (int *) EG_alloc(4*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on BSpline Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_BSplineCurve) hBSpline = 
            Handle(Geom_BSplineCurve)::DownCast(hCurve);
          int rational = 0;
          if (hBSpline->IsRational()) rational = 1;
          ints[0] = rational*2;
          if (hBSpline->IsPeriodic()) ints[0] |= 4;
          ints[1] = hBSpline->Degree();
          ints[2] = hBSpline->NbPoles();
          ints[3] = 0;
          for (i = 1; i <= hBSpline->NbKnots(); i++)
            ints[3] += hBSpline->Multiplicity(i);
          len = ints[3] + ints[2]*3;
          if (rational == 1) len += ints[2];
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc BSpline Data (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (i = 1; i <= hBSpline->NbKnots(); i++) {
            int km = hBSpline->Multiplicity(i);
            for (j = 1; j <= km; j++, len++) data[len] = hBSpline->Knot(i);
          }
          j = len;
          for (i = 1; i <= ints[2]; i++, len+=3) {
            gp_Pnt P    = hBSpline->Pole(i);
            data[len  ] = P.X();
            data[len+1] = P.Y();
            data[len+2] = P.Z();
          }
          if (rational == 1)
            for (i = 1; i <= ints[2]; i++,len++) 
              data[len] = hBSpline->Weight(i);
          *ivec = ints;
          *rvec = data;
        }
        break;
      
      case OFFSET:
        data = (double *) EG_alloc(4*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Offset Curve (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_OffsetCurve) hOffst = 
            Handle(Geom_OffsetCurve)::DownCast(hCurve);
          gp_Dir direct = hOffst->Direction();
          data[0] = direct.X();
          data[1] = direct.Y();
          data[2] = direct.Z();
          data[3] = hOffst->Offset();
          *rvec   = data;
        }
        break;
    }
    
  } else {
  
    egadsSurface *psurf = (egadsSurface *) geom->blind;
    Handle(Geom_Surface) hSurf = psurf->handle;
    *refGeom = psurf->basis;

    switch (geom->mtype) {
    
      case PLANE:
        data = (double *) EG_alloc(9*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Plane (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_Plane) hPlane = Handle(Geom_Plane)::DownCast(hSurf);
          gp_Pln plane = hPlane->Pln();
          gp_Pnt locat = plane.Location();
          gp_Ax1 xaxis = plane.XAxis();
          gp_Ax1 yaxis = plane.YAxis();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = xaxis.Direction().X();
          data[4] = xaxis.Direction().Y();
          data[5] = xaxis.Direction().Z();
          data[6] = yaxis.Direction().X();
          data[7] = yaxis.Direction().Y();
          data[8] = yaxis.Direction().Z();
          *rvec   = data;
        }
        break;
        
      case SPHERICAL:
        data = (double *) EG_alloc(10*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Sphere (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_SphericalSurface) hSphere = 
            Handle(Geom_SphericalSurface)::DownCast(hSurf);
          gp_Sphere sphere = hSphere->Sphere();
          gp_Pnt locat     = sphere.Location();
          gp_Ax1 xaxis     = sphere.XAxis();
          gp_Ax1 yaxis     = sphere.YAxis();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = xaxis.Direction().X();
          data[4] = xaxis.Direction().Y();
          data[5] = xaxis.Direction().Z();
          data[6] = yaxis.Direction().X();
          data[7] = yaxis.Direction().Y();
          data[8] = yaxis.Direction().Z();
          data[9] = sphere.Radius();
          *rvec   = data;
        }
        break;

      case CONICAL:
        data = (double *) EG_alloc(14*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Conical (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_ConicalSurface) hCone = 
            Handle(Geom_ConicalSurface)::DownCast(hSurf);
          gp_Cone cone  = hCone->Cone();
          gp_Ax3  axes  = cone.Position();
          gp_Pnt  locat = cone.Location();
          data[ 0] = locat.X();
          data[ 1] = locat.Y();
          data[ 2] = locat.Z();
          data[ 3] = axes.XDirection().X();
          data[ 4] = axes.XDirection().Y();
          data[ 5] = axes.XDirection().Z();
          data[ 6] = axes.YDirection().X();
          data[ 7] = axes.YDirection().Y();
          data[ 8] = axes.YDirection().Z();
          data[ 9] = axes.Direction().X();
          data[10] = axes.Direction().Y();
          data[11] = axes.Direction().Z();
          data[12] = cone.SemiAngle();
          data[13] = cone.RefRadius();
          *rvec    = data;
        }
        break;
        
      case CYLINDRICAL:
        data = (double *) EG_alloc(13*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Cylinder (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_CylindricalSurface) hCyl =
            Handle(Geom_CylindricalSurface)::DownCast(hSurf);
          gp_Cylinder cyl   = hCyl->Cylinder();
          gp_Ax3      axes  = cyl.Position();
          gp_Pnt      locat = cyl.Location();
          data[ 0] = locat.X();
          data[ 1] = locat.Y();
          data[ 2] = locat.Z();
          data[ 3] = axes.XDirection().X();
          data[ 4] = axes.XDirection().Y();
          data[ 5] = axes.XDirection().Z();
          data[ 6] = axes.YDirection().X();
          data[ 7] = axes.YDirection().Y();
          data[ 8] = axes.YDirection().Z();
          data[ 9] = axes.Direction().X();
          data[10] = axes.Direction().Y();
          data[11] = axes.Direction().Z();
          data[12] = cyl.Radius();
          *rvec    = data;
        }
        break;

      case TOROIDAL:
        data = (double *) EG_alloc(14*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Cylinder (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_ToroidalSurface) hTorus =
            Handle(Geom_ToroidalSurface)::DownCast(hSurf);
          gp_Torus torus = hTorus->Torus();
          gp_Ax3   axes  = torus.Position();
          gp_Pnt   locat = torus.Location();
          data[ 0] = locat.X();
          data[ 1] = locat.Y();
          data[ 2] = locat.Z();
          data[ 3] = axes.XDirection().X();
          data[ 4] = axes.XDirection().Y();
          data[ 5] = axes.XDirection().Z();
          data[ 6] = axes.YDirection().X();
          data[ 7] = axes.YDirection().Y();
          data[ 8] = axes.YDirection().Z();
          data[ 9] = axes.Direction().X();
          data[10] = axes.Direction().Y();
          data[11] = axes.Direction().Z();
          data[12] = torus.MajorRadius();
          data[13] = torus.MinorRadius();
          *rvec    = data;
        }
        break;
     
      case BEZIER:
        ints = (int *) EG_alloc(5*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc on Bezier header (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_BezierSurface) hBezier = 
            Handle(Geom_BezierSurface)::DownCast(hSurf);
          int rational = 0;
          if (hBezier->IsURational()) rational = 1;
          if (hBezier->IsVRational()) rational = 1;
          ints[0] = rational*2;
          if (hBezier->IsUPeriodic()) ints[0] |=  4;
          if (hBezier->IsVPeriodic()) ints[0] |=  8;
          ints[1] = hBezier->UDegree();
          ints[3] = hBezier->VDegree();
          ints[2] = hBezier->NbUPoles();
          ints[4] = hBezier->NbVPoles();
          int nCP = ints[2];
          nCP    *= ints[4];
          len     = nCP*3;
          if (rational == 1) len += nCP;
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc on Bezier Surf (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (j = 1; j <= ints[4]; j++)
            for (i = 1; i <= ints[2]; i++, len+=3) {
              gp_Pnt P    = hBezier->Pole(i, j);
              data[len  ] = P.X();
              data[len+1] = P.Y();
              data[len+2] = P.Z();
            }
          if (rational == 1)
            for (j = 1; j <= ints[4]; j++)
              for (i = 1; i <= ints[2]; i++,len++) 
                data[len] = hBezier->Weight(i, j);
          *ivec = ints;
          *rvec = data;
        }
        break;

      case BSPLINE:
        ints = (int *) EG_alloc(7*sizeof(int));
        if (ints == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc BSpline header (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_BSplineSurface) hBSpline = 
            Handle(Geom_BSplineSurface)::DownCast(hSurf);
          int rational = 0;
          if (hBSpline->IsURational()) rational = 1;
          if (hBSpline->IsVRational()) rational = 1;
          ints[0] = rational*2;
          if (hBSpline->IsUPeriodic()) ints[0] |=  4;
          if (hBSpline->IsVPeriodic()) ints[0] |=  8;
          ints[1] = hBSpline->UDegree();
          ints[4] = hBSpline->VDegree();
          ints[2] = hBSpline->NbUPoles();
          ints[5] = hBSpline->NbVPoles();
          ints[3] = ints[6] = 0;
          for (i = 1; i <= hBSpline->NbUKnots(); i++)
            ints[3] += hBSpline->UMultiplicity(i);
          for (i = 1; i <= hBSpline->NbVKnots(); i++)
            ints[6] += hBSpline->VMultiplicity(i);
          int nCP = ints[2];
          nCP    *= ints[5];
          len     = ints[3] + ints[6] + nCP*3;
          if (rational == 1) len += nCP;
          data = (double *) EG_alloc(len*sizeof(double));
          if (data == NULL) {
            if (outLevel > 0)
              printf(" EGADS Error: Malloc BSpline Surf (EG_getGeometry)!\n");
              EG_free(ints);
            return EGADS_MALLOC;
          }
          len = 0;
          for (i = 1; i <= hBSpline->NbUKnots(); i++) {
            int km = hBSpline->UMultiplicity(i);
            for (j = 1; j <= km; j++, len++) data[len] = hBSpline->UKnot(i);
          }
          for (i = 1; i <= hBSpline->NbVKnots(); i++) {
            int km = hBSpline->VMultiplicity(i);
            for (j = 1; j <= km; j++, len++) data[len] = hBSpline->VKnot(i);
          }
          for (j = 1; j <= ints[5]; j++)
            for (i = 1; i <= ints[2]; i++, len+=3) {
              gp_Pnt P    = hBSpline->Pole(i, j);
              data[len  ] = P.X();
              data[len+1] = P.Y();
              data[len+2] = P.Z();
            }
          if (rational == 1)
            for (j = 1; j <= ints[5]; j++)
              for (i = 1; i <= ints[2]; i++,len++) 
                data[len] = hBSpline->Weight(i, j);
          *ivec = ints;
          *rvec = data;
        }
        break;
        
      case OFFSET:
        data = (double *) EG_alloc(sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc Offset Surface (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_OffsetSurface) hOffst = 
            Handle(Geom_OffsetSurface)::DownCast(hSurf);
          data[0] = hOffst->Offset();
          *rvec   = data;
        }
        break;
        
      case TRIMMED:
        data = (double *) EG_alloc(4*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc Trimmed Surface (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_RectangularTrimmedSurface) hTrim = 
            Handle(Geom_RectangularTrimmedSurface)::DownCast(hSurf);
          hTrim->Bounds(data[0], data[1], data[2], data[3]);
          *rvec = data;
        }
        break;
        
      case EXTRUSION:
        data = (double *) EG_alloc(3*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc Linear Extrusion (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_SurfaceOfLinearExtrusion) hSLExtr = 
            Handle(Geom_SurfaceOfLinearExtrusion)::DownCast(hSurf);
          gp_Dir direct = hSLExtr->Direction();
          data[0] = direct.X();
          data[1] = direct.Y();
          data[2] = direct.Z();
          *rvec   = data;
        }
        break;   

      case REVOLUTION:
        data = (double *) EG_alloc(6*sizeof(double));
        if (data == NULL) {
          if (outLevel > 0)
            printf(" EGADS Error: Malloc Revolved Surface (EG_getGeometry)!\n");
          return EGADS_MALLOC;
        } else {
          Handle(Geom_SurfaceOfRevolution) hSORev = 
            Handle(Geom_SurfaceOfRevolution)::DownCast(hSurf);
          gp_Pnt locat     = hSORev->Location();
          gp_Ax1 axis      = hSORev->Axis();
          data[0] = locat.X();
          data[1] = locat.Y();
          data[2] = locat.Z();
          data[3] = axis.Direction().X();
          data[4] = axis.Direction().Y();
          data[5] = axis.Direction().Z();
          *rvec   = data;
        }
        break;

    }
  
  }

  return EGADS_SUCCESS;
}


int  
EG_makeGeometry(egObject *context, int oclass, int mtype, 
                /*@null@*/ egObject *refGeom, /*@null@*/ const int *ints,
                const double *data, egObject **geom)
{
  int      i, j, len, stat, outLevel, rational, nmult;
  egObject *obj = NULL, *basis = NULL;

  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  outLevel = EG_outLevel(context);

  if ((oclass < PCURVE) || (oclass > SURFACE)) {
    if (outLevel > 0)
      printf(" EGADS Error: oclass = %d (EG_makeGeometry)!\n", oclass);
    return EGADS_NOTGEOM;
  }
  
  if (oclass == PCURVE) {
  
    if ((mtype < LINE) || (mtype > OFFSET)) {
      if (outLevel > 0)
        printf(" EGADS Error: PCurve mtype = %d (EG_makeGeometry)!\n", 
               mtype);
      return EGADS_RANGERR;
    }
    if ((mtype == TRIMMED) || (mtype == OFFSET)) {
      if (refGeom == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: PCrv mtype = %d Ref is NULL (EG_makeGeometry)!\n", 
                 mtype);
        return EGADS_NULLOBJ;
      }
      if (refGeom->oclass != PCURVE) {
        if (outLevel > 0)
          printf(" EGADS Error: PCrv mtype = %d Ref is %d (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NOTGEOM;
      }
      basis = refGeom;
      if (basis->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: PCrv mtype = %d Ref has no data (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NODATA;
      }
    }
    
    Handle(Geom2d_Curve) hCurve;
    try {
      switch (mtype) {

        case LINE:
          {
            gp_Pnt2d pntl(data[0], data[1]);
            gp_Dir2d dirl(data[2], data[3]);
            hCurve = new Geom2d_Line(pntl, dirl);
          }
          break;
    
        case CIRCLE:
          {
            gp_Pnt2d pntc(data[0], data[1]);
            gp_Dir2d dirx(data[2], data[3]);
            gp_Dir2d diry(data[4], data[5]);
            gp_Ax22d axi2(pntc, dirx, diry);
            hCurve = new Geom2d_Circle(axi2, data[6]);
          }
          break;

        case ELLIPSE:
          {
            gp_Pnt2d pnte(data[0], data[1]);
            gp_Dir2d dirx(data[2], data[3]);
            gp_Dir2d diry(data[4], data[5]);
            gp_Ax22d axi2(pnte, dirx, diry);
            hCurve = new Geom2d_Ellipse(axi2, data[6], data[7]);
          }
          break;
      
        case PARABOLA:
          {
            gp_Pnt2d pntp(data[0], data[1]);
            gp_Dir2d dirx(data[2], data[3]);
            gp_Dir2d diry(data[4], data[5]);
            gp_Ax22d axi2(pntp, dirx, diry);
            hCurve = new Geom2d_Parabola(axi2, data[6]);
          }
          break;
     
        case HYPERBOLA:
          {
            gp_Pnt2d pnth(data[0], data[1]);
            gp_Dir2d dirx(data[2], data[3]);
            gp_Dir2d diry(data[4], data[5]);
            gp_Ax22d axi2(pnth, dirx, diry);
            hCurve = new Geom2d_Hyperbola(axi2, data[6], data[7]);
          } 
          break;
   
        case TRIMMED:
          {
            egadsPCurve *ppcurv = (egadsPCurve *) basis->blind;
            hCurve = new Geom2d_TrimmedCurve(ppcurv->handle, data[0], data[1]);
          }
          break;
     
        case BEZIER:
          {
            rational = 0;
            if ((ints[0]&2) != 0) rational = 1;
            len = ints[2];
            TColgp_Array1OfPnt2d aPoles(1, len);
            for (i = 1; i <= ints[2]; i++)
              aPoles(i) = gp_Pnt2d(data[2*i-2], data[2*i-1]);
            if (rational == 0) {
              hCurve = new Geom2d_BezierCurve(aPoles);
            } else {
              TColStd_Array1OfReal aWeights(1, len);
              len = 2*ints[2];
              for (i = 1; i <= ints[2]; i++, len++) 
                aWeights(i) = data[len];
              hCurve = new Geom2d_BezierCurve(aPoles, aWeights);
            }
          }
          break;
    
        case BSPLINE:
          {
            Standard_Boolean periodic = Standard_False;
            rational = 0;
            if ((ints[0]&2) != 0) rational = 1;
            if ((ints[0]&4) != 0) periodic = Standard_True;
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) len++;
            TColStd_Array1OfReal    aKnots(1, len);
            TColStd_Array1OfInteger aMults(1, len);
            aKnots(1) = data[0];
            aMults(1) = nmult = 1;
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) {
                len++;
                aKnots(len) = data[i];
                aMults(len) = nmult = 1;
              } else {
                nmult++;
                aMults(len) = nmult;
              }
            len = ints[3];
            TColgp_Array1OfPnt2d aPoles(1, ints[2]);
            for (i = 1; i <= ints[2]; i++, len+=2)
              aPoles(i) = gp_Pnt2d(data[len], data[len+1]);
            if (rational == 0) {
              hCurve = new Geom2d_BSplineCurve(aPoles, aKnots, aMults,
                                               ints[1], periodic);
            } else {
              TColStd_Array1OfReal aWeights(1, ints[2]);
              for (i = 1; i <= ints[2]; i++, len++) 
                aWeights(i) = data[len];
              hCurve = new Geom2d_BSplineCurve(aPoles, aWeights, aKnots, 
                                               aMults, ints[1], periodic);
            }
          }
          break;
    
        case OFFSET:
          {
            egadsPCurve *ppcurv = (egadsPCurve *) basis->blind;
            hCurve = new Geom2d_OffsetCurve(ppcurv->handle, data[0]);
          }
          break;
      }
    }
    catch (Standard_Failure) 
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("                %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...)
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      return EGADS_GEOMERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make PCurve = %d (EG_makeGeometry)!\n", stat);
      return stat;
    }
    obj->oclass         = PCURVE;
    obj->mtype          = mtype;
    egadsPCurve *ppcurv = new egadsPCurve;
    ppcurv->handle      = hCurve;
    ppcurv->basis       = basis;
    ppcurv->topFlg      = 1;
    obj->blind          = ppcurv;
    EG_referenceObject(obj, context);
    if (basis != NULL) EG_referenceTopObj(basis, obj);
  
  } else if (oclass == CURVE) {

    if ((mtype < LINE) || (mtype > OFFSET)) {
      if (outLevel > 0)
        printf(" EGADS Error: Curve mtype = %d (EG_makeGeometry)!\n", 
               mtype);
      return EGADS_RANGERR;
    }
    if ((mtype == TRIMMED) || (mtype == OFFSET)) {
      if (refGeom == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Crv mtype = %d Ref is NULL (EG_makeGeometry)!\n", 
                 mtype);
        return EGADS_NULLOBJ;
      }
      if (refGeom->oclass != CURVE) {
        if (outLevel > 0)
          printf(" EGADS Error: Crv mtype = %d Ref is %d (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NOTGEOM;
      }
      basis = refGeom;
      if (basis->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Crv mtype = %d Ref has no data (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NODATA;
      }
    }
    
    Handle(Geom_Curve) hCurve;
    try {
      switch (mtype) {

        case LINE:
          {
            gp_Pnt pntl(data[0], data[1], data[2]);
            gp_Dir dirl(data[3], data[4], data[5]);
            hCurve = new Geom_Line(pntl, dirl);
          }
          break;
    
        case CIRCLE:
          {
            gp_Pnt pntc(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Dir dirz = dirx.Crossed(diry);
            gp_Ax2 axi2(pntc, dirz, dirx);
            hCurve = new Geom_Circle(axi2, data[9]);
          }
          break;

        case ELLIPSE:
          {
            gp_Pnt pnte(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Dir dirz = dirx.Crossed(diry);
            gp_Ax2 axi2(pnte, dirz, dirx);
            hCurve = new Geom_Ellipse(axi2, data[9], data[10]);
          }
          break;
      
        case PARABOLA:
          {
            gp_Pnt pntp(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Dir dirz = dirx.Crossed(diry);
            gp_Ax2 axi2(pntp, dirz, dirx);
            hCurve = new Geom_Parabola(axi2, data[9]);
          }
          break;
     
        case HYPERBOLA:
          {
            gp_Pnt pnth(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Dir dirz = dirx.Crossed(diry);
            gp_Ax2 axi2(pnth, dirz, dirx);
            hCurve = new Geom_Hyperbola(axi2, data[9], data[10]);
          } 
          break;
   
        case TRIMMED:
          {
            egadsCurve *pcurve = (egadsCurve *) basis->blind;
            hCurve = new Geom_TrimmedCurve(pcurve->handle, data[0], data[1]);
          }
          break;
     
        case BEZIER:
          {
            rational = 0;
            if ((ints[0]&2) != 0) rational = 1;
            len = ints[2];
            TColgp_Array1OfPnt aPoles(1, len);
            for (i = 1; i <= ints[2]; i++)
              aPoles(i) = gp_Pnt(data[3*i-3], data[3*i-2], data[3*i-1]);
            if (rational == 0) {
              hCurve = new Geom_BezierCurve(aPoles);
            } else {
              TColStd_Array1OfReal aWeights(1, len);
              len = 3*ints[2];
              for (i = 1; i <= ints[2]; i++, len++) 
                aWeights(i) = data[len];
              hCurve = new Geom_BezierCurve(aPoles, aWeights);
            }
          }
          break;
    
        case BSPLINE:
          {
            Standard_Boolean periodic = Standard_False;
            rational = 0;
            if ((ints[0]&2) != 0) rational = 1;
            if ((ints[0]&4) != 0) periodic = Standard_True;
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) len++;
            TColStd_Array1OfReal    aKnots(1, len);
            TColStd_Array1OfInteger aMults(1, len);
            aKnots(1) = data[0];
            aMults(1) = nmult = 1;
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) {
                len++;
                aKnots(len) = data[i];
                aMults(len) = nmult = 1;
              } else {
                nmult++;
                aMults(len) = nmult;
              }            
            len = ints[3];
            TColgp_Array1OfPnt aPoles(1, ints[2]);
            for (i = 1; i <= ints[2]; i++, len+=3)
              aPoles(i) = gp_Pnt(data[len], data[len+1], data[len+2]);
            if (rational == 0) {
              hCurve = new Geom_BSplineCurve(aPoles, aKnots, aMults,
                                             ints[1], periodic);
            } else {
              TColStd_Array1OfReal aWeights(1, ints[2]);
              for (i = 1; i <= ints[2]; i++, len++) 
                aWeights(i) = data[len];
              hCurve = new Geom_BSplineCurve(aPoles, aWeights, aKnots, 
                                             aMults, ints[1], periodic);
            }
          }
          break;
    
        case OFFSET:
          {
            egadsCurve *pcurve = (egadsCurve *) basis->blind;
            gp_Dir dir(data[0], data[1], data[2]);
            hCurve = new Geom_OffsetCurve(pcurve->handle, data[3], dir);
          }
          break;
      }
    }
    catch (Standard_Failure)
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("                %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...)
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      return EGADS_GEOMERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Curve = %d (EG_makeGeometry)!\n", stat);
      return stat;
    }
    obj->oclass        = CURVE;
    obj->mtype         = mtype;
    egadsCurve *pcurve = new egadsCurve;
    pcurve->handle     = hCurve;
    pcurve->basis      = basis;
    pcurve->topFlg     = 1;
    obj->blind         = pcurve;
    EG_referenceObject(obj, context);
    if (basis != NULL) EG_referenceTopObj(basis, obj);

  } else {
  
    if ((mtype < PLANE) || (mtype > EXTRUSION)) {
      if (outLevel > 0)
        printf(" EGADS Error: Surface mtype = %d (EG_makeGeometry)!\n", 
               mtype);
      return EGADS_RANGERR;
    }
    if ((mtype == EXTRUSION) || (mtype == REVOLUTION)) {
      if (refGeom == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref is NULL (EG_makeGeometry)!\n", 
                 mtype);
        return EGADS_NULLOBJ;
      }
      if (refGeom->oclass != CURVE) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref is %d (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NOTGEOM;
      }
      basis = refGeom;
      if (basis->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref has no data (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NODATA;
      }
    }  
    if ((mtype == OFFSET) || (mtype == TRIMMED)) {
      if (refGeom == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref is NULL (EG_makeGeometry)!\n", 
                 mtype);
        return EGADS_NULLOBJ;
      }
      if (refGeom->oclass != SURFACE) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref is %d (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NOTGEOM;
      }
      basis = refGeom;
      if (basis->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Srf mtype = %d Ref has no data (EG_makeGeometry)!\n", 
                 mtype, refGeom->oclass);
        return EGADS_NODATA;
      }
    }
    
    Handle(Geom_Surface) hSurf;
    try {
      switch (mtype) {

        case PLANE:
          {
            gp_Pnt pntp(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Ax2 axi2;
            axi2.SetLocation(pntp);
            axi2.SetXDirection(dirx);
            axi2.SetYDirection(diry);
            gp_Ax3 axi3(axi2);
            hSurf = new Geom_Plane(axi3);
          }
          break;
          
        case SPHERICAL:
          {
            gp_Pnt pnts(data[0], data[1], data[2]);
            gp_Dir dirx(data[3], data[4], data[5]);
            gp_Dir diry(data[6], data[7], data[8]);
            gp_Ax2 axi2;
            axi2.SetLocation(pnts);
            axi2.SetXDirection(dirx);
            axi2.SetYDirection(diry);
            gp_Ax3 axi3(axi2);
            hSurf = new Geom_SphericalSurface(axi3, data[9]);
          }
          break;
          
        case CONICAL:
          {
            gp_Pnt pntc(data[0], data[1],  data[2]);
            gp_Dir dirx(data[3], data[4],  data[5]);
            gp_Dir diry(data[6], data[7],  data[8]);
            gp_Dir dirz(data[9], data[10], data[11]);
            gp_Ax3 axi3(pntc, dirz);
            axi3.SetXDirection(dirx);
            axi3.SetYDirection(diry);
            hSurf = new Geom_ConicalSurface(axi3, data[12], data[13]);
          }
          break;
          
        case CYLINDRICAL:
          {
            gp_Pnt pntc(data[0], data[1],  data[2]);
            gp_Dir dirx(data[3], data[4],  data[5]);
            gp_Dir diry(data[6], data[7],  data[8]);
            gp_Dir dirz(data[9], data[10], data[11]);
            gp_Ax3 axi3(pntc, dirz);
            axi3.SetXDirection(dirx);
            axi3.SetYDirection(diry);
            hSurf = new Geom_CylindricalSurface(axi3, data[12]);
          }
          break;

        case TOROIDAL:
          {
            gp_Pnt pntt(data[0], data[1],  data[2]);
            gp_Dir dirx(data[3], data[4],  data[5]);
            gp_Dir diry(data[6], data[7],  data[8]);
            gp_Dir dirz(data[9], data[10], data[11]);
            gp_Ax3 axi3(pntt, dirz);
            axi3.SetXDirection(dirx);
            axi3.SetYDirection(diry);
            hSurf = new Geom_ToroidalSurface(axi3, data[12], data[13]);
          }
          break;
        
        case BEZIER:
          {
            rational = 0;
            if ((ints[0]&2) != 0) rational = 1;
            TColgp_Array2OfPnt aPoles(1, ints[2], 1, ints[4]);
            len = 0;
            for (j = 1; j <= ints[4]; j++)
              for (i = 1; i <= ints[2]; i++, len+=3)
                aPoles(i, j) = gp_Pnt(data[len], data[len+1], data[len+2]);
            if (rational == 0) {
              hSurf = new Geom_BezierSurface(aPoles);
            } else {
              TColStd_Array2OfReal aWeights(1, ints[2], 1, ints[4]);
              for (j = 1; j <= ints[4]; j++)
                for (i = 1; i <= ints[2]; i++, len++)
                  aWeights(i, j) = data[len];
              hSurf = new Geom_BezierSurface(aPoles, aWeights);
            }
          }
          break;
          
        case BSPLINE:
          {
            Standard_Boolean uPeriodic = Standard_False;
            Standard_Boolean vPeriodic = Standard_False;
            rational = 0;
            if ((ints[0]&2) != 0) rational  = 1;
            if ((ints[0]&4) != 0) uPeriodic = Standard_True;
            if ((ints[0]&8) != 0) vPeriodic = Standard_True;
//          uKnots
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) len++;
            TColStd_Array1OfReal    uKnots(1, len);
            TColStd_Array1OfInteger uMults(1, len);
            uKnots(1) = data[0];
            uMults(1) = nmult = 1;
            for (len = i = 1; i < ints[3]; i++)
              if (fabs(data[i]-data[i-1]) > KNACC) {
                len++;
                uKnots(len) = data[i];
                uMults(len) = nmult = 1;
              } else {
                nmult++;
                uMults(len) = nmult;
              }
//          vKnots
            for (len = i = 1; i < ints[6]; i++)
              if (fabs(data[ints[3]+i]-data[ints[3]+i-1]) > KNACC) len++;
            TColStd_Array1OfReal    vKnots(1, len);
            TColStd_Array1OfInteger vMults(1, len);
            vKnots(1) = data[ints[3]];
            vMults(1) = nmult = 1;
            for (len = i = 1; i < ints[6]; i++)
              if (fabs(data[ints[3]+i]-data[ints[3]+i-1]) > KNACC) {
                len++;
                vKnots(len) = data[ints[3]+i];
                vMults(len) = nmult = 1;
              } else {
                nmult++;
                vMults(len) = nmult;
              }
            len = ints[3]+ints[6];
            TColgp_Array2OfPnt aPoles(1, ints[2], 1, ints[5]);
            for (j = 1; j <= ints[5]; j++)
              for (i = 1; i <= ints[2]; i++, len+=3)
                aPoles(i, j) = gp_Pnt(data[len], data[len+1], data[len+2]);
            if (rational == 0) {
              hSurf = new Geom_BSplineSurface(aPoles, uKnots, vKnots, 
                                              uMults, vMults, ints[1], ints[4], 
                                              uPeriodic, vPeriodic);
            } else {
              TColStd_Array2OfReal aWeights(1, ints[2], 1, ints[5]);
              for (j = 1; j <= ints[5]; j++)
                for (i = 1; i <= ints[2]; i++, len++)
                  aWeights(i, j) = data[len];
              hSurf = new Geom_BSplineSurface(aPoles, aWeights, uKnots, vKnots, 
                                              uMults, vMults, ints[1], ints[4], 
                                              uPeriodic, vPeriodic);
            }
          }
          break;

        case OFFSET:
          {
            egadsSurface *psurf = (egadsSurface *) basis->blind;
            hSurf = new Geom_OffsetSurface(psurf->handle, data[0]);
          }
          break;
        
        case TRIMMED:
          {
            egadsSurface *psurf = (egadsSurface *) basis->blind;
            hSurf = new Geom_RectangularTrimmedSurface(psurf->handle,
                                        data[0], data[1], data[2], data[3]);
          }
          break;
        
      case EXTRUSION:
          {
            gp_Dir dir(data[0], data[1],  data[2]);
            egadsCurve *pcurve = (egadsCurve *) basis->blind;
            hSurf = new Geom_SurfaceOfLinearExtrusion(pcurve->handle, dir);
          }
          break;   

        case REVOLUTION:
          {
            gp_Pnt pnt(data[0], data[1], data[2]);
            gp_Dir dir(data[3], data[4], data[5]);
            gp_Ax1 axi1(pnt, dir);
            egadsCurve *pcurve = (egadsCurve *) basis->blind;
            hSurf = new Geom_SurfaceOfRevolution(pcurve->handle, axi1);
          }
          break;

      }
    }
    catch (Standard_Failure)
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      Handle_Standard_Failure e = Standard_Failure::Caught();
      printf("                %s\n", e->GetMessageString());
      return EGADS_GEOMERR;
    }
    catch (...)
    {
      printf(" EGADS Warning: Geometry Creation Error (EG_makeGeometry)!\n");
      return EGADS_GEOMERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Surface = %d (EG_makeGeometry)!\n", stat);
      return stat;
    }
    obj->oclass         = SURFACE;
    obj->mtype          = mtype;
    egadsSurface *psurf = new egadsSurface;
    psurf->handle       = hSurf;
    psurf->basis        = basis;
    psurf->topFlg       = 1;
    obj->blind          = psurf;
    EG_referenceObject(obj, context);
    if (basis != NULL) EG_referenceTopObj(basis, obj);

  }

  *geom = obj;
  return EGADS_SUCCESS;
}


int
EG_getRange(const egObject *geom, double *range, int *periodic)
{
  int per;

  *periodic = 0;
  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass != PCURVE) &&
      (geom->oclass != CURVE)  && (geom->oclass != SURFACE) &&
      (geom->oclass != EDGE)   && (geom->oclass != FACE))
                                   return EGADS_NOTGEOM;
  if (geom->blind == NULL)         return EGADS_NODATA;
  
  if (geom->oclass == PCURVE) {
  
    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    if (hCurve->IsPeriodic()) *periodic = 1;
    range[0] = hCurve->FirstParameter();
    range[1] = hCurve->LastParameter();
  
  } else if (geom->oclass == CURVE) {
    
    egadsCurve *pcurve = (egadsCurve *) geom->blind;
    Handle(Geom_Curve) hCurve = pcurve->handle;
    if (hCurve->IsPeriodic()) *periodic = 1;
    range[0] = hCurve->FirstParameter();
    range[1] = hCurve->LastParameter();

  } else if (geom->oclass == SURFACE) {
  
    egadsSurface *psurf = (egadsSurface *) geom->blind;
    Handle(Geom_Surface) hSurf = psurf->handle;
    per = 0;
    if (hSurf->IsUPeriodic()) per  = 1;
    if (hSurf->IsVPeriodic()) per |= 2;
    *periodic = per;
    hSurf->Bounds(range[0],range[1], range[2],range[3]);
    
  } else if (geom->oclass == EDGE) {
  
    egadsEdge *pedge = (egadsEdge *) geom->blind;
    BRep_Tool::Range(pedge->edge, range[0],range[1]);
    BRepAdaptor_Curve aCurv(pedge->edge);
    if (aCurv.IsPeriodic()) *periodic = 1;
  
  } else {

    egadsFace *pface = (egadsFace *) geom->blind;
    BRepTools::UVBounds(pface->face, range[0],range[1], 
                                     range[2],range[3]);
    BRepAdaptor_Surface aSurf(pface->face, Standard_True);
    per = 0;
    if (aSurf.IsUPeriodic()) per  = 1;
    if (aSurf.IsVPeriodic()) per |= 2;
    *periodic = per;

  }
  
  return EGADS_SUCCESS;
}


int
EG_evaluate(const egObject *geom, const double *param, 
                                        double *result)
{
  int    outLevel;
  gp_Pnt P0;
  gp_Vec V1, V2, U1, U2, UV;

  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass != PCURVE) &&
      (geom->oclass != CURVE)  && (geom->oclass != SURFACE) &&
      (geom->oclass != EDGE)   && (geom->oclass != FACE))
                                   return EGADS_NOTGEOM;
  if (geom->blind == NULL)         return EGADS_NODATA;
  outLevel = EG_outLevel(geom);
  
  if (geom->oclass == PCURVE) {
    gp_Pnt2d P2d;
    gp_Vec2d V12d, V22d;

    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    hCurve->D2(*param, P2d, V12d, V22d);
    result[0] = P2d.X();
    result[1] = P2d.Y();
    result[2] = V12d.X();
    result[3] = V12d.Y();
    result[4] = V22d.X();
    result[5] = V22d.Y();
  
  } else if ((geom->oclass == CURVE) || (geom->oclass == EDGE)) {
  
    // 1D -- curves & Edges
    if (geom->oclass == CURVE) {
      egadsCurve *pcurve = (egadsCurve *) geom->blind;
      Handle(Geom_Curve) hCurve = pcurve->handle;
      hCurve->D2(*param, P0, V1, V2);
    } else {
      egadsEdge *pedge = (egadsEdge *) geom->blind;
#ifdef ADAPTOR
      BRepAdaptor_Curve aCurv(pedge->edge);
      aCurv.D2(*param, P0, V1, V2);
#else
      egObject *curv = pedge->curve;
      if (curv == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No curve Object for Edge (EG_evaluate)!\n");
        return EGADS_NULLOBJ;
      }
      egadsCurve *pcurve = (egadsCurve *) curv->blind;
      if (pcurve == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No curve Data for Edge (EG_evaluate)!\n");
        return EGADS_NODATA;
      }
      Handle(Geom_Curve) hCurve = pcurve->handle;
      hCurve->D2(*param, P0, V1, V2);
#endif
    }
    result[0] = P0.X();
    result[1] = P0.Y();
    result[2] = P0.Z();
    result[3] = V1.X();
    result[4] = V1.Y();
    result[5] = V1.Z();
    result[6] = V2.X();
    result[7] = V2.Y();
    result[8] = V2.Z();
  
  } else {
  
    // 2D -- surfaces & Faces
    if (geom->oclass == SURFACE) {
      egadsSurface *psurf = (egadsSurface *) geom->blind;
      Handle(Geom_Surface) hSurface = psurf->handle;
      hSurface->D2(param[0], param[1], P0, U1, V1, U2, V2, UV);
    } else {
      egadsFace *pface = (egadsFace *) geom->blind;
#ifdef ADAPTOR
      BRepAdaptor_Surface aSurf(pface->face, Standard_True);
      aSurf.D2(param[0], param[1], P0, U1, V1, U2, V2, UV);
#else
      egObject *surf = pface->surface;
      if (surf == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No Surf Object for Face (EG_evaluate)!\n");
        return EGADS_NULLOBJ;
      }
      egadsSurface *psurf = (egadsSurface *) surf->blind;
      if (psurf == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No Surf Data for Face (EG_evaluate)!\n");
        return EGADS_NODATA;
      }
      Handle(Geom_Surface) hSurface = psurf->handle;
      hSurface->D2(param[0], param[1], P0, U1, V1, U2, V2, UV);
#endif
    }
    result[ 0] = P0.X();
    result[ 1] = P0.Y();
    result[ 2] = P0.Z();
    result[ 3] = U1.X();
    result[ 4] = U1.Y();
    result[ 5] = U1.Z();
    result[ 6] = V1.X();
    result[ 7] = V1.Y();
    result[ 8] = V1.Z();
    result[ 9] = U2.X();
    result[10] = U2.Y();
    result[11] = U2.Z();
    result[12] = UV.X();
    result[13] = UV.Y();
    result[14] = UV.Z();
    result[15] = V2.X();
    result[16] = V2.Y();
    result[17] = V2.Z();
    
  }
  
  return EGADS_SUCCESS;
}


static void
EG_nearestCurve(Handle_Geom_Curve hCurve, double *coor, 
                double tmin, double tmax, double *t, double *xyz)
{
  int    i;
  double a, b, tx, pw[3];
  gp_Pnt pnt;
  gp_Vec t1, t2;
  static double ratios[5] = {0.02, 0.25, 0.5, 0.75, 0.98};

  // sample and pick closest
  for (i = 0; i < 5; i++) {
    tx = (1.0-ratios[i])*tmin + ratios[i]*tmax;
    hCurve->D0(tx, pnt);
    a = (pnt.X()-coor[0])*(pnt.X()-coor[0]) + 
        (pnt.Y()-coor[1])*(pnt.Y()-coor[1]) +
        (pnt.Z()-coor[2])*(pnt.Z()-coor[2]);
    if (i == 0) {
      *t = tx;
      b  = a;
    } else {
      if (a < b) {
        *t = tx;
        b  = a;
      }
    }
  }
  
  // netwon-raphson from picked position
  for (i = 0; i < 20; i++) {
    if ((*t < tmin) || (*t > tmax)) break;
    hCurve->D2(*t, pnt, t1, t2);
    pw[0] = pnt.X() - coor[0];
    pw[1] = pnt.Y() - coor[1];
    pw[2] = pnt.Z() - coor[2];
    b     = -( pw[0]*t1.X() +  pw[1]*t1.Y() +  pw[2]*t1.Z());
    a     =  (t1.X()*t1.X() + t1.Y()*t1.Y() + t1.Z()*t1.Z()) +
             ( pw[0]*t2.X() +  pw[1]*t2.Y() +  pw[2]*t2.Z());
    if (a == 0.0) break;
    b  /= a;
    if (fabs(b) < 1.e-10*(tmax-tmin)) break;
    *t += b;
  }
  if (*t < tmin) *t = tmin;
  if (*t > tmax) *t = tmax;
  
  hCurve->D0(*t, pnt);
  xyz[0] = pnt.X();
  xyz[1] = pnt.Y();
  xyz[2] = pnt.Z();
}


static int
EG_nearestSurface(Handle_Geom_Surface hSurface, double *range,
                  double *point, double *uv, double *coor)
{
  int    i, j, count;
  gp_Pnt P0;
  gp_Vec V1, V2, U1, U2, UV;
  double a00, a10, a11, b0, b1, det, dist, ldist, dx[3], uvs[2];
  static double ratios[5] = {0.02, 0.25, 0.5, 0.75, 0.98};
  
  // find candidate starting point
  ldist = 1.e308;
  for (j = 0; j < 5; j++) {
    uvs[1] = (1.0-ratios[j])*range[2] + ratios[j]*range[3];
    for (i = 0; i < 5; i++) {
      uvs[0] = (1.0-ratios[i])*range[0] + ratios[i]*range[1];
      hSurface->D0(uv[0], uv[1], P0);
      dx[0] = P0.X() - point[0];
      dx[1] = P0.Y() - point[1];
      dx[2] = P0.Z() - point[2];
      dist  = sqrt(dx[0]*dx[0] + dx[1]*dx[1] + dx[2]*dx[2]);
      if (dist < ldist) {
        ldist = dist;
        uv[0] = uvs[0];
        uv[1] = uvs[1];
      }  
    }
  }

  // newton iteration
  for (count = 0; count < 10; count++) {
    hSurface->D2(uv[0], uv[1], P0, U1, V1, U2, V2, UV);
    dx[0] = P0.X() - point[0];
    dx[1] = P0.Y() - point[1];
    dx[2] = P0.Z() - point[2];
    dist  = sqrt(dx[0]*dx[0] + dx[1]*dx[1] + dx[2]*dx[2]);
    if (dist < Precision::Confusion()) break;
    if (count != 0) {
      if (fabs(dist-ldist) < Precision::Confusion()) break;
      if (dist > ldist) {
        uv[0] = uvs[0];
        uv[1] = uvs[1];
        hSurface->D0(uv[0], uv[1], P0);
        coor[0] = P0.X();
        coor[1] = P0.Y();
        coor[2] = P0.Z();
        return EGADS_EMPTY;
      }
    }

    b0  = -dx[0]*U1.X() -  dx[1]*U1.Y() -  dx[2]*U1.Z();
    b1  = -dx[0]*V1.X() -  dx[1]*V1.Y() -  dx[2]*V1.Z();
    a00 = U1.X()*U1.X() + U1.Y()*U1.Y() + U1.Z()*U1.Z() +
           dx[0]*U2.X() +  dx[1]*U2.Y() +  dx[2]*U2.Z();
    a10 = U1.X()*V1.X() + U1.Y()*V1.Y() + U1.Z()*V1.Z() +
           dx[0]*UV.X() +  dx[1]*UV.Y() +  dx[2]*UV.Z();
    a11 = V1.X()*V1.X() + V1.Y()*V1.Y() + V1.Z()*V1.Z() +
           dx[0]*V2.X() +  dx[1]*V2.Y() +  dx[2]*V2.Z();

    det    = a00*a11 - a10*a10;
    if (det == 0.0) return EGADS_DEGEN;
    det    = 1.0/det;
    uvs[0] = uv[0];
    uvs[1] = uv[1];
    uv[0] += det*(b0*a11 - b1*a10);
    uv[1] += det*(b1*a00 - b0*a10);
    ldist  = dist;
//  printf("   %d: %lf %lf   %le\n", count, uv[0], uv[1], ldist);
  }

  hSurface->D0(uv[0], uv[1], P0);
  coor[0] = P0.X();
  coor[1] = P0.Y();
  coor[2] = P0.Z();
  if (count == 10) return EGADS_EMPTY;

  return EGADS_SUCCESS;
}


int
EG_invEvaluate(const egObject *geom, double *xyz, 
               double *param, double *result)
{
  int           outLevel, stat;
  Standard_Real period, t, u, v, range[4], coor[3];

  if  (geom == NULL)               return EGADS_NULLOBJ;
  if  (geom->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((geom->oclass != PCURVE) &&
      (geom->oclass != CURVE)  && (geom->oclass != SURFACE) &&
      (geom->oclass != EDGE)   && (geom->oclass != FACE))
                                   return EGADS_NOTGEOM;
  if (geom->blind == NULL)         return EGADS_NODATA;
  outLevel = EG_outLevel(geom);
  
  if (geom->oclass == PCURVE) {
  
    // 2D on PCurves
    gp_Pnt2d pnt(xyz[0], xyz[1]);
    egadsPCurve *ppcurv = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    Geom2dAPI_ProjectPointOnCurve projPnt(pnt, hCurve);
    if (projPnt.NbPoints() == 0) {
      if (outLevel > 0)
        printf(" EGADS Warning: No projection on PCurve (EG_invEvaluate)!\n");
      return EGADS_NOTFOUND;
    }
    t = projPnt.LowerDistanceParameter();
    if (hCurve->IsPeriodic()) {
      period   = hCurve->Period();
      range[0] = hCurve->FirstParameter();
      range[1] = hCurve->LastParameter();
      if ((t+PARAMACC < range[0]) || (t-PARAMACC > range[1])) {
        if (period != 0.0)
          if (t+PARAMACC < range[0]) {
            if (t+period-PARAMACC < range[1]) t += period;
          } else {
            if (t-period+PARAMACC > range[0]) t -= period;
          }
        }
      }
    pnt = projPnt.NearestPoint();
    result[0] = pnt.X();
    result[1] = pnt.Y();
    *param    = t;
    return EGADS_SUCCESS;
  }
  
  // make the point
  gp_Pnt pnt(xyz[0], xyz[1], xyz[2]);
  
  if ((geom->oclass == CURVE) || (geom->oclass == EDGE)) {
  
    // 1D -- curves & Edges
    Handle(Geom_Curve) hCurve;
    if (geom->oclass == CURVE) {
      egadsCurve *pcurve = (egadsCurve *) geom->blind;
      hCurve   = pcurve->handle;
      range[0] = hCurve->FirstParameter();
      range[1] = hCurve->LastParameter();
    } else {
      egadsEdge *pedge = (egadsEdge *) geom->blind;
      BRep_Tool::Range(pedge->edge, range[0], range[1]);
      egObject  *curv  = pedge->curve;
      if (curv == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No curve Object for Edge (EG_invEvaluate)!\n");
        return EGADS_NULLOBJ;
      }
      egadsCurve *pcurve = (egadsCurve *) curv->blind;
      if (pcurve == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No curve Data for Edge (EG_invEvaluate)!\n");
        return EGADS_NODATA;
      }
      hCurve = pcurve->handle;
    }

    GeomAPI_ProjectPointOnCurve projPnt(pnt, hCurve);
    if (projPnt.NbPoints() == 0) {
      EG_nearestCurve(hCurve, xyz, range[0], range[1], &t, result);
      pnt.SetX(result[0]);
      pnt.SetY(result[1]);
      pnt.SetZ(result[2]);
    } else {
      pnt = projPnt.NearestPoint();
      t   = projPnt.LowerDistanceParameter();
    }

    if (hCurve->IsPeriodic()) {
      period = hCurve->Period();
      if ((t+PARAMACC < range[0]) || (t-PARAMACC > range[1])) {
        if (period != 0.0)
          if (t+PARAMACC < range[0]) {
            if (t+period-PARAMACC < range[1]) t += period;
          } else {
            if (t-period+PARAMACC > range[0]) t -= period;
          }
      }
    }
    
    /* clip it? */
    if (geom->oclass == EDGE)
      if ((t < range[0]) || (t > range[1])) {
        if (t < range[0]) t = range[0];
        if (t > range[1]) t = range[1];
        hCurve->D0(t, pnt);
      }

    result[0] = pnt.X();
    result[1] = pnt.Y();
    result[2] = pnt.Z();
    *param    = t;

  } else {
  
    // 2D -- surfaces & Faces
    Handle(Geom_Surface) hSurface;
    if (geom->oclass == SURFACE) {
      egadsSurface *psurf = (egadsSurface *) geom->blind;
      hSurface = psurf->handle;
      hSurface->Bounds(range[0],range[1], range[2],range[3]);
    } else {
      egadsFace *pface = (egadsFace *) geom->blind;
      BRepTools::UVBounds(pface->face, range[0],range[1], 
                                       range[2],range[3]);
      egObject  *surf  = pface->surface;
      if (surf == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No Surf Object for Face (EG_invEvaluate)!\n");
        return EGADS_NULLOBJ;
      }
      egadsSurface *psurf = (egadsSurface *) surf->blind;
      if (psurf == NULL) {
        if (outLevel > 0)
          printf(" EGADS Warning: No Surf Data for Face (EG_invEvaluate)!\n");
        return EGADS_NODATA;
      }
      hSurface = psurf->handle;
    }
    
    GeomAPI_ProjectPointOnSurf projPnt(pnt, hSurface);
    if (!projPnt.IsDone()) {
      stat = EG_nearestSurface(hSurface, range, xyz, param, result);
      if (stat == EGADS_DEGEN) {
        if (outLevel > 0)
          printf(" EGADS Warning: Surf Proj Incomplete - DEGEN (EG_invEvaluate)!\n");
        return stat;
      } else if (stat == EGADS_EMPTY) {
        if (outLevel > 1)
          printf(" EGADS Warning: Surf Proj Incomplete (EG_invEvaluate)!\n");
      }
      u  = param[0];
      v  = param[1];
      pnt.SetX(result[0]);
      pnt.SetY(result[1]);
      pnt.SetZ(result[2]);
    } else {
      if (projPnt.NbPoints() == 0) {
        if (outLevel > 0)
          printf(" EGADS Warning: No projection on Surf (EG_invEvaluate)!\n");
        return EGADS_NOTFOUND;
      }
      pnt = projPnt.NearestPoint();
      projPnt.LowerDistanceParameters(u, v);
    }

    if (hSurface->IsUPeriodic()) {
      period = hSurface->UPeriod();
      if ((u+PARAMACC < range[0]) || (u-PARAMACC > range[1])) {
        if (period != 0.0)
          if (u+PARAMACC < range[0]) {
            if (u+period-PARAMACC < range[1]) u += period;
          } else {
            if (u-period+PARAMACC > range[0]) u -= period;
          }
      }
    }
    if (hSurface->IsVPeriodic()) {
      period = hSurface->VPeriod();
      if ((v+PARAMACC < range[2]) || (v-PARAMACC > range[3])) {
        if (period != 0.0)
          if (v+PARAMACC < range[2]) {
            if (v+period-PARAMACC < range[3]) v += period;
          } else {
            if (v-period+PARAMACC > range[2]) v -= period;
          }
      }
    }

    if (geom->oclass == FACE) {
      egadsFace *pface = (egadsFace *) geom->blind;
      Standard_Real tol = BRep_Tool::Tolerance(pface->face);
      gp_Pnt2d pnt2d(u, v);
      TopOpeBRep_PointClassifier pClass;
      pClass.Load(pface->face);
      if (pClass.Classify(pface->face, pnt2d, tol) == TopAbs_OUT) {
        /* find closest clipped point on the edges */
        double dist2 = 1.e308;
        gp_Pnt pnts(xyz[0], xyz[1], xyz[2]);
        gp_Pnt pntt(xyz[0], xyz[1], xyz[2]);
        TopExp_Explorer ExpW;
        for (ExpW.Init(pface->face, TopAbs_WIRE); ExpW.More(); ExpW.Next()) {
          TopoDS_Shape shapw = ExpW.Current();
          TopoDS_Wire  wire  = TopoDS::Wire(shapw);
          BRepTools_WireExplorer ExpWE;
          for (ExpWE.Init(wire); ExpWE.More(); ExpWE.Next()) {
            TopoDS_Shape shape = ExpWE.Current();
            TopoDS_Edge  wedge = TopoDS::Edge(shape);
            if (BRep_Tool::Degenerated(wedge)) continue;
            Standard_Real t1, t2;
            Handle(Geom_Curve) hCurve = BRep_Tool::Curve(wedge, t1, t2);
            GeomAPI_ProjectPointOnCurve projPnt(pnts, hCurve);
            if (projPnt.NbPoints() == 0) {
              EG_nearestCurve(hCurve, xyz, t1, t2, &t, result);
              pnt.SetX(result[0]);
              pnt.SetY(result[1]);
              pnt.SetZ(result[2]);
            } else {
              pnt = projPnt.NearestPoint();
              t   = projPnt.LowerDistanceParameter();
            }
            if ((t < t1) || (t > t2)) {
              if (t < t1) t = t1;
              if (t > t2) t = t2;
              hCurve->D0(t, pnt);
            }
            double d = (pnts.X()-pnt.X())*(pnts.X()-pnt.X()) +
                       (pnts.Y()-pnt.Y())*(pnts.Y()-pnt.Y()) +
                       (pnts.Z()-pnt.Z())*(pnts.Z()-pnt.Z());
            if (d < dist2) {
              pntt  = pnt;
              dist2 = d;
            }
          }
        }
        // project again but with clipped point
        GeomAPI_ProjectPointOnSurf projPnt(pntt, hSurface);
        if (!projPnt.IsDone()) {
          coor[0] = pntt.X();
          coor[1] = pntt.Y();
          coor[2] = pntt.Z();
          stat = EG_nearestSurface(hSurface, range, coor, param, result);
          if (stat == EGADS_DEGEN) {
            if (outLevel > 0)
              printf(" EGADS Warning: Face Proj Incomplete - DEGEN (EG_invEvaluate)!\n");
            return stat;
          } else if (stat == EGADS_EMPTY) {
            if (outLevel > 1)
              printf(" EGADS Warning: Face Proj Incomplete (EG_invEvaluate)!\n");
          }
          u  = param[0];
          v  = param[1];
          pnt.SetX(result[0]);
          pnt.SetY(result[1]);
          pnt.SetZ(result[2]);
        } else {
          if (projPnt.NbPoints() == 0) {
            if (outLevel > 0)
              printf(" EGADS Warning: No projection on Face (EG_invEvaluate)!\n");
            return EGADS_NOTFOUND;
          }
          pnt = projPnt.NearestPoint();
          projPnt.LowerDistanceParameters(u, v);
        }

        if (hSurface->IsUPeriodic()) {
          period = hSurface->UPeriod();
          if ((u+PARAMACC < range[0]) || (u-PARAMACC > range[1])) {
            if (period != 0.0)
              if (u+PARAMACC < range[0]) {
                if (u+period-PARAMACC < range[1]) u += period;
              } else {
                if (u-period+PARAMACC > range[0]) u -= period;
              }
          }
        }
        if (hSurface->IsVPeriodic()) {
          period = hSurface->VPeriod();
          if ((v+PARAMACC < range[2]) || (v-PARAMACC > range[3])) {
            if (period != 0.0)
              if (v+PARAMACC < range[2]) {
                if (v+period-PARAMACC < range[3]) v += period;
              } else {
                if (v-period+PARAMACC > range[2]) v -= period;
              }
          }
        }
             
      }
    }

    result[0] = pnt.X();
    result[1] = pnt.Y();
    result[2] = pnt.Z();
    param[0]  = u;
    param[1]  = v;

  }
  
  return EGADS_SUCCESS;
}


int  
EG_approximate(egObject *context, int maxdeg, double tol, const int *sizes, 
               const double *data, egObject **bspline)
{
  int      outLevel, stat, len = 0;
  egObject *obj;

  *bspline = NULL;
  if (context == NULL)               return EGADS_NULLOBJ;
  if (context->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (context->oclass != CONTXT)     return EGADS_NOTCNTX;
  outLevel = EG_outLevel(context);
  
  if ((maxdeg < 3) || (maxdeg > 8)) {
    if (outLevel > 0)
      printf(" EGADS Warning: maxDeg = %d (EG_approximate)!\n", maxdeg);
    return EGADS_RANGERR;
  }

  if (sizes[1] == 0) {
  
    // curve
    Handle(Geom_BSplineCurve) hCurve;
    try {
      TColgp_Array1OfPnt aPnts(1, sizes[0]);
      for (int i = 1; i <= sizes[0]; i++, len+=3)
        aPnts(i) = gp_Pnt(data[len], data[len+1], data[len+2]);
      hCurve = GeomAPI_PointsToBSpline(aPnts, 3, maxdeg, GeomAbs_C2, 
                                       tol).Curve();
    }
    catch (Standard_Failure)
    {
      if (outLevel > 0) {
        printf(" EGADS Warning: Internal Error (EG_approximate)!\n");
        Handle_Standard_Failure e = Standard_Failure::Caught();
        printf("                %s\n", e->GetMessageString());
      }
      return EGADS_GEOMERR;
    }
    catch (...)
    {
      if (outLevel > 0)
        printf(" EGADS Warning: Internal Error (EG_approximate)!\n");
      return EGADS_GEOMERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: make Curve = %d (EG_approximate)!\n",
               stat);
      return stat;
    }
    obj->oclass        = CURVE;
    obj->mtype         = BSPLINE;
    egadsCurve *pcurve = new egadsCurve;
    pcurve->handle     = hCurve;
    pcurve->basis      = NULL;
    pcurve->topFlg     = 0;
    obj->blind         = pcurve;
    EG_referenceObject(obj, context);

  } else {
  
    // surface
    Handle(Geom_BSplineSurface) hSurf;
    try {    
      TColgp_Array2OfPnt aPnts(1, sizes[0], 1, sizes[1]);
      for (int j = 1; j <= sizes[1]; j++)
        for (int i = 1; i <= sizes[0]; i++, len+=3)
          aPnts(i, j) = gp_Pnt(data[len], data[len+1], data[len+2]);
      if (tol != 0.0) {
        hSurf = GeomAPI_PointsToBSplineSurface(aPnts, 3, maxdeg, GeomAbs_C2, 
                                               tol).Surface();
      } else {
        GeomAPI_PointsToBSplineSurface P2BSpl;
        P2BSpl.Interpolate(aPnts);
        hSurf = P2BSpl.Surface();
      }
    }
    catch (Standard_Failure)
    {
      if (outLevel > 0) {
        printf(" EGADS Warning: Internal Error (EG_approximate)!\n");
        Handle_Standard_Failure e = Standard_Failure::Caught();
        printf("                %s\n", e->GetMessageString());
      }
      return EGADS_GEOMERR;
    }
    catch (...)
    {
      if (outLevel > 0)
        printf(" EGADS Warning: Internal Error (EG_approximate)!\n");
      return EGADS_GEOMERR;
    }

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: make Surface = %d (EG_approximate)!\n",
               stat);
      return stat;
    }
    obj->oclass         = SURFACE;
    obj->mtype          = BSPLINE;
    egadsSurface *psurf = new egadsSurface;
    psurf->handle       = hSurf;
    psurf->basis        = NULL;
    psurf->topFlg       = 0;
    obj->blind          = psurf;
    EG_referenceObject(obj, context);

  }
  
  *bspline = obj;
  return EGADS_SUCCESS;
}


int
EG_otherCurve(const egObject *surface, const egObject *curve, 
              double tol, egObject **newcurve)
{
  int      outLevel, stat;
  egObject *context, *obj;

  *newcurve = NULL;
  if (surface == NULL)               return EGADS_NULLOBJ;
  if (surface->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (surface->oclass != SURFACE)    return EGADS_NOTGEOM;
  if (surface->blind == NULL)        return EGADS_NODATA;
  outLevel = EG_outLevel(surface);
  context  = EG_context(surface);

  if (curve == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Input Curve (EG_otherCurve)!\n"); 
    return EGADS_NULLOBJ;
  }
  if ((curve->oclass != PCURVE) && (curve->oclass != CURVE) &&
      (curve->oclass != EDGE)) {
    if (outLevel > 0)
      printf(" EGADS Error: Not a PCurve/Curve or Edge (EG_otherCurve)!\n"); 
    return EGADS_NOTGEOM;
  }
  if (curve->blind == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: PCurve has no data (EG_otherCurve)!\n"); 
    return EGADS_NODATA;
  }
  if (EG_context(curve) != context) {
    if (outLevel > 0)
      printf(" EGADS Error: Context mismatch (EG_otherCurve)!\n");
    return EGADS_MIXCNTX;
  }

  egadsSurface *psurf           = (egadsSurface *) surface->blind;
  Handle(Geom_Surface) hSurface = psurf->handle;
  Standard_Real prec            = tol;
  if (prec < Precision::Confusion()) prec = Precision::Confusion();
  
  if (curve->oclass == PCURVE) {
  
    Standard_Real maxDev, aveDev;
  
    egadsPCurve *ppcurv         = (egadsPCurve *) curve->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    GeomAdaptor_Surface  aGAS   = hSurface;
    Handle(GeomAdaptor_HSurface) aHGAS = new GeomAdaptor_HSurface(aGAS);
    Handle(Geom2dAdaptor_HCurve) Crv   = new Geom2dAdaptor_HCurve(hCurve);
    Adaptor3d_CurveOnSurface ConS(Crv,aHGAS);
    
    Handle(Geom_Curve) newcrv;
    GeomLib::BuildCurve3d(prec, ConS, hCurve->FirstParameter(),
                          hCurve->LastParameter(), newcrv, maxDev, aveDev);

    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Curve = %d (EG_otherCurve)!\n", stat);
      return stat;
    }
    EG_completeCurve(obj, newcrv);

  } else {
  
    Handle(Geom2d_Curve) newcrv;

    if (curve->oclass == EDGE) {
    
      Standard_Real t1, t2;

      egadsEdge *pedge = (egadsEdge *) curve->blind;
      egObject  *geom  = pedge->curve;
      if (geom->blind == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: NULL Curve Data (EG_otherCurve)!\n");
        return EGADS_NODATA;
      }
      Handle(Geom_Curve) hCurve = BRep_Tool::Curve(pedge->edge, t1, t2);
      newcrv = GeomProjLib::Curve2d(hCurve, t1, t2, hSurface, prec);

    } else {
    
      egadsCurve *pcurve        = (egadsCurve *) curve->blind;
      Handle(Geom_Curve) hCurve = pcurve->handle;
      newcrv = GeomProjLib::Curve2d(hCurve, hCurve->FirstParameter(),
                                    hCurve->LastParameter(), hSurface, prec);

    }

    if (EG_getPCurveType(newcrv) == 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Cannot construct PCurve (EG_otherCurve)!\n");
      return EGADS_CONSTERR;
    }
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make PCurve = %d (EG_otherCurve)!\n", stat);
      return stat;
    }
    EG_completePCurve(obj, newcrv);
  }
  
  EG_referenceObject(obj, context);
  *newcurve = obj;
  return EGADS_SUCCESS;
}


int
EG_isoCline(const egObject *surface, int UV, double value, 
                  egObject **newcurve)
{
  int      stat;
  egObject *context, *obj;

  *newcurve = NULL;
  if (surface == NULL)               return EGADS_NULLOBJ;
  if (surface->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (surface->oclass != SURFACE)    return EGADS_NOTGEOM;
  if (surface->blind == NULL)        return EGADS_NODATA;
  context = EG_context(surface);

  egadsSurface *psurf           = (egadsSurface *) surface->blind;
  Handle(Geom_Surface) hSurface = psurf->handle;
  Handle(Geom_Curve)   newcrv;
  if (UV == UISO) {
    newcrv = hSurface->UIso(value);
  } else {
    newcrv = hSurface->VIso(value);
  }

  stat = EG_makeObject(context, &obj);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: make Curve = %d (EG_otherCurve)!\n", stat);
    return stat;
  }
  EG_completeCurve(obj, newcrv);
  EG_referenceObject(obj, context);
  *newcurve = obj;

  return EGADS_SUCCESS;
}


int
EG_convertToBSpline(egObject *object, egObject **bspline) 
{
  int           outLevel, stat;
  egObject      *obj, *geom, *context;
  Standard_Real range[4];

  if  (object == NULL)               return EGADS_NULLOBJ;
  if  (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((object->oclass != PCURVE) &&
      (object->oclass != CURVE)  && (object->oclass != SURFACE) &&
      (object->oclass != EDGE)   && (object->oclass != FACE))
                                     return EGADS_NOTGEOM;
  if (object->blind == NULL)         return EGADS_NODATA;
  outLevel = EG_outLevel(object);
  context  = EG_context(object);
  geom     = object;
  
  if (object->oclass == EDGE) {
    egadsEdge *pedge = (egadsEdge *) object->blind;
    geom = pedge->curve;
    if (geom->blind == NULL) return EGADS_NODATA;
  }
  if (object->oclass == FACE) {
    egadsFace *pface = (egadsFace *) object->blind;
    geom = pface->surface;
    if (geom->blind == NULL) return EGADS_NODATA;
  }
  if (geom->mtype == BSPLINE) {
    *bspline = geom;
    return EGADS_SUCCESS;
  }
  
  if (geom->oclass == PCURVE) {
  
    egadsPCurve *ppcurv         = (egadsPCurve *) geom->blind;
    Handle(Geom2d_Curve) hCurve = ppcurv->handle;
    range[0] = hCurve->FirstParameter();
    range[1] = hCurve->LastParameter();
    ShapeConstruct_Curve ShapeCC;
    Handle(Geom2d_BSplineCurve)
      hBSpline = ShapeCC.ConvertToBSpline(hCurve, range[0], range[1],
                                          Precision::Confusion());
    if (hBSpline.IsNull()) {
      if (outLevel > 0)
        printf(" EGADS Warning: Failure to convert (EG_convertToBSpline)!\n");
      return EGADS_GEOMERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make PCurve = %d (EG_convertToBSpline)!\n", 
             stat);
      return stat;
    }
    obj->oclass        = PCURVE;
    obj->mtype         = BSPLINE;
    egadsPCurve *ppcrv = new egadsPCurve;
    ppcrv->handle      = hBSpline;
    ppcrv->basis       = NULL;
    ppcrv->topFlg      = 0;
    obj->blind         = ppcrv;

  } else if (geom->oclass == CURVE) {
  
    egadsCurve *pcurve        = (egadsCurve *) geom->blind;
    Handle(Geom_Curve) hCurve = pcurve->handle;
    range[0] = hCurve->FirstParameter();
    range[1] = hCurve->LastParameter();
    ShapeConstruct_Curve ShapeCC;
    Handle(Geom_BSplineCurve)
      hBSpline = ShapeCC.ConvertToBSpline(hCurve, range[0], range[1],
                                          Precision::Confusion());
    if (hBSpline.IsNull()) {
      if (outLevel > 0)
        printf(" EGADS Warning: Failure to convert (EG_convertToBSpline)!\n");
      return EGADS_GEOMERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Curve = %d (EG_convertToBSpline)!\n", 
             stat);
      return stat;
    }
    obj->oclass       = CURVE;
    obj->mtype        = BSPLINE;
    egadsCurve *pcurv = new egadsCurve;
    pcurv->handle     = hBSpline;
    pcurv->basis      = NULL;
    pcurv->topFlg     = 0;
    obj->blind        = pcurv;

  } else {
  
    egadsSurface *psurface        = (egadsSurface *) geom->blind;
    Handle(Geom_Surface) hSurface = psurface->handle;
    hSurface->Bounds(range[0],range[1], range[2],range[3]);
    Handle(Geom_BSplineSurface)
      hBSpline = ShapeConstruct::ConvertSurfaceToBSpline(hSurface,
                                 range[0], range[1], range[2], range[3],
                                 Precision::Confusion(), GeomAbs_C2, 100, 20);
    if (hBSpline.IsNull()) {
      if (outLevel > 0)
        printf(" EGADS Warning: Failure to Convert (EG_convertToBSpline)!\n");
      return EGADS_GEOMERR;
    }
    
    stat = EG_makeObject(context, &obj);
    if (stat != EGADS_SUCCESS) {
      printf(" EGADS Error: make Surface = %d (EG_convertToBSpline)!\n", 
             stat);
      return stat;
    }
    obj->oclass         = SURFACE;
    obj->mtype          = BSPLINE;
    egadsSurface *psurf = new egadsSurface;
    psurf->handle       = hBSpline;
    psurf->basis        = NULL;
    psurf->topFlg       = 0;
    obj->blind          = psurf;

  }
  
  *bspline = obj;
  EG_referenceObject(obj, context);
    
  return EGADS_SUCCESS;
}
