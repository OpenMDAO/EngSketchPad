/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Tessellation Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>	/* Needed in some systems for DBL_MAX definition */
#include <limits.h>	/* Needed in other systems for DBL_EPSILON */

#include "egadsTypes.h"
#include "egadsInternals.h"
#include "egadsTris.h"


#define NOTFILLED	-1
#define TOL		 1.e-7
#define PI               3.14159265358979324
#define MAXELEN          1024                   /* max Edge length */


#define AREA2D(a,b,c)   ((a[0]-c[0])*(b[1]-c[1]) - (a[1]-c[1])*(b[0]-c[0]))
#define CROSS(a,b,c)      a[0] = (b[1]*c[2]) - (b[2]*c[1]);\
                          a[1] = (b[2]*c[0]) - (b[0]*c[2]);\
                          a[2] = (b[0]*c[1]) - (b[1]*c[0])
#define DOT(a,b)         (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define DIST2(a,b)      ((a[0]-b[0])*(a[0]-b[0]) + (a[1]-b[1])*(a[1]-b[1]))
#define DOT2(a,b)       (((a)[0]*(b)[0]) + ((a)[1]*(b)[1]))
#define VSUB2(a,b,c)    (c)[0] = (a)[0] - (b)[0]; (c)[1] = (a)[1] - (b)[1];
#define MAX(a,b)        (((a) > (b)) ?  (a) : (b))
#define MIN(a,b)        (((a) < (b)) ?  (a) : (b))
#define ABS(a)          (((a) <   0) ? -(a) : (a))


  /* structures */

  typedef struct {
    int   sleft;		/* left  segment in front */
    int   i0;                   /* left  vertex index */
    int   i1;                   /* right vertex index */
    int   sright;		/* right segment in front */
    short snew;                 /* is this a new segment? */
    short mark;                 /* is this segment marked? */
  } Front;
  
  typedef struct {
    int    mfront;
    int    nfront;
    int    mpts;
    int    npts;
    int   *pts;
    int    nsegs;
    int   *segs;
    Front *front;
  } fillArea;

  typedef struct {
    int node1;                  /* 1nd node number for edge */
    int node2;                  /* 2nd node number for edge */
    int *tri;                   /* 1st triangle storage or NULL for match */
    int thread;                 /* thread to next face with 1st node number */
  } connect;


  extern int EG_getTolerance( const egObject *topo, double *tol );
  extern int EG_getBodyTopos( const egObject *body, /*@null@*/ egObject *src,
                                   int oclass, int *ntopo, egObject ***topos );
  extern int EG_indexBodyTopo( const egObject *body, const egObject *src );
  extern int EG_getEdgeUV( const egObject *face, const egObject *edge,
                           int sense, double t, double *result );
  extern int EG_getGeometry( const egObject *geom, int *oclass, int *type, 
                             egObject **rGeom, int **ivec, double **rvec );
  extern int EG_evaluate( const egObject *geom, const double *param, 
                          double *result );
  extern int EG_invEvaluate( const egObject *geom, double *xyz, double *param, 
                             double *result );
                                 
  extern int EG_tessellate( int outLevel, triStruct *ts );
  extern int EG_quadFill( const egObject *face, double *parms, int *elens, 
                          double *uv, int *npts, double **uvs, int *npat, 
                          int *pats, int **vpats );



static int
EG_faceConnIndex(egFconn conn, int face)
{
  int i;

  if (conn.nface == 1) {
    if (conn.index == face) return 1;
  } else {
    for (i = 0; i < conn.nface; i++)
      if (conn.faces[i] == face) return i+1;
  }
    
  return 0;
}


#ifdef CHECK
static void
EG_checkTriangulation(egTessel *btess)
{
  int i, j, k, n, n1, n2, nf, iface, itri, ie, iv, side;
  static int sides[3][2] = {1,2, 2,0, 0,1};

  for (iface = 1; iface <= btess->nFace; iface++) {
    for (itri = 1; itri <= btess->tess2d[iface-1].ntris; itri++) {
      for (j = 0; j < 3; j++) {
        if ((btess->tess2d[iface-1].tris[3*itri+j-3] >
             btess->tess2d[iface-1].npts) ||
            (btess->tess2d[iface-1].tris[3*itri+j-3] <= 0))
          printf(" checkTriangulation: Face %d, Tri %d[%d] = %d!\n",
                 iface, itri, j, btess->tess2d[iface-1].tris[3*itri+j-3]);
        n = btess->tess2d[iface-1].tric[3*itri+j-3];
        if (n > btess->tess2d[iface-1].ntris) {
          printf(" checkTriangulation: Face %d, Nei %d[%d] = %d (%d)!\n",
                 iface, itri, j, n, btess->tess2d[iface-1].ntris);
        } else if (n == 0) {
          printf(" checkTriangulation: Face %d, No Neighbor %d[%d]\n",
                 iface, itri, j);          
        } else if (n > 0) {
          side = -1;
          if (btess->tess2d[iface-1].tric[3*n-3] == itri) side = 0;
          if (btess->tess2d[iface-1].tric[3*n-2] == itri) side = 1;
          if (btess->tess2d[iface-1].tric[3*n-1] == itri) side = 2;
          if (side == -1) {
            printf(" checkTriangulation: Face %d, Tri Nei %d[%d] = %d!\n",
                   iface, itri, j, n);
            printf("                             Tri Nei %d[0] = %d\n",
                    n, btess->tess2d[iface-1].tric[3*n-3]);
            printf("                             Tri Nei %d[1] = %d\n",
                    n, btess->tess2d[iface-1].tric[3*n-2]);
            printf("                             Tri Nei %d[2] = %d\n",
                    n, btess->tess2d[iface-1].tric[3*n-1]);
          } else {
            n1 = btess->tess2d[iface-1].tris[3*itri+sides[j][0]-3];
            n2 = btess->tess2d[iface-1].tris[3*itri+sides[j][1]-3];
            if (((n1 != btess->tess2d[iface-1].tris[3*n+sides[side][0]-3]) ||
                 (n2 != btess->tess2d[iface-1].tris[3*n+sides[side][1]-3])) &&
                ((n1 != btess->tess2d[iface-1].tris[3*n+sides[side][1]-3]) ||
                 (n2 != btess->tess2d[iface-1].tris[3*n+sides[side][0]-3]))) {
              printf(" checkTriangulation: Face %d, Tri Nei %d[%d] = %d!\n",
                     iface, itri, j, n);
              printf("                             verts = %d %d, %d %d\n", 
                     n1, n2, btess->tess2d[iface-1].tris[3*n+sides[side][0]-3],
                             btess->tess2d[iface-1].tris[3*n+sides[side][1]-3]);
            }
          }
        } else {
          n1 = btess->tess2d[iface-1].tris[3*itri+sides[j][0]-3];
          n2 = btess->tess2d[iface-1].tris[3*itri+sides[j][1]-3];
          ie = -n;
          iv =  0;
          if (btess->tess2d[iface-1].ptype[n1-1] == -1) {
            printf(" checkTriangulation: Face %d, Tri Nei1 %d[%d] Interior Vert!\n",
                   iface, itri, j);            
          } else if (btess->tess2d[iface-1].ptype[n1-1] > 0) {
            if (btess->tess2d[iface-1].pindex[n1-1] != ie) {
              printf(" checkTriangulation: Face %d, Tri Nei1 %d[%d] Edge %d %d!\n",
                     iface, itri, j, ie, btess->tess2d[iface-1].pindex[n1-1]);
            } else {
              iv = btess->tess2d[iface-1].ptype[n1-1];
            }
          }
          if (btess->tess2d[iface-1].ptype[n2-1] == -1) {
            printf(" checkTriangulation: Face %d, Tri Nei2 %d[%d] Interior Vert!\n",
                   iface, itri, j);
            iv = 0;
          } else if (btess->tess2d[iface-1].ptype[n2-1] > 0) {
            if (btess->tess2d[iface-1].pindex[n2-1] != ie) {
              printf(" checkTriangulation: Face %d, Tri Nei2 %d[%d] Edge %d %d!\n",
                     iface, itri, j, ie, btess->tess2d[iface-1].pindex[n2-1]);
              iv = 0;
            } else {
              if ((iv != 0) && (iv > btess->tess2d[iface-1].ptype[n2-1]))
                iv = btess->tess2d[iface-1].ptype[n2-1];
            }
          } else {
            iv = 0;
          }
          if ((ie < 1) || (ie > btess->nEdge)) {
            printf(" checkTriangulation: Face %d, Tri Nei %d[%d] = %d (%d)!\n",
                   iface, itri, j, ie, btess->nEdge);
          } else {
            if (iv == 0) {
              for (i = 0; i <  btess->tess1d[ie-1].npts-1; 
                          i += btess->tess1d[ie-1].npts-2) {
                nf = btess->tess1d[ie-1].faces[0].nface;
                if (nf > 0) {
                  k = EG_faceConnIndex(btess->tess1d[ie-1].faces[0], iface);
                  if (k != 0)
                    if (btess->tess1d[ie-1].faces[0].tric[i*nf+k-1] == itri) break;
                }
                nf = btess->tess1d[ie-1].faces[1].nface;
                if (nf > 0) {
                  k = EG_faceConnIndex(btess->tess1d[ie-1].faces[1], iface);
                  if (k != 0) 
                    if (btess->tess1d[ie-1].faces[1].tric[i*nf+k-1] == itri) break;
                }
              }
              if (i > btess->tess1d[ie-1].npts-1)
                printf(" checkTriangulation: Face %d, Tri Nei %d[%d] Not Found in %d!\n",
                       iface, itri, j, ie);
            } else {
              i  = 0;
              nf = btess->tess1d[ie-1].faces[0].nface;
              if (nf > 0) {
                k = EG_faceConnIndex(btess->tess1d[ie-1].faces[0], iface);
                if (k != 0)
                  if (btess->tess1d[ie-1].faces[0].tric[(iv-1)*nf+k-1] == itri) i++;
              }
              nf = btess->tess1d[ie-1].faces[1].nface;
              if (nf > 0) {
                k = EG_faceConnIndex(btess->tess1d[ie-1].faces[1], iface);
                if (k != 0)
                  if (btess->tess1d[ie-1].faces[1].tric[(iv-1)*nf+k-1] == itri) i++;
              }
              if (i == 0) {
                printf(" checkTriangulation: Face %d, Tri Nei %d[%d] Edge %d =",
                       iface, itri, j, ie);
                nf = btess->tess1d[ie-1].faces[0].nface;
                k  = EG_faceConnIndex(btess->tess1d[ie-1].faces[0], iface);
                if (k != 0)
                  printf(" %d", btess->tess1d[ie-1].faces[0].tric[(iv-1)*nf+k-1]);
                nf = btess->tess1d[ie-1].faces[1].nface;
                k  = EG_faceConnIndex(btess->tess1d[ie-1].faces[1], iface);
                if (k != 0)                
                  printf(" %d", btess->tess1d[ie-1].faces[1].tric[(iv-1)*nf+k-1]);
                printf("!\n");
              }
            }
          }
        }
      }
    }
  }
}
#endif


/* 
 * determine if this line segment crosses any active segments 
 * pass:      0 - first pass; conservative algorithm
 * 	      1 - second pass; use dirty tricks
 */

static int
EG_crossSeg(int index, const double *mid, int i2, const double *vertices, 
            int pass, fillArea *fa)
{
  int    i, i0, i1, iF0, iF1;
  double angle, cosan, sinan, dist2, distF, eps, ty0, ty1, frac;
  double uv0[2], uv1[2], uv2[2], x[2];
  double uvF0[2], uvF1[2];

  uv2[0] = vertices[2*i2  ];
  uv2[1] = vertices[2*i2+1];

  /*  Store away coordinates of front */
  iF0 	  = fa->front[index].i0;
  iF1 	  = fa->front[index].i1;
  uvF0[0] = vertices[2*iF0  ];
  uvF0[1] = vertices[2*iF0+1];
  uvF1[0] = vertices[2*iF1  ];
  uvF1[1] = vertices[2*iF1+1];

  dist2  = DIST2( mid,  uv2);
  distF  = DIST2(uvF0, uvF1);
  eps    = (dist2 + distF) * DBL_EPSILON;

  /* transform so that we are in mid-uv2 coordinate frame */
  angle = atan2(uv2[1]-mid[1], uv2[0]-mid[0]);
  cosan = cos(angle);
  sinan = sin(angle);

  /* look at the current front */

  for (i = 0; i < fa->nfront; i++) {
    if ((i == index) || (fa->front[i].sright == NOTFILLED)) continue;
    if (fa->front[i].snew == 0) continue;
    i0 = fa->front[i].i0;
    i1 = fa->front[i].i1;
    if ((i0 == i2) || (i1 == i2)) continue;
    uv0[0] = vertices[2*i0  ];
    uv0[1] = vertices[2*i0+1];
    uv1[0] = vertices[2*i1  ];
    uv1[1] = vertices[2*i1+1];

    /* look to see if the transformed y's from uv2-mid cross 0.0 */
    ty0 = (uv0[1]-mid[1])*cosan - (uv0[0]-mid[0])*sinan;
    ty1 = (uv1[1]-mid[1])*cosan - (uv1[0]-mid[0])*sinan;
    if ((ty0 == 0.0) && (ty1 == 0.0)) return 1;
    if  (ty0*ty1 >= 0.0) continue;

    /* get fraction of line for crossing */
    frac = -ty0/(ty1-ty0);
    if ((frac < 0.0) || (frac > 1.0)) continue;

    /* get the actual coordinates */
    x[0] = uv0[0] + frac*(uv1[0]-uv0[0]);
    x[1] = uv0[1] + frac*(uv1[1]-uv0[1]);

    /* are we in the range for the line seg? */
    frac = (x[0]-mid[0])*cosan + (x[1]-mid[1])*sinan;
    if ((frac > 0.0) && (frac*frac < dist2*(1.0+TOL))) return 2;
  }

  /* look at our original loops */

  for (i = 0; i < fa->nsegs; i++) {
    double area10, area01, area11, area00;

    i0 = fa->segs[2*i  ];
    i1 = fa->segs[2*i+1];

    if (((i0 == fa->front[index].i0) && (i1 == fa->front[index].i1)) ||
        ((i0 == fa->front[index].i1) && (i1 == fa->front[index].i0))) 
      continue;

    uv0[0] = vertices[2*i0  ];
    uv0[1] = vertices[2*i0+1];
    uv1[0] = vertices[2*i1  ];
    uv1[1] = vertices[2*i1+1];

    if (pass != 0) {
      area10 = AREA2D(uv2, uv1, uvF0);
      area00 = AREA2D(uv2, uv0, uvF0);
      area10 = ABS(area10);
      area00 = ABS(area00);
    }
    if ((pass != 0) && area10 < eps && area00 < eps) {
      /* I2 and Boundary Segment are collinear with IF0 (Front.I0) */
      double del0[2], del1[2], del2[2];

      VSUB2(uv2, uvF0, del2);
      VSUB2(uv1, uvF0, del1);
      VSUB2(uv0, uvF0, del0);
      /*  See if I1 is between IF0 and I2 */
      if (i1 != iF0 && DOT2(del2, del1) > 0 &&
	  DOT2(del2, del2) > DOT2(del1, del1)) return 5;
      /*  See if I0 is between IF0 and I2 */
      if (i0 != iF0 && DOT2(del2, del0) > 0 && 
	  DOT2(del2, del2) > DOT2(del0, del0)) return 6;
    }
    if (pass != 0) {
      area11 = AREA2D(uv2, uv1, uvF1);
      area01 = AREA2D(uv2, uv0, uvF1);
      area11 = ABS(area11);
      area01 = ABS(area01);
    }
    if ((pass != 0) && area11 < eps && area01 < eps) {
      /* I2 and Boundary Segment are collinear with IF1 (Front.I1) */
      double del0[2], del1[2], del2[2];

      VSUB2(uv2, uvF1, del2);
      VSUB2(uv1, uvF1, del1);
      VSUB2(uv0, uvF1, del0);
      /*  See if I1 is between IF1 and I2 */
      if (i1 != iF1 && DOT2(del2, del1) > 0 &&
	  DOT2(del2, del2) > DOT2(del1, del1)) return 7;
      /*  See if I0 is between IF1 and I2 */
      if (i0 != iF1 && DOT2(del2, del0) > 0 && 
	  DOT2(del2, del2) > DOT2(del0, del0)) return 8;
    }

    if ((i1 == i2) || (i0 == i2)) continue;
    /* look to see if the transformed y's from uv2-mid cross 0.0 */
    ty0 = (uv0[1]-mid[1])*cosan - (uv0[0]-mid[0])*sinan;
    ty1 = (uv1[1]-mid[1])*cosan - (uv1[0]-mid[0])*sinan;
    if ((ty0 == 0.0) && (ty1 == 0.0)) return 3;
    if  (ty0*ty1 >= 0.0) continue;

    /* get fraction of line for crossing */
    frac = -ty0/(ty1-ty0);
    if ((frac < 0.0) || (frac > 1.0)) continue;

    /* get the actual coordinates */
    x[0] = uv0[0] + frac*(uv1[0]-uv0[0]);
    x[1] = uv0[1] + frac*(uv1[1]-uv0[1]);

    /* are we in the range for the line seg? */
    frac = (x[0]-mid[0])*cosan + (x[1]-mid[1])*sinan;
    if ((frac > 0.0) && (frac*frac < dist2*(1.0+TOL))) return 4;
  }

  return 0;
}


/* Input specified as contours.
 * Outer contour must be counterclockwise.
 * All inner contours must be clockwise.
 *  
 * Every contour is specified by giving all its points in order. No
 * point shoud be repeated. i.e. if the outer contour is a square,
 * only the four distinct endpoints should be specified in order.
 *  
 * ncontours: #contours
 * cntr: An array describing the number of points in each
 *	 contour. Thus, cntr[i] = #points in the i'th contour.
 * vertices: Input array of vertices. Vertices for each contour
 *           immediately follow those for previous one. Array location
 *           vertices[0] must NOT be used (i.e. i/p starts from
 *           vertices[1] instead. The output triangles are
 *	     specified  WRT the indices of these vertices.
 * triangles: Output array to hold triangles (allocated before the call)
 * pass:      0 - first pass; conservative algorithm
 * 	      1 - second pass; use dirty tricks
 *  
 * The number of output triangles produced for a polygon with n points is:
 *    (n - 2) + 2*(#holes)
 *
 * returns: -1 degenerate contour (zero length segment)
 *           0 allocation error
 *           + number of triangles
 */
