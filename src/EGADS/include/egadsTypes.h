#ifndef EGADSTYPES_H
#define EGADSTYPES_H
/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             General Object Header
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include "egadsErrors.h"


#define EGADSMAJOR     1
#define EGADSMINOR    00
#define EGADSPROP     EGADSprop: Revision 1.00

#define MAGIC      98789

/* OBJECT CLASSES */

#define CONTXT         0
#define TRANSFORM      1
#define TESSELLATION   2
#define NIL            3        /* allocated but not assigned */
#define EMPTY          4        /* in the pool */
#define REFERENCE      5
#define PCURVE        10
#define CURVE         11
#define SURFACE       12
#define NODE          20
#define EDGE          21
#define LOOP          22
#define FACE          23
#define SHELL         24
#define BODY          25
#define MODEL         26


/* MEMBER TYPES */

  /* PCURVES & CURVES */
#define LINE           1
#define CIRCLE         2
#define ELLIPSE        3
#define PARABOLA       4
#define HYPERBOLA      5
#define TRIMMED        6
#define BEZIER         7
#define BSPLINE        8
#define OFFSET         9

  /* SURFACES */
#define PLANE          1
#define SPHERICAL      2
#define CYLINDRICAL    3
#define REVOLUTION     4 
#define TOROIDAL       5
#define CONICAL       10
#define EXTRUSION     11

  /* TOPOLOGY */
#define SREVERSE      -1
#define NOMTYPE        0
#define SFORWARD       1
#define ONENODE        1
#define TWONODE        2
#define OPEN           3
#define CLOSED         4
#define DEGENERATE     5
#define WIREBODY       6
#define FACEBODY       7
#define SHEETBODY      8
#define SOLIDBODY      9


/* ATTRIBUTE TYPES */

#define ATTRINT        1
#define ATTRREAL       2
#define ATTRSTRING     3


/* SOLID BOOLEAN OPERATIONS */

#define SUBTRACTION    1
#define INTERSECTION   2
#define FUSION         3


/* SOLID BODY TYPES */

#define BOX      1
#define SPHERE   2
#define CONE     3
#define CYLINDER 4
#define TORUS    5


/* ISOCLINE TYPES */

#define UISO	0
#define VISO    1


typedef struct {
  char     *name;               /* Attribute Name */
  int      type;                /* Attribute Type */
  int      length;              /* number of values */
  union {
    int    integer;             /* single int -- length == 1 */
    int    *integers;           /* multiple ints */
    double real;                /* single double -- length == 1 */
    double *reals;              /* mutiple doubles */
    char   *string;             /* character string (no single char) */
  } vals;
} egAttr;


typedef struct {
  int     nattrs;               /* number of attributes */
  egAttr *attrs;                /* the attributes */
} egAttrs;


typedef struct egObject {
  int     magicnumber;          /* must be set to validate the object */
  short   oclass;               /* object Class */
  short   mtype;                /* member Type */
  void   *attrs;                /* object Attributes or Reference*/
  void   *blind;		/* blind pointer to object data */
  struct egObject *topObj;      /* top of the hierarchy or context (if top) */
  struct egObject *tref;        /* threaded list of references */
  struct egObject *prev;        /* back pointer */
  struct egObject *next;        /* forward pointer */
} egObject;
typedef struct egObject* ego;


typedef struct {
  int      outLevel;		/* output level for messages
                                   0 none, 1 minimal, 2 verbose, 3 debug */
  char     **signature;
  egObject *pool;               /* available object structures for use */
  egObject *last;               /* the last object in the list */
} egCntxt;


typedef struct {
  int index;                    /* face index or last for more than 1 */
  int nface;                    /* number of faces (when more than 1) */
  int *faces;                   /* the face indices when multiples */
  int *tric;                    /* connection into tris for each face */
} egFconn;


typedef struct {
  egObject *obj;                /* edge object */
  int      nodes[2];            /* node indices */
  egFconn  faces[2];            /* minus and plus face connectivity */
  double   *xyz;                /* point coordinates */
  double   *t;                  /* parameter values */
  int      npts;                /* number of points */
} egTess1D;


typedef struct {
  int *ipts;                    /* index for point (nu*nv) */
  int *bounds;                  /* bound index (2*nu + 2*nv) */
  int nu;
  int nv;
} egPatch;


typedef struct {
  double  *xyz;                 /* point coordinates */
  double  *uv;                  /* parameter values */
  int     *ptype;               /* point type */
  int     *pindex;              /* point index */
  int     *tris;                /* triangle indices */
  int     *tric;                /* triangle neighbors */
  egPatch *patch;               /* patches */
  int     npts;                 /* number of points */
  int     ntris;                /* number of tris */
  int     npatch;               /* number of quad patches */
} egTess2D;


typedef struct {
  egObject *src;                /* source of the tessellation */
  double   *xyzs;               /* storage for geom */
  egTess1D *tess1d;             /* Edge tessellations */
  egTess2D *tess2d;             /* Face tessellations (tris then quads) */
  double   params[6];           /* suite of parameters used */
  int      nEdge;               /* number of Edge tessellations */
  int      nFace;               /* number of Face tessellations */
  int      nu;                  /* number of us for surface / ts for curve */
  int      nv;                  /* number of vs for surface tessellation */
} egTessel;

#endif