static int
EG_fillArea(int ncontours, const int *cntr, const double *vertices,
            int *triangles, int *n_fig8, int pass, fillArea *fa)
{
  int    i, j, i0, i1, i2, index, indx2, k, l, npts, neg;
  int    start, next, left, right, ntri, mtri;
  double side2, dist, d, area, uv0[2], uv1[2], uv2[2], mid[2];
  Front  *tmp;
  int    *itmp;

  *n_fig8 = 0;
  for (i = 0; i < ncontours; i++) if (cntr[i] < 3) return -1;
  for (fa->nfront = i = 0; i < ncontours; i++) fa->nfront += cntr[i];
  if (fa->nfront == 0) return -1;
  fa->npts = fa->nsegs = fa->nfront;

  mtri = fa->nfront - 2 + 2*(ncontours-1);
  ntri = 0;

  /* allocate the memory for the front */

  if (fa->front == NULL) {
    fa->mfront = CHUNK;
    while (fa->mfront < fa->nfront) fa->mfront += CHUNK;
    fa->front = (Front *) EG_alloc(fa->mfront*sizeof(Front));
    if (fa->front == NULL) return 0;
    fa->segs  = (int *) EG_alloc(2*fa->mfront*sizeof(int));
    if (fa->segs  == NULL) {
      EG_free(fa->front);
      fa->front = NULL;
      return 0;
    }
  } else {
    if (fa->mfront < fa->nfront) {
      i = fa->mfront;
      while (i < fa->nfront) i += CHUNK;
      tmp = (Front *) EG_reall(fa->front, i*sizeof(Front));
      if (tmp == NULL) return 0;
      itmp = (int *) EG_reall(fa->segs, 2*i*sizeof(int));
      if (itmp == NULL) {
        EG_free(tmp);
        return 0;
      }
      fa->mfront = i;
      fa->front  =  tmp;
      fa->segs   = itmp;
    }
  }

  /* allocate the memory for our point markers */
  npts = fa->nfront+1;
  if (fa->pts == NULL) {
    fa->mpts = CHUNK;
    while (fa->mpts < npts) fa->mpts += CHUNK;
    fa->pts = (int *) EG_alloc(fa->mpts*sizeof(int));
    if (fa->pts == NULL) return 0;
  } else {
    if (fa->mpts < npts) {
      i = fa->mpts;
      while (i < npts) i += CHUNK;
      itmp = (int *) EG_reall(fa->pts, i*sizeof(int));
      if (itmp == NULL) return 0;
      fa->mpts = i;
      fa->pts  = itmp;
    }
  }

  /* initialize the front */
  for (start = index = i = 0; i < ncontours; i++) {
    left = start + cntr[i] - 1;
    for (j = 0; j < cntr[i]; j++, index++) {
      fa->segs[2*index  ]     = left      + 1;
      fa->segs[2*index+1]     = start + j + 1;
      fa->front[index].sleft  = left;
      fa->front[index].i0     = left      + 1;
      fa->front[index].i1     = start + j + 1;
      fa->front[index].sright = start + j + 1;
      fa->front[index].snew   = 0;
      left = start + j;
    }
    fa->front[index-1].sright = start;

    /* look for fig 8 nodes in the contour */
    for (j = 0; j < cntr[i]-1; j++) {
      i0 = start + j + 1;
      for (k = j+1; k < cntr[i]; k++) {
        i1 = start + k + 1;
        if ((vertices[2*i0  ] == vertices[2*i1  ]) &&
            (vertices[2*i0+1] == vertices[2*i1+1])) {
          if (i0+1 == i1) {
            printf(" EGADS Internal: Null in loop %d -> %d %d\n", i, i0, i1);
            continue;
          }
          printf(" EGADS Internal: Fig 8 in loop %d (%d) -> %d %d (removed)\n",
                 i, ncontours, i0, i1);
	  /* figure 8's in the external loop decrease the triangle count */
          if (i == 0) (*n_fig8)++;  /* . . . . sometimes                 */
          for (l = 0; l < index; l++) {
            if (fa->front[l].i0 == i1) fa->front[l].i0 = i0;
            if (fa->front[l].i1 == i1) fa->front[l].i1 = i0;
          }
        }
      }
    }
    start += cntr[i];
  }

  /* collapse the front while building the triangle list*/

  neg = 0;
  do {

    /* count the number of vertex hits (right-hand links) */

    for (i = 0; i < npts; i++) fa->pts[i] = 0;
    for (i = 0; i < fa->nfront; i++) 
      if (fa->front[i].sright != NOTFILLED) fa->pts[fa->front[i].i1]++;

    /* remove any simple isolated triangles */

    for (j = i = 0; i < fa->nfront; i++) {
      if (fa->front[i].sright == NOTFILLED) continue;
      i0    = fa->front[i].i0;
      i1    = fa->front[i].i1;
      right = fa->front[i].sright;
      left  = fa->front[right].sright;
      if (fa->front[left].i1 == i0) {
        i2     = fa->front[right].i1;
        uv0[0] = vertices[2*i0  ];
        uv0[1] = vertices[2*i0+1];
        uv1[0] = vertices[2*i1  ];
        uv1[1] = vertices[2*i1+1];
        uv2[0] = vertices[2*i2  ];
        uv2[1] = vertices[2*i2+1];
        area   = AREA2D(uv0, uv1, uv2);
        if ((neg == 0) && (area <= 0.0)) continue;
        if (fa->front[left].sright != i) {
          start = fa->front[left].sright;
          fa->front[start].sleft  = fa->front[i].sleft;
          start = fa->front[i].sleft;
          fa->front[start].sright = fa->front[left].sright;
        }
        triangles[3*ntri  ]    = i0;
        triangles[3*ntri+1]    = i1;
        triangles[3*ntri+2]    = i2;
        fa->front[i].sleft     = fa->front[i].sright     = NOTFILLED;
        fa->front[right].sleft = fa->front[right].sright = NOTFILLED;
        fa->front[left].sleft  = fa->front[left].sright  = NOTFILLED;
        ntri++;
        j++;
        if (ntri >= mtri) break;
        neg = 0;
      }
    }
    if (j != 0) continue;

    /* look for triangles hidden by "figure 8" vetrices */

    for (j = i = 0; i < fa->nfront; i++) {
      if (fa->front[i].sright == NOTFILLED) continue;
      i0 = fa->front[i].i0;
      i1 = fa->front[i].i1;
      if (fa->pts[i1] == 1) continue;
      for (k = 0; k < fa->nfront; k++) {
        if (fa->front[k].sright == NOTFILLED) continue;
        if (k == fa->front[i].sright) continue;
        if (fa->front[k].i0 != i1) continue;
        i2 = fa->front[k].i1;
        uv0[0] = vertices[2*i0  ];
        uv0[1] = vertices[2*i0+1];
        uv1[0] = vertices[2*i1  ];
        uv1[1] = vertices[2*i1+1];
        uv2[0] = vertices[2*i2  ];
        uv2[1] = vertices[2*i2+1];
        area   = AREA2D(uv0, uv1, uv2);
        if ((neg == 0) && (area <= 0.0)) continue;
        for (l = 0; l < fa->nfront; l++) {
          if (fa->front[l].sright == NOTFILLED) continue;
          if (fa->front[l].sleft  == NOTFILLED) continue;
          if ((fa->front[l].i0 == i2) && (fa->front[l].i1 == i0)) {
            if (fa->front[i].sleft != l) {
              index = fa->front[i].sleft;
              indx2 = fa->front[l].sright;
              fa->front[i].sleft      = l;
              fa->front[l].sright     = i;
              fa->front[index].sright = indx2;
              fa->front[indx2].sleft  = index;
            }
            if (fa->front[i].sright != k) {
              index = fa->front[i].sright;
              indx2 = fa->front[k].sleft;
              fa->front[i].sright     = k;
              fa->front[k].sleft      = i;
              fa->front[index].sleft  = indx2;
              fa->front[indx2].sright = index;
            }
            if (fa->front[k].sright != l) {
              index = fa->front[k].sright;
              indx2 = fa->front[l].sleft;
              fa->front[k].sright     = l;
              fa->front[l].sleft      = k;
              fa->front[index].sleft  = indx2;
              fa->front[indx2].sright = index;
            }

            left   = fa->front[i].sleft;
            right  = fa->front[i].sright;
            triangles[3*ntri  ]    = i0;
            triangles[3*ntri+1]    = i1;
            triangles[3*ntri+2]    = i2;
            fa->front[i].sleft     = fa->front[i].sright     = NOTFILLED;
            fa->front[right].sleft = fa->front[right].sright = NOTFILLED;
            fa->front[left].sleft  = fa->front[left].sright  = NOTFILLED;
            ntri++;
            j++;
            if (ntri >= mtri) break;
            neg = 0;
          }
        }
        if (ntri >= mtri) break;
      }
      if (ntri >= mtri) break;
    }
    if (j != 0) continue;

    /* get smallest segment left */

    for (i = 0; i < fa->nfront; i++) fa->front[i].mark = 0;
small:
    index = -1;
    side2 = DBL_MAX;
    for (i = 0; i < fa->nfront; i++) {
      if (fa->front[i].sright == NOTFILLED) continue;
      if (fa->front[i].mark == 1) continue;
      i0     = fa->front[i].i0;
      i1     = fa->front[i].i1;
      uv0[0] = vertices[2*i0  ];
      uv0[1] = vertices[2*i0+1];
      uv1[0] = vertices[2*i1  ];
      uv1[1] = vertices[2*i1+1];
      d      = DIST2(uv0, uv1);
      if (d < side2) {
        side2 = d;
        index = i;
      }
    }
    if (index == -1) {
      for (k = 0; k < *n_fig8; k++) 
        if (ntri + 2*k == mtri) break;
      if (neg == 0) {
        neg = 1;
        continue;
      }
      printf(" EGADS Internal: can't find segment!\n");
      goto error;
    }

    /* find the best candidate -- closest to midpoint and correct area */

    i0     = fa->front[index].i0;
    i1     = fa->front[index].i1;
    uv0[0] = vertices[2*i0  ];
    uv0[1] = vertices[2*i0+1];
    uv1[0] = vertices[2*i1  ];
    uv1[1] = vertices[2*i1+1];
    mid[0] = 0.5*(uv0[0] + uv1[0]);
    mid[1] = 0.5*(uv0[1] + uv1[1]);

    indx2 = -1;
    dist  = DBL_MAX;
    for (i = 0; i < fa->nfront; i++) {
      if ((i == index) || (fa->front[i].sright == NOTFILLED)) continue;
      i2 = fa->front[i].i1;
      if ((i2 == i0) || (i2 == i1)) continue;
      uv2[0] = vertices[2*i2  ];
      uv2[1] = vertices[2*i2+1];
      area   = AREA2D(uv0, uv1, uv2);
      if (area > 0.0) {
        d = DIST2(mid, uv2)/area;
        if (d < dist) {
          if (EG_crossSeg(index, mid, i2, vertices, pass, fa)) continue;
          dist  = d;
          indx2 = i;
        }
      }
    }
    /* may not find a candidate for segments that are too small
               retry with next largest (and hope for closure later) */
    if (indx2 == -1) {
      fa->front[index].mark = 1;
      goto small;
    }

    /* construct the triangle */

    i2 = fa->front[indx2].i1;
    triangles[3*ntri  ] = i0;
    triangles[3*ntri+1] = i1;
    triangles[3*ntri+2] = i2;
    ntri++;
    neg = 0;

    /* patch up the front */

    left  = fa->front[index].sleft;
    right = fa->front[index].sright;

    if (i2 == fa->front[left].i0) {
      /* 1) candate is in the left segment */

      fa->front[left].sright = right;
      fa->front[left].i1     = i1;
      fa->front[left].snew   = 1;
      fa->front[right].sleft = left;
      fa->front[index].sleft = fa->front[index].sright = NOTFILLED;

    } else if (i2 == fa->front[right].i1) {
      /* 2) candate is in the right segment */

      fa->front[left].sright = right;
      fa->front[right].sleft = left;
      fa->front[right].i0    = i0;
      fa->front[right].snew  = 1;
      fa->front[index].sleft = fa->front[index].sright = NOTFILLED;

    } else {
      /* 3) some other situation */

      start = 0;

      /* "figure 8" vertices? */

      if (fa->pts[i0] != 1) 
        for (i = 0; i < fa->nfront; i++) {
          if (fa->front[i].sright == NOTFILLED) continue;
          if (fa->front[i].i0 != i2) continue;
          if (fa->front[i].i1 != i0) continue;
          j = fa->front[i].sright;
          fa->front[left].sright = j;
          fa->front[j].sleft     = left;
          fa->front[index].sleft = i;
          fa->front[i].sright    = index;
          left = i;
          fa->front[left].sright = right;
          fa->front[left].i1     = i1;
          fa->front[left].snew   = 1;
          fa->front[right].sleft = left;
          fa->front[index].sleft = fa->front[index].sright = NOTFILLED;
          start = 1;
          break;
        }

      if ((fa->pts[i1] != 1) && (start == 0))
        for (i = 0; i < fa->nfront; i++) {
          if (fa->front[i].sright == NOTFILLED) continue;
          if (fa->front[i].i0 != i1) continue;
          if (fa->front[i].i1 != i2) continue;
          j = fa->front[i].sleft;
          fa->front[right].sleft  = j;
          fa->front[j].sright     = right;
          fa->front[index].sright = i;
          fa->front[i].sleft      = index;
          right = i;
          fa->front[left].sright = right;
          fa->front[right].sleft = left;
          fa->front[right].i0    = i0;
          fa->front[right].snew  = 1;
          fa->front[index].sleft = fa->front[index].sright = NOTFILLED;
          start = 1;
          break;
        }

      /* no, add a segment */

      if (start == 0) {

        next = -1;
        for (i = 0; i < fa->nfront; i++)
          if (fa->front[i].sright == NOTFILLED) {
            next = i;
            break;
          }

        if (next == -1) {
          if (fa->nfront >= fa->mfront) {
            i = fa->mfront + CHUNK;
            tmp = (Front *) EG_reall(fa->front, i*sizeof(Front));
            if (tmp == NULL) return 0;
            itmp = (int *) EG_reall(fa->segs, 2*i*sizeof(int));
            if (itmp == NULL) {
              EG_free(tmp);
              return 0;
            }
            fa->mfront = i;
            fa->front  =  tmp;
            fa->segs   = itmp;
          }
          next = fa->nfront;
          fa->nfront++;
        }

        start = fa->front[indx2].sright;
        fa->front[index].i1     = i2;
        fa->front[index].sright = start;
        fa->front[index].snew   = 1;
        fa->front[start].sleft  = index;
        fa->front[indx2].sright = next;
        fa->front[right].sleft  = next;
        fa->front[next].sleft   = indx2;
        fa->front[next].i0      = i2;
        fa->front[next].i1      = i1;
        fa->front[next].sright  = right;
        fa->front[next].snew    = 1;
      }
    }

  } while (ntri < mtri);

error:
  for (j = i = 0; i < fa->nfront; i++) 
    if (fa->front[i].sright != NOTFILLED) j++;

  if (j != 0) {
#ifdef DEBUG
    printf(" EGADS Internal: # unused segments = %d\n", j);
#endif
    ntri = 0;
  }

  return ntri;
}


static void
EG_makeConnect(int k1, int k2, int *tri, int *kedge, int *ntable,
               connect *etable, int face)
{
  int kn1, kn2, iface, oface, look;

  if (k1 > k2) {
    kn1 = k2-1;
    kn2 = k1-1;
  } else {
    kn1 = k1-1;
    kn2 = k2-1;
  }

  /* add to edge table */

  if (ntable[kn1] == NOTFILLED) {

    /* virgin node */

    *kedge               += 1;
    ntable[kn1]           = *kedge;
    etable[*kedge].node1  = kn1;
    etable[*kedge].node2  = kn2;
    etable[*kedge].tri    = tri;
    etable[*kedge].thread = NOTFILLED;
    return;

  } else {

    /* old node */
    iface = ntable[kn1];

again:
    if (etable[iface].node2 == kn2) {
      if (etable[iface].tri != NULL) {
        look = *etable[iface].tri;
        *etable[iface].tri = *tri;
        *tri = look;
        etable[iface].tri = NULL;
      } else {
        printf("EGADS Internal: Face %d", face);
        printf(", Side %d %d complete [but %d] (EG_makeConnect)!\n",
                k1+1, k2+1, *tri);
      }
      return;
    } else {
      oface = iface;
      iface = etable[oface].thread;
    }

    /* try next position in thread */
    if (iface == NOTFILLED) {
      *kedge               += 1;
      etable[oface].thread  = *kedge;
      etable[*kedge].node1  = kn1;
      etable[*kedge].node2  = kn2;
      etable[*kedge].tri    = tri;
      etable[*kedge].thread = NOTFILLED;
      return;
    }

    goto again;
  }
}


static int
EG_makeNeighbors(triStruct *ts, int f)
{
  int     *ntab, nside, j;
  connect *etab;
  
  ntab = (int *) EG_alloc(ts->nverts*sizeof(int));
  if (ntab == NULL) {
    printf(" EGADS Error: Vert Table Malloc (EG_makeTessBody)!\n");
    return EGADS_MALLOC;    
  }
  etab = (connect *) EG_alloc(ts->ntris*3*sizeof(connect));
  if (etab == NULL) {
    printf(" EGADS Error: Edge Table Malloc (EG_makeTessBody)!\n");
    EG_free(ntab);
    return EGADS_MALLOC;    
  }

  nside = -1;
  for (j = 0; j < ts->nverts; j++) ntab[j] = NOTFILLED;
  for (j = 0; j < ts->ntris;  j++) {
    EG_makeConnect( ts->tris[j].indices[1], ts->tris[j].indices[2],
                   &ts->tris[j].neighbors[0], &nside, ntab, etab, f);
    EG_makeConnect( ts->tris[j].indices[0], ts->tris[j].indices[2], 
                   &ts->tris[j].neighbors[1], &nside, ntab, etab, f);
    EG_makeConnect( ts->tris[j].indices[0], ts->tris[j].indices[1],
                   &ts->tris[j].neighbors[2], &nside, ntab, etab, f);
  }

  for (j = 0; j < ts->nsegs; j++)
    EG_makeConnect( ts->segs[j].indices[0], ts->segs[j].indices[1],
                   &ts->segs[j].neighbor, &nside, ntab, etab, f);
                   
  /* report any unconnected triangle sides */

  for (j = 0; j <= nside; j++) {
    if (etab[j].tri == NULL) continue;
    printf(" EGADS Info: Face %d, Unconnected Side %d %d = %d\n",
           f, etab[j].node1+1, etab[j].node2+1, *etab[j].tri);
    *etab[j].tri = 0;
  }

  EG_free(etab);
  EG_free(ntab);
  return EGADS_SUCCESS;
}


static void
EG_updateTris(triStruct *ts, egTessel *btess, int fIndex)
{
  int    i, j, k, m, n, nf, edge, *ptype, *pindex, *tris, *tric;
  double *xyz, *uv;
  
  xyz    = (double *) EG_alloc(3*ts->nverts*sizeof(double));
  uv     = (double *) EG_alloc(2*ts->nverts*sizeof(double));
  ptype  = (int *)    EG_alloc(  ts->nverts*sizeof(int));
  pindex = (int *)    EG_alloc(  ts->nverts*sizeof(int));
  tris   = (int *)    EG_alloc(3*ts->ntris* sizeof(int));
  tric   = (int *)    EG_alloc(3*ts->ntris* sizeof(int));
  if ((xyz    == NULL) || (uv   == NULL) || (ptype == NULL) ||
      (pindex == NULL) || (tris == NULL) || (tric  == NULL)) {
    printf(" EGADS Error: Cannot Allocate Tessellation Memory for %d!\n",
           fIndex);
    if (tric   != NULL) EG_free(tric);
    if (tris   != NULL) EG_free(tris);
    if (pindex != NULL) EG_free(pindex);
    if (ptype  != NULL) EG_free(ptype);
    if (uv     != NULL) EG_free(uv);
    if (xyz    != NULL) EG_free(xyz);
    return;
  }

  /* fix up the vertices */

  for (i = 0; i < ts->nverts; i++) {
    xyz[3*i  ] = ts->verts[i].xyz[0];
    xyz[3*i+1] = ts->verts[i].xyz[1];
    xyz[3*i+2] = ts->verts[i].xyz[2];
    uv[2*i  ]  = ts->verts[i].uv[0];
    uv[2*i+1]  = ts->verts[i].uv[1];
    ptype[i]   = pindex[i] = -1;
    if (ts->verts[i].type == NODE) {
      ptype[i]  = 0;
      pindex[i] = ts->verts[i].index;
    } else if (ts->verts[i].type == EDGE) {
      ptype[i]  = ts->verts[i].index;
      pindex[i] = ts->verts[i].edge;
    }
  }
  btess->tess2d[fIndex-1].xyz    = xyz;
  btess->tess2d[fIndex-1].uv     = uv;
  btess->tess2d[fIndex-1].ptype  = ptype;
  btess->tess2d[fIndex-1].pindex = pindex;
  btess->tess2d[fIndex-1].npts   = ts->nverts;
  
  /* fix up the triangles */
  
  for (i = 0; i < ts->ntris; i++) {
    tris[3*i  ] = ts->tris[i].indices[0];
    tris[3*i+1] = ts->tris[i].indices[1];
    tris[3*i+2] = ts->tris[i].indices[2];
    tric[3*i  ] = ts->tris[i].neighbors[0];
    tric[3*i+1] = ts->tris[i].neighbors[1];
    tric[3*i+2] = ts->tris[i].neighbors[2];
  }
  for (i = 0; i < ts->ntris; i++)
    for (j = 0; j < 3; j++) 
      if (tric[3*i+j] < 0) {
        n    = -tric[3*i+j];
        edge = abs(ts->segs[n-1].edge);
        k    = ts->segs[n-1].index-1;
        if (ts->segs[n-1].edge > 0) {
          nf = btess->tess1d[edge-1].faces[1].nface;
          m  = EG_faceConnIndex(btess->tess1d[edge-1].faces[1], fIndex);
          if (m == 0) {
            printf(" EGADS Internal: Face %d not found in Edge (+) %d!\n",
                   fIndex, edge);
          } else {
            btess->tess1d[edge-1].faces[1].tric[k*nf+m-1] = i+1;
          }
        } else {
          nf = btess->tess1d[edge-1].faces[0].nface;
          m  = EG_faceConnIndex(btess->tess1d[edge-1].faces[0], fIndex);
          if (m == 0) {
            printf(" EGADS Internal: Face %d not found in Edge (-) %d!\n",
                   fIndex, edge);
          } else {
            btess->tess1d[edge-1].faces[0].tric[k*nf+m-1] = i+1;
          }

        }
        tric[3*i+j] = -edge;
      }
  btess->tess2d[fIndex-1].tris  = tris;
  btess->tess2d[fIndex-1].tric  = tric;
  btess->tess2d[fIndex-1].ntris = ts->ntris;
}


int
EG_getTessEdge(const egObject *tess, int index, int *len, 
               const double **xyz, const double **t)
{
  int      outLevel;
  egTessel *btess;
  egObject *obj;

  *len = 0;
  *xyz = *t = NULL;
  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getTessEdge)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getTessEdge)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getTessEdge)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_getTessEdge)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess1d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Edge Tessellations (EG_getTessEdge)!\n");
    return EGADS_NODATA;  
  }
  if ((index < 1) || (index > btess->nEdge)) {
    if (outLevel > 0)
      printf(" EGADS Error: Index = %d [1-%d] (EG_getTessEdge)!\n",
             index, btess->nEdge);
    return EGADS_INDEXERR;
  }

  *len = btess->tess1d[index-1].npts;
  *xyz = btess->tess1d[index-1].xyz;
  *t   = btess->tess1d[index-1].t;
  return EGADS_SUCCESS;
}


int
EG_getTessFace(const egObject *tess, int index, int *len, const double **xyz, 
               const double **uv, const int **ptype, const int **pindex, 
               int *ntri, const int **tris, const int **tric)
{
  int      outLevel;
  egTessel *btess;
  egObject *obj;

  *len   = *ntri   = 0;
  *xyz   = *uv     = NULL;
  *ptype = *pindex = NULL;
  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getTessFace)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getTessFace)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getTessFace)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_getTessFace)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess2d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Face Tessellations (EG_getTessFace)!\n");
    return EGADS_NODATA;  
  }
  if ((index < 1) || (index > btess->nFace)) {
    if (outLevel > 0)
      printf(" EGADS Error: Index = %d [1-%d] (EG_getTessFace)!\n",
             index, btess->nFace);
    return EGADS_INDEXERR;
  }

  *len    = btess->tess2d[index-1].npts;
  *xyz    = btess->tess2d[index-1].xyz;
  *uv     = btess->tess2d[index-1].uv;
  *ptype  = btess->tess2d[index-1].ptype;
  *pindex = btess->tess2d[index-1].pindex;
  *ntri   = btess->tess2d[index-1].ntris;
  *tris   = btess->tess2d[index-1].tris;
  *tric   = btess->tess2d[index-1].tric;

  return EGADS_SUCCESS;
}


static int
EG_fillTris(egObject *body, int iFace, egObject *face, egObject *tess, 
            triStruct *ts, fillArea *fa)
{
  int      i, j, k, m, n, stat, nedge, nloop, oclass, mtype, or, np, degen;
  int      *senses, *sns, *lps, *tris, npts, ntot, ntri, sen, n_fig8, nd, st;
  int      outLevel;
  double   range[4], trange[2], *uvs;
  egObject *geom, **edges, **loops, **nds;
  egTessel *btess;
  triVert  *tv;
  triTri   *tt;
  triSeg   *tsg;
  static double scl[3][2] = {1.0, 1.0,  10.0, 1.0,  0.1, 10.0};
  const  double *xyzs, *tps;

  outLevel = EG_outLevel(body);
  btess    = (egTessel *) tess->blind;

  /* get the Loops */

  stat = EG_getTopology(face, &geom, &oclass, &or, range, &nloop, &loops,
                        &senses);
  if (stat != EGADS_SUCCESS) return stat;
#ifdef DEBUG
  printf(" Face %d: nLoop = %d   Range = %lf %lf  %lf %lf\n", 
         iFace, nloop, range[0], range[1], range[2], range[3]);
#endif
  ts->fIndex = iFace;
  ts->face   = face;
  ts->orUV   = or;
  ts->planar = 0;
  if (geom->mtype == PLANE) ts->planar = 1;

  /* get the point count */

  for (ntot = i = 0; i < nloop; i++) {
    stat = EG_getTopology(loops[i], &geom, &oclass, &mtype, NULL, &nedge,
                          &edges, &senses);
    if (stat != EGADS_SUCCESS) return stat;
    for (j = 0; j < nedge; j++) {
      k = EG_indexBodyTopo(body, edges[j]);
      if (k <= EGADS_SUCCESS) {
        printf(" EGADS Error: Face %d -> Can not find Edge = %d!\n", 
               iFace, k);
        return EGADS_NOTFOUND;
      }
      stat = EG_getTopology(edges[j], &geom, &oclass, &mtype, trange, &nd,
                            &nds, &sns);
      if (stat != EGADS_SUCCESS) return stat;
      if (mtype == DEGENERATE) continue;
      stat = EG_getTessEdge(tess, k, &npts, &xyzs, &tps);
      if (stat != EGADS_SUCCESS) return stat;
      ntot += npts-1;
    }
  }
  
  ntri = ntot-2 + 2*(nloop-1);
#ifdef DEBUG
  printf("    total points = %d,  total tris = %d\n", ntot, ntri);
#endif

  /* get enough storage for the verts & boundary segs */
  
  n  = ntot/CHUNK + 1;
  n *= CHUNK;
  if (ts->verts == NULL) {
    ts->verts = (triVert *) EG_alloc(n*sizeof(triVert));
    if (ts->verts == NULL) return EGADS_MALLOC;
    ts->mverts = n;
  } else {
    if (n > ts->mverts) {
      tv = (triVert *) EG_reall(ts->verts, n*sizeof(triVert));
      if (tv == NULL) return EGADS_MALLOC;
      ts->verts  = tv;
      ts->mverts = n;
    }
  }
  ts->nverts = ntot;

  n  = ntot/CHUNK + 1;
  n *= CHUNK;
  if (ts->segs == NULL) {
    ts->segs = (triSeg *) EG_alloc(n*sizeof(triSeg));
    if (ts->segs == NULL) return EGADS_MALLOC;
    ts->msegs = n;
  } else {
    if (n > ts->msegs) {
      tsg = (triSeg *) EG_reall(ts->segs, n*sizeof(triSeg));
      if (tsg == NULL) return EGADS_MALLOC;
      ts->segs  = tsg;
      ts->msegs = n;
    }
  }
  ts->nsegs = ntot;
  
  /* get memory for the loops */

  uvs = (double *) EG_alloc((ntot*2+2)*sizeof(double));
  if (uvs == NULL) return EGADS_MALLOC;
  lps = (int *) EG_alloc(nloop*sizeof(int));
  if (lps == NULL) {
    EG_free(uvs);
    return EGADS_MALLOC;
  }
 
  /* fill in the loops & mark the boundary segments */
  
  np     = 1;
  uvs[0] = uvs[1] = 0.0;
  for (i = 0; i < nloop; i++) {
    st   = np;
    stat = EG_getTopology(loops[i], &geom, &oclass, &mtype, NULL, &nedge,
                          &edges, &senses);
    if (stat != EGADS_SUCCESS) {
      EG_free(lps);
      EG_free(uvs);
      return stat;
    }
    n = 0;
    if (or == SREVERSE) n = nedge-1;
    for (degen = ntot = j = 0; j < nedge; j++, n += or) {
      k = EG_indexBodyTopo(body, edges[n]);
      if (k <= EGADS_SUCCESS) {
        printf(" EGADS Error: Face %d -> Can not find Edge = %d!\n", 
               iFace, k);
        EG_free(lps);
        EG_free(uvs);
        return EGADS_NOTFOUND;
      }
      stat = EG_getTopology(edges[n], &geom, &oclass, &mtype, trange, &nd,
                            &nds, &sns);
      if (stat != EGADS_SUCCESS) {
        EG_free(lps);
        EG_free(uvs);
        return stat;
      }
      if (mtype == DEGENERATE) {
        degen = 1;
        continue;
      }
      stat = EG_getTessEdge(tess, k, &npts, &xyzs, &tps);
      if (stat != EGADS_SUCCESS) {
        EG_free(lps);
        EG_free(uvs);
        return stat;
      }
      sen = senses[n]*or;
      if (sen == 1) {
        for (m = 0; m < npts-1; m++, np++) {
          stat = EG_getEdgeUV(face, edges[n], senses[n], tps[m], &uvs[2*np]);
          if (stat != EGADS_SUCCESS) {
            printf(" EGADS Error: getEdgeUV+ = %d  for Face %d/%d, Edge = %d\n",
                   stat, iFace, i+1, n+1);
            EG_free(lps);
            EG_free(uvs);
            return stat;
          }
          ts->verts[np-1].type   = EDGE;
          ts->verts[np-1].edge   = k;
          ts->verts[np-1].index  = m+1;
          ts->verts[np-1].xyz[0] = xyzs[3*m  ];
          ts->verts[np-1].xyz[1] = xyzs[3*m+1];
          ts->verts[np-1].xyz[2] = xyzs[3*m+2];
          ts->verts[np-1].uv[0]  = uvs[2*np  ];
          ts->verts[np-1].uv[1]  = uvs[2*np+1];
          if (m == 0) {
            ts->verts[np-1].type   = NODE;
            ts->verts[np-1].edge   = 0;
            ts->verts[np-1].index  = EG_indexBodyTopo(body, nds[0]);
            if (degen == 1) {
#ifdef DEBUG
              printf(" Face %d, Vertex %d: Node = %d is Degen!\n",
                     iFace, np, ts->verts[np-1].index);
#endif
              ts->verts[np-1].edge = -1;
              degen = 0;
            }
          }
          ts->segs[np-1].indices[0] =  np;
          ts->segs[np-1].indices[1] =  np+1;
          ts->segs[np-1].neighbor   = -np;
          ts->segs[np-1].edge       =  senses[n]*k;
          ts->segs[np-1].index      =  m+1;
#ifdef DEBUG
          printf("    %lf %lf\n", uvs[2*np  ], uvs[2*np+1]);
#endif
        }
      } else {
        for (m = npts-1; m > 0; m--, np++) {
          stat = EG_getEdgeUV(face, edges[n], senses[n], tps[m], &uvs[2*np]);
          if (stat != EGADS_SUCCESS) {
            printf(" EGADS Error: getEdgeUV- = %d  for Face %d/%d, Edge = %d\n",
                   stat, iFace, i+1, n+1);
            EG_free(lps);
            EG_free(uvs);
            return stat;
          }
          ts->verts[np-1].type   = EDGE;
          ts->verts[np-1].edge   = k;
          ts->verts[np-1].index  = m+1;
          ts->verts[np-1].xyz[0] = xyzs[3*m  ];
          ts->verts[np-1].xyz[1] = xyzs[3*m+1];
          ts->verts[np-1].xyz[2] = xyzs[3*m+2];
          ts->verts[np-1].uv[0]  = uvs[2*np  ];
          ts->verts[np-1].uv[1]  = uvs[2*np+1];
          if (m == npts-1) {
            ts->verts[np-1].type   = NODE;
            ts->verts[np-1].edge   = 0;
            if (mtype == TWONODE) {
              ts->verts[np-1].index = EG_indexBodyTopo(body, nds[1]);
            } else {
              ts->verts[np-1].index = EG_indexBodyTopo(body, nds[0]);
            }
            if (degen == 1) {
#ifdef DEBUG
              printf(" Face %d, Vertex %d: Node = %d is Degen!\n",
                     iFace, np, ts->verts[np-1].index);
#endif
              ts->verts[np-1].edge = -1;
              degen = 0;
            }
          }
          ts->segs[np-1].indices[0] =  np;
          ts->segs[np-1].indices[1] =  np+1;
          ts->segs[np-1].neighbor   = -np;
          ts->segs[np-1].edge       =  senses[n]*k;
          ts->segs[np-1].index      =  m;
#ifdef DEBUG
          printf("    %lf %lf\n", uvs[2*np  ], uvs[2*np+1]);
#endif
        }
      }
#ifdef DEBUG
      printf("  **** End Edge %d sen = %d ****\n", k+1, sen);
#endif
      ntot += npts-1;
    }
    ts->segs[np-2].indices[1] = st;
    if (degen == 1) {
      if (ts->verts[st-1].edge != 0) {
        printf(" EGADS Error: Degen setting w/ Face %d  Marker = %d %d %d\n",
               iFace, ts->verts[st-1].type, ts->verts[st-1].edge, 
                      ts->verts[st-1].index);
      } else {
#ifdef DEBUG
        printf(" Face %d, Vertex %d: Node = %d is Degen!\n",
               iFace, st, ts->verts[st-1].index);
#endif
        ts->verts[st-1].edge = -1;
      }
    }
#ifdef DEBUG
    printf("  **** End Loop %d: nedge = %d  %d ****\n", i+1, nedge, ntot);
#endif
    lps[i] = ntot;
  }
  
  /* fill in the interior with triangles */
  
  tris = (int *) EG_alloc(3*ntri*sizeof(int));
  if (tris == NULL) {
    EG_free(lps);
    EG_free(uvs);
    return EGADS_MALLOC;
  }
  
  n = EG_fillArea(nloop, lps, uvs, tris, &n_fig8, 0, fa);
  
  /* adjust for figure 8 configurations */
  if (n_fig8 != 0) {
    printf(" EG_fillArea Warning: Face %d -> Found %d figure 8's!\n", 
           iFace, n_fig8);
    for (i = 0; i < n_fig8; i++) if (n+2*i == ntri) ntri = n;
  }
#ifdef DEBUG  
  printf("   EG_fillArea = %d (%d),  #loops = %d, or = %d,  #fig8 = %d\n", 
         n, ntri, nloop, or, n_fig8);
#endif
         
  if (n != ntri) {
    range[0] = range[2] = uvs[2];
    range[1] = range[3] = uvs[3];
    for (i = 2; i < np; i++) {
      if (uvs[2*i  ] < range[0]) range[0] = uvs[2*i  ];
      if (uvs[2*i+1] < range[1]) range[1] = uvs[2*i+1];
      if (uvs[2*i  ] > range[2]) range[2] = uvs[2*i  ];
      if (uvs[2*i+1] > range[3]) range[3] = uvs[2*i+1];
    }
    for (i = 1; i < np; i++) {
      uvs[2*i  ] = (uvs[2*i  ]-range[0])/(range[2]-range[0]);
      uvs[2*i+1] = (uvs[2*i+1]-range[1])/(range[3]-range[1]);
    }
    for (j = 0; j < 3; j++) {
      for (i = 1; i < np; i++) {
        uvs[2*i  ] *= scl[j][0];
        uvs[2*i+1] *= scl[j][1];
      }
      n = EG_fillArea(nloop, lps, uvs, tris, &n_fig8, 1, fa);
      printf(" EGADS Internal: Face %d -> Renormalizing %d, ntris = %d (%d)!\n",
             iFace, j, ntri, n);
      if (n == ntri) break;
    }
  }
  EG_free(lps);
  EG_free(uvs);
  if (n != ntri) {
    EG_free(tris);
    return EGADS_DEGEN;
  }
  
  /* fill up the triangles */
  
  n  = ntri/CHUNK + 1;
  n *= CHUNK;
  if (ts->tris == NULL) {
    ts->tris = (triTri *) EG_alloc(n*sizeof(triTri));
    if (ts->tris == NULL) {
      EG_free(tris);
      EG_free(lps);
      EG_free(uvs);
      return EGADS_MALLOC;
    }
    ts->mtris = n;
  } else {
    if (n > ts->mtris) {
      tt = (triTri *) EG_reall(ts->tris, n*sizeof(triTri));
      if (tt == NULL) {
        EG_free(tris);
        EG_free(lps);
        EG_free(uvs);
        return EGADS_MALLOC;
      }
      ts->tris  = tt;
      ts->mtris = n;
    }
  } 
  
  for (i = 0; i < ntri; i++) {
    ts->tris[i].mark         = 0;
    ts->tris[i].indices[0]   = tris[3*i  ];
    ts->tris[i].indices[1]   = tris[3*i+1];
    ts->tris[i].indices[2]   = tris[3*i+2];
    ts->tris[i].neighbors[0] = i+1;
    ts->tris[i].neighbors[1] = i+1;
    ts->tris[i].neighbors[2] = i+1;
  }
  ts->ntris = ntri;
  EG_free(tris);
  
  /* flip tri orientation if face is reversed */
  if (or == SREVERSE)
    for (i = 0; i < ts->ntris; i++) {
      j                      = ts->tris[i].indices[1];
      ts->tris[i].indices[1] = ts->tris[i].indices[2];
      ts->tris[i].indices[2] = j;
    }
    
  /* connect the triangles and make the neighbor info */
  
  stat = EG_makeNeighbors(ts, iFace);
  if (stat != EGADS_SUCCESS) return stat;

  /* enhance the tessellation */

  stat = EG_tessellate(outLevel, ts);
  if (stat == EGADS_SUCCESS) {
    /* set it in the tessellation structure */
    EG_updateTris(ts, btess, iFace);
  }

  return stat;
}


static void
EG_cleanupTess(egTessel *btess)
{
  int i;
  
  if (btess->tess1d != NULL) {
    for (i = 0; i < btess->nEdge; i++) {
      if (btess->tess1d[i].faces[0].faces != NULL)
        EG_free(btess->tess1d[i].faces[0].faces);
      if (btess->tess1d[i].faces[1].faces != NULL)
        EG_free(btess->tess1d[i].faces[1].faces);
      if (btess->tess1d[i].faces[0].tric  != NULL)
        EG_free(btess->tess1d[i].faces[0].tric);
      if (btess->tess1d[i].faces[1].tric  != NULL)
        EG_free(btess->tess1d[i].faces[1].tric);
      if (btess->tess1d[i].xyz != NULL) 
        EG_free(btess->tess1d[i].xyz);
      if (btess->tess1d[i].t   != NULL) 
        EG_free(btess->tess1d[i].t);
    }
    EG_free(btess->tess1d);
  }
  
  if (btess->tess2d != NULL) {
    for (i = 0; i < 2*btess->nFace; i++) {
      if (btess->tess2d[i].xyz    != NULL) 
        EG_free(btess->tess2d[i].xyz);
      if (btess->tess2d[i].uv     != NULL) 
        EG_free(btess->tess2d[i].uv);
      if (btess->tess2d[i].ptype  != NULL) 
        EG_free(btess->tess2d[i].ptype);
      if (btess->tess2d[i].pindex != NULL) 
        EG_free(btess->tess2d[i].pindex);
      if (btess->tess2d[i].tris   != NULL) 
        EG_free(btess->tess2d[i].tris);
      if (btess->tess2d[i].tric   != NULL) 
        EG_free(btess->tess2d[i].tric);
    }
    EG_free(btess->tess2d);
  }

}


static double
EG_curvNorm(egObject *face, int i, int sense, double d, double *dx, 
            double aux[][3])
{
  int    stat;
  double area;
  double uv[2], result[18], x1[3], x2[3], nrme[3], nrmi[3], ds[3];
  
  /* get normal at mid-point in UV */
  uv[0] = 0.5*(aux[i][0] + aux[i+1][0]);
  uv[1] = 0.5*(aux[i][1] + aux[i+1][1]);
  stat  = EG_evaluate(face, uv, result);
  if (stat != EGADS_SUCCESS) return -2.0;
  x1[0] = result[3];
  x1[1] = result[4];
  x1[2] = result[5];
  x2[0] = result[6];
  x2[1] = result[7];
  x2[2] = result[8];
  CROSS(nrme, x1, x2);
  area = DOT(nrme, nrme);
  if (area == 0.0) return -2.0;
  area     = 1.0/sqrt(area);
  nrme[0] *= area;
  nrme[1] *= area;
  nrme[2] *= area;
            
  /* get interior Face normal */
  area   = sqrt(d);
  dx[0] /= area;
  dx[1] /= area;
  dx[2] /= area;
  CROSS(ds, dx, nrme);
  if (sense == 1) {
    ds[0] = -ds[0];
    ds[1] = -ds[1];
    ds[2] = -ds[2];
  }
  area /= 4.0;
  x1[0] = result[0] + area*ds[0];
  x1[1] = result[1] + area*ds[1];
  x1[2] = result[2] + area*ds[2];
  stat  = EG_invEvaluate(face, x1, uv, x2);
  if (stat != EGADS_SUCCESS) return -2.0;
  stat  = EG_evaluate(face, uv, result);
  if (stat != EGADS_SUCCESS) return -2.0;
  x1[0] = result[3];
  x1[1] = result[4];
  x1[2] = result[5];
  x2[0] = result[6];
  x2[1] = result[7];
  x2[2] = result[8];
  CROSS(nrmi, x1, x2);
  area = DOT(nrmi, nrmi);
  if (area == 0.0) return -2.0;
  area     = 1.0/sqrt(area);
  nrmi[0] *= area;
  nrmi[1] *= area;
  nrmi[2] *= area;
            
  /* dot the normals */
  return DOT(nrme, nrmi);
}


static int
EG_tessEdges(egTessel *btess, /*@null@*/ int *retess)
{
  int      i, j, k, n, npts, stat, outLevel, nedge, oclass, mtype, nnode;
  int      nf, nface, nloop, ntype, ndum, face, sense, *senses, *finds;
  double   xyz[MAXELEN][3], t[MAXELEN], aux[MAXELEN][3], mindist, tol, dist;
  double   d, dotnrm, dot, limits[2], mid[3], range[4], dx[3], result[18];
  egObject *body, *geom, *ref, **faces, **loops, **edges, **nodes, **dum;
  
  body     = btess->src;
  outLevel = EG_outLevel(body);
  
  stat = EG_getBodyTopos(body, NULL, EDGE, &nedge, &edges);
  if (stat != EGADS_SUCCESS) return stat;
  stat = EG_getBodyTopos(body, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) return stat;
  
  if (retess == NULL) {
    btess->tess1d = (egTess1D *) EG_alloc(nedge*sizeof(egTess1D));
    if (btess->tess1d == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Alloc %d Edges (EG_tessEdges)!\n", nedge);
      EG_free(faces);
      EG_free(edges);
      return EGADS_MALLOC;  
    }
    for (j = 0; j < nedge; j++) {
      btess->tess1d[j].obj            = edges[j];
      btess->tess1d[j].faces[0].index = 0;
      btess->tess1d[j].faces[0].nface = 0;
      btess->tess1d[j].faces[0].faces = NULL;
      btess->tess1d[j].faces[0].tric  = NULL;
      btess->tess1d[j].faces[1].index = 0;
      btess->tess1d[j].faces[1].nface = 0;
      btess->tess1d[j].faces[1].faces = NULL;
      btess->tess1d[j].faces[1].tric  = NULL;
      btess->tess1d[j].nodes[0]       = 0;
      btess->tess1d[j].nodes[1]       = 0;
      btess->tess1d[j].xyz            = NULL;
      btess->tess1d[j].t              = NULL;
      btess->tess1d[j].npts           = 0;
    }
    btess->nEdge = nedge;
  
    /* get the face indices (if any) */
    for (i = 0; i < nface; i++) {
      stat = EG_getTopology(faces[i], &geom, &oclass, &mtype, limits,
                            &nloop, &loops, &senses);
      if (stat != EGADS_SUCCESS) continue;
      for (j = 0; j < nloop; j++) {
        stat = EG_getTopology(loops[j], &geom, &oclass, &mtype, limits,
                            &ndum, &dum, &senses);
        if (stat != EGADS_SUCCESS) continue;
        for (k = 0; k < ndum; k++) {
          n = EG_indexBodyTopo(body, dum[k]);
          if (n <= EGADS_SUCCESS) continue;
          if (senses[k] < 0) {
            if (btess->tess1d[n-1].faces[0].nface != 0) {
              if (btess->tess1d[n-1].faces[0].nface == 1) {
                btess->tess1d[n-1].faces[0].faces = (int *) EG_alloc(2*sizeof(int));
                if (btess->tess1d[n-1].faces[0].faces == NULL) {
                  if (outLevel > 0)
                    printf(" EGADS Error: Alloc (-) Edge %d (EG_tessEdges)!\n", n);
                  EG_free(faces);
                  EG_free(edges);
                  return EGADS_MALLOC;
                }
                btess->tess1d[n-1].faces[0].faces[0] = btess->tess1d[n-1].faces[0].index;
                btess->tess1d[n-1].faces[0].faces[1] = i+1;
              } else {
                finds = (int *) EG_reall( btess->tess1d[n-1].faces[0].faces,
                                         (btess->tess1d[n-1].faces[0].nface+1)*sizeof(int));
                if (finds == NULL) {
                  if (outLevel > 0)
                    printf(" EGADS Error: ReAlloc (-) Edge %d (EG_tessEdges)!\n", n);
                  EG_free(faces);
                  EG_free(edges);
                  return EGADS_MALLOC;
                } 
                finds[btess->tess1d[n-1].faces[0].nface] = i+1;
                btess->tess1d[n-1].faces[0].faces = finds;
              }
            }
            btess->tess1d[n-1].faces[0].index = i+1;
            btess->tess1d[n-1].faces[0].nface++;
          } else {
            if (btess->tess1d[n-1].faces[1].nface != 0) {
              if (btess->tess1d[n-1].faces[1].nface == 1) {
                btess->tess1d[n-1].faces[1].faces = (int *) EG_alloc(2*sizeof(int));
                if (btess->tess1d[n-1].faces[1].faces == NULL) {
                  if (outLevel > 0)
                    printf(" EGADS Error: Alloc (+) Edge %d (EG_tessEdges)!\n", n);
                  EG_free(faces);
                  EG_free(edges);
                  return EGADS_MALLOC;
                }
                btess->tess1d[n-1].faces[1].faces[0] = btess->tess1d[n-1].faces[1].index;
                btess->tess1d[n-1].faces[1].faces[1] = i+1;
              } else {
                finds = (int *) EG_reall( btess->tess1d[n-1].faces[1].faces,
                                         (btess->tess1d[n-1].faces[1].nface+1)*sizeof(int));
                if (finds == NULL) {
                  if (outLevel > 0)
                    printf(" EGADS Error: ReAlloc (+) Edge %d (EG_tessEdges)!\n", n);
                  EG_free(faces);
                  EG_free(edges);
                  return EGADS_MALLOC;
                }
                finds[btess->tess1d[n-1].faces[1].nface] = i+1;
                btess->tess1d[n-1].faces[1].faces = finds;
              }
            }
            btess->tess1d[n-1].faces[1].index = i+1;
            btess->tess1d[n-1].faces[1].nface++;
          }
        }
      }
    }
    /* report any non-manifold Edges */
    if (outLevel > 1)
      for (j = 0; j < nedge; j++) {
        if (btess->tess1d[j].faces[0].nface > 1) {
          printf(" EGADS Internal: Non-manifold Edge %d (-) with Faces", j+1);
          if (btess->tess1d[j].faces[0].faces != NULL)
            for (k = 0; k < btess->tess1d[j].faces[0].nface; k++)
              printf(" %d", btess->tess1d[j].faces[0].faces[k]);
          printf("!\n");
        }
        if (btess->tess1d[j].faces[1].nface > 1) {
          printf(" EGADS Internal: Non-manifold Edge %d (+) with Faces", j+1);
          if (btess->tess1d[j].faces[1].faces != NULL)
            for (k = 0; k < btess->tess1d[j].faces[1].nface; k++)
              printf(" %d", btess->tess1d[j].faces[1].faces[k]);
          printf("!\n");
        }
      }
  }

  /* do the Edges -- one at a time */

  dist = fabs(btess->params[2]);
  if (dist > 30.0) dist = 30.0;
  if (dist <  0.5) dist =  0.5;
  dotnrm = cos(PI*dist/180.0);

  for (j = 0; j < nedge; j++) {
    if (retess != NULL)
      if (retess[j] == 0) continue;
    stat = EG_getTopology(edges[j], &geom, &oclass, &mtype, limits,
                          &nnode, &nodes, &senses);
    if (stat != EGADS_SUCCESS) {
      EG_free(faces);
      EG_free(edges);
      return stat;
    }
#ifdef DEBUG
    printf(" Edge %d: type = %d, geom type = %d, limits = %lf %lf, nnode = %d\n",
           j+1, mtype, geom->mtype, limits[0], limits[1], nnode);
#endif
           
    /* set end points */
    stat = EG_getTopology(nodes[0], &ref, &oclass, &ntype, xyz[0],
                          &ndum, &dum, &senses);
    if (stat != EGADS_SUCCESS) {
      EG_free(faces);
      EG_free(edges);
      return stat;
    }
    npts      = 2;
    t[0]      = limits[0];
    xyz[1][0] = xyz[0][0];
    xyz[1][1] = xyz[0][1];
    xyz[1][2] = xyz[0][2];
    t[1]      = limits[1];
    btess->tess1d[j].nodes[0] = EG_indexBodyTopo(body, nodes[0]);
    btess->tess1d[j].nodes[1] = btess->tess1d[j].nodes[0];
    if (mtype == TWONODE) {
      stat = EG_getTopology(nodes[1], &ref, &oclass, &ntype, xyz[1],
                            &ndum, &dum, &senses);
      if (stat != EGADS_SUCCESS) {
        EG_free(faces);
        EG_free(edges);
        return stat;
      }
      btess->tess1d[j].nodes[1] = EG_indexBodyTopo(body, nodes[1]);
    }

    /* degenerate -- finish up */
    if (mtype == DEGENERATE) {
      btess->tess1d[j].xyz = (double *) EG_alloc(3*npts*sizeof(double));
      if (btess->tess1d[j].xyz == NULL) {
        if (outLevel > 0)
          printf(" EGADS Error: Alloc %d Pts Edge %d (EG_tessEdges)!\n", 
                 npts, j+1);
        EG_free(faces);
        EG_free(edges);
        return EGADS_MALLOC;  
      }
      btess->tess1d[j].t = (double *) EG_alloc(npts*sizeof(double));
      if (btess->tess1d[j].t == NULL) {
        EG_free(btess->tess1d[j].xyz);
        btess->tess1d[j].xyz = NULL;
        if (outLevel > 0)
          printf(" EGADS Error: Alloc %d Ts Edge %d (EG_tessEdges)!\n", 
                 npts, j+1);
        EG_free(faces);
        EG_free(edges);
        return EGADS_MALLOC;  
      }
      for (i = 0; i < npts; i++) {
        btess->tess1d[j].xyz[3*i  ] = xyz[i][0];
        btess->tess1d[j].xyz[3*i+1] = xyz[i][1];
        btess->tess1d[j].xyz[3*i+2] = xyz[i][2];
        btess->tess1d[j].t[i]       = t[i];
      }
      btess->tess1d[j].npts = npts;
      continue;
    }
    
    /* get minimum distance */
    stat = EG_evaluate(edges[j], &t[0], result);
    if (stat != EGADS_SUCCESS) {
      EG_free(faces);
      EG_free(edges);
      return stat;
    }
    mindist = (xyz[0][0]-result[0])*(xyz[0][0]-result[0]) +
              (xyz[0][1]-result[1])*(xyz[0][1]-result[1]) +
              (xyz[0][2]-result[2])*(xyz[0][0]-result[2]);
    stat = EG_evaluate(edges[j], &t[1], result);
    if (stat != EGADS_SUCCESS) {
      EG_free(faces);
      EG_free(edges);
      return stat;
    }
    dist = (xyz[1][0]-result[0])*(xyz[1][0]-result[0]) +
           (xyz[1][1]-result[1])*(xyz[1][1]-result[1]) +
           (xyz[1][2]-result[2])*(xyz[1][0]-result[2]);
    if (dist > mindist) mindist = dist;
    mindist = sqrt(mindist);
    if (0.1*btess->params[1] > mindist) 
      mindist = 0.1*btess->params[1];
#ifdef DEBUG
    printf("     minDist = %le\n", mindist);
#endif
    
    /* periodic -- add a vertex */
    if (mtype == ONENODE) {
      xyz[2][0] = xyz[1][0];
      xyz[2][1] = xyz[1][1];
      xyz[2][2] = xyz[1][2];
      aux[2][0] = aux[1][0];
      aux[2][1] = aux[1][1];
      aux[2][2] = aux[1][2];
      t[2]      = t[1];
      t[1]      = 0.5*(t[0]+t[2]);
      stat      = EG_evaluate(edges[j], &t[1], result);
      if (stat != EGADS_SUCCESS) {
        EG_free(faces);
        EG_free(edges);
        return stat;
      }
      dist      = sqrt(result[3]*result[3] + result[4]*result[4] +
                       result[5]*result[5]);
      if (dist == 0) dist = 1.0;
      xyz[1][0] = result[0];
      xyz[1][1] = result[1];
      xyz[1][2] = result[2];
      aux[1][0] = result[3]/dist;
      aux[1][1] = result[4]/dist;
      aux[1][2] = result[5]/dist;
      npts      = 3;
    }

    /* non-linear curve types */    
    if (geom->mtype != LINE) {
      
      /* angle criteria - aux is normalized tangent */
      if (btess->params[2] != 0.0) {
        stat = EG_evaluate(edges[j], &t[0], result);
        if (stat != EGADS_SUCCESS) {
          EG_free(faces);
          EG_free(edges);
          return stat;
        }
        dist = sqrt(result[3]*result[3] + result[4]*result[4] +
                    result[5]*result[5]);
        if (dist == 0) dist = 1.0;
        aux[0][0] = result[3]/dist;
        aux[0][1] = result[4]/dist;
        aux[0][2] = result[5]/dist;
        stat = EG_evaluate(edges[j], &t[npts-1], result);
        if (stat != EGADS_SUCCESS) {
          EG_free(faces);
          EG_free(edges);
          return stat;
        }
        dist = sqrt(result[3]*result[3] + result[4]*result[4] +
                    result[5]*result[5]);
        if (dist == 0) dist = 1.0;
        aux[npts-1][0] = result[3]/dist;
        aux[npts-1][1] = result[4]/dist;
        aux[npts-1][2] = result[5]/dist;

        while (npts < MAXELEN) {
          /* find the segment with the largest angle */
          k   = -1;
          dot =  1.0;
          for (i = 0; i < npts-1; i++) {
            dist = (xyz[i][0]-xyz[i+1][0])*(xyz[i][0]-xyz[i+1][0]) +
                   (xyz[i][1]-xyz[i+1][1])*(xyz[i][1]-xyz[i+1][1]) +
                   (xyz[i][2]-xyz[i+1][2])*(xyz[i][2]-xyz[i+1][2]);
            if (dist < mindist*mindist) continue;
            d = aux[i][0]*aux[i+1][0] + aux[i][1]*aux[i+1][1] + 
                aux[i][2]*aux[i+1][2];
            if (d < dot) {
              dot = d;
              k   = i;
            }
          }
          if ((dot > dotnrm) || (k == -1)) break;
          /* insert */
          for (i = npts-1; i > k; i--) {
            xyz[i+1][0] = xyz[i][0];
            xyz[i+1][1] = xyz[i][1];
            xyz[i+1][2] = xyz[i][2];
            aux[i+1][0] = aux[i][0];
            aux[i+1][1] = aux[i][1];
            aux[i+1][2] = aux[i][2];
            t[i+1]      = t[i];
          }
          t[k+1] = 0.5*(t[k]+t[k+2]);
          stat   = EG_evaluate(edges[j], &t[k+1], result);
          if (stat != EGADS_SUCCESS) {
            EG_free(faces);
            EG_free(edges);
            return stat;
          }
          dist   = sqrt(result[3]*result[3] + result[4]*result[4] +
                        result[5]*result[5]);
          if (dist == 0.0) dist = 1.0;
          xyz[k+1][0] = result[0];
          xyz[k+1][1] = result[1];
          xyz[k+1][2] = result[2];
          aux[k+1][0] = result[3]/dist;
          aux[k+1][1] = result[4]/dist;
          aux[k+1][2] = result[5]/dist;
          npts++;
        }
#ifdef DEBUG
        printf("     Angle  Phase npts = %d @ %lf (%lf)\n",
               npts, dot, dotnrm);
#endif
      }
      
      /* sag - aux is midpoint value */
      if (btess->params[1] > 0.0) {
        for (i = 0; i < npts-1; i++) {
          d    = 0.5*(t[i]+t[i+1]);
          stat = EG_evaluate(edges[j], &d, result);
          if (stat != EGADS_SUCCESS) {
            EG_free(faces);
            EG_free(edges);
            return stat;
          }
          aux[i][0] = result[0];
          aux[i][1] = result[1];
          aux[i][2] = result[2];
        }
        while (npts < MAXELEN) {
          /* find the biggest deviation */
          k    = -1;
          dist = 0.0;
          for (i = 0; i < npts-1; i++) {
            dot = (xyz[i][0]-xyz[i+1][0])*(xyz[i][0]-xyz[i+1][0]) +
                  (xyz[i][1]-xyz[i+1][1])*(xyz[i][1]-xyz[i+1][1]) +
                  (xyz[i][2]-xyz[i+1][2])*(xyz[i][2]-xyz[i+1][2]);
            if (dot < mindist*mindist) continue;
            mid[0] = 0.5*(xyz[i][0] + xyz[i+1][0]);
            mid[1] = 0.5*(xyz[i][1] + xyz[i+1][1]);
            mid[2] = 0.5*(xyz[i][2] + xyz[i+1][2]);
            d      = (aux[i][0]-mid[0])*(aux[i][0]-mid[0]) +
                     (aux[i][1]-mid[1])*(aux[i][1]-mid[1]) +
                     (aux[i][2]-mid[2])*(aux[i][2]-mid[2]);
            if (d > dist) {
              dist = d;
              k    = i;
            }
          }
          if ((dist < btess->params[1]*btess->params[1]) || 
              (k == -1)) break;
          /* insert */
          for (i = npts-1; i > k; i--) {
            xyz[i+1][0] = xyz[i][0];
            xyz[i+1][1] = xyz[i][1];
            xyz[i+1][2] = xyz[i][2];
            aux[i+1][0] = aux[i][0];
            aux[i+1][1] = aux[i][1];
            aux[i+1][2] = aux[i][2];
            t[i+1]      = t[i];
          }
          t[k+1]      = 0.5*(t[k]+t[k+2]);
          xyz[k+1][0] = aux[k][0];
          xyz[k+1][1] = aux[k][1];
          xyz[k+1][2] = aux[k][2];
          d    = 0.5*(t[k+1]+t[k+2]);
          stat = EG_evaluate(edges[j], &d, result);
          if (stat != EGADS_SUCCESS) {
            EG_free(faces);
            EG_free(edges);
            return stat;
          }
          aux[k+1][0] = result[0];
          aux[k+1][1] = result[1];
          aux[k+1][2] = result[2];
          d    = 0.5*(t[k]+t[k+1]);
          stat = EG_evaluate(edges[j], &d, result);
          if (stat != EGADS_SUCCESS) {
            EG_free(faces);
            EG_free(edges);
            return stat;
          }
          aux[k][0] = result[0];
          aux[k][1] = result[1];
          aux[k][2] = result[2];
          npts++;
        }
#ifdef DEBUG
        printf("     Sag    Phase npts = %d @ %lf (%lf)\n", 
               npts, sqrt(dist), btess->params[1]);
#endif
      }     
    }
    
    /* look at non-planar faces for curvature -- aux is uv*/
    if (btess->params[2] > 0.0)
      for (n = 0; n < 2; n++) {
                    sense =  1;
        if (n == 0) sense = -1;
        for (nf = 0; nf < btess->tess1d[j].faces[n].nface; nf++) {
          face = btess->tess1d[j].faces[n].index;
          if (btess->tess1d[j].faces[n].nface > 1)
            face = btess->tess1d[j].faces[n].faces[nf];
          if (face <= 0) continue;
          stat = EG_getTopology(faces[face-1], &ref, &oclass, &ntype, range,
                                &ndum, &dum, &senses);
          if (stat != EGADS_SUCCESS) continue;
          if (ref == NULL) continue;
          if (ref->mtype == PLANE) continue;
          stat = EG_getTolerance(faces[face-1], &tol);
          if (stat != EGADS_SUCCESS) continue;
          if (btess->params[1] > tol) tol = btess->params[1];

          for (i = 0; i < npts; i++) {
            aux[i][2] = 1.0;
            stat = EG_getEdgeUV(faces[face-1], edges[j], sense, t[i], 
                                aux[i]);
            if (stat != EGADS_SUCCESS) aux[i][2] = 0.0;
          }
          for (i = 0; i < npts-1; i++) {
            if (aux[i][2]   <= 0.0) continue;
            if (aux[i+1][2] == 0.0) continue;
            dx[0] = xyz[i+1][0] - xyz[i][0];
            dx[1] = xyz[i+1][1] - xyz[i][1];
            dx[2] = xyz[i+1][2] - xyz[i][2];
            d     = DOT(dx, dx);
            if (d < tol*tol) {
              aux[i][2] = -1.0;
              continue;
            }
            /* get normal at mid-point in UV */
            dot = EG_curvNorm(faces[face-1], i, sense*ntype, d, dx, aux);
            if ((dot > dotnrm) || (dot < -1.1)) aux[i][2] = -1.0;
          }

          while (npts < MAXELEN) {
            /* find the largest segment with Face curvature too big */
            k    = -1;
            dist =  tol*tol;
            for (i = 0; i < npts-1; i++) {
              if (aux[i][2]   <= 0.0) continue;
              if (aux[i+1][2] == 0.0) continue;
              dx[0] = xyz[i+1][0] - xyz[i][0];
              dx[1] = xyz[i+1][1] - xyz[i][1];
              dx[2] = xyz[i+1][2] - xyz[i][2];
              d     = DOT(dx, dx);
              if (d < tol*tol) {
                aux[i][2] = -1.0;
                continue;
              }
              if (d < dist) continue;
              dist = d;
              k    = i;
            }
            if (k == -1) break;

            /* insert */
            for (i = npts-1; i > k; i--) {
              xyz[i+1][0] = xyz[i][0];
              xyz[i+1][1] = xyz[i][1];
              xyz[i+1][2] = xyz[i][2];
              aux[i+1][0] = aux[i][0];
              aux[i+1][1] = aux[i][1];
              aux[i+1][2] = aux[i][2];
              t[i+1]      = t[i];
            }
            t[k+1] = 0.5*(t[k]+t[k+2]);
            stat   = EG_evaluate(edges[j], &t[k+1], result);
            if (stat != EGADS_SUCCESS) {
              EG_free(faces);
              EG_free(edges);
              return stat;
            }
            xyz[k+1][0] = result[0];
            xyz[k+1][1] = result[1];
            xyz[k+1][2] = result[2];
            aux[k+1][2] = 1.0;
            stat = EG_getEdgeUV(faces[face-1], edges[j], sense, t[k+1], 
                                aux[k+1]);
            if (stat != EGADS_SUCCESS) aux[k+1][2] = 0.0;
            dx[0] = xyz[k+1][0] - xyz[k][0];
            dx[1] = xyz[k+1][1] - xyz[k][1];
            dx[2] = xyz[k+1][2] - xyz[k][2];
            d     = DOT(dx, dx);
            dot   = EG_curvNorm(faces[face-1], k, sense*ntype, d, dx, aux);
            if ((dot > dotnrm) || (dot < -1.1)) aux[k][2] = -1.0;      
            dx[0] = xyz[k+2][0] - xyz[k+1][0];
            dx[1] = xyz[k+2][1] - xyz[k+1][1];
            dx[2] = xyz[k+2][2] - xyz[k+1][2];
            d     = DOT(dx, dx);
            dot   = EG_curvNorm(faces[face-1], k+1, sense*ntype, d, dx, aux);
            if ((dot > dotnrm) || (dot < -1.1)) aux[k+1][2] = -1.0;
            npts++;
          }
#ifdef DEBUG
          printf("     FacNrm Phase npts = %d @ %lf  Face = %d\n",
                 npts, dotnrm, face);
#endif
        }
      }
    
    /* max side -- for all curve types */

    if (btess->params[0] > 0.0) {
      for (i = 0; i < npts-1; i++)
        aux[i][0] = (xyz[i][0]-xyz[i+1][0])*(xyz[i][0]-xyz[i+1][0]) +
                    (xyz[i][1]-xyz[i+1][1])*(xyz[i][1]-xyz[i+1][1]) +
                    (xyz[i][2]-xyz[i+1][2])*(xyz[i][2]-xyz[i+1][2]);
      aux[npts-1][0] = 0.0;
      while (npts < MAXELEN) {
        /* find the biggest segment */
        k    = 0;
        dist = aux[0][0];
        for (i = 1; i < npts-1; i++) {
          d = aux[i][0];
          if (d > dist) {
            dist = d;
            k    = i;
          }
        }
        if (dist < btess->params[0]*btess->params[0]) break;
        /* insert */
        for (i = npts-1; i > k; i--) {
          xyz[i+1][0] = xyz[i][0];
          xyz[i+1][1] = xyz[i][1];
          xyz[i+1][2] = xyz[i][2];
          aux[i+1][0] = aux[i][0];
          t[i+1]      = t[i];
        }
        t[k+1] = 0.5*(t[k]+t[k+2]);
        stat   = EG_evaluate(edges[j], &t[k+1], result);
        if (stat != EGADS_SUCCESS) {
          EG_free(faces);
          EG_free(edges);
          return stat;
        }
        xyz[k+1][0] = result[0];
        xyz[k+1][1] = result[1];
        xyz[k+1][2] = result[2];
        npts++;
        d = (xyz[k][0]-xyz[k+1][0])*(xyz[k][0]-xyz[k+1][0]) +
            (xyz[k][1]-xyz[k+1][1])*(xyz[k][1]-xyz[k+1][1]) +
            (xyz[k][2]-xyz[k+1][2])*(xyz[k][2]-xyz[k+1][2]);
        aux[k][0] = d;
        if (d < 0.0625*btess->params[0]*btess->params[0]) break;
        d = (xyz[k+2][0]-xyz[k+1][0])*(xyz[k+2][0]-xyz[k+1][0]) +
            (xyz[k+2][1]-xyz[k+1][1])*(xyz[k+2][1]-xyz[k+1][1]) +
            (xyz[k+2][2]-xyz[k+1][2])*(xyz[k+2][2]-xyz[k+1][2]);
        aux[k+1][0] = d;
        if (d < 0.0625*btess->params[0]*btess->params[0]) break;
      }
    }
#ifdef DEBUG
    if (btess->params[0] > 0.0)
      printf("     MxSide Phase npts = %d @ %lf (%lf)\n", 
             npts, sqrt(dist), btess->params[0]);
#endif
    
    /* fill in the 1D structure */
    btess->tess1d[j].xyz = (double *) EG_alloc(3*npts*sizeof(double));
    if (btess->tess1d[j].xyz == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Alloc %d Pts Edge %d (EG_tessEdges)!\n", 
               npts, j+1);
      EG_free(faces);
      EG_free(edges);
      return EGADS_MALLOC;  
    }
    btess->tess1d[j].t = (double *) EG_alloc(npts*sizeof(double));
    if (btess->tess1d[j].t == NULL) {
      EG_free(btess->tess1d[j].xyz);
      btess->tess1d[j].xyz = NULL;
      if (outLevel > 0)
        printf(" EGADS Error: Alloc %d Ts Edge %d (EG_tessEdges)!\n", 
               npts, j+1);
      EG_free(faces);
      EG_free(edges);
      return EGADS_MALLOC;  
    }
    nf = btess->tess1d[j].faces[0].nface;
    if (nf > 0) {
      btess->tess1d[j].faces[0].tric = (int *) EG_alloc((nf*(npts-1))*sizeof(int));
      if (btess->tess1d[j].faces[0].tric == NULL) {
        EG_free(btess->tess1d[j].t);
        btess->tess1d[j].t   = NULL;
        EG_free(btess->tess1d[j].xyz);
        btess->tess1d[j].xyz = NULL;
        if (outLevel > 0)
          printf(" EGADS Error: Alloc %d Tric- Edge %d (EG_tessEdges)!\n", 
                 npts, j+1);
        EG_free(faces);
        EG_free(edges);
        return EGADS_MALLOC;
      }
    }
    nf = btess->tess1d[j].faces[1].nface;
    if (nf > 0) {
      btess->tess1d[j].faces[1].tric = (int *) EG_alloc((nf*(npts-1))*sizeof(int));
      if (btess->tess1d[j].faces[1].tric == NULL) {
        if (btess->tess1d[j].faces[0].tric != NULL)
          EG_free(btess->tess1d[j].faces[0].tric);
        btess->tess1d[j].faces[0].tric = NULL;
        EG_free(btess->tess1d[j].t);
        btess->tess1d[j].t   = NULL;
        EG_free(btess->tess1d[j].xyz);
        btess->tess1d[j].xyz = NULL;
        if (outLevel > 0)
          printf(" EGADS Error: Alloc %d Tric+ Edge %d (EG_tessEdges)!\n", 
                 npts, j+1);
        EG_free(faces);
        EG_free(edges);
        return EGADS_MALLOC;
      }
    }
    for (i = 0; i < npts; i++) {
      btess->tess1d[j].xyz[3*i  ] = xyz[i][0];
      btess->tess1d[j].xyz[3*i+1] = xyz[i][1];
      btess->tess1d[j].xyz[3*i+2] = xyz[i][2];
      btess->tess1d[j].t[i]       = t[i];
    }
    for (i = 0; i < npts-1; i++) {
      nf = btess->tess1d[j].faces[0].nface;
      for (k = 0; k < nf; k++)
        btess->tess1d[j].faces[0].tric[i*nf+k] = 0.0;
      nf = btess->tess1d[j].faces[1].nface;
      for (k = 0; k < nf; k++)
        btess->tess1d[j].faces[1].tric[i*nf+k] = 0.0;
    }
    btess->tess1d[j].npts = npts;
  }

  EG_free(faces);
  EG_free(edges);
  return EGADS_SUCCESS;
}


int
EG_makeTessGeom(egObject *obj, double *params, int *sizes, egObject **tess)
{
  int      i, j, k, stat, outLevel, np, nu, nv = 0;
  double   *dtess, uv[2], result[18];
  egTessel *btess;
  egObject *gtess, *context;

  *tess = NULL;
  if  (obj == NULL)               return EGADS_NULLOBJ;
  if  (obj->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if ((obj->oclass != SURFACE) && (obj->oclass != CURVE)) return EGADS_NOTGEOM;
  outLevel = EG_outLevel(obj);
  context  = EG_context(obj);

  nu = abs(sizes[0]);
  np = nu;
  if (obj->oclass == SURFACE) {
    nv = abs(sizes[1]);
    if ((nu < 2) || (nv < 2)) {
      if (outLevel > 0)
        printf(" EGADS Error: Surface size = %d %d (EG_makeTessGeom)!\n",
               nu, nv);
      return EGADS_INDEXERR;
    }
    np *= nv;
  } else {
    if (nu < 2) {
      if (outLevel > 0)
        printf(" EGADS Error: Curve len = %d (EG_makeTessGeom)!\n", nu);
      return EGADS_INDEXERR;
    }
  }
  
  btess = EG_alloc(sizeof(egTessel));
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Blind Malloc (EG_makeTessGeom)!\n");
    return EGADS_MALLOC;
  }
  btess->src    = obj;
  btess->xyzs   = NULL;
  btess->tess1d = NULL;
  btess->tess2d = NULL;
  btess->nEdge  = 0;
  btess->nFace  = 0;
  btess->nu     = nu;
  btess->nv     = nv;
  
  /* get the storage for the tessellation */
  dtess = (double *) EG_alloc(3*np*sizeof(double));
  if (dtess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Data Malloc (EG_makeTessGeom)!\n");
    EG_free(btess);
    return EGADS_MALLOC;
  }
  
  stat = EG_makeObject(context, &gtess);
  if (stat != EGADS_SUCCESS) {
    EG_free(dtess);
    EG_free(btess);
    return stat;
  }
  gtess->oclass = TESSELLATION;
  gtess->mtype  = obj->oclass;
  gtess->blind  = btess;
  EG_referenceObject(gtess, context);
  EG_referenceTopObj(obj,   gtess);
  
  /* fill the data */
  
  if (obj->oclass == SURFACE) {
  
    for (k = j = 0; j < nv; j++) {
      if (sizes[1] < 0) {
        uv[1] = params[2] + (nv-j-1)*(params[3]-params[2])/(nv-1);
      } else {
        uv[1] = params[2] +        j*(params[3]-params[2])/(nv-1);
      }
      for (i = 0; i < nu; i++, k++) {
        if (sizes[0] < 0) {
          uv[0] = params[0] + (nu-i-1)*(params[1]-params[0])/(nu-1);
        } else {
          uv[0] = params[0] +        i*(params[1]-params[0])/(nu-1);
        }
        stat  = EG_evaluate(obj, uv, result);
        dtess[3*k  ] = result[0];
        dtess[3*k+1] = result[1];
        dtess[3*k+2] = result[2];
        if (stat == EGADS_SUCCESS) continue;
        if (outLevel > 0)
          printf(" EGADS Warning: %d/%d, %d/%d eval ret = %d  (EG_makeTessGeom)!\n",
                 i+1, nv, j+1, nv, stat);
      }
    }

  } else {
  
    for (i = 0; i < nu; i++) {
      if (sizes[0] < 0) {
        uv[0] = params[0] + (nu-i-1)*(params[1]-params[0])/(nu-1);
      } else {
        uv[0] = params[0] +        i*(params[1]-params[0])/(nu-1);
      }
      stat  = EG_evaluate(obj, uv, result);
      dtess[3*i  ] = result[0];
      dtess[3*i+1] = result[1];
      dtess[3*i+2] = result[2];
      if (stat == EGADS_SUCCESS) continue;
      if (outLevel > 0)
        printf(" EGADS Warning: %d/%d evaluate ret = %d  (EG_makeTessGeom)!\n",
               i+1, nv, stat);
    }

  }
  
  btess->xyzs      = dtess;
  btess->params[0] = params[0];
  btess->params[1] = params[1];
  btess->params[2] = nu;
  if (nv == 0) {
    btess->params[3] = btess->params[4] = btess->params[5] = 0.0;
  } else {
    btess->params[3] = params[2];
    btess->params[4] = params[3];
    btess->params[5] = nv;
  }

  *tess = gtess;
  return EGADS_SUCCESS;
}


int
EG_getTessGeom(const egObject *tess, int *sizes, double **xyz)
{
  int      outLevel;
  egTessel *btess;
  egObject *obj;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);

  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getTessGeom)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getTessGeom)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getTessGeom)!\n");
    return EGADS_NOTOBJ;
  }
  if ((obj->oclass != SURFACE) && (obj->oclass != CURVE)) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not a Curve/Surface (EG_getTessGeom)!\n");
    return EGADS_NOTGEOM;
  }

  sizes[0] = btess->nu;
  sizes[1] = btess->nv;
  *xyz     = btess->xyzs;
  return EGADS_SUCCESS;
}


static void
EG_deleteQuads(egTessel *btess, int iface)
{
  int i, j;
  
  i = btess->nFace + iface - 1;
  if (btess->tess2d[i].xyz    != NULL) EG_free(btess->tess2d[i].xyz);
  if (btess->tess2d[i].uv     != NULL) EG_free(btess->tess2d[i].uv);
  if (btess->tess2d[i].ptype  != NULL) EG_free(btess->tess2d[i].ptype);
  if (btess->tess2d[i].pindex != NULL) EG_free(btess->tess2d[i].pindex);
  for (j = 0; j < btess->tess2d[i].npatch; j++) {
    if (btess->tess2d[i].patch[j].ipts != NULL) 
      EG_free(btess->tess2d[i].patch[j].ipts);
    if (btess->tess2d[i].patch[j].bounds != NULL) 
      EG_free(btess->tess2d[i].patch[j].bounds);
  }
  EG_free(btess->tess2d[i].patch);
  btess->tess2d[i].xyz    = NULL;
  btess->tess2d[i].uv     = NULL;
  btess->tess2d[i].ptype  = NULL;
  btess->tess2d[i].pindex = NULL;
  btess->tess2d[i].npts   = 0;
  btess->tess2d[i].patch  = NULL;
  btess->tess2d[i].npatch = 0;
}


int
EG_moveEdgeVert(egObject *tess, int eIndex, int vIndex, double t)
{
  int      i, j, m, nf, stat, outLevel, nedge, nface, iface, itri, ivrt;
  int      sense;
  double   result[9], uv[2];
  egTessel *btess;
  egObject *obj, **edges, **faces;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_moveEdgeVert)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_moveEdgeVert)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_moveEdgeVert)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_moveEdgeVert)!\n");
    return EGADS_NOTBODY;
  }
  if (obj->mtype == WIREBODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source is WireBody (EG_moveEdgeVert)!\n");
    return EGADS_TOPOERR;
  }
  if (btess->tess1d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Edge Tessellations (EG_moveEdgeVert)!\n");
    return EGADS_NODATA;  
  }
  if ((eIndex < 1) || (eIndex > btess->nEdge)) {
    if (outLevel > 0)
      printf(" EGADS Error: eIndex = %d [1-%d] (EG_moveEdgeVert)!\n",
             eIndex, btess->nEdge);
    return EGADS_INDEXERR;
  }
  if ((vIndex < 2) || (eIndex >= btess->tess1d[eIndex-1].npts)) {
    if (outLevel > 0)
      printf(" EGADS Error: vIndex = %d [2-%d] (EG_moveEdgeVert)!\n",
             vIndex, btess->tess1d[eIndex-1].npts-1);
    return EGADS_INDEXERR;
  }
  if ((t <= btess->tess1d[eIndex-1].t[vIndex-2]) ||
      (t >= btess->tess1d[eIndex-1].t[vIndex])) {
    if (outLevel > 0)
      printf(" EGADS Error: t = %lf [%lf-%lf] (EG_moveEdgeVert)!\n",
             t, btess->tess1d[eIndex-1].t[vIndex-2], 
                btess->tess1d[eIndex-1].t[vIndex]);
    return EGADS_RANGERR;
  }
  stat = EG_getBodyTopos(btess->src, NULL, EDGE, &nedge, &edges);
  if (stat != EGADS_SUCCESS) return stat;
  stat = EG_getBodyTopos(btess->src, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) {
    EG_free(edges);
    return stat;
  }
  
  stat = EG_evaluate(edges[eIndex-1], &t, result);
  if (stat != EGADS_SUCCESS) {
    EG_free(faces);
    EG_free(edges);
    return stat;
  }
  /* make sure we can get UVs */
  for (m = 0; m < 2; m++) {
    nf = btess->tess1d[eIndex-1].faces[m].nface;
    for (j = 0; j < nf; j++) {
      iface = btess->tess1d[eIndex-1].faces[m].index;
      if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[j];
      if (iface != 0) {
        sense = faces[iface-1]->mtype;
        if (EG_faceConnIndex(btess->tess1d[eIndex-1].faces[1-m], iface) == 0)
          sense = 0;
        if (m == 0) sense = -sense;
        stat = EG_getEdgeUV(faces[iface-1], edges[eIndex-1], sense, t, uv);
        if (stat != EGADS_SUCCESS) {
          EG_free(faces);
          EG_free(edges);
          return stat;
        }
      }
    }
  }
  
  /* got everything -- update the tessellation */
  btess->tess1d[eIndex-1].xyz[3*vIndex-3] = result[0];
  btess->tess1d[eIndex-1].xyz[3*vIndex-2] = result[1];
  btess->tess1d[eIndex-1].xyz[3*vIndex-1] = result[2];
  btess->tess1d[eIndex-1].t[vIndex-1]     = t;
  for (m = 0; m < 2; m++) {
    nf = btess->tess1d[eIndex-1].faces[m].nface;
    for (j = 0; j < nf; j++) {
      iface = btess->tess1d[eIndex-1].faces[m].index;
      if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[j];
      if (iface == 0) continue;
      sense = faces[iface-1]->mtype;
      if (EG_faceConnIndex(btess->tess1d[eIndex-1].faces[1-m], iface) == 0)
        sense = 0;
      if (m == 0) sense = -sense;
      EG_getEdgeUV(faces[iface-1], edges[eIndex-1], sense, t, uv);
      itri = btess->tess1d[eIndex-1].faces[m].tric[(vIndex-1)*nf+j] - 1;
      for (i = 0; i < 3; i++) {
        ivrt = btess->tess2d[iface-1].tris[3*itri+i] - 1;
        if ((btess->tess2d[iface-1].pindex[ivrt] == eIndex) &&
            (btess->tess2d[iface-1].ptype[ivrt]  == vIndex)) {
          btess->tess2d[iface-1].xyz[3*ivrt  ] = result[0];
          btess->tess2d[iface-1].xyz[3*ivrt+1] = result[1];
          btess->tess2d[iface-1].xyz[3*ivrt+2] = result[2];
          btess->tess2d[iface-1].uv[2*ivrt  ]  = uv[0];
          btess->tess2d[iface-1].uv[2*ivrt+1]  = uv[1];
          break;
        }
      }
    }
    /* delete any quads */
    EG_deleteQuads(btess, iface);
  }
  EG_free(faces);
  EG_free(edges);
  
  return EGADS_SUCCESS;
}


int
EG_deleteEdgeVert(egObject *tess, int eIndex, int vIndex, int dir)
{
  int      i, k, m, n, nf, outLevel, iface, iv[2], it, ivert;
  int      n1, n2, ie, i1, i2, i3, pt1, pi1, pt2, pi2, ref, nfr;
  egTessel *btess;
  egObject *obj;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  if ((dir != -1) && (dir != 1)) {
    if (outLevel > 0)
      printf(" EGADS Error: Collapse Dir = %d (EG_deleteEdgeVert)!\n",
             dir);  
    return EGADS_RANGERR;
  }
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_deleteEdgeVert)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_deleteEdgeVert)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_deleteEdgeVert)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_deleteEdgeVert)!\n");
    return EGADS_NOTBODY;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_deleteEdgeVert)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess1d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Edge Tessellations (EG_deleteEdgeVert)!\n");
    return EGADS_NODATA;  
  }
  if ((eIndex < 1) || (eIndex > btess->nEdge)) {
    if (outLevel > 0)
      printf(" EGADS Error: eIndex = %d [1-%d] (EG_deleteEdgeVert)!\n",
             eIndex, btess->nEdge);
    return EGADS_INDEXERR;
  }
  if ((vIndex < 2) || (eIndex >= btess->tess1d[eIndex-1].npts)) {
    if (outLevel > 0)
      printf(" EGADS Error: vIndex = %d [2-%d] (EG_deleteEdgeVert)!\n",
             vIndex, btess->tess1d[eIndex-1].npts-1);
    return EGADS_INDEXERR;
  }
 
  /* fix up each face */
  for (m = 0; m < 2; m++) {
    nf = btess->tess1d[eIndex-1].faces[m].nface;
    for (n = 0; n < nf; n++) {
      iface = btess->tess1d[eIndex-1].faces[m].index;
      if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[n];
      if (iface == 0) continue;
      if (dir == -1) {
        it = btess->tess1d[eIndex-1].faces[m].tric[nf*(vIndex-2)+n];
      } else {
        it = btess->tess1d[eIndex-1].faces[m].tric[nf*(vIndex-1)+n];
      }
      iv[0] = iv[1] = -1;
      /* find the vert to remove */
      for (i = 0; i < 3; i++) {
        ivert = btess->tess2d[iface-1].tris[3*it+i-3];
        if ((btess->tess2d[iface-1].pindex[ivert-1] == eIndex) &&
            (btess->tess2d[iface-1].ptype[ivert-1]  == vIndex)) {
          iv[0] = ivert;
          break;
        }
      }
      /* find the vert to collpse to */
      for (i = 0; i < 3; i++) {
        ivert = btess->tess2d[iface-1].tris[3*it+i-3];
        if ((btess->tess2d[iface-1].pindex[ivert-1] == eIndex) &&
            (btess->tess2d[iface-1].ptype[ivert-1]  == vIndex+dir)) {
          iv[1] = ivert;
          break;
        }
      }
      if ((iv[0] == -1) || (iv[1] == -1)) {
        printf(" EGADS Internal: EG_deleteEdgeVert Verts = %d %d!\n", iv[0], iv[1]);
        return EGADS_GEOMERR;
      }

      pt1 =       vIndex;
      pi1 = pi2 = eIndex;
      pt2 = vIndex + dir;
      if (pt2 == 1) {
        pt2 = 0;
        pi2 = btess->tess1d[eIndex-1].nodes[0];
      }
      if (pt2 == btess->tess1d[eIndex-1].npts) {
        pt2 = 0;
        pi2 = btess->tess1d[eIndex-1].nodes[1];
      }

      /* patch up the neighbors for the removed triangle */
      i1 = btess->tess2d[iface-1].tris[3*it-3]-1;
      i2 = btess->tess2d[iface-1].tris[3*it-2]-1;
      i3 = btess->tess2d[iface-1].tris[3*it-1]-1;        
      if (((btess->tess2d[iface-1].pindex[i2] == pi1) &&
           (btess->tess2d[iface-1].ptype[i2]  == pt1) &&
           (btess->tess2d[iface-1].pindex[i3] == pi2) &&
           (btess->tess2d[iface-1].ptype[i3]  == pt2)) || 
          ((btess->tess2d[iface-1].pindex[i2] == pi2) &&
           (btess->tess2d[iface-1].ptype[i2]  == pt2) &&
           (btess->tess2d[iface-1].pindex[i3] == pi1) &&
           (btess->tess2d[iface-1].ptype[i3]  == pt1))) {
        n1 = btess->tess2d[iface-1].tric[3*it-2];
        n2 = btess->tess2d[iface-1].tric[3*it-1];
      } else if (((btess->tess2d[iface-1].pindex[i1] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt1) &&
                  (btess->tess2d[iface-1].pindex[i3] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i3]  == pt2)) || 
                 ((btess->tess2d[iface-1].pindex[i1] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt2) &&
                  (btess->tess2d[iface-1].pindex[i3] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i3]  == pt1))) {
        n1 = btess->tess2d[iface-1].tric[3*it-3];
        n2 = btess->tess2d[iface-1].tric[3*it-1];
      } else if (((btess->tess2d[iface-1].pindex[i1] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt1) &&
                  (btess->tess2d[iface-1].pindex[i2] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i2]  == pt2)) || 
                 ((btess->tess2d[iface-1].pindex[i1] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt2) &&
                  (btess->tess2d[iface-1].pindex[i2] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i2]  == pt1))) {
        n1 = btess->tess2d[iface-1].tric[3*it-3];
        n2 = btess->tess2d[iface-1].tric[3*it-2];
      } else {
        printf(" EGADS Internal: Can not find segment for %d %d  %d %d - %d!\n",
               pt1, pi1, pt2, pi2, btess->tess1d[eIndex-1].npts);
        return EGADS_GEOMERR;
      }
#ifdef DEBUG
      printf(" verts = %d (%d %d) -> %d (%d %d)   tri = %d   ns = %d %d,  dir = %d\n", 
             iv[0], pt1, pi1, iv[1], pt2, pi2, it, n1, n2, dir);
      printf("      tri = %d   %d %d %d   %d %d %d\n", it,
             btess->tess2d[iface-1].tris[3*it-3], btess->tess2d[iface-1].tris[3*it-2],
             btess->tess2d[iface-1].tris[3*it-1], btess->tess2d[iface-1].tric[3*it-3],
             btess->tess2d[iface-1].tris[3*it-2], btess->tess2d[iface-1].tric[3*it-1]);
      printf("      tri = %d   %d %d %d   %d %d %d\n", n1,
             btess->tess2d[iface-1].tris[3*n1-3], btess->tess2d[iface-1].tris[3*n1-2],
             btess->tess2d[iface-1].tris[3*n1-1], btess->tess2d[iface-1].tric[3*n1-3],
             btess->tess2d[iface-1].tris[3*n1-2], btess->tess2d[iface-1].tric[3*n1-1]);
      printf("      tri = %d   %d %d %d   %d %d %d\n", n2,
             btess->tess2d[iface-1].tris[3*n2-3], btess->tess2d[iface-1].tris[3*n2-2],
             btess->tess2d[iface-1].tris[3*n2-1], btess->tess2d[iface-1].tric[3*n2-3],
             btess->tess2d[iface-1].tris[3*n2-2], btess->tess2d[iface-1].tric[3*n2-1]);
#endif
      if (n1 > 0) {
        for (i = 0; i < 3; i++)
          if (btess->tess2d[iface-1].tric[3*n1+i-3] == it) {
            btess->tess2d[iface-1].tric[3*n1+i-3] = n2;
            break;
          }
      } else if (n1 < 0) {
        ie  = -n1;
        ref = EG_faceConnIndex(btess->tess1d[ie-1].faces[0], iface);
        nfr = btess->tess1d[ie-1].faces[0].nface;
        if (ref != 0)
          for (k = 0; k < btess->tess1d[ie-1].npts-1; k++)
            if (btess->tess1d[ie-1].faces[0].tric[nfr*k+ref] == it)
              btess->tess1d[ie-1].faces[0].tric[nfr*k+ref] = n2;
        ref = EG_faceConnIndex(btess->tess1d[ie-1].faces[1], iface);
        nfr = btess->tess1d[ie-1].faces[1].nface;
        if (ref != 0)
          for (k = 0; k < btess->tess1d[ie-1].npts-1; k++)
            if (btess->tess1d[ie-1].faces[1].tric[nfr*k+ref] == it)
              btess->tess1d[ie-1].faces[1].tric[nfr*k+ref] = n2;
      }
      if (n2 > 0) {
        for (i = 0; i < 3; i++)
          if (btess->tess2d[iface-1].tric[3*n2+i-3] == it) {
            btess->tess2d[iface-1].tric[3*n2+i-3] = n1;
            break;
          }
      } else if (n2 < 0) {
        ie  = -n2;
        ref = EG_faceConnIndex(btess->tess1d[ie-1].faces[0], iface);
        nfr = btess->tess1d[ie-1].faces[0].nface;
        if (ref != 0)
          for (k = 0; k < btess->tess1d[ie-1].npts-1; k++)
            if (btess->tess1d[ie-1].faces[0].tric[nfr*k+ref] == it)
              btess->tess1d[ie-1].faces[0].tric[nfr*k+ref] = n1;
        ref = EG_faceConnIndex(btess->tess1d[ie-1].faces[1], iface);
        nfr = btess->tess1d[ie-1].faces[1].nface;
        if (ref != 0)
          for (k = 0; k < btess->tess1d[ie-1].npts-1; k++)
            if (btess->tess1d[ie-1].faces[1].tric[nfr*k+ref] == it)
              btess->tess1d[ie-1].faces[1].tric[nfr*k+ref] = n1;
      }
      /* collapse the vert from the triangulation by subsitution*/
      for (i = 0; i < btess->tess2d[iface-1].ntris; i++) {
        if (btess->tess2d[iface-1].tris[3*i  ] == iv[0])
          btess->tess2d[iface-1].tris[3*i  ] = iv[1];
        if (btess->tess2d[iface-1].tris[3*i+1] == iv[0])
          btess->tess2d[iface-1].tris[3*i+1] = iv[1];
        if (btess->tess2d[iface-1].tris[3*i+2] == iv[0])
          btess->tess2d[iface-1].tris[3*i+2] = iv[1];
      }
  
      /* compress the face */
      for (i = 0; i < btess->tess2d[iface-1].npts; i++)
        if ((btess->tess2d[iface-1].pindex[i] == eIndex) &&
            (btess->tess2d[iface-1].ptype[i]  >= vIndex))
          btess->tess2d[iface-1].ptype[i]--;

      for (i = 0; i < btess->tess2d[iface-1].ntris; i++) {
        if (btess->tess2d[iface-1].tris[3*i  ] > iv[0])
          btess->tess2d[iface-1].tris[3*i  ]--;
        if (btess->tess2d[iface-1].tris[3*i+1] > iv[0])
          btess->tess2d[iface-1].tris[3*i+1]--;
        if (btess->tess2d[iface-1].tris[3*i+2] > iv[0])
          btess->tess2d[iface-1].tris[3*i+2]--;
        if (btess->tess2d[iface-1].tric[3*i  ] > it)
          btess->tess2d[iface-1].tric[3*i  ]--;
        if (btess->tess2d[iface-1].tric[3*i+1] > it)
          btess->tess2d[iface-1].tric[3*i+1]--;
        if (btess->tess2d[iface-1].tric[3*i+2] > it)
          btess->tess2d[iface-1].tric[3*i+2]--;
      }
      for (ie = 0; ie < btess->nEdge; ie++) {
        nfr = btess->tess1d[ie].faces[0].nface;
        for (i = 0; i < nfr; i++) {
          k = btess->tess1d[ie].faces[0].index;
          if (nfr > 1) k = btess->tess1d[ie].faces[0].faces[i];
          if (iface != k) continue;
          for (k = 0; k < btess->tess1d[ie].npts-1; k++)
            if (btess->tess1d[ie].faces[0].tric[nfr*k+i] > it)
              btess->tess1d[ie].faces[0].tric[nfr*k+i]--;
        }
        nfr = btess->tess1d[ie].faces[1].nface;
        for (i = 0; i < nfr; i++) {
          k = btess->tess1d[ie].faces[1].index;
          if (nfr > 1) k = btess->tess1d[ie].faces[1].faces[i];
          if (iface != k) continue;
          for (k = 0; k < btess->tess1d[ie].npts-1; k++)
            if (btess->tess1d[ie].faces[1].tric[nfr*k+i] > it)
              btess->tess1d[ie].faces[1].tric[nfr*k+i]--;
        }
      }
      btess->tess2d[iface-1].npts--;
      for (i = iv[0]-1; i < btess->tess2d[iface-1].npts; i++) {
        btess->tess2d[iface-1].xyz[3*i  ] = btess->tess2d[iface-1].xyz[3*i+3];
        btess->tess2d[iface-1].xyz[3*i+1] = btess->tess2d[iface-1].xyz[3*i+4];
        btess->tess2d[iface-1].xyz[3*i+2] = btess->tess2d[iface-1].xyz[3*i+5];
        btess->tess2d[iface-1].uv[2*i  ]  = btess->tess2d[iface-1].uv[2*i+2];
        btess->tess2d[iface-1].uv[2*i+1]  = btess->tess2d[iface-1].uv[2*i+3];
        btess->tess2d[iface-1].ptype[i]   = btess->tess2d[iface-1].ptype[i+1];
        btess->tess2d[iface-1].pindex[i]  = btess->tess2d[iface-1].pindex[i+1];
      }
      btess->tess2d[iface-1].ntris--;
      for (i = it-1; i < btess->tess2d[iface-1].ntris; i++) {
        btess->tess2d[iface-1].tris[3*i  ] = btess->tess2d[iface-1].tris[3*i+3];
        btess->tess2d[iface-1].tris[3*i+1] = btess->tess2d[iface-1].tris[3*i+4];
        btess->tess2d[iface-1].tris[3*i+2] = btess->tess2d[iface-1].tris[3*i+5];
        btess->tess2d[iface-1].tric[3*i  ] = btess->tess2d[iface-1].tric[3*i+3];
        btess->tess2d[iface-1].tric[3*i+1] = btess->tess2d[iface-1].tric[3*i+4];
        btess->tess2d[iface-1].tric[3*i+2] = btess->tess2d[iface-1].tric[3*i+5];
      }

      /* remove any quads */
      EG_deleteQuads(btess, iface);
    }
  }
  
  /* compress the Edge storage */
                 k = vIndex-1;
  if (dir == -1) k = vIndex-2;
  btess->tess1d[eIndex-1].npts--;
  for (i = k; i < btess->tess1d[eIndex-1].npts; i++) {
    if (i != btess->tess1d[eIndex-1].npts-1)
      for (m = 0; m < 2; m++) {
        nf = btess->tess1d[eIndex-1].faces[m].nface;
        for (n = 0; n < nf; n++)
          btess->tess1d[eIndex-1].faces[m].tric[nf*i+n] = 
            btess->tess1d[eIndex-1].faces[m].tric[nf*(i+1)+n];
    }
    btess->tess1d[eIndex-1].xyz[3*i  ] = btess->tess1d[eIndex-1].xyz[3*i+3];
    btess->tess1d[eIndex-1].xyz[3*i+1] = btess->tess1d[eIndex-1].xyz[3*i+4];
    btess->tess1d[eIndex-1].xyz[3*i+2] = btess->tess1d[eIndex-1].xyz[3*i+5];
    btess->tess1d[eIndex-1].t[i]       = btess->tess1d[eIndex-1].t[i+1];
  }
  
#ifdef CHECK
  EG_checkTriangulation(btess);
#endif

  return EGADS_SUCCESS;
}


int
EG_insertEdgeVerts(egObject *tess, int eIndex, int vIndex, int npts,
                   double *t)
{
  int      i, j, k, m, nf, nx, stat, outLevel, nedge, nface, iface, itri;
  int      n0, n1, v0, v1, vert, vn, nl, nn, sense, pt1, pi1, pt2, pi2;
  int      i1, i2, i3, cnt, stripe;
  int      *etric[2], *pindex, *ptype, *tris, *tric;
  double   result[9], *vals, *xyzs, *ts, *xyz, *uv;
  egTessel *btess;
  egObject *obj, **edges, **faces;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  if (npts <= 0) {
    if (outLevel > 0)
      printf(" EGADS Error: Zero Inserts (EG_insertEdgeVerts)!\n");
    return EGADS_RANGERR;
  }
  for (i = 0; i < npts-1; i++)
    if (t[i+1] <= t[i]) {
      if (outLevel > 0)
        printf(" EGADS Error: Ts are NOT monitonic (EG_insertEdgeVerts)!\n");  
      return EGADS_RANGERR;
    }

  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_insertEdgeVerts)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_insertEdgeVerts)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_insertEdgeVerts)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_insertEdgeVerts)!\n");
    return EGADS_NOTBODY;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_insertEdgeVert)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess1d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Edge Tessellations (EG_insertEdgeVerts)!\n");
    return EGADS_NODATA;  
  }
  if ((eIndex < 1) || (eIndex > btess->nEdge)) {
    if (outLevel > 0)
      printf(" EGADS Error: eIndex = %d [1-%d] (EG_insertEdgeVerts)!\n",
             eIndex, btess->nEdge);
    return EGADS_INDEXERR;
  }
  if ((vIndex < 1) || (vIndex >= btess->tess1d[eIndex-1].npts)) {
    if (outLevel > 0)
      printf(" EGADS Error: vIndex = %d [1-%d] (EG_insertEdgeVerts)!\n",
             vIndex, btess->tess1d[eIndex-1].npts-1);
    return EGADS_INDEXERR;
  }
  if ((t[0]      <= btess->tess1d[eIndex-1].t[vIndex-1]) ||
      (t[npts-1] >= btess->tess1d[eIndex-1].t[vIndex])) {
    if (outLevel > 0)
      printf(" EGADS Error: t = %lf %lf [%lf-%lf] (EG_insertEdgeVerts)!\n",
             t[0], t[npts-1], btess->tess1d[eIndex-1].t[vIndex-1], 
                              btess->tess1d[eIndex-1].t[vIndex]);
    return EGADS_RANGERR;
  }
  
  /* make sure we are not inserting along a DEGEN Edge */
  for (cnt = m = 0; m < 2; m++) {
    nf = btess->tess1d[eIndex-1].faces[m].nface;
    for (nx = 0; nx < nf; nx++) {
      iface = btess->tess1d[eIndex-1].faces[m].index;
      if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[nx];
      if (iface == 0) continue;
      itri = btess->tess1d[eIndex-1].faces[m].tric[(vIndex-1)*nf+nx];
      i1   = btess->tess2d[iface-1].tris[3*itri-3]-1;
      i2   = btess->tess2d[iface-1].tris[3*itri-2]-1;
      i3   = btess->tess2d[iface-1].tris[3*itri-1]-1;
      if (btess->tess2d[iface-1].pindex[i1] == btess->tess2d[iface-1].pindex[i2])
        if ((btess->tess2d[iface-1].ptype[i1] == 0) &&
            (btess->tess2d[iface-1].ptype[i2] == 0)) {
          if (outLevel > 0) {
            printf(" EGADS Error: Degen EDGE (EG_insertEdgeVerts)!\n");
            printf("        Face %d: tri = %d, %d/%d  %d/%d  %d/%d\n", iface, itri,
                   btess->tess2d[iface-1].ptype[i1], 
                   btess->tess2d[iface-1].pindex[i1],
                   btess->tess2d[iface-1].ptype[i2], 
                   btess->tess2d[iface-1].pindex[i2],
                   btess->tess2d[iface-1].ptype[i3], 
                   btess->tess2d[iface-1].pindex[i3]);
            return EGADS_TOPOERR;
          }
        }
      if (btess->tess2d[iface-1].pindex[i2] == btess->tess2d[iface-1].pindex[i3])
        if ((btess->tess2d[iface-1].ptype[i2] == 0) &&
            (btess->tess2d[iface-1].ptype[i3] == 0)) {
          if (outLevel > 0) {
            printf(" EGADS Error: Degen EDGE (EG_insertEdgeVerts)!\n");
            printf("        Face %d: tri = %d, %d/%d  %d/%d  %d/%d\n", iface, itri,
                   btess->tess2d[iface-1].ptype[i1], 
                   btess->tess2d[iface-1].pindex[i1],
                   btess->tess2d[iface-1].ptype[i2], 
                   btess->tess2d[iface-1].pindex[i2],
                   btess->tess2d[iface-1].ptype[i3], 
                   btess->tess2d[iface-1].pindex[i3]);
            return EGADS_TOPOERR;
          }
        }
      if (btess->tess2d[iface-1].pindex[i1] == btess->tess2d[iface-1].pindex[i3])
        if ((btess->tess2d[iface-1].ptype[i1] == 0) &&
            (btess->tess2d[iface-1].ptype[i3] == 0)) {
          if (outLevel > 0) {
            printf(" EGADS Error: Degen EDGE (EG_insertEdgeVerts)!\n");
            printf("        Face %d: tri = %d, %d/%d  %d/%d  %d/%d\n", iface, itri,
                   btess->tess2d[iface-1].ptype[i1], 
                   btess->tess2d[iface-1].pindex[i1],
                   btess->tess2d[iface-1].ptype[i2], 
                   btess->tess2d[iface-1].pindex[i2],
                   btess->tess2d[iface-1].ptype[i3], 
                   btess->tess2d[iface-1].pindex[i3]);
            return EGADS_TOPOERR; 
          }
        }
      cnt++;
    }
  }

  stripe = 3 + 2*cnt;
  vals   = (double *) EG_alloc(stripe*npts*sizeof(double));
  if (vals == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on Tmp %d %d (EG_insertEdgeVerts)!\n",
             npts, stripe);
    return EGADS_MALLOC;
  }
  stat = EG_getBodyTopos(btess->src, NULL, EDGE, &nedge, &edges);
  if (stat != EGADS_SUCCESS) {
    EG_free(vals);
    return stat;
  }
  stat = EG_getBodyTopos(btess->src, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) {
    EG_free(edges);
    EG_free(vals);
    return stat;
  }

  /* get the new data on the Edge and Faces */
  for (i = 0; i < npts; i++) {
    stat = EG_evaluate(edges[eIndex-1], &t[i], result);
    if (stat != EGADS_SUCCESS) {
      EG_free(faces);
      EG_free(edges);
      EG_free(vals);
      return stat;
    }
    vals[stripe*i  ] = result[0];
    vals[stripe*i+1] = result[1];
    vals[stripe*i+2] = result[2];
    for (cnt = m = 0; m < 2; m++) {
      nf = btess->tess1d[eIndex-1].faces[m].nface;
      for (nx = 0; nx < nf; nx++) {
        iface = btess->tess1d[eIndex-1].faces[m].index;
        if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[nx];
        if (iface == 0) continue;
        sense = faces[iface-1]->mtype;
        if (EG_faceConnIndex(btess->tess1d[eIndex-1].faces[1-m], iface) == 0)
          sense = 0;
        if (m == 0) sense = -sense;
        stat = EG_getEdgeUV(faces[iface-1], edges[eIndex-1], sense, 
                            t[i], &vals[stripe*i+3+2*cnt]);
        if (stat != EGADS_SUCCESS) {
          EG_free(faces);
          EG_free(edges);
          EG_free(vals);
          return stat;
        }
        cnt++;
      }
    }
  }
  EG_free(faces);
  EG_free(edges);
  
  /* get all of the Edge memory we will need */
  xyzs = (double *) EG_alloc(3*(npts+btess->tess1d[eIndex-1].npts)*
                             sizeof(double));
  ts   = (double *) EG_alloc(  (npts+btess->tess1d[eIndex-1].npts)*
                             sizeof(double));
  if ((xyzs == NULL) || (ts == NULL)) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on Edge %d %d (EG_insertEdgeVerts)!\n",
             npts, btess->tess1d[eIndex-1].npts);
    if (ts   != NULL) EG_free(ts);
    if (xyzs != NULL) EG_free(xyzs);
    EG_free(vals);
    return EGADS_MALLOC;
  }
  etric[0] = etric[1] = NULL;
  nf = btess->tess1d[eIndex-1].faces[0].nface;
  if (nf > 0) {
    etric[0] = (int *) EG_alloc(nf*(npts+btess->tess1d[eIndex-1].npts-1)*
                                sizeof(int));
    if (etric[0] == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Malloc on Edge- %d %d (EG_insertEdgeVerts)!\n",
               npts, btess->tess1d[eIndex-1].npts-1);
      EG_free(ts);
      EG_free(xyzs);
      EG_free(vals);
      return EGADS_MALLOC;
    }
  }
  nf = btess->tess1d[eIndex-1].faces[1].nface;
  if (nf > 0) {
    etric[1] = (int *) EG_alloc(nf*(npts+btess->tess1d[eIndex-1].npts-1)*
                                sizeof(int));
    if (etric[1] == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: Malloc on Edge+ %d %d (EG_insertEdgeVerts)!\n",
               npts, btess->tess1d[eIndex-1].npts-1);
      if (etric[0] != NULL) EG_free(etric[0]);
      EG_free(ts);
      EG_free(xyzs);
      EG_free(vals);
      return EGADS_MALLOC;
    }
  }

  /* set the new Edge tessellation information */
  for (j = i = 0; i < btess->tess1d[eIndex-1].npts; i++, j++) {
    xyzs[3*j  ] = btess->tess1d[eIndex-1].xyz[3*i  ];
    xyzs[3*j+1] = btess->tess1d[eIndex-1].xyz[3*i+1];
    xyzs[3*j+2] = btess->tess1d[eIndex-1].xyz[3*i+2];
    ts[j]       = btess->tess1d[eIndex-1].t[i];
    if (i != btess->tess1d[eIndex-1].npts-1)
      for (m = 0; m < 2; m++) {
        nf = btess->tess1d[eIndex-1].faces[m].nface;
        for (nx = 0; nx < nf; nx++)
          etric[m][j*nf+nx] = btess->tess1d[eIndex-1].faces[m].tric[i*nf+nx];
      }
    if (i != vIndex-1) continue;
    for (k = 0; k < npts; k++) {
      j++;
      xyzs[3*j  ] = vals[stripe*k  ];
      xyzs[3*j+1] = vals[stripe*k+1];
      xyzs[3*j+2] = vals[stripe*k+2];
      ts[j]       = t[k];
      for (m = 0; m < 2; m++) {
        nf = btess->tess1d[eIndex-1].faces[m].nface;
        for (nx = 0; nx < nf; nx++) etric[m][j*nf+nx] = 0;
      }
    }
  }

  /* do each Face touched by the Edge */
  for (cnt = m = 0; m < 2; m++) {
    nf = btess->tess1d[eIndex-1].faces[m].nface;
    for (nx = 0; nx < nf; nx++) {
      iface = btess->tess1d[eIndex-1].faces[m].index;
      if (nf > 1) iface = btess->tess1d[eIndex-1].faces[m].faces[nx];
      if (iface == 0) continue;
      xyz    = (double *) EG_alloc(3*(npts+btess->tess2d[iface-1].npts)*
                                   sizeof(double));
      uv     = (double *) EG_alloc(2*(npts+btess->tess2d[iface-1].npts)*
                                   sizeof(double));
      ptype  = (int *)    EG_alloc(  (npts+btess->tess2d[iface-1].npts)*
                                   sizeof(int));
      pindex = (int *)    EG_alloc(  (npts+btess->tess2d[iface-1].npts)*
                                   sizeof(int));
      tris   = (int *)    EG_alloc(3*(npts+btess->tess2d[iface-1].ntris)*
                                   sizeof(int));
      tric   = (int *)    EG_alloc(3*(npts+btess->tess2d[iface-1].ntris)*
                                   sizeof(int));
      if ((xyz == NULL)    || (uv == NULL)   || (ptype == NULL) ||
          (pindex == NULL) || (tris == NULL) || (tric == NULL)) {
        if (outLevel > 0)
          printf(" EGADS Error: Malloc on Edge %d %d (EG_insertEdgeVerts)!\n",
                 npts, btess->tess1d[eIndex-1].npts);
        if (tric     != NULL) EG_free(tric);
        if (tris     != NULL) EG_free(tris);
        if (pindex   != NULL) EG_free(pindex);
        if (ptype    != NULL) EG_free(ptype);
        if (uv       != NULL) EG_free(uv);
        if (xyz      != NULL) EG_free(xyz);
        if (etric[0] != NULL) EG_free(etric[0]);
        if (etric[1] != NULL) EG_free(etric[1]);
        EG_free(ts);
        EG_free(xyzs);
        EG_free(vals);
        if (cnt != 0) EG_deleteObject(tess);
        return EGADS_MALLOC;
      }
      for (i = 0; i < btess->tess2d[iface-1].npts; i++) {
        xyz[3*i  ] = btess->tess2d[iface-1].xyz[3*i  ];
        xyz[3*i+1] = btess->tess2d[iface-1].xyz[3*i+1];
        xyz[3*i+2] = btess->tess2d[iface-1].xyz[3*i+2];
        uv[2*i  ]  = btess->tess2d[iface-1].uv[2*i  ];
        uv[2*i+1]  = btess->tess2d[iface-1].uv[2*i+1];
        ptype[i]   = btess->tess2d[iface-1].ptype[i];
        pindex[i]  = btess->tess2d[iface-1].pindex[i];
        if (pindex[i] == eIndex) 
          if (ptype[i] > vIndex) ptype[i] += npts;
      }
      j = btess->tess2d[iface-1].npts;
      for (i = 0; i < npts; i++) {
        xyz[3*(j+i)  ] = vals[stripe*i  ];
        xyz[3*(j+i)+1] = vals[stripe*i+1];
        xyz[3*(j+i)+2] = vals[stripe*i+2];
        uv[2*(j+i)  ]  = vals[stripe*i+3+2*cnt  ];
        uv[2*(j+i)+1]  = vals[stripe*i+3+2*cnt+1];
        ptype[j+i]     = vIndex + i+1;
        pindex[j+i]    = eIndex;
      }
      for (i = 0; i < btess->tess2d[iface-1].ntris; i++) {
        tris[3*i  ] = btess->tess2d[iface-1].tris[3*i  ];
        tris[3*i+1] = btess->tess2d[iface-1].tris[3*i+1];
        tris[3*i+2] = btess->tess2d[iface-1].tris[3*i+2];
        tric[3*i  ] = btess->tess2d[iface-1].tric[3*i  ];
        tric[3*i+1] = btess->tess2d[iface-1].tric[3*i+1];
        tric[3*i+2] = btess->tess2d[iface-1].tric[3*i+2];
      }
      
      /* adjust the Face tessellation */
      sense = 1;
      itri  = etric[m][(vIndex-1)*nf+nx];
      pt1   =       vIndex;
      pi1   = pi2 = eIndex;
      pt2   = vIndex + 1;
      if (vIndex == 1) {
        pt1 = 0;
        pi1 = btess->tess1d[eIndex-1].nodes[0];
      }
      if (pt2 == btess->tess1d[eIndex-1].npts-npts) {
        pt2 = 0;
        pi2 = btess->tess1d[eIndex-1].nodes[1];
      }
      i1 = tris[3*itri-3]-1;
      i2 = tris[3*itri-2]-1;
      i3 = tris[3*itri-1]-1;        
      if (((btess->tess2d[iface-1].pindex[i2] == pi1) &&
           (btess->tess2d[iface-1].ptype[i2]  == pt1) &&
           (btess->tess2d[iface-1].pindex[i3] == pi2) &&
           (btess->tess2d[iface-1].ptype[i3]  == pt2)) || 
          ((btess->tess2d[iface-1].pindex[i2] == pi2) &&
           (btess->tess2d[iface-1].ptype[i2]  == pt2) &&
           (btess->tess2d[iface-1].pindex[i3] == pi1) &&
           (btess->tess2d[iface-1].ptype[i3]  == pt1))) {
        vert = i1 + 1;
        v0   = i2 + 1;
        v1   = i3 + 1;
        n0   = tric[3*itri-2];
        n1   = tric[3*itri-1];
/*      printf("    0: neighbor = %d\n", tric[m][3*itri-3]);  */
      } else if (((btess->tess2d[iface-1].pindex[i1] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt1) &&
                  (btess->tess2d[iface-1].pindex[i3] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i3]  == pt2)) || 
                 ((btess->tess2d[iface-1].pindex[i1] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt2) &&
                  (btess->tess2d[iface-1].pindex[i3] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i3]  == pt1))) {
        v1   = i1 + 1;
        vert = i2 + 1;
        v0   = i3 + 1;
        n1   = tric[3*itri-3];
        n0   = tric[3*itri-1];
/*      printf("    1: neighbor = %d\n", tric[m][3*itri-2]);  */
      } else if (((btess->tess2d[iface-1].pindex[i1] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt1) &&
                  (btess->tess2d[iface-1].pindex[i2] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i2]  == pt2)) || 
                 ((btess->tess2d[iface-1].pindex[i1] == pi2) &&
                  (btess->tess2d[iface-1].ptype[i1]  == pt2) &&
                  (btess->tess2d[iface-1].pindex[i2] == pi1) &&
                  (btess->tess2d[iface-1].ptype[i2]  == pt1))) {
        v0   = i1 + 1;
        v1   = i2 + 1;
        vert = i3 + 1;
        n0   = tric[3*itri-3];
        n1   = tric[3*itri-2];
/*      printf("    2: neighbor = %d\n", tric[m][3*itri-1]);  */
      } else {
        printf(" EGADS Internal: Can not find segment for %d %d  %d %d - %d!\n",
               pt1, pi1, pt2, pi2, btess->tess1d[eIndex-1].npts);
      }
      if ((btess->tess2d[iface-1].ptype[v1-1]  == pt1) &&
          (btess->tess2d[iface-1].pindex[v1-1] == pi1)) {
        i     =  v0;
        v0    =  v1;
        v1    =  i;
        i     =  n0;
        n0    =  n1;
        n1    =  i;
        sense = -1;
      }
/*    printf("       %d: %d", iface, itri); 
      printf(" vert = %d (%d/%d)  v0 = %d (%d/%d)  v1 = %d (%d/%d), sense = %d\n", 
             vert, btess->tess2d[iface-1].ptype[vert-1], 
                   btess->tess2d[iface-1].pindex[vert-1], 
             v0,   btess->tess2d[iface-1].ptype[v0-1], 
                   btess->tess2d[iface-1].pindex[v0-1], 
             v1,   btess->tess2d[iface-1].ptype[v1-1], 
                   btess->tess2d[iface-1].pindex[v1-1], sense);  */
      for (i = 0; i < 3; i++) {
        if (btess->tess2d[iface-1].tris[3*itri+i-3] == v1) 
          tris[3*itri+i-3] = btess->tess2d[iface-1].npts  + 1;
        if (btess->tess2d[iface-1].tris[3*itri+i-3] == v0) 
          tric[3*itri+i-3] = btess->tess2d[iface-1].ntris + 1;
      } 
      nl = itri;
      for (i = 0; i < npts; i++) {
        j  = btess->tess2d[iface-1].ntris + i;
        v0 = btess->tess2d[iface-1].npts  + i + 1;
        vn = btess->tess2d[iface-1].npts  + i + 2;
        nn = j+2;
        if (i == npts-1) {
          vn = v1;
          nn = n0;
        }
        tris[3*j  ] =  vert;
        tric[3*j  ] = -eIndex;
        if (sense == 1) {
          tris[3*j+1] = v0;
          tris[3*j+2] = vn;
          tric[3*j+1] = nn;
          tric[3*j+2] = nl;
        } else {
          tris[3*j+1] = vn;
          tris[3*j+2] = v0;
          tric[3*j+1] = nl;
          tric[3*j+2] = nn;
        }
        etric[m][nf*(vIndex+i)+nx] = j + 1;
        nl = j+1;
      }
      if (n0 > 0) {
        for (i = 0; i < 3; i++)
          if (btess->tess2d[iface-1].tric[3*n0+i-3] == itri) 
            tric[3*n0+i-3] = btess->tess2d[iface-1].ntris+npts;
      } else if (n0 < 0) {
        j = -n0 - 1;
        if (btess->tess1d[j].faces[0].index == iface) {
          for (k = i = 0; i < btess->tess1d[j].npts-1; i++)
            if (etric[m][nf*i+nx] == itri) k++;
          for (i = 0; i < btess->tess1d[j].npts-1; i++) {
            if ((k > 1) && (i >= vIndex-1) && (i < vIndex+npts-1)) continue;
            if (etric[m][nf*i+nx] == itri)
              etric[m][nf*i+nx] = btess->tess2d[iface-1].ntris+npts;
          }
        }
      }

      /* update the Face pointers */
      if (btess->tess2d[iface-1].xyz    != NULL) 
        EG_free(btess->tess2d[iface-1].xyz);
      if (btess->tess2d[iface-1].uv     != NULL) 
        EG_free(btess->tess2d[iface-1].uv);
      if (btess->tess2d[iface-1].ptype  != NULL) 
        EG_free(btess->tess2d[iface-1].ptype);
      if (btess->tess2d[iface-1].pindex != NULL) 
        EG_free(btess->tess2d[iface-1].pindex);
      if (btess->tess2d[iface-1].tris   != NULL) 
        EG_free(btess->tess2d[iface-1].tris);
      if (btess->tess2d[iface-1].tric   != NULL) 
        EG_free(btess->tess2d[iface-1].tric);
      btess->tess2d[iface-1].xyz    = xyz;
      btess->tess2d[iface-1].uv     = uv;
      btess->tess2d[iface-1].ptype  = ptype;
      btess->tess2d[iface-1].pindex = pindex;
      btess->tess2d[iface-1].tris   = tris;
      btess->tess2d[iface-1].tric   = tric;
      btess->tess2d[iface-1].ntris += npts;
      btess->tess2d[iface-1].npts  += npts;

      /* delete any quads */
      EG_deleteQuads(btess, iface);

      cnt++;
    }
  }
  EG_free(vals);
  
  /* set the updated Edge tessellation */
  if (btess->tess1d[eIndex-1].faces[0].tric != NULL) 
    EG_free(btess->tess1d[eIndex-1].faces[0].tric);
  if (btess->tess1d[eIndex-1].faces[1].tric != NULL) 
    EG_free(btess->tess1d[eIndex-1].faces[1].tric);
  btess->tess1d[eIndex-1].faces[0].tric = etric[0];
  btess->tess1d[eIndex-1].faces[1].tric = etric[1];
  if (btess->tess1d[eIndex-1].xyz  != NULL) 
    EG_free(btess->tess1d[eIndex-1].xyz);
  if (btess->tess1d[eIndex-1].t    != NULL) 
    EG_free(btess->tess1d[eIndex-1].t);
  btess->tess1d[eIndex-1].xyz   = xyzs;
  btess->tess1d[eIndex-1].t     = ts;
  btess->tess1d[eIndex-1].npts += npts;
  
#ifdef CHECK
  EG_checkTriangulation(btess);
#endif

  return EGADS_SUCCESS;
}


int
EG_makeTessBody(egObject *object, double *params, egObject **tess)
{
  int       j, stat, outLevel, nface;
  double    dist;
  triStruct tst;
  fillArea  fast;
  egTessel  *btess;
  egObject  *ttess, *context, **faces;

  *tess = NULL;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass != BODY)       return EGADS_NOTBODY;
  outLevel = EG_outLevel(object);
  context  = EG_context(object);

  btess = EG_alloc(sizeof(egTessel));
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Blind Malloc (EG_makeTessBody)!\n");
    return EGADS_MALLOC;
  }
  btess->src       = object;
  btess->xyzs      = NULL;
  btess->tess1d    = NULL;
  btess->tess2d    = NULL;
  btess->nEdge     = 0;
  btess->nFace     = 0;
  btess->nu        = 0;
  btess->nv        = 0;
  btess->params[0] = params[0];
  btess->params[1] = params[1];
  btess->params[2] = params[2];
  
  /* do the Edges & make the Tessellation Object */
  
  stat = EG_tessEdges(btess, NULL);
  if (stat != EGADS_SUCCESS) {
    EG_cleanupTess(btess);
    EG_free(btess);
    return stat;  
  }
  stat = EG_makeObject(context, &ttess);
  if (stat != EGADS_SUCCESS) {
    EG_cleanupTess(btess);
    EG_free(btess);
    return stat;
  }
  ttess->oclass = TESSELLATION;
  ttess->blind  = btess;
  EG_referenceObject(ttess,  context);
  EG_referenceTopObj(object, ttess);
  *tess = ttess;
  
  /* Wire Body */
  if (object->mtype == WIREBODY) return EGADS_SUCCESS;

  /* not a WireBody */

  stat = EG_getBodyTopos(object, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: EG_getBodyTopos = %d (EG_makeTessBody)!\n",
           stat);
    EG_deleteObject(ttess);
    *tess = NULL;
    return stat;
  }
  btess->tess2d = (egTess2D *) EG_alloc(2*nface*sizeof(egTess2D));
 if (btess->tess2d == NULL) {
    printf(" EGADS Error: Alloc %d Faces (EG_makeTessBody)!\n", nface);  
    EG_deleteObject(ttess);
    *tess = NULL;
    return EGADS_MALLOC;
  }
  for (j = 0; j < 2*nface; j++) {
   btess->tess2d[j].xyz    = NULL;
   btess->tess2d[j].uv     = NULL;
   btess->tess2d[j].ptype  = NULL;
   btess->tess2d[j].pindex = NULL;
   btess->tess2d[j].tris   = NULL;
   btess->tess2d[j].tric   = NULL;
   btess->tess2d[j].patch  = NULL;
   btess->tess2d[j].npts   = 0;
   btess->tess2d[j].ntris  = 0;
   btess->tess2d[j].npatch = 0;
  }
  btess->nFace = nface;

  dist = fabs(params[2]);
  if (dist > 30.0) dist = 30.0;
  if (dist <  0.5) dist =  0.5;
  tst.maxlen  = params[0];
  tst.chord   = params[1];
  tst.dotnrm  = cos(PI*dist/180.0);
  tst.mverts  = tst.nverts = 0;
  tst.verts   = NULL;
  tst.mtris   = tst.ntris  = 0;
  tst.tris    = NULL;
  tst.msegs   = tst.nsegs  = 0;
  tst.segs    = NULL;
  tst.numElem = -1;
  tst.hashTab = NULL;
  
  fast.pts    = NULL;
  fast.segs   = NULL;
  fast.front  = NULL;

  for (j = 0; j < nface; j++) {
    stat = EG_fillTris(object, j+1, faces[j], ttess, &tst, &fast);
    if (stat != EGADS_SUCCESS) 
      printf(" EGADS Warning: Face %d -> EG_fillTris = %d (EG_makeTessBody)!\n",
             j+1, stat);
  }
#ifdef CHECK
  EG_checkTriangulation(btess);
#endif
    
  /* cleanup */

  if (tst.verts  != NULL) EG_free(tst.verts);
  if (tst.tris   != NULL) EG_free(tst.tris);
  if (tst.segs   != NULL) EG_free(tst.segs); 
   
  if (fast.segs  != NULL) EG_free(fast.segs);
  if (fast.pts   != NULL) EG_free(fast.pts);
  if (fast.front != NULL) EG_free(fast.front);
  EG_free(faces); 

  return EGADS_SUCCESS;
}


int
EG_remakeTess(egObject *tess, int nobj, egObject **objs, double *params)
{
  int       i, j, mx, stat, outLevel, iface, nface, hit;
  int       *ed, *marker = NULL;
  double    dist, save[3];
  triStruct tst;
  fillArea  fast;
  egObject  *context, *object, **faces;
  egTessel  *btess;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  if (tess->blind == NULL)          return EGADS_NODATA;
  btess  = tess->blind;
  object = btess->src;
  if (object == NULL)               return EGADS_NULLOBJ;
  if (object->magicnumber != MAGIC) return EGADS_NOTOBJ;
  if (object->oclass != BODY)       return EGADS_NOTBODY;
  if (nobj <= 0)                    return EGADS_NODATA;
  outLevel = EG_outLevel(object);
  context  = EG_context(object);
  
  for (hit = j = 0; j < nobj; j++) {
    if (objs[j] == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: NULL Object[%d] (EG_remakeTess)!\n",
               j+1);
      return EGADS_NULLOBJ;
    }
    if (objs[j]->magicnumber != MAGIC) {
      if (outLevel > 0)
        printf(" EGADS Error: Not an Object[%d] (EG_remakeTess)!\n",
               j+1);
      return EGADS_NOTOBJ;
    }
    if ((objs[j]->oclass != EDGE) && (objs[j]->oclass != FACE)) {
      if (outLevel > 0)
        printf(" EGADS Error: Not Edge/Face[%d] (EG_remakeTess)!\n",
               j+1);
      return EGADS_NOTOBJ;
    }
    stat = EG_indexBodyTopo(object, objs[j]);
    if (stat == EGADS_NOTFOUND) {
      if (outLevel > 0)
        printf(" EGADS Error: Object[%d] Not in Body (EG_remakeTess)!\n",
               j+1);
      return stat;
    }
    if (objs[j]->oclass == FACE) continue;
    if (objs[j]->mtype == DEGENERATE) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge[%d] is DEGENERATE (EG_remakeTess)!\n",
               j+1);
      return EGADS_DEGEN;
    }
    hit++;
  }
  
  /* mark faces */
  
  if (btess->nFace != 0) {
    marker = (int *) EG_alloc(btess->nFace*sizeof(int));
    if (marker == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: MALLOC on %d Faces (EG_remakeTess)!\n",
               btess->nFace);
      return EGADS_MALLOC;
    }
    for (j = 0; j < btess->nFace; j++) marker[j] = 0;
    for (j = 0; j < nobj; j++) {
      i = EG_indexBodyTopo(object, objs[j]);
      if (objs[j]->oclass == EDGE) {
        for (mx = 0; mx < 2; mx++) {
          iface = btess->tess1d[i-1].faces[mx].index;
          if (iface == 0) continue;
          marker[iface-1] = 1;
          EG_deleteQuads(btess, iface);
        }
      } else {
        marker[i-1] = 1;
      }
    }
  }
  
  /* do egdes */
  
  if (hit != 0) {
    ed = (int *) EG_alloc(btess->nEdge*sizeof(int));
    if (ed == NULL) {
      if (outLevel > 0)
        printf(" EGADS Error: MALLOC on %d Faces (EG_remakeTess)!\n",
               btess->nFace);
      EG_free(marker);
      return EGADS_MALLOC;
    }
    for (j = 0; j < btess->nEdge; j++) ed[j] = 0;
    for (j = 0; j < nobj; j++) {
      if (objs[j]->oclass != EDGE) continue;
      i = EG_indexBodyTopo(object, objs[j]);
      if (btess->tess1d[i-1].xyz != NULL) EG_free(btess->tess1d[i-1].xyz);
      if (btess->tess1d[i-1].t   != NULL) EG_free(btess->tess1d[i-1].t);
      if (btess->tess1d[i-1].faces[0].tric  != NULL)
        EG_free(btess->tess1d[i-1].faces[0].tric);
      if (btess->tess1d[i-1].faces[1].tric  != NULL)
        EG_free(btess->tess1d[i-1].faces[1].tric);
      btess->tess1d[i-1].faces[0].tric = NULL;
      btess->tess1d[i-1].faces[1].tric = NULL;
      btess->tess1d[i-1].xyz           = NULL;
      btess->tess1d[i-1].t             = NULL;
      btess->tess1d[i-1].npts          = 0;
      ed[i-1] = 1;
    }
    save[0] = btess->params[0];
    save[1] = btess->params[1];
    save[2] = btess->params[2];
    btess->params[0] = params[0];
    btess->params[1] = params[1];
    btess->params[2] = params[2];
    stat = EG_tessEdges(btess, ed);
    btess->params[0] = save[0];
    btess->params[1] = save[1];
    btess->params[2] = save[2];
    EG_free(ed);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: EG_tessEdges =  %d (EG_remakeTess)!\n",
               stat);
      EG_free(marker);
      return stat;
    }
  }
  if (marker == NULL) return EGADS_SUCCESS;
 
  /* do faces */
  
  dist = fabs(params[2]);
  if (dist > 30.0) dist = 30.0;
  if (dist <  0.5) dist =  0.5;
  tst.maxlen  = params[0];
  tst.chord   = params[1];
  tst.dotnrm  = cos(PI*dist/180.0);
  tst.mverts  = tst.nverts = 0;
  tst.verts   = NULL;
  tst.mtris   = tst.ntris  = 0;
  tst.tris    = NULL;
  tst.msegs   = tst.nsegs  = 0;
  tst.segs    = NULL;
  tst.numElem = -1;
  tst.hashTab = NULL;
  
  fast.pts    = NULL;
  fast.segs   = NULL;
  fast.front  = NULL;
  
  stat = EG_getBodyTopos(object, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) {
    printf(" EGADS Error: EG_getBodyTopos = %d (EG_remakeTess)!\n",
           stat);
    EG_free(marker);
    return stat;
  }

  for (j = 0; j < btess->nFace; j++) {
    if (marker[j] == 0) continue;
    
    if (btess->tess2d[j].xyz    != NULL) EG_free(btess->tess2d[j].xyz);
    if (btess->tess2d[j].uv     != NULL) EG_free(btess->tess2d[j].uv);
    if (btess->tess2d[j].ptype  != NULL) EG_free(btess->tess2d[j].ptype);
    if (btess->tess2d[j].pindex != NULL) EG_free(btess->tess2d[j].pindex);
    if (btess->tess2d[j].tris   != NULL) EG_free(btess->tess2d[j].tris);
    if (btess->tess2d[j].tric   != NULL) EG_free(btess->tess2d[j].tric);
    btess->tess2d[j].xyz    = NULL;
    btess->tess2d[j].uv     = NULL;
    btess->tess2d[j].ptype  = NULL;
    btess->tess2d[j].pindex = NULL;
    btess->tess2d[j].tris   = NULL;
    btess->tess2d[j].tric   = NULL;
    btess->tess2d[j].npts   = 0;
    btess->tess2d[j].ntris  = 0;
  
    stat = EG_fillTris(object, j+1, faces[j], tess, &tst, &fast);
    if (stat != EGADS_SUCCESS) 
      printf(" EGADS Warning: Face %d -> EG_fillTris = %d (EG_makeTessBody)!\n",
             j+1, stat);
  }
#ifdef CHECK
  EG_checkTriangulation(btess);
#endif

  if (tst.verts  != NULL) EG_free(tst.verts);
  if (tst.tris   != NULL) EG_free(tst.tris);
  if (tst.segs   != NULL) EG_free(tst.segs); 
   
  if (fast.segs  != NULL) EG_free(fast.segs);
  if (fast.pts   != NULL) EG_free(fast.pts);
  if (fast.front != NULL) EG_free(fast.front);
  EG_free(faces);
  EG_free(marker);
  
  return EGADS_SUCCESS;
}


int
EG_getTessQuads(const egObject *tess, int *nquad, int **fIndices)
{
  int      i, n, outLevel, *ivec;
  egTessel *btess;
  egObject *obj;
  
  *nquad    = 0;
  *fIndices = NULL;
  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getTessQuads)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getTessQuads)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getTessQuads)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_getTessQuads)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess2d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Face Tessellations (EG_getTessQuads)!\n");
    return EGADS_NODATA;  
  }

  for (n = i = 0; i < btess->nFace; i++)
    if (btess->tess2d[i+btess->nFace].xyz != NULL) n++;
  if (n == 0) return EGADS_SUCCESS;
  
  ivec = (int *) EG_alloc(n*sizeof(int));
  if (ivec == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on %d Faces (EG_getTessQuads)!\n", n);
    return EGADS_MALLOC;
  }
  
  for (n = i = 0; i < btess->nFace; i++)
    if (btess->tess2d[i+btess->nFace].xyz != NULL) {
      ivec[n] = i+1;
      n++;
    }
  *nquad    = n;
  *fIndices = ivec;
  
  return EGADS_SUCCESS;
}


static int
EG_quadLoop(egTessel *btess, int outLevel, int nedge, int *eindex, 
            int *senses, double *parms, int *lim)
{
  int    i, j, ie0, ie1, imax, nside;
  double t0[3], t1[3], dist, dmax, edgeTOL = 0.05;
  
  if ((parms[0] >= 0.001) && (parms[0] <= 0.5)) edgeTOL = parms[0];
  nside = nedge;

  while (nside > 4) {
  
    /* merge the 2 Edges with the smallest delta in tangent */
    dmax = -1.0;
    imax = -1; 
    for (i = 0; i < 4; i++) {
      ie0 = eindex[lim[i]  ]-1;
      ie1 = eindex[lim[i]+1]-1;
      if (senses[lim[i]  ] == 1) {
        j     = btess->tess1d[ie0].npts-2;
        t0[0] = btess->tess1d[ie0].xyz[3*j+3]-btess->tess1d[ie0].xyz[3*j  ];
        t0[1] = btess->tess1d[ie0].xyz[3*j+4]-btess->tess1d[ie0].xyz[3*j+1];
        t0[2] = btess->tess1d[ie0].xyz[3*j+5]-btess->tess1d[ie0].xyz[3*j+2];
      } else {
        t0[0] = btess->tess1d[ie0].xyz[0]-btess->tess1d[ie0].xyz[3];
        t0[1] = btess->tess1d[ie0].xyz[1]-btess->tess1d[ie0].xyz[4];
        t0[2] = btess->tess1d[ie0].xyz[2]-btess->tess1d[ie0].xyz[5];
      }
      dist = sqrt(t0[0]*t0[0] + t0[1]*t0[1] + t0[2]*t0[2]);
      if (dist != 0.0) {
        t0[0] /= dist;
        t0[1] /= dist;
        t0[2] /= dist;
      }
      if (senses[lim[i]+1] == 1) {
        t1[0] = btess->tess1d[ie1].xyz[3]-btess->tess1d[ie1].xyz[0];
        t1[1] = btess->tess1d[ie1].xyz[4]-btess->tess1d[ie1].xyz[1];
        t1[2] = btess->tess1d[ie1].xyz[5]-btess->tess1d[ie1].xyz[2];
      } else {
        j     = btess->tess1d[ie1].npts-2;
        t1[0] = btess->tess1d[ie1].xyz[3*j  ]-btess->tess1d[ie1].xyz[3*j+3];
        t1[1] = btess->tess1d[ie1].xyz[3*j+1]-btess->tess1d[ie1].xyz[3*j+4];
        t1[2] = btess->tess1d[ie1].xyz[3*j+2]-btess->tess1d[ie1].xyz[3*j+5];
      }
      dist = sqrt(t1[0]*t1[0] + t1[1]*t1[1] + t1[2]*t1[2]);
      if (dist != 0.0) {
        t1[0] /= dist;
        t1[1] /= dist;
        t1[2] /= dist;
      }
      dist = t0[0]*t1[0] + t0[1]*t1[1] + t0[2]*t1[2];
      if (outLevel > 1)
        printf("  Dot between %d %d = %lf\n", ie0+1, ie1+1, dist);
      if (dist > dmax) {
        dmax = dist;
        imax = i;
      }
    }
    if (imax == -1) return EGADS_INDEXERR;
    if (dmax < 1.0-edgeTOL) return EGADS_INDEXERR;

    for (i = imax; i < 3; i++) lim[i] = lim[i+1];
    lim[3]++;
    nside--;
    if (outLevel > 1)
      printf("  endIndex = %d %d %d %d,  nSide = %d\n", 
             lim[0], lim[1], lim[2], lim[3], nside);
  }

  return EGADS_SUCCESS;
}


int
EG_makeQuads(egObject *tess, double *parms, int index)
{
  int      i, j, k, l, m, n, outLevel, stat, oclass, mtype, ftype;
  int      nface, nloop, nedge, sens, npt, nx, *eindex, lim[4];
  int      npts, npat, save, iv, iv1, nside, pats[34], lens[4];
  int      *ptype, *pindex, *pin, *vpats, *senses, *ntable;
  double   *uvs, *quv, *xyz, *xyzs, limits[4], res[18], area;
  connect  *etable;
  egTessel *btess;
  egPatch  *patch;
  egObject *obj, *geom, **faces, **loops, **edges;

  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_makeQuads)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_makeQuads)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_makeQuads)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_makeQuads)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess2d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Face Tessellations (EG_makeQuads)!\n");
    return EGADS_NODATA;  
  }
  if ((index < 1) || (index > btess->nFace)) {
    if (outLevel > 0)
      printf(" EGADS Error: Index = %d [1-%d] (EG_makeQuads)!\n",
             index, btess->nFace);
    return EGADS_INDEXERR;
  }

  /* quad patch based on current Edge tessellations */
  
  stat = EG_getBodyTopos(obj, NULL, FACE, &nface, &faces);
  if (stat != EGADS_SUCCESS) return stat;
  stat = EG_getTopology(faces[index-1], &geom, &oclass, &ftype, limits,
                        &nloop, &loops, &senses);
  if (stat != EGADS_SUCCESS) {
    EG_free(faces);
    return stat;
  }
  if (nloop != 1) {
    if (outLevel > 0)
      printf(" EGADS Error: Face %d has %d loops (EG_makeQuads)!\n",
             index, nloop);
    EG_free(faces);
    return EGADS_TOPOERR;
  }
  stat = EG_getTopology(loops[0], &geom, &oclass, &mtype, limits,
                        &nedge, &edges, &senses);
  if (stat != EGADS_SUCCESS) {
    EG_free(faces);
    return stat;
  }
  if (nedge < 4) {
    if (outLevel > 0)
      printf(" EGADS Error: %d Edges in Face %d (EG_makeQuads)!\n", 
             nedge, index);
    EG_free(faces);
    return EGADS_INDEXERR;
  }

  /* get Edge Indices */
  eindex = (int *) EG_alloc(nedge*sizeof(int));
  if (eindex == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on %d Edges (EG_makeQuads)!\n",
             nedge);
    EG_free(faces);
    return EGADS_MALLOC;
  }
  for (i = 0; i < nedge; i++) {
    if (edges[i]->mtype == DEGENERATE) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge in Face %d is Degenerate (EG_makeQuads)!\n", 
               index);
      EG_free(eindex);
      EG_free(faces);
      return EGADS_INDEXERR;
    }
    eindex[i] = 0;
    for (j = 0; j < btess->nEdge; j++)
      if (edges[i] == btess->tess1d[j].obj) {
        eindex[i] = j+1;
        break;
      }
    if (eindex[i] == 0) {
      if (outLevel > 0)
        printf(" EGADS Error: Edge Not Found in Tess (EG_makeQuads)!\n");
      EG_free(eindex);
      EG_free(faces);
      return EGADS_NOTFOUND;
    }
  }
  
  /* block off the 4 sides if available */
  lim[0] = lens[0] = lens[1] = lens[2] = lens[3] = 0;
  lim[1] = 1;
  lim[2] = 2;
  lim[3] = 3;
  if (nedge > 4) {
    stat = EG_quadLoop(btess, outLevel, nedge, eindex, senses, 
                       parms, lim);
    if (stat != EGADS_SUCCESS) {
      if (outLevel > 0)
        printf(" EGADS Error: %d Edges in Face %d (EG_makeQuads)!\n", 
               nedge, index);
      EG_free(eindex);
      EG_free(faces);
      return stat;
    }
  }
  for (npts = l = i = 0; i < nedge; i++) {
    j     = eindex[i]-1;
    npts += btess->tess1d[j].npts-1;
    if (ftype == SFORWARD) {
      lens[l]   += btess->tess1d[j].npts-1;
    } else {
      lens[3-l] += btess->tess1d[j].npts-1;
    }
    if (lim[l] == i) l++;
  }

  /* allocate the info for the frame of the blocking */
  xyzs = (double *) EG_alloc(3*npts*sizeof(double));
  if (xyzs == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on %d XYZs (EG_makeQuads)!\n",
             npts);
    EG_free(eindex);
    EG_free(faces);
    return EGADS_MALLOC;
  }
  uvs = (double *) EG_alloc(2*npts*sizeof(double));
  if (uvs == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on %d Points (EG_makeQuads)!\n",
             npts);
    EG_free(xyzs);
    EG_free(eindex);
    EG_free(faces);
    return EGADS_MALLOC;
  }
  pin = (int *) EG_alloc(3*npts*sizeof(int));
  if (pin == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc on %d Pindex (EG_makeQuads)!\n",
             npts);
    EG_free(uvs);
    EG_free(xyzs);
    EG_free(eindex);
    EG_free(faces);
    return EGADS_MALLOC;
  }
 
  /* fill in uvs around the loop */

  for (npts = i = 0; i < nedge; i++) {
    if (ftype == SFORWARD) {
      j    =  eindex[i] - 1;
      sens =  senses[i];
      m    =  senses[i];
    } else {
      j    =  eindex[nedge-i-1] - 1;
      sens = -senses[nedge-i-1];
      m    =  senses[nedge-i-1];
    }
    if (sens == 1) {
      for (k = 0; k < btess->tess1d[j].npts-1; k++, npts++) {
        stat = EG_getEdgeUV(faces[index-1], btess->tess1d[j].obj, 
                            m, btess->tess1d[j].t[k], &uvs[2*npts]);
        if (stat != EGADS_SUCCESS) {
          EG_free(uvs);
          EG_free(pin);
          EG_free(xyzs);
          EG_free(eindex);
          EG_free(faces);
          return stat;        
        }
        xyzs[3*npts  ] = btess->tess1d[j].xyz[3*k  ];
        xyzs[3*npts+1] = btess->tess1d[j].xyz[3*k+1];
        xyzs[3*npts+2] = btess->tess1d[j].xyz[3*k+2];
        pin[3*npts  ]  =  j+1;
        pin[3*npts+1]  =  k+1;
        pin[3*npts+2]  = -j-1;
        if (k == 0) {
          pin[3*npts  ] = btess->tess1d[j].nodes[0];
          pin[3*npts+1] = 0;
        }
      }
    } else {
      for (k = btess->tess1d[j].npts-1; k >= 1; k--, npts++) {
        stat = EG_getEdgeUV(faces[index-1], btess->tess1d[j].obj, 
                            m, btess->tess1d[j].t[k], &uvs[2*npts]);
        if (stat != EGADS_SUCCESS) {
          EG_free(uvs);
          EG_free(pin);
          EG_free(xyzs);
          EG_free(eindex);
          EG_free(faces);
          return stat;        
        }
        xyzs[3*npts  ] = btess->tess1d[j].xyz[3*k  ];
        xyzs[3*npts+1] = btess->tess1d[j].xyz[3*k+1];
        xyzs[3*npts+2] = btess->tess1d[j].xyz[3*k+2];
        pin[3*npts  ]  =  j+1;
        pin[3*npts+1]  =  k+1;
        pin[3*npts+2]  = -j-1;
        if (k == btess->tess1d[j].npts-1) {
          pin[3*npts  ] = btess->tess1d[j].nodes[1];
          pin[3*npts+1] = 0;
        }
      }
    }
  }
  EG_free(eindex);
  
  i = npts-1;
  area = (uvs[0]+uvs[2*i])*(uvs[1]-uvs[2*i+1]);
  for (i = 0; i < npts-1; i++)
    area += (uvs[2*i+2]+uvs[2*i])*(uvs[2*i+3]-uvs[2*i+1]);
  area /= 2.0;
  if (outLevel > 1) 
    printf(" makeQuads: loop area = %lf,  ori = %d\n", area, ftype);

  stat = EG_quadFill(faces[index-1], parms, lens, uvs, 
                     &npt, &quv, &npat, pats, &vpats);
  EG_free(uvs);
  if (stat != EGADS_SUCCESS) {
    if (outLevel > 0)
      printf(" EGADS Error: quadFill = %d (EG_makeQuads)!\n",
             stat);
    EG_free(pin);
    EG_free(xyzs);
    EG_free(faces);
    return EGADS_CONSTERR;
  }

  xyz    = (double *) EG_alloc(3*npt*sizeof(double));
  ptype  = (int *)    EG_alloc(  npt*sizeof(int));
  pindex = (int *)    EG_alloc(  npt*sizeof(int));
  if ((xyz == NULL) || (ptype == NULL) || (pindex == NULL)) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc npts = %d (EG_makeQuads)!\n",
             npt);
    if (pindex != NULL) EG_free(pindex);
    if (ptype  != NULL) EG_free(ptype);
    if (xyz    != NULL) EG_free(xyz);
    EG_free(vpats);
    EG_free(quv);
    EG_free(pin);
    EG_free(xyzs);
    EG_free(faces);
    return EGADS_MALLOC;
  }
  for (i = 0; i < npts; i++) {
    pindex[i]  = pin[3*i  ];
    ptype[i]   = pin[3*i+1];
    xyz[3*i  ] = xyzs[3*i  ];
    xyz[3*i+1] = xyzs[3*i+1];
    xyz[3*i+2] = xyzs[3*i+2];
  }
  EG_free(xyzs);  
  for (i = npts; i < npt; i++) {
    pindex[i] = ptype[i] = -1;
    EG_evaluate(faces[index-1], &quv[2*i], res);
    xyz[3*i  ] = res[0];
    xyz[3*i+1] = res[1];
    xyz[3*i+2] = res[2];
  }
  EG_free(faces);
  patch = (egPatch *) EG_alloc(npat*sizeof(egPatch));
  if (patch == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Malloc npatchs = %d (EG_makeQuads)!\n",
             npat);
    EG_free(pindex);
    EG_free(ptype);
    EG_free(xyz);
    EG_free(vpats);
    EG_free(quv);
    EG_free(pin);
    return EGADS_MALLOC;
  }

  /* put back in face orientation */
  if (ftype != SFORWARD)
    for (iv = k = 0; k < npat; k++) {
      nx = pats[2*k  ];
      for (j = 0; j < pats[2*k+1]; j++) {
        for (i = 0; i < nx/2; i++) {
          m           = nx - i - 1;
          save        = vpats[iv+i];
          vpats[iv+i] = vpats[iv+m];
          vpats[iv+m] = save;
        }
        iv += nx;
      }
    }
    
  for (nx = m = 0; m < npat; m++) 
    nx += 2*pats[2*m] + 2*pats[2*m+1] - 4;
  ntable = (int *) EG_alloc(npt*sizeof(int));
  if (ntable == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Vert Table Malloc (EG_makeQuads)!\n");
    EG_free(patch);
    EG_free(pindex);
    EG_free(ptype);
    EG_free(xyz);
    EG_free(vpats);
    EG_free(quv);
    EG_free(pin);
    return EGADS_MALLOC;    
  }
  etable = (connect *) EG_alloc((nx+1)*sizeof(connect));
  if (etable == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: Edge Table Malloc (EG_makeQuads)!\n");
    EG_free(ntable);
    EG_free(patch);
    EG_free(pindex);
    EG_free(ptype);
    EG_free(xyz);
    EG_free(vpats);
    EG_free(quv);
    EG_free(pin);
    return EGADS_MALLOC;    
  }
  for (j = 0; j < npt; j++) ntable[j] = NOTFILLED;

  /* fill in the patch */
  for (k = m = 0; m < npat; m++) {
    if (outLevel > 1)
      printf("  Patch %d: size = %d %d\n", m+1, pats[2*m  ], pats[2*m+1]);
    patch[m].nu     = pats[2*m  ];
    patch[m].nv     = pats[2*m+1];
    patch[m].ipts   = (int *) EG_alloc(pats[2*m]*pats[2*m+1]*sizeof(int));
    patch[m].bounds = (int *) EG_alloc((2*(pats[2*m  ]-1) + 
                                        2*(pats[2*m+1]-1))*sizeof(int));
    for (n = j = 0; j < pats[2*m+1]; j++)
      for (i = 0; i < pats[2*m]; i++, n++, k++) 
        if (patch[m].ipts != NULL) patch[m].ipts[n] = vpats[k] + 1;
  }

  /* connect the patches */
  nside = -1;
  for (j = 0; j < npts-1; j++)
    EG_makeConnect(j+1, j+2, &pin[3*j+2], 
                   &nside, ntable, etable, index);
  EG_makeConnect(npts, 1, &pin[3*npts-1], 
                 &nside, ntable, etable, index);
  for (l = n = m = 0; m < npat; m++) {
    if (patch[m].bounds == NULL) continue;
    for (k = i = 0; i < patch[m].nu-1; i++, k++) {
      iv  = vpats[l+i  ] + 1;
      iv1 = vpats[l+i+1] + 1;
      patch[m].bounds[k] = n+i+1;
      EG_makeConnect(iv, iv1, &patch[m].bounds[k], 
                     &nside, ntable, etable, index);
    }
    for (i = 0; i < patch[m].nv-1; i++, k++) {
      iv  = vpats[l+(i+1)*patch[m].nu-1] + 1;
      iv1 = vpats[l+(i+2)*patch[m].nu-1] + 1;
      patch[m].bounds[k] = n+(i+1)*(patch[m].nu-1);
      EG_makeConnect(iv, iv1, &patch[m].bounds[k], 
                     &nside, ntable, etable, index);
    }
    for (i = 0; i < patch[m].nu-1; i++, k++) {
      iv  = vpats[l+patch[m].nu*patch[m].nv-i-1] + 1;
      iv1 = vpats[l+patch[m].nu*patch[m].nv-i-2] + 1;
      patch[m].bounds[k] = n+(patch[m].nu-1)*(patch[m].nv-1)-i;
      EG_makeConnect(iv, iv1, &patch[m].bounds[k], 
                     &nside, ntable, etable, index);
    }
    for (i = 0; i < patch[m].nv-1; i++, k++) {
      iv  = vpats[l+(patch[m].nv-i-1)*patch[m].nu] + 1;
      iv1 = vpats[l+(patch[m].nv-i-2)*patch[m].nu] + 1;
      patch[m].bounds[k] = n+(patch[m].nv-i-2)*(patch[m].nu-1);
      EG_makeConnect(iv, iv1, &patch[m].bounds[k], 
                     &nside, ntable, etable, index);
    }
    n += (patch[m].nu-1)*(patch[m].nv-1);
    l +=  patch[m].nu   * patch[m].nv;
  }

  /* report any unconnected boundary sides */
  for (j = 0; j <= nside; j++) {
    if (etable[j].tri == NULL) continue;
    printf(" EGADS Info: Face %d, Unconnected Quad Side %d %d = %d\n",
           index, etable[j].node1, etable[j].node2, *etable[j].tri);
    *etable[j].tri = 0;
  }

  EG_free(etable);
  EG_free(ntable);
  EG_free(vpats);
  EG_free(pin);
  
  /* delete any existing quads */
  EG_deleteQuads(btess, index);

  /* save away the patches */

  i = btess->nFace + index - 1;
  btess->tess2d[i].xyz    = xyz;
  btess->tess2d[i].uv     = quv;
  btess->tess2d[i].ptype  = ptype;
  btess->tess2d[i].pindex = pindex;
  btess->tess2d[i].npts   = npt;
  btess->tess2d[i].patch  = patch;
  btess->tess2d[i].npatch = npat;
  
  return EGADS_SUCCESS;
}


int
EG_getQuads(const egObject *tess, int index, int *len, const double **xyz, 
            const double **uv, const int **ptype, const int **pindex, 
            int *npatch)
{
  int      i, outLevel;
  egTessel *btess;
  egObject *obj;

  *len   = *npatch = 0;
  *xyz   = *uv     = NULL;
  *ptype = *pindex = NULL;
  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getQuads)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getQuads)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getQuads)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_getQuads)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess2d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Face Tessellations (EG_getQuads)!\n");
    return EGADS_NODATA;  
  }
  if ((index < 1) || (index > btess->nFace)) {
    if (outLevel > 0)
      printf(" EGADS Error: Index = %d [1-%d] (EG_getQuads)!\n",
             index, btess->nFace);
    return EGADS_INDEXERR;
  }

  i       = btess->nFace + index - 1;
  *len    = btess->tess2d[i].npts;
  *xyz    = btess->tess2d[i].xyz;
  *uv     = btess->tess2d[i].uv;
  *ptype  = btess->tess2d[i].ptype;
  *pindex = btess->tess2d[i].pindex;
  *npatch = btess->tess2d[i].npatch;

  return EGADS_SUCCESS;
}


int
EG_getPatch(const egObject *tess, int index, int patch, int *nu, int *nv, 
            const int **ipts, const int **bounds)
{
  int      i, outLevel;
  egTessel *btess;
  egObject *obj;

  *nu   = *nv     = 0;
  *ipts = *bounds = NULL;
  if (tess == NULL)                 return EGADS_NULLOBJ;
  if (tess->magicnumber != MAGIC)   return EGADS_NOTOBJ;
  if (tess->oclass != TESSELLATION) return EGADS_NOTTESS;
  outLevel = EG_outLevel(tess);
  
  btess = (egTessel *) tess->blind;
  if (btess == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Blind Object (EG_getPatch)!\n");  
    return EGADS_NOTFOUND;
  }
  obj = btess->src;
  if (obj == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: NULL Source Object (EG_getPatch)!\n");
    return EGADS_NULLOBJ;
  }
  if (obj->magicnumber != MAGIC) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not an Object (EG_getPatch)!\n");
    return EGADS_NOTOBJ;
  }
  if (obj->oclass != BODY) {
    if (outLevel > 0)
      printf(" EGADS Error: Source Not Body (EG_getPatch)!\n");
    return EGADS_NOTBODY;
  }
  if (btess->tess2d == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Face Tessellations (EG_getPatch)!\n");
    return EGADS_NODATA;  
  }
  if ((index < 1) || (index > btess->nFace)) {
    if (outLevel > 0)
      printf(" EGADS Error: Index = %d [1-%d] (EG_getPatch)!\n",
             index, btess->nFace);
    return EGADS_INDEXERR;
  }
  i = btess->nFace + index - 1;
  if (btess->tess2d[i].patch == NULL) {
    if (outLevel > 0)
      printf(" EGADS Error: No Patch Data (EG_getPatch)!\n");
    return EGADS_NODATA;
  }
  if ((patch < 1) || (patch > btess->tess2d[i].npatch)) {
    if (outLevel > 0)
      printf(" EGADS Error: Patch index = %d [1-%d] (EG_getPatch)!\n",
             patch, btess->tess2d[i].npatch);
    return EGADS_INDEXERR;
  }

  *nu     = btess->tess2d[i].patch[patch-1].nu;
  *nv     = btess->tess2d[i].patch[patch-1].nv;
  *ipts   = btess->tess2d[i].patch[patch-1].ipts;
  *bounds = btess->tess2d[i].patch[patch-1].bounds;
  
  return EGADS_SUCCESS;
}
