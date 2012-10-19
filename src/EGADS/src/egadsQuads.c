/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Quad Tessellation Functions
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "egadsTypes.h"
#include "egadsInternals.h"


#define MAXSIDE 501

#define AREA2D(a,b,c)   ((a[0]-c[0])*(b[1]-c[1]) -  (a[1]-c[1])*(b[0]-c[0]))
#define CROSS(a,b,c)      a[0] = (b[1]*c[2]) - (b[2]*c[1]);\
                          a[1] = (b[2]*c[0]) - (b[0]*c[2]);\
                          a[2] = (b[0]*c[1]) - (b[1]*c[0])
#define DOT(a,b)         (a[0]*b[0] + a[1]*b[1] + a[2]*b[2])
#define MAX(a,b)	(((a) > (b)) ? (a) : (b))
#define MIN(a,b)        (((a) < (b)) ? (a) : (b))


typedef struct {
  int nodes[4];         /* quad indices into Node list */
} Quad;


typedef struct {
  double uv[2];         /* (u,v) for node */
  double duv[2];        /* delta for coordinate update */
  double area;          /* accumulated area; -1 is boundary node */
  double xyz[3];	/* xyz for the node */
} Node;


  static Quad   *quads;
  static Node   *verts;
  static double flip;
  static int    nvert, nquad, sizes[8], last[MAXSIDE], unmap = 0;
  static int    *vpatch, patch[17][2], npatch;

  extern int    EG_evaluate( const egObject *geom, const double *param, 
                             double *results );


/* Compute arclength basis functions for TFI use */

static void 
EG_arcBasis(int nx, int ny, int *sideptr[], double *abasis[2])
{
  int    i, j, k, i0, im, j0, jm, nny = ny+1;
  double xi, et;
  double anorm;

  for (j = k = 0; j <= ny; j += ny, k = 2) {		/* j const boundaries */
    abasis[0][j] = 0.0;					/* i == 0 */
    for (i = 1; i <= nx; i++) {
      i0 = sideptr[k][i  ];
      im = sideptr[k][i-1];
      abasis[0][nny*i+j] = abasis[0][nny*(i-1)+j] + 
                                sqrt((verts[i0].uv[0] - verts[im].uv[0]) *
                                     (verts[i0].uv[0] - verts[im].uv[0]) +
                                     (verts[i0].uv[1] - verts[im].uv[1]) *
                                     (verts[i0].uv[1] - verts[im].uv[1]));
    }
    if (abasis[0][nny*nx+j] > 1.0e-6) {			/* degenerate */
      anorm = 1.0/abasis[0][nny*nx+j];
      for (i = 0; i <= nx; i++) abasis[0][nny*i+j] *= anorm;
    } else {
      anorm = 1.0/(double)(nx);
      for (i = 0; i <= nx; i++) abasis[0][nny*i+j] = (double) i*anorm;
    }
  }
  for (i = 0; i <= nx; i++) {				/* boundaries */
    abasis[1][nny*i   ] = 0.0;
    abasis[1][nny*i+ny] = 1.0;
  }

  for (i = 0, k = 1; i <= nx; i+=nx, k = 3) {		/* i const boundaries */
    abasis[1][nny*i] = 0.0;
    for (j = 1; j <= ny; j++) {
      j0 = sideptr[k][j  ];
      jm = sideptr[k][j-1];
      abasis[1][nny*i+j] = abasis[1][nny*i+(j-1)] + 
                              sqrt((verts[j0].uv[0] - verts[jm].uv[0]) *
                                   (verts[j0].uv[0] - verts[jm].uv[0]) +
                                   (verts[j0].uv[1] - verts[jm].uv[1]) *
                                   (verts[j0].uv[1] - verts[jm].uv[1]));
    }
    if (abasis[1][nny*i+ny] > 1.0e-6) {			/* degenerate */
      anorm = 1.0/abasis[1][nny*i+ny];
      for (j = 0; j <= ny; j++) abasis[1][nny*i+j] *= anorm;
    } else {
      anorm = 1.0/(double)(ny);
      for (j = 0; j <= ny; j++) abasis[1][nny*i+j] = (double) j*anorm;
    }
  }
  for (j = 0; j <= ny; j++) {
    abasis[0][       j] = 0.0;
    abasis[0][nny*nx+j] = 1.0;
  }

  for (j = 1; j < ny; j++) {
    for (i = 1; i < nx; i++) {
      anorm = 1.0 - (abasis[0][nny*i +ny]-abasis[0][nny*i  ]) *
                    (abasis[1][nny*nx+ j]-abasis[1][      j]);

      xi = ( abasis[0][nny*i  ] -
             abasis[1][      j]*(abasis[0][nny*i +ny]-abasis[0][nny*i    ]) ) /
           anorm;

      et = ( abasis[1][      j] -
             abasis[0][nny*i  ]*(abasis[1][nny*nx+ j]-abasis[1][        j]) ) /
           anorm;

      abasis[0][nny*i+j] = xi;
      abasis[1][nny*i+j] = et;
    }
  }

}


/* remap into the actual UV space */

static void
EG_getside(int iuv, double t, int len, int *side, double *uvx,
           double *uv, double *uvi)
{
  int    i0, i1, j;
  double dis;

  for (j = 1; j < len; j++) {
    i0 = side[j-1];
    i1 = side[j  ];
    if (((t >= uvx[2*i0+iuv]) && (t <= uvx[2*i1+iuv])) ||
        ((t >= uvx[2*i1+iuv]) && (t <= uvx[2*i0+iuv]))) {
      dis = (t - uvx[2*i0+iuv])/(uvx[2*i1+iuv]-uvx[2*i0+iuv]);
      uvi[0] = uv[2*i0  ] - dis*(uv[2*i0  ] - uv[2*i1  ]);
      uvi[1] = uv[2*i0+1] - dis*(uv[2*i0+1] - uv[2*i1+1]);
      return;
    }
  }
}


static int
EG_dQuadTFI(int *elen, double *uv, int npts, double *uvx)
{
  int    i, j, k, len, ll, lr, ur, ul;
  int    cipt[4], *sideptr[4];
  double et, xi, uvi[2][2], uvj[2][2], smap[4];

  cipt[ 0] = 0;
  len      = elen[0];
  cipt[ 1] = len;
  len     += elen[1];
  cipt[ 2] = len;
  len     += elen[2];
  cipt[ 3] = len;

  /* set the exterior block sides */

  for (i = 0; i < 4; i++) sideptr[i] = NULL;
  for (j = i = 0; i < 4; i++) {
    len = elen[i] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      return -1;
    }
    if (i >= 2) {
      for (k = len-1; k > 0; k--, j++) {
        sideptr[i][k] = j;
      }
      sideptr[i][0] = j;
      if (i == 3) sideptr[i][0] = 0;
    } else {
      for (k = 0; k < len-1; k++, j++) {
        sideptr[i][k] = j;
      }
      sideptr[i][len-1] = j;
    }
  }

  /* create the quads and get coordinates via TFI */

  len = elen[0] + elen[1] + elen[2] + elen[3];
  ll  = 2*cipt[0];
  lr  = 2*cipt[1];
  ur  = 2*cipt[2];
  ul  = 2*cipt[3];
  for (k = len; k < npts; k++) {
    EG_getside(0, uvx[2*k  ], elen[3]+1, sideptr[3], uvx, uv, uvi[0]);
    EG_getside(0, uvx[2*k  ], elen[1]+1, sideptr[1], uvx, uv, uvi[1]);
    EG_getside(1, uvx[2*k+1], elen[0]+1, sideptr[0], uvx, uv, uvj[0]);
    EG_getside(1, uvx[2*k+1], elen[2]+1, sideptr[2], uvx, uv, uvj[1]);
    smap[3] = sqrt((uvi[0][0]-uv[ll  ])*(uvi[0][0]-uv[ll  ]) +
                   (uvi[0][1]-uv[ll+1])*(uvi[0][1]-uv[ll+1])) /
              sqrt((uv[ul  ] -uv[ll  ])*(uv[ul  ] -uv[ll  ]) +
                   (uv[ul+1] -uv[ll+1])*(uv[ul+1] -uv[ll+1]));
    smap[1] = sqrt((uvi[1][0]-uv[lr  ])*(uvi[1][0]-uv[lr  ]) +
                   (uvi[1][1]-uv[lr+1])*(uvi[1][1]-uv[lr+1])) /
              sqrt((uv[ur  ] -uv[lr  ])*(uv[ur  ] -uv[lr  ]) +
                   (uv[ur+1] -uv[lr+1])*(uv[ur+1] -uv[lr+1]));
    smap[0] = sqrt((uvj[0][0]-uv[ll  ])*(uvj[0][0]-uv[ll  ]) +
                   (uvj[0][1]-uv[ll+1])*(uvj[0][1]-uv[ll+1])) /
              sqrt((uv[lr  ] -uv[ll  ])*(uv[lr  ] -uv[ll  ]) +
                   (uv[lr+1] -uv[ll+1])*(uv[lr+1] -uv[ll+1]));
    smap[2] = sqrt((uvj[1][0]-uv[ul  ])*(uvj[1][0]-uv[ul  ]) +
                   (uvj[1][1]-uv[ul+1])*(uvj[1][1]-uv[ul+1])) /
              sqrt((uv[ur  ] -uv[ul  ])*(uv[ur  ] -uv[ul  ]) +
                   (uv[ur+1] -uv[ul+1])*(uv[ur+1] -uv[ul+1]));
    et = smap[3]*(1.0-uvx[2*k+1]) + smap[1]*uvx[2*k+1];
    xi = smap[0]*(1.0-uvx[2*k  ]) + smap[2]*uvx[2*k  ];

    uvx[2*k  ] = (1.0-xi)            * uvi[0][0] +
                 (    xi)            * uvi[1][0] +
                            (1.0-et) * uvj[0][0] +
                            (    et) * uvj[1][0] -
                 (1.0-xi) * (1.0-et) * uv[ll  ] -
                 (1.0-xi) * (    et) * uv[ul  ] -
                 (    xi) * (1.0-et) * uv[lr  ] -
                 (    xi) * (    et) * uv[ur  ];
    uvx[2*k+1] = (1.0-xi)            * uvi[0][1] +
                 (    xi)            * uvi[1][1] +
                            (1.0-et) * uvj[0][1] +
                            (    et) * uvj[1][1] -
                 (1.0-xi) * (1.0-et) * uv[ll+1] -
                 (1.0-xi) * (    et) * uv[ul+1] -
                 (    xi) * (1.0-et) * uv[lr+1] -
                 (    xi) * (    et) * uv[ur+1];
  }
  for (k = 0; k < 2*len; k++) uvx[k] = uv[k];

  /* free up our integrated sides */
  for (i = 0; i < 4; i++) EG_free(sideptr[i]);

  return 0;
}


/* get the vertex count for the suite of blocks */

static int
EG_getVertCnt(int len, int blocks[][6])
{
  int k, cnt;
  
  npatch = len;
  for (cnt = k = 0; k < len; k++) {
    patch[k][0] = sizes[blocks[k][0]] + 1;
    patch[k][1] = sizes[blocks[k][1]] + 1;
    cnt += patch[k][0]*patch[k][1];
  }

  return cnt;
}


/* sets the individual quads by looping through the blocks */

static void 
EG_setQuads(int len, int blocks[][6], int *sideptr[])
{
  int    i, j, k, i0, i1, i2, i3, ilast, nx, ny;
  int    ll, lr, ur, ul, j0, jm, ii, im, iv, sav;
  double et, xi;

  nquad = iv = 0; 
  for (k = 0; k < len; k++) {
    nx = sizes[blocks[k][0]];
    ny = sizes[blocks[k][1]];
    i0 = blocks[k][2];
    i1 = blocks[k][3];
    i2 = blocks[k][4];
    i3 = blocks[k][5];
    ll = sideptr[i2][0];
    lr = sideptr[i2][nx];
    ur = sideptr[i3][nx];
    ul = sideptr[i3][0];
    for (i = 0; i < nx+1; i++) last[i] = sideptr[i2][i];
    for (j = 0; j < ny; j++) {
      ii    = sideptr[i0][j+1];
      if (i1 > 0) {
        im  = sideptr[i1][j+1];
      } else {
        im = sideptr[-i1][ny-j-1];
      }
      et    = ((double) (j+1)) / ((double) ny);
      ilast = sideptr[i0][j+1];
      sav   = nquad;
      for (i = 0; i < nx; i++) {
        j0 = sideptr[i2][i+1];
        jm = sideptr[i3][i+1];
        xi = ((double) (i+1)) / ((double) nx);
        quads[nquad].nodes[0] = last[i  ];
        quads[nquad].nodes[1] = last[i+1];
        if (j == ny-1) {
          quads[nquad].nodes[2] = sideptr[i3][i+1];
          quads[nquad].nodes[3] = sideptr[i3][i  ];
        } else {
          if (i == nx-1) {
            if (i1 > 0) {
              quads[nquad].nodes[2] = sideptr[i1][j+1];
              last[i]  = ilast;
              last[nx] = sideptr[i1][j+1];
            } else {
              quads[nquad].nodes[2] = sideptr[-i1][ny-j-1];
              last[i]  = ilast;
              last[nx] = sideptr[-i1][ny-j-1];
            }
          } else {
            quads[nquad].nodes[2] = nvert;
            verts[nvert].uv[0]    = (1.0-xi)            * verts[ii].uv[0] +
                                    (    xi)            * verts[im].uv[0] +
                                               (1.0-et) * verts[j0].uv[0] +
                                               (    et) * verts[jm].uv[0] -
                                    (1.0-xi) * (1.0-et) * verts[ll].uv[0] -
                                    (1.0-xi) * (    et) * verts[ul].uv[0] -
                                    (    xi) * (1.0-et) * verts[lr].uv[0] -
                                    (    xi) * (    et) * verts[ur].uv[0];
            verts[nvert].uv[1]    = (1.0-xi)            * verts[ii].uv[1] +
                                    (    xi)            * verts[im].uv[1] +
                                               (1.0-et) * verts[j0].uv[1] +
                                               (    et) * verts[jm].uv[1] -
                                    (1.0-xi) * (1.0-et) * verts[ll].uv[1] -
                                    (1.0-xi) * (    et) * verts[ul].uv[1] -
                                    (    xi) * (1.0-et) * verts[lr].uv[1] -
                                    (    xi) * (    et) * verts[ur].uv[1];
            verts[nvert].area     = 0.0;
            nvert++;
          }
          quads[nquad].nodes[3] = ilast;
          last[i] = ilast;
          ilast   = nvert-1;
        }
        nquad++;
      }
      if (j == 0) {
        vpatch[iv] = quads[sav].nodes[0];
        iv++;
        for (i = 0; i < nx; i++, iv++) 
          vpatch[iv] = quads[sav+i].nodes[1];
      }
      vpatch[iv] = quads[sav].nodes[3];
      iv++;
      for (i = 0; i < nx; i++, sav++, iv++)
        vpatch[iv] = quads[sav].nodes[2];
    }
  }
}


/* perform the laplacian smoothing on the grid vertices */

static void
EG_smoothQuads(const egObject *face, int len, int npass)
{
  int           i, j, i0, i1, i2, i3, status, pass, outLevel;
  double        qarea, sum, big, delta1, sums[2], x1[3], x2[3], xn[3];
  double        tAreaUV, tAreaXYZ, holdArea, results[18];
  static double wXYZ = 0.75;

  outLevel = EG_outLevel(face);

  /* outer iteration -- pass 1 (uv only) */

  for (i = 0; i < len; i++) {

    /* initialize deltas */
    for (j = 0; j < nvert; j++) { 
      verts[j].duv[0] = verts[j].duv[1] = 0.0;
      if (verts[j].area > 0.0) verts[j].area = 0.0;
    }
    /* calculate and distribute change */
    for (j = 0; j < nquad; j++) {
      i0    = quads[j].nodes[0];
      i1    = quads[j].nodes[1];
      i2    = quads[j].nodes[2];
      i3    = quads[j].nodes[3];
      qarea = flip*(AREA2D(verts[i0].uv, verts[i1].uv, verts[i2].uv) +
                    AREA2D(verts[i0].uv, verts[i2].uv, verts[i3].uv));
      if (qarea <= 0.0) {
        qarea = -qarea;
#ifdef DEBUG
        printf(" Quad %d: Neg Area = %le\n", j, qarea);
#endif
      }

      sum = qarea*(verts[i0].uv[0] + verts[i1].uv[0] + verts[i2].uv[0] +
                   verts[i3].uv[0])/4.0;
      verts[i0].duv[0] += sum;
      verts[i1].duv[0] += sum;
      verts[i2].duv[0] += sum;
      verts[i3].duv[0] += sum;
      sum = qarea*(verts[i0].uv[1] + verts[i1].uv[1] + verts[i2].uv[1] +
                   verts[i3].uv[1])/4.0;
      verts[i0].duv[1] += sum;
      verts[i1].duv[1] += sum;
      verts[i2].duv[1] += sum;
      verts[i3].duv[1] += sum;

      if (verts[i0].area >= 0.0) verts[i0].area += qarea;
      if (verts[i1].area >= 0.0) verts[i1].area += qarea;
      if (verts[i2].area >= 0.0) verts[i2].area += qarea;
      if (verts[i3].area >= 0.0) verts[i3].area += qarea;
    }
    /* update distributions */
    big = 0.0;
    for (j = 0; j < nvert; j++) {
      if (verts[j].area <= 0.0) continue;
      sums[0] = verts[j].duv[0]/verts[j].area;
      sums[1] = verts[j].duv[1]/verts[j].area;
      sum     = fabs(sums[0] - verts[j].uv[0]);
      if (big < sum) big = sum;
      sum     = fabs(sums[1] - verts[j].uv[1]);
      if (big < sum) big = sum;
      verts[j].uv[0] = sums[0];
      verts[j].uv[1] = sums[1];
    }
    if (i == 0) {
      delta1 = big;
      if (delta1 == 0.0) break;
    } else {
      if (big/delta1 < 1.e-3) break;
    }
  }

  /* pseudo non-linear loop */

  for (pass = 0; pass < npass; pass++) {

    /* get xyz */
    for (j = 0; j < nvert; j++) {
      status = EG_evaluate(face, verts[j].uv, results);
      if (status != EGADS_SUCCESS) {
        if (outLevel > 0)
          printf(" EGADS Info: EG_evaluate = %d (EG_smoothQuad)!\n", 
                 status);
        return;
      }
      verts[j].xyz[0] = results[0];
      verts[j].xyz[1] = results[1];
      verts[j].xyz[2] = results[2];
    }

    tAreaUV = tAreaXYZ = 0.0;
    for (j  = 0; j < nquad; j++) {
      i0        = quads[j].nodes[0];
      i1        = quads[j].nodes[1];
      i2        = quads[j].nodes[2];
      i3        = quads[j].nodes[3];
      holdArea  = flip*(AREA2D(verts[i0].uv, verts[i1].uv, verts[i2].uv) +
                        AREA2D(verts[i0].uv, verts[i2].uv, verts[i3].uv));
      if (holdArea < 0.0) holdArea = -holdArea;
      tAreaUV  += holdArea;
      x1[0]     = verts[i1].xyz[0] - verts[i0].xyz[0];
      x2[0]     = verts[i2].xyz[0] - verts[i0].xyz[0];
      x1[1]     = verts[i1].xyz[1] - verts[i0].xyz[1];
      x2[1]     = verts[i2].xyz[1] - verts[i0].xyz[1];
      x1[2]     = verts[i1].xyz[2] - verts[i0].xyz[2];
      x2[2]     = verts[i2].xyz[2] - verts[i0].xyz[2];
      CROSS(xn, x1, x2);
      holdArea  = DOT(xn, xn);
      if (holdArea <  0.0) holdArea = -holdArea;
      tAreaXYZ += holdArea;
      x1[0]     = verts[i3].xyz[0] - verts[i0].xyz[0];
      x1[1]     = verts[i3].xyz[1] - verts[i0].xyz[1];
      x1[2]     = verts[i3].xyz[2] - verts[i0].xyz[2];
      CROSS(xn, x2, x1);
      holdArea  = DOT(xn, xn);
      if (holdArea <  0.0) holdArea = -holdArea;
      tAreaXYZ += holdArea;
    }
#ifdef DEBUG
    printf(" ** %d   Areas = %le  %le **\n", pass, tAreaUV, tAreaXYZ);
#endif

    /* outer iteration -- pass 2 (mix) */
    for (i = 0; i < len; i++) {

      /* initialize deltas */
      for (j = 0; j < nvert; j++) { 
        verts[j].duv[0] = verts[j].duv[1] = 0.0;
        if (verts[j].area > 0.0) verts[j].area = 0.0;
      }
      /* calculate and distribute change */
      for (j = 0; j < nquad; j++) {
        i0    = quads[j].nodes[0];
        i1    = quads[j].nodes[1];
        i2    = quads[j].nodes[2];
        i3    = quads[j].nodes[3];
        qarea = flip*(AREA2D(verts[i0].uv, verts[i1].uv, verts[i2].uv) +
                      AREA2D(verts[i0].uv, verts[i2].uv, verts[i3].uv));
        if (qarea <= 0.0) {
          qarea = -qarea;
#ifdef DEBUG
          printf(" Quad %d: Neg Area = %le\n", j, qarea);
#endif
        }
        qarea *= (1.0-wXYZ)/tAreaUV;

        x1[0]     = verts[i1].xyz[0] - verts[i0].xyz[0];
        x2[0]     = verts[i2].xyz[0] - verts[i0].xyz[0];
        x1[1]     = verts[i1].xyz[1] - verts[i0].xyz[1];
        x2[1]     = verts[i2].xyz[1] - verts[i0].xyz[1];
        x1[2]     = verts[i1].xyz[2] - verts[i0].xyz[2];
        x2[2]     = verts[i2].xyz[2] - verts[i0].xyz[2];
        CROSS(xn, x1, x2);
        holdArea  = DOT(xn, xn);
        if (holdArea <  0.0) holdArea = -holdArea;
        qarea    += holdArea*wXYZ/tAreaXYZ;
        x1[0]     = verts[i3].xyz[0] - verts[i0].xyz[0];
        x1[1]     = verts[i3].xyz[1] - verts[i0].xyz[1];
        x1[2]     = verts[i3].xyz[2] - verts[i0].xyz[2];
        CROSS(xn, x2, x1);
        holdArea  = DOT(xn, xn);
        if (holdArea <  0.0) holdArea = -holdArea;
        qarea    += holdArea*wXYZ/tAreaXYZ;

        sum = qarea*(verts[i0].uv[0] + verts[i1].uv[0] + verts[i2].uv[0] +
                     verts[i3].uv[0])/4.0;
        verts[i0].duv[0] += sum;
        verts[i1].duv[0] += sum;
        verts[i2].duv[0] += sum;
        verts[i3].duv[0] += sum;
        sum = qarea*(verts[i0].uv[1] + verts[i1].uv[1] + verts[i2].uv[1] +
                     verts[i3].uv[1])/4.0;
        verts[i0].duv[1] += sum;
        verts[i1].duv[1] += sum;
        verts[i2].duv[1] += sum;
        verts[i3].duv[1] += sum;

        if (verts[i0].area >= 0.0) verts[i0].area += qarea;
        if (verts[i1].area >= 0.0) verts[i1].area += qarea;
        if (verts[i2].area >= 0.0) verts[i2].area += qarea;
        if (verts[i3].area >= 0.0) verts[i3].area += qarea;
      }
      /* update distributions */
      big = 0.0;
      for (j = 0; j < nvert; j++) {
        if (verts[j].area <= 0.0) continue;
        sums[0] = verts[j].duv[0]/verts[j].area;
        sums[1] = verts[j].duv[1]/verts[j].area;
        sum     = fabs(sums[0] - verts[j].uv[0]);
        if (big < sum) big = sum;
        sum     = fabs(sums[1] - verts[j].uv[1]);
        if (big < sum) big = sum;
        verts[j].uv[0] = sums[0];
        verts[j].uv[1] = sums[1];
      }
      if (i == 0) {
        delta1 = big;
        if (delta1 == 0.0) break;
      } else {
        if (big/delta1 < 1.e-3) break;
      }
    }

  }

}


/* general blocking case */

static int
EG_quadFillG(const egObject *face, int nsp, int *indices, int *elens, 
             double *uv, int *npts, double **uvs)
{
  int    i, j, k, len, N, M, P, Q, outLevel;
  int    i0, i1, cipt[26], *sideptr[42];
  double sums[2], cpts[26][2], *uvb;
  static int sides[42][3] = {  0,  0,  4,   1,  4, 13,   2, 13, 19, 
                               3, 19, 20,   6, 20, 21,   4, 21, 22,
                               5, 22, 24,   2, 23, 24,   7, 18, 23,
                               1, 25, 18,   6, 11, 25,   7,  7, 11,
                               0,  3,  7,   5,  2,  3,   4,  1,  2,
                               3,  0,  1,   3,  4,  5,   0,  1,  5,
                               3, 13, 14,   1,  5, 14,   2, 14, 20,
                               4,  5,  6,   0,  2,  6,   6,  5,  8,
                               4,  8,  9,   6,  6,  9,   6, 14, 15,
                               1,  8, 15,   2, 15, 21,   4, 15, 16,
                               1,  9, 16,   2, 16, 22,   5, 16, 23,
                               7, 17, 16,   5, 17, 18,   7,  9, 12,
                               1, 12, 17,   7,  6, 10,   6, 10, 12,
                               5,  6,  7,   5, 10, 11,   5, 12, 25 };

                      /*       size    sides           */
  static int blocks[17][6] = { 0, 3,   15, 16,  0, 17,
                               1, 3,   16, 18,  1, 19,
                               2, 3,   18,  3,  2, 20,
                               0, 4,   14, 21, 17, 22,
                               6, 4,   21, 24, 23, 25,
                               1, 6,   23, 26, 19, 27,
                               2, 6,   26,  4, 20, 28,
                               1, 4,   24, 29, 27, 30,
                               2, 4,   29,  5, 28, 31,
                               2, 5,   32,  6, 31,  7,
                               7, 5,   34, 32, 33,  8,
                               1, 7,   35,-33, 30, 36,
                               6, 7,   37, 35, 25, 38,
                               0, 5,   13, 39, 22, 12,
                               7, 5,   39, 40, 37, 11,
                               6, 5,   40, 41, 38, 10,
                               1, 5,   41, 34, 36,  9 };

                      /*      center   stencil nodes     */     
  static int interior[10][6] = { 5,    1,  4, 14,  8,  6,
                                 6,    5,  7,  9, 10,  2,
                                 8,    5,  9, 15, -1, -1,
                                 9,    8, 12, 16,  6, -1,
                                10,   12,  6, 11, -1, -1,
                                12,    9, 25, 17, 10, -1,
                                14,   13, 15, 20,  5, -1,
                                15,   14, 16, 21,  8, -1,
                                16,   15, 17, 22,  9, 25,
                                17,   16, 18, 12, -1, -1 };

  outLevel = EG_outLevel(face);
  
  /* get and check sizes */

  N =  elens[0];
  M =  elens[3];
  P =  elens[1] - M;
  Q = (elens[2] - N - P)/2;
  if (Q*2 != elens[2]-N-P) {
    if (outLevel > 0) {
      printf(" EGADS Info: General case off by 1 - %d %d  %d %d\n",
             elens[0], elens[2], elens[1], elens[3]);
      printf("             N = %d, M = %d, P = %d, Q = %d\n",
             N, M, P, Q);
    }
    return -2;
  }

  sizes[0] = sizes[1] = sizes[2] = N/3;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[0]++;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[2]++;
  sizes[3] = sizes[4] = sizes[5] = M/3;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[3]++;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[5]++;
  sizes[6] = P;
  sizes[7] = Q;
  for (i = 0; i < 8; i++) 
    if (sizes[i] > MAXSIDE-1) return -3;

  /* set the 26 critical points -- 16 exterior */

  cpts[ 0][0] = uv[0];
  cpts[ 0][1] = uv[1];
  cipt[ 0]    = indices[0];
  len  = sizes[0];
  cpts[ 4][0] = uv[2*len  ];
  cpts[ 4][1] = uv[2*len+1];
  cipt[ 4]    = indices[len];
  len += sizes[1];
  cpts[13][0] = uv[2*len  ];
  cpts[13][1] = uv[2*len+1];
  cipt[13]    = indices[len];
  len += sizes[2];
  cpts[19][0] = uv[2*len  ];
  cpts[19][1] = uv[2*len+1];
  cipt[19]    = indices[len];
  len += sizes[3];
  cpts[20][0] = uv[2*len  ];
  cpts[20][1] = uv[2*len+1];
  cipt[20]    = indices[len];
  len += sizes[6];
  cpts[21][0] = uv[2*len  ];
  cpts[21][1] = uv[2*len+1];
  cipt[21]    = indices[len];
  len += sizes[4];
  cpts[22][0] = uv[2*len  ];
  cpts[22][1] = uv[2*len+1];
  cipt[22]    = indices[len];
  len += sizes[5];
  cpts[24][0] = uv[2*len  ];
  cpts[24][1] = uv[2*len+1];
  cipt[24]    = indices[len];
  len += sizes[2];
  cpts[23][0] = uv[2*len  ];
  cpts[23][1] = uv[2*len+1];
  cipt[23]    = indices[len];
  len += sizes[7];
  cpts[18][0] = uv[2*len  ];
  cpts[18][1] = uv[2*len+1];
  cipt[18]    = indices[len];
  len += sizes[1];
  cpts[25][0] = uv[2*len  ];
  cpts[25][1] = uv[2*len+1];
  cipt[25]    = indices[len];
  len += sizes[6];
  cpts[11][0] = uv[2*len  ];
  cpts[11][1] = uv[2*len+1];
  cipt[11]    = indices[len];
  len += sizes[7];
  cpts[ 7][0] = uv[2*len  ];
  cpts[ 7][1] = uv[2*len+1];
  cipt[ 7]    = indices[len];
  len += sizes[0];
  cpts[ 3][0] = uv[2*len  ];
  cpts[ 3][1] = uv[2*len+1];
  cipt[ 3]    = indices[len];
  len += sizes[5];
  cpts[ 2][0] = uv[2*len  ];
  cpts[ 2][1] = uv[2*len+1];
  cipt[ 2]    = indices[len];
  len += sizes[4];
  cpts[ 1][0] = uv[2*len  ];
  cpts[ 1][1] = uv[2*len+1];
  cipt[ 1]    = indices[len];

  /* guess the interior */
  for (j = 0; j < 10; j++) 
    cpts[interior[j][0]][0] = cpts[interior[j][0]][1] = 0.0;
  for (i = 0; i < 10; i++)
    for (j = 0; j < 10; j++) {
      sums[0] = sums[1] = 0.0;
      for (len = k = 0; k < 5; k++) {
        if (interior[j][k+1] < 0) continue;
        sums[0] += cpts[interior[j][k+1]][0];
        sums[1] += cpts[interior[j][k+1]][1];
        len++;
      }
      cpts[interior[j][0]][0] = sums[0]/len;
      cpts[interior[j][0]][1] = sums[1]/len;
    }

  len    = EG_getVertCnt(17, blocks);
  vpatch = (int *) EG_alloc(len*sizeof(int));
  if (vpatch == NULL) return -1;
  
  /* allocate our temporary storage */

  len   = MAX(elens[1], elens[2]);
  quads = (Quad *) EG_alloc(len*len*sizeof(Quad));
  if (quads == NULL) {
    EG_free(vpatch);
    return -1;
  }
  verts = (Node *) EG_alloc((len+1)*(len+1)*sizeof(Node));
  if (verts == NULL) {
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }

  /* initialize the vertices */

  nvert = elens[0] + elens[1] + elens[2] + elens[3];
  for (i = 0; i < nvert; i++) {
    j = indices[i];
    verts[j].uv[0] = uv[2*i  ];
    verts[j].uv[1] = uv[2*i+1];
    verts[j].area  = -1.0;
  }
  for (i = 0; i < 10; i++) {
    j = interior[i][0];
    verts[nvert].uv[0] = cpts[j][0];
    verts[nvert].uv[1] = cpts[j][1];
    verts[nvert].area  = 0.0;
    cipt[j]            = nvert;
    nvert++;
  }

  /* set the exterior block sides */

  for (i = 0; i < 42; i++) sideptr[i] = NULL;
  for (j = i = 0; i < 16; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      EG_free(vpatch);
      return -1;
    }
    if (i >= 7) {
      for (k = len-1; k > 0; k--, j++) sideptr[i][k] = indices[j];
      if (i == 15) {
        sideptr[i][0] = indices[0];
      } else {
        sideptr[i][0] = indices[j];
      }
    } else {
      for (k = 0; k < len-1; k++, j++) sideptr[i][k] = indices[j];
      sideptr[i][len-1] = indices[j];
    }
  }

  /* do the interior sides */

  for (i = 16; i < 42; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      EG_free(vpatch);
      return -1;
    }
    i0 = sides[i][1];
    i1 = sides[i][2];
    sideptr[i][0] = cipt[i0];
    for (j = 1; j < len-1; j++) {
      verts[nvert].uv[0] = cpts[i0][0] + j*(cpts[i1][0]-cpts[i0][0])/(len-1);
      verts[nvert].uv[1] = cpts[i0][1] + j*(cpts[i1][1]-cpts[i0][1])/(len-1);
      verts[nvert].area  = 0.0;
      sideptr[i][j]      = nvert;
      nvert++;
    }
    sideptr[i][len-1] = cipt[i1];
  }

  /* start filling the quads by specifying the 17 blocks */

  EG_setQuads(17, blocks, sideptr);

  /* free up our integrated sides */
  for (i = 0; i < 42; i++) EG_free(sideptr[i]);

  /* get the actual storage that we return the data with */

  uvb = (double *) EG_alloc(2*nvert*sizeof(double));
  if (uvb == NULL) {
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;    
  }

  /* calculate the actual coordinates */

  len = elens[1]*elens[2];
  EG_smoothQuads(face, len, nsp);

  /* fill the memory to be returned */

  for (j = 0; j < nvert; j++) {
    uvb[2*j  ] = verts[j].uv[0];
    uvb[2*j+1] = verts[j].uv[1];
  }

  /* cleanup and exit */

  *npts = nvert;
  *uvs  = uvb;
  EG_free(verts);

  return 0;
}


/* No P case */

static int
EG_quadFillQ(const egObject *face, int nsp, int *indices, int *elens, 
             double *uv, int *npts, double **uvs)
{
  int    i, j, k, len, N, M, P, Q, outLevel;
  int    i0, i1, cipt[20], *sideptr[31];
  double sums[2], cpts[20][2], *uvb;
  static int sides[31][3] = {  0,  0,  1,   1,  1,  2,   2,  2,  3, 
                               3,  3,  4,   4,  4,  5,   5,  5,  6,
                               2,  7,  6,   7,  8,  7,   1,  9,  8,
                               7, 10,  9,   0, 11, 10,   5, 12, 11,
                               4, 13, 12,   3,  0, 13,   3,  1, 14,
                               0, 13, 14,   3,  2, 15,   1, 14, 15,
                               2, 15,  4,   4, 14, 16,   0, 12, 16,
                               4, 15, 17,   1, 16, 17,   2, 17,  5,
                               5, 16, 10,   7, 16, 18,   5, 18,  9,
                               7, 19, 17,   1, 18, 19,   5, 17,  7,
                               5, 19,  8 };

                      /*       size    sides           */
  static int blocks[12][6] = { 0, 3,   13, 14,  0, 15,
                               1, 3,   14, 16,  1, 17,
                               2, 3,   16,  3,  2, 18,
                               0, 4,   12, 19, 15, 20,
                               1, 4,   19, 21, 17, 22,
                               2, 4,   21,  4, 18, 23,
                               0, 5,   11, 24, 20, 10,
                               7, 5,   24, 26, 25,  9,
                               1, 7,   25,-27, 22, 28,
                               7, 5,   30, 29, 27,  7,
                               2, 5,   29,  5, 23,  6,
                               1, 5,   26, 30, 28,  8 };

                      /*      center   stencil nodes     */     
  static int interior[6][6] = { 14,    1, 13, 15, 16, -1,
                                15,    2, 14,  4, 17, -1,
                                16,   14, 12, 10, 17, 18,
                                17,   16, 19, 15,  7,  5,
                                18,   16,  9, 19, -1, -1,
                                19,   18, 17,  8, -1, -1 };

  outLevel = EG_outLevel(face);

  /* get and check sizes */

  N =  elens[0];
  M =  elens[3];
  P =  elens[1] - M;
  Q = (elens[2] - N - P)/2;
  if (Q*2 != elens[2]-N-P) {
    if (outLevel > 0) {
      printf(" EGADS Info: Q Case off by 1 - %d %d  %d %d\n",
             elens[0], elens[2], elens[1], elens[3]);
      printf("             N = %d, M = %d, P = %d, Q = %d\n",
             N, M, P, Q);
    }
    return -2;
  }
  if (P != 0) return -2;

  sizes[0] = sizes[1] = sizes[2] = N/3;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[0]++;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[2]++;
  sizes[3] = sizes[4] = sizes[5] = M/3;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[3]++;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[5]++;
  sizes[6] = P;
  sizes[7] = Q;
  for (i = 0; i < 8; i++)
    if (sizes[i] > MAXSIDE-1) return -3;

  /* set the 20 critical points -- 14 exterior */

  cpts[ 0][0] = uv[0];
  cpts[ 0][1] = uv[1];
  cipt[ 0]    = indices[0];;
  len  = sizes[0];
  cpts[ 1][0] = uv[2*len  ];
  cpts[ 1][1] = uv[2*len+1];
  cipt[ 1]    = indices[len];
  len += sizes[1];
  cpts[ 2][0] = uv[2*len  ];
  cpts[ 2][1] = uv[2*len+1];
  cipt[ 2]    = indices[len];
  len += sizes[2];
  cpts[ 3][0] = uv[2*len  ];
  cpts[ 3][1] = uv[2*len+1];
  cipt[ 3]    = indices[len];
  len += sizes[3];
  cpts[ 4][0] = uv[2*len  ];
  cpts[ 4][1] = uv[2*len+1];
  cipt[ 4]    = indices[len];
  len += sizes[4];
  cpts[ 5][0] = uv[2*len  ];
  cpts[ 5][1] = uv[2*len+1];
  cipt[ 5]    = indices[len];
  len += sizes[5];
  cpts[ 6][0] = uv[2*len  ];
  cpts[ 6][1] = uv[2*len+1];
  cipt[ 6]    = indices[len];
  len += sizes[2];
  cpts[ 7][0] = uv[2*len  ];
  cpts[ 7][1] = uv[2*len+1];
  cipt[ 7]    = indices[len];
  len += sizes[7];
  cpts[ 8][0] = uv[2*len  ];
  cpts[ 8][1] = uv[2*len+1];
  cipt[ 8]    = indices[len];
  len += sizes[1];
  cpts[ 9][0] = uv[2*len  ];
  cpts[ 9][1] = uv[2*len+1];
  cipt[ 9]    = indices[len];
  len += sizes[7];
  cpts[10][0] = uv[2*len  ];
  cpts[10][1] = uv[2*len+1];
  cipt[10]    = indices[len];
  len += sizes[0];
  cpts[11][0] = uv[2*len  ];
  cpts[11][1] = uv[2*len+1];
  cipt[11]    = indices[len];
  len += sizes[5];
  cpts[12][0] = uv[2*len  ];
  cpts[12][1] = uv[2*len+1];
  cipt[12]    = indices[len];
  len += sizes[4];
  cpts[13][0] = uv[2*len  ];
  cpts[13][1] = uv[2*len+1];
  cipt[13]    = indices[len];

  /* guess the interior */
  for (j = 0; j < 6; j++) 
    cpts[interior[j][0]][0] = cpts[interior[j][0]][1] = 0.0;
  for (i = 0; i < 10; i++)
    for (j = 0; j < 6; j++) {
      sums[0] = sums[1] = 0.0;
      for (len = k = 0; k < 5; k++) {
        if (interior[j][k+1] < 0) continue;
        sums[0] += cpts[interior[j][k+1]][0];
        sums[1] += cpts[interior[j][k+1]][1];
        len++;
      }
      cpts[interior[j][0]][0] = sums[0]/len;
      cpts[interior[j][0]][1] = sums[1]/len;
    }
    
  len    = EG_getVertCnt(12, blocks);
  vpatch = (int *) EG_alloc(len*sizeof(int));
  if (vpatch == NULL) return -1;

  /* allocate our temporary storage */

  len   = MAX(elens[1], elens[2]);
  quads = (Quad *) EG_alloc(len*len*sizeof(Quad));
  if (quads == NULL) {
    EG_free(vpatch);
    return -1;
  }
  verts = (Node *) EG_alloc((len+1)*(len+1)*sizeof(Node));
  if (verts == NULL) {
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }

  /* initialize the vertices */

  nvert = elens[0] + elens[1] + elens[2] + elens[3];
  for (i = 0; i < nvert; i++) {
    j = indices[i];
    verts[j].uv[0] = uv[2*i  ];
    verts[j].uv[1] = uv[2*i+1];
    verts[j].area  = -1.0;
  }
  for (i = 0; i < 6; i++) {
    j = interior[i][0];
    verts[nvert].uv[0] = cpts[j][0];
    verts[nvert].uv[1] = cpts[j][1];
    verts[nvert].area  = 0.0;
    cipt[j]            = nvert;
    nvert++;
  }

  /* set the exterior block sides */

  for (i = 0; i < 31; i++) sideptr[i] = NULL;
  for (j = i = 0; i < 14; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      EG_free(vpatch);
      return -1;
    }
    if (i >= 6) {
      for (k = len-1; k > 0; k--, j++) sideptr[i][k] = indices[j];
      if (i == 13) {
        sideptr[i][0] = indices[0];
      } else {
        sideptr[i][0] = indices[j];
      }
    } else {
      for (k = 0; k < len-1; k++, j++) sideptr[i][k] = indices[j];
      sideptr[i][len-1] = indices[j];
    }
  }

  /* do the interior sides */

  for (i = 14; i < 31; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      return -1;
    }
    i0 = sides[i][1];
    i1 = sides[i][2];
    sideptr[i][0] = cipt[i0];
    for (j = 1; j < len-1; j++) {
      verts[nvert].uv[0] = cpts[i0][0] + j*(cpts[i1][0]-cpts[i0][0])/(len-1);
      verts[nvert].uv[1] = cpts[i0][1] + j*(cpts[i1][1]-cpts[i0][1])/(len-1);
      verts[nvert].area  = 0.0;
      sideptr[i][j]      = nvert;
      nvert++;
    }
    sideptr[i][len-1] = cipt[i1];
  }

  /* start filling the quads by specifying the 12 blocks */

  EG_setQuads(12, blocks, sideptr);

  /* free up our integrated sides */
  for (i = 0; i < 31; i++) EG_free(sideptr[i]);

  /* get the actual storage that we return the data with */

  uvb = (double *) EG_alloc(2*nvert*sizeof(double));
  if (uvb == NULL) {
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;    
  }

  /* calculate the actual coordinates */

  len = elens[1]*elens[2];
  EG_smoothQuads(face, len, nsp);

  /* fill the memory to be returned */

  for (j = 0; j < nvert; j++) {
    uvb[2*j  ] = verts[j].uv[0];
    uvb[2*j+1] = verts[j].uv[1];
  }

  /* cleanup and exit */

  *npts = nvert;
  *uvs  = uvb;
  EG_free(verts);
  
  return 0;
}


/* No Q case */

static int
EG_quadFillP(const egObject *face, int nsp, int *indices, int *elens, 
             double *uv, int *npts, double **uvs)
{
  int    i, j, k, len, N, M, P, Q, outLevel;
  int    i0, i1, cipt[21], *sideptr[33];
  double sums[2], cpts[21][2], *uvb;
  static int sides[33][3] = {  0,  0,  1,   1,  1,  2,   2,  2,  3, 
                               3,  3,  4,   6,  4,  5,   4,  5,  6,
                               5,  6,  7,   2,  8,  7,   1,  9,  8,
                               6, 10,  9,   0, 11, 10,   5, 12, 11,
                               4, 13, 12,   3,  0, 13,   3,  1, 14,
                               0, 13, 14,   3,  2, 15,   1, 14, 15,
                               2, 15,  4,   4, 14, 18,   0, 12, 18,
                               6, 14, 16,   4, 16, 19,   6, 15, 17,
                               1, 16, 17,   2, 17,  5,   1, 19, 20,
                               2, 20,  6,   5, 18, 10,   6, 18, 19,
                               5, 19,  9,   4, 17, 20,   5, 20,  8 };

                      /*       size    sides           */
  static int blocks[13][6] = { 0, 3,   13, 14,  0, 15,
                               1, 3,   14, 16,  1, 17,
                               2, 3,   16,  3,  2, 18,
                               0, 4,   12, 19, 15, 20,
                               6, 4,   19, 22, 21, 29,
                               1, 6,   21, 23, 17, 24,
                               2, 6,   23,  4, 18, 25,
                               1, 4,   22, 31, 24, 26,
                               2, 4,   31,  5, 25, 27,
                               0, 5,   11, 28, 20, 10,
                               6, 5,   28, 30, 29,  9,
                               1, 5,   30, 32, 26,  8,
                               2, 5,   32,  6, 27,  7 };

                      /*      center   stencil nodes     */     
  static int interior[7][6] = { 14,    1, 13, 18, 16, 15,
                                15,    2,  4, 14, 17, -1,
                                16,   14, 17, 19, -1, -1,
                                17,    5, 15, 16, 20, -1,
                                18,   10, 12, 14, 19, -1,
                                19,    9, 16, 18, 20, -1,
                                20,    6,  8, 17, 19, -1 };

  outLevel = EG_outLevel(face);

  /* get and check sizes */

  N =  elens[0];
  M =  elens[3];
  P =  elens[1] - M;
  Q = (elens[2] - N - P)/2;
  if (Q*2 != elens[2]-N-P) {
    if (outLevel > 0) {
      printf(" EGADS Info: P Case off by 1 - %d %d  %d %d\n",
             elens[0], elens[2], elens[1], elens[3]);
      printf("             N = %d, M = %d, P = %d, Q = %d\n",
             N, M, P, Q);
    }
    return -2;
  }
  if (Q != 0) return -2;

  sizes[0] = sizes[1] = sizes[2] = N/3;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[0]++;
  if (sizes[0]+sizes[1]+sizes[2] != N) sizes[2]++;
  sizes[3] = sizes[4] = sizes[5] = M/3;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[3]++;
  if (sizes[3]+sizes[4]+sizes[5] != M) sizes[5]++;
  sizes[6] = P;
  sizes[7] = Q;
  for (i = 0; i < 8; i++)
    if (sizes[i] > MAXSIDE-1) return -3;

  /* set the 21 critical points -- 14 exterior */

  cpts[ 0][0] = uv[0];
  cpts[ 0][1] = uv[1];
  cipt[ 0]    = indices[0];
  len  = sizes[0];
  cpts[ 1][0] = uv[2*len  ];
  cpts[ 1][1] = uv[2*len+1];
  cipt[ 1]    = indices[len];
  len += sizes[1];
  cpts[ 2][0] = uv[2*len  ];
  cpts[ 2][1] = uv[2*len+1];
  cipt[ 2]    = indices[len];
  len += sizes[2];
  cpts[ 3][0] = uv[2*len  ];
  cpts[ 3][1] = uv[2*len+1];
  cipt[ 3]    = indices[len];
  len += sizes[3];
  cpts[ 4][0] = uv[2*len  ];
  cpts[ 4][1] = uv[2*len+1];
  cipt[ 4]    = indices[len];
  len += sizes[6];
  cpts[ 5][0] = uv[2*len  ];
  cpts[ 5][1] = uv[2*len+1];
  cipt[ 5]    = indices[len];
  len += sizes[4];
  cpts[ 6][0] = uv[2*len  ];
  cpts[ 6][1] = uv[2*len+1];
  cipt[ 6]    = indices[len];
  len += sizes[5];
  cpts[ 7][0] = uv[2*len  ];
  cpts[ 7][1] = uv[2*len+1];
  cipt[ 7]    = indices[len];
  len += sizes[2];
  cpts[ 8][0] = uv[2*len  ];
  cpts[ 8][1] = uv[2*len+1];
  cipt[ 8]    = indices[len];
  len += sizes[1];
  cpts[ 9][0] = uv[2*len  ];
  cpts[ 9][1] = uv[2*len+1];
  cipt[ 9]    = indices[len];
  len += sizes[6];
  cpts[10][0] = uv[2*len  ];
  cpts[10][1] = uv[2*len+1];
  cipt[10]    = indices[len];
  len += sizes[0];
  cpts[11][0] = uv[2*len  ];
  cpts[11][1] = uv[2*len+1];
  cipt[11]    = indices[len];
  len += sizes[5];
  cpts[12][0] = uv[2*len  ];
  cpts[12][1] = uv[2*len+1];
  cipt[12]    = indices[len];
  len += sizes[4];
  cpts[13][0] = uv[2*len  ];
  cpts[13][1] = uv[2*len+1];
  cipt[13]    = indices[len];

  /* guess the interior */
  for (j = 0; j < 7; j++) 
    cpts[interior[j][0]][0] = cpts[interior[j][0]][1] = 0.0;
  for (i = 0; i < 10; i++)
    for (j = 0; j < 7; j++) {
      sums[0] = sums[1] = 0.0;
      for (len = k = 0; k < 5; k++) {
        if (interior[j][k+1] < 0) continue;
        sums[0] += cpts[interior[j][k+1]][0];
        sums[1] += cpts[interior[j][k+1]][1];
        len++;
      }
      cpts[interior[j][0]][0] = sums[0]/len;
      cpts[interior[j][0]][1] = sums[1]/len;
    }
    
  len    = EG_getVertCnt(13, blocks);
  vpatch = (int *) EG_alloc(len*sizeof(int));
  if (vpatch == NULL) return -1;

  /* allocate our temporary storage */

  len   = MAX(elens[1], elens[2]);
  quads = (Quad *) EG_alloc(len*len*sizeof(Quad));
  if (quads == NULL) {
    EG_free(vpatch);
    return -1;
  }
  verts = (Node *) EG_alloc((len+1)*(len+1)*sizeof(Node));
  if (verts == NULL) {
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }

  /* initialize the vertices */

  nvert = elens[0] + elens[1] + elens[2] + elens[3];
  for (i = 0; i < nvert; i++) {
    j = indices[i];
    verts[j].uv[0] = uv[2*i  ];
    verts[j].uv[1] = uv[2*i+1];
    verts[j].area  = -1.0;
  }
  for (i = 0; i < 7; i++) {
    j = interior[i][0];
    verts[nvert].uv[0] = cpts[j][0];
    verts[nvert].uv[1] = cpts[j][1];
    verts[nvert].area  = 0.0;
    cipt[j] = nvert;
    nvert++;
  }

  /* set the exterior block sides */

  for (i = 0; i < 33; i++) sideptr[i] = NULL;
  for (j = i = 0; i < 14; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      return -1;
    }
    if (i >= 7) {
      for (k = len-1; k > 0; k--, j++) sideptr[i][k] = indices[j];
      if (i == 13) {
        sideptr[i][0] = indices[0];
      } else {
        sideptr[i][0] = indices[j];
      }
    } else {
      for (k = 0; k < len-1; k++, j++) sideptr[i][k] = indices[j];
      sideptr[i][len-1] = indices[j];
    }
  }

  /* do the interior sides */

  for (i = 14; i < 33; i++) {
    len = sizes[sides[i][0]] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      return -1;
    }
    i0 = sides[i][1];
    i1 = sides[i][2];
    sideptr[i][0] = cipt[i0];
    for (j = 1; j < len-1; j++) {
      verts[nvert].uv[0] = cpts[i0][0] + j*(cpts[i1][0]-cpts[i0][0])/(len-1);
      verts[nvert].uv[1] = cpts[i0][1] + j*(cpts[i1][1]-cpts[i0][1])/(len-1);
      verts[nvert].area  = 0.0;
      sideptr[i][j]      = nvert;
      nvert++;
    }
    sideptr[i][len-1] = cipt[i1];
  }

  /* start filling the quads by specifying the 13 blocks */

  EG_setQuads(13, blocks, sideptr);

  /* free up our integrated sides */
  for (i = 0; i < 33; i++) EG_free(sideptr[i]);

  /* get the actual storage that we return the data with */

  uvb = (double *) EG_alloc(2*nvert*sizeof(double));
  if (uvb == NULL) {
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;    
  }

  /* calculate the actual coordinates */

  len = elens[1]*elens[2];
  EG_smoothQuads(face, len, nsp);

  /* fill the memory to be returned */

  for (j = 0; j < nvert; j++) {
    uvb[2*j  ] = verts[j].uv[0];
    uvb[2*j+1] = verts[j].uv[1];
  }

  /* cleanup and exit */

  *npts = nvert;
  *uvs  = uvb;
  EG_free(verts);

  return 0;
}


/* TFI case */

static int
EG_quadFillT(int *elens, double *uv, int *npts, double **uvs)
{
  int    i, j, k, m, nx, ny, len, ilast;
  int    cipt[4], *sideptr[4];
  int    ll, lr, ur, ul, j0, jm, i0, im, iv, sav;
  double et, xi, *uvb, *uv0, *uv1, *uv2, *abasis[2];

  nx = elens[0];
  ny = elens[1];

#ifdef DEBUG
  printf(" TFI Case -- Sizes: nx = %d,  ny = %d\n", nx, ny);
#endif
  if (nx >= MAXSIDE) return -3;
  if (ny >= MAXSIDE) return -3;

  cipt[ 0] = 0;
  len      = nx;
  cipt[ 1] = len;
  len     += ny;
  cipt[ 2] = len;
  len     += nx;
  cipt[ 3] = len;
  sizes[0] = sizes[2] = nx;
  sizes[1] = sizes[3] = ny;
  
  len    = (nx+1)*(ny+1);
  vpatch = (int *) EG_alloc(len*sizeof(int));
  if (vpatch == NULL) return -1;

  /* allocate our temporary storage */

  len   = nx*ny;
  quads = (Quad *) EG_alloc(len*sizeof(Quad));
  if (quads == NULL) {
    EG_free(vpatch);
    return -1;
  }
  len   = (nx+1)*(ny+1) + 1;
  verts = (Node *) EG_alloc(len*sizeof(Node));
  if (verts == NULL) {
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }
  npatch      = 1;
  patch[0][0] = nx + 1;
  patch[0][1] = ny + 1;

  /* initialize the vertices */

  nvert = elens[0] + elens[1] + elens[2] + elens[3];
  for (i = 0; i < nvert; i++) {
    verts[i].uv[0] = uv[2*i  ];
    verts[i].uv[1] = uv[2*i+1];
    verts[i].area  = -1.0;
  }

  /* set the exterior block sides */

  for (i = 0; i < 4; i++) sideptr[i] = NULL;
  for (j = i = 0; i < 4; i++) {
    len = sizes[i] + 1;
    sideptr[i] = (int *) EG_alloc(len*sizeof(int));
    if (sideptr[i] == NULL) {
      for (k = 0; k < i; k++) EG_free(sideptr[k]);
      EG_free(verts);
      EG_free(quads);
      EG_free(vpatch);
      return -1;
    }
    if (i >= 2) {
      for (k = len-1; k > 0; k--, j++) sideptr[i][k] = j;
      sideptr[i][0] = j;
      if (i == 3) sideptr[i][0] = 0;
    } else {
      for (k = 0; k < len-1; k++, j++) sideptr[i][k] = j;
      sideptr[i][len-1] = j;
    }
  }

  /* compute arclength basis functions */

  abasis[0] = (double *) EG_alloc((nx+1)*(ny+1)*sizeof(double));
  if (abasis[0] == NULL) {
    for (i = 0; i < 4; i++) EG_free(sideptr[i]);
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }

  abasis[1] = (double *) EG_alloc((nx+1)*(ny+1)*sizeof(double));
  if (abasis[1] == NULL) {
    EG_free(abasis[0]);
    for (i = 0; i < 4; i++) EG_free(sideptr[i]);
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;
  }

  /* Finally compute the basis functions */
  EG_arcBasis(nx, ny, sideptr, abasis);

  /* create the quads and get coordinates via TFI */

  nquad = iv = 0;
  for (i = 0; i < nx+1; i++) last[i] = sideptr[0][i];
  ll = cipt[0];
  lr = cipt[1];
  ur = cipt[2];
  ul = cipt[3];
  for (j = 0; j < ny; j++) {
    i0    = sideptr[3][j+1];
    im    = sideptr[1][j+1];
    ilast = i0;
    sav   = nquad;
    for (i = 0; i < nx; i++) {
      quads[nquad].nodes[0] = last[i  ];
      quads[nquad].nodes[1] = last[i+1];
      if (j == ny-1) {
        quads[nquad].nodes[2] = sideptr[2][i+1];
        quads[nquad].nodes[3] = sideptr[2][i  ];
      } else {
        if (i == nx-1) {
          quads[nquad].nodes[2] = sideptr[1][j+1];
          last[i]  = ilast;
          last[nx] = sideptr[1][j+1];
        } else {
          j0 = sideptr[0][i+1];
          jm = sideptr[2][i+1];
          k  = (i+1)*(ny+1) + (j+1);
          xi = abasis[0][k];
          et = abasis[1][k];
          quads[nquad].nodes[2] = nvert;
          verts[nvert].uv[0]    = (1.0-xi)            * verts[i0].uv[0] +
                                  (    xi)            * verts[im].uv[0] +
                                             (1.0-et) * verts[j0].uv[0] +
                                             (    et) * verts[jm].uv[0] -
                                  (1.0-xi) * (1.0-et) * verts[ll].uv[0] -
                                  (1.0-xi) * (    et) * verts[ul].uv[0] -
                                  (    xi) * (1.0-et) * verts[lr].uv[0] -
                                  (    xi) * (    et) * verts[ur].uv[0];
          verts[nvert].uv[1]    = (1.0-xi)            * verts[i0].uv[1] +
                                  (    xi)            * verts[im].uv[1] +
                                             (1.0-et) * verts[j0].uv[1] +
                                             (    et) * verts[jm].uv[1] -
                                  (1.0-xi) * (1.0-et) * verts[ll].uv[1] -
                                  (1.0-xi) * (    et) * verts[ul].uv[1] -
                                  (    xi) * (1.0-et) * verts[lr].uv[1] -
                                  (    xi) * (    et) * verts[ur].uv[1];
          verts[nvert].area     = 0.0;
          nvert++;
        }
        quads[nquad].nodes[3] = ilast;
        last[i] = ilast;
        ilast   = nvert-1;
      }
      nquad++;
    }
    if (j == 0) {
      vpatch[iv] = quads[sav].nodes[0];
      iv++;
      for (i = 0; i < nx; i++, iv++) 
        vpatch[iv] = quads[sav+i].nodes[1];
    }
    vpatch[iv] = quads[sav].nodes[3];
    iv++;
    for (i = 0; i < nx; i++, sav++, iv++)
      vpatch[iv] = quads[sav].nodes[2];
  }

  /* free up our basis functions */
  EG_free(abasis[1]);
  EG_free(abasis[0]);

  /* free up our integrated sides */
  for (i = 0; i < 4; i++) EG_free(sideptr[i]);

  /* get the actual storage that we return the data with */

  uvb = (double *) EG_alloc(2*nvert*sizeof(double));
  if (uvb == NULL) {
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    return -1;    
  }

  /* fill the memory to be returned */

  for (j = 0; j < nvert; j++) {
    uvb[2*j  ] = verts[j].uv[0];
    uvb[2*j+1] = verts[j].uv[1];
  }
  for (k = i = 0; i < nquad; i++) {
    m   = k;
    uv0 = &uvb[2*quads[i].nodes[0]];
    uv1 = &uvb[2*quads[i].nodes[1]];
    uv2 = &uvb[2*quads[i].nodes[2]];
    if (AREA2D(uv0, uv1, uv2) <= 0.0) k++;
    if (m != k) continue;
    uv1 = &uvb[2*quads[i].nodes[2]];
    uv2 = &uvb[2*quads[i].nodes[3]];
    if (AREA2D(uv0, uv1, uv2) <= 0.0) k++;
  }
  if (k != 0) {
#ifdef DEBUG
    printf(" Bad mapping: %d non-positive of %d quads\n", 
           k, nquad);
#endif
    EG_free(uvb);
    EG_free(verts);
    EG_free(quads);
    EG_free(vpatch);
    *npts = 0;
    *uvs  = NULL;
    return -6;
  }

  /* cleanup and exit */

  *npts = nvert;
  *uvs  = uvb;
  EG_free(verts);
  EG_free(quads);

  return 0;
}


/* takes a simple quad loop and fills it with quads based on a sub-blocking
 *       scheme that supports differing sizes per side
 *
 * where: parms[0] - Edge Tol
 *        parms[1] - Side Ratio
 *        parms[2] - # smoothing passes
 *
 *        elens[0] - number of segments on the left side
 *        elens[1] - number of segments on the bottom
 *        elens[2] - number of segments on the right
 *        elens[3] - number of segments on the top side
 *
 *        uv[]     - input (u,v) pairs going around the loop CCW with
 *                   no duplicates at corners starting at UL corner (the
 *                   start of side 0)
 *                   len = 2*(sum of elens)
 *
 *        npts     - total number of resultant points (returned)
 *        uvs      - pointer to (u,v) pairs of all of the points (returned,
 *                   should be free'd);  note -> the first set matches uv
 *        npat     - resultant number of quad patchs (max = 17)
 *        pat      - filled patch sizes (at least 2*17 in len)
 *        vpat     - pointer to patch indices of uvs that support the 
 *                   quad patch(s) (returned, should be free'd)
 *
 * return codes:  0 - success
 *               -1 - malloc error
 *               -2 - elen error
 *               -3 - block side too big
 *               -4 - extra edge not found
 *               -6 - neg area tris
 *               -7 - mismatched sides
 */

int
EG_quadFill(const egObject *face, double *parms, int *elens, double *uv, 
            int *npts, double **uvs, int *npat, int *pats, int **vpats)
{
  int    i, j, k, m, N, M, P, Q, len, ret, lens[4], *indices;
  int    outLevel, iv, nx, save, align = 0, nsp = 0;
  double *uvx, *uv0, *uv1, *uv2, dist, sav[2], xylim[2][2], slim[4][2][2];
  double edgeTOL = 0.05, sideRAT = 3.0;

  *npts    = *npat = 0;
  *uvs     = NULL;
  *vpats   = NULL;
  flip     = 1.0;
  outLevel = EG_outLevel(face);

  /* note: all zeros gives the default values */
  if ((parms[0] >= 0.001) && (parms[0] <= 0.5)) edgeTOL = parms[0];
  if ((parms[1] > 0.0) && (parms[1] <= 1000.0)) sideRAT = parms[1];
  if ((parms[2] > 0.5) && (parms[2] <= 100.0))  nsp     = parms[2]+0.1;

  /* can we use a simple TFI scheme? */

  if ((elens[0] == elens[2]) && (elens[1] == elens[3])) {
    ret = EG_quadFillT(elens, uv, npts, uvs);
    if (ret != EGADS_SUCCESS) return ret;
    *npat   = npatch;
    pats[0] = patch[0][0];
    pats[1] = patch[0][1];
    *vpats  = vpatch;
    return EGADS_SUCCESS;
  } else if ((elens[0] == elens[2]) && (abs(elens[1]-elens[3]) == 1)) {
    if (outLevel > 0)
      printf(" EGADS Info: TFI off by 1 on top/bottom!\n");
  } else if ((elens[1] == elens[3]) && (abs(elens[0]-elens[2]) == 1)) {
    if (outLevel > 0)
      printf(" EGADS Info: TFI off by 1 on left/right!\n");
  }

  sav[0] = elens[0];
  sav[1] = elens[2];
  if (MAX(sav[0],sav[1])/MIN(sav[0],sav[1]) > sideRAT) {
    if (outLevel > 0)
      printf(" EGADS Info: Edge ratio0 %lf exceeded: %g %g\n", 
             sideRAT, sav[0], sav[1]);
    return -7;
  }
  sav[0] = elens[1];
  sav[1] = elens[3];
  if (MAX(sav[0],sav[1])/MIN(sav[0],sav[1]) > sideRAT) {
    if (outLevel > 0)
      printf(" EGADS Info: Edge ratio1 %lf exceeded: %g %g\n",
             sideRAT, sav[0], sav[1]);
    return -7;
  }

  /* no -- use our 3 templates */

  len     = elens[0] + elens[1] + elens[2] + elens[3] + 1;
  indices = (int *) EG_alloc(len*sizeof(int));
  if (indices == NULL) return -1;
  uvx = (double *) EG_alloc(4*len*sizeof(double));
  if (uvx == NULL) {
    EG_free(indices);
    return -1;
  }
  for (i = 0; i < len-1; i++) {
    j          = len+i;
    uvx[2*j  ] = uv[2*i  ];
    uvx[2*j+1] = uv[2*i+1];
  }

  /* determine if our quad sides align with U & V */

  xylim[0][0] = xylim[1][0] = uv[0];
  xylim[0][1] = xylim[1][1] = uv[1];
  for (i = 1; i < len-1; i++) {
    if (xylim[0][0] > uv[2*i  ]) xylim[0][0] = uv[2*i  ];
    if (xylim[1][0] < uv[2*i  ]) xylim[1][0] = uv[2*i  ];
    if (xylim[0][1] > uv[2*i+1]) xylim[0][1] = uv[2*i+1];
    if (xylim[1][1] < uv[2*i+1]) xylim[1][1] = uv[2*i+1];
  }
  slim[0][0][0] = slim[1][0][0] = slim[2][0][0] = slim[3][0][0] = xylim[1][0];
  slim[0][1][0] = slim[1][1][0] = slim[2][1][0] = slim[3][1][0] = xylim[0][0];
  slim[0][0][1] = slim[1][0][1] = slim[2][0][1] = slim[3][0][1] = xylim[1][1];
  slim[0][1][1] = slim[1][1][1] = slim[2][1][1] = slim[3][1][1] = xylim[0][1];
  for (i = 0; i <= elens[0]; i++) {
    if (slim[0][0][0] > uv[2*i  ]) slim[0][0][0] = uv[2*i  ];
    if (slim[0][1][0] < uv[2*i  ]) slim[0][1][0] = uv[2*i  ];
    if (slim[0][0][1] > uv[2*i+1]) slim[0][0][1] = uv[2*i+1];
    if (slim[0][1][1] < uv[2*i+1]) slim[0][1][1] = uv[2*i+1];
  }
  for (i = elens[0]; i <= elens[0]+elens[1]; i++) {
    if (slim[1][0][0] > uv[2*i  ]) slim[1][0][0] = uv[2*i  ];
    if (slim[1][1][0] < uv[2*i  ]) slim[1][1][0] = uv[2*i  ];
    if (slim[1][0][1] > uv[2*i+1]) slim[1][0][1] = uv[2*i+1];
    if (slim[1][1][1] < uv[2*i+1]) slim[1][1][1] = uv[2*i+1];
  }
  for (i = elens[0]+elens[1]; i <= elens[0]+elens[1]+elens[2]; i++) {
    if (slim[2][0][0] > uv[2*i  ]) slim[2][0][0] = uv[2*i  ];
    if (slim[2][1][0] < uv[2*i  ]) slim[2][1][0] = uv[2*i  ];
    if (slim[2][0][1] > uv[2*i+1]) slim[2][0][1] = uv[2*i+1];
    if (slim[2][1][1] < uv[2*i+1]) slim[2][1][1] = uv[2*i+1];
  }
  for (i = elens[0]+elens[1]+elens[2]; i < len-1; i++) {
    if (slim[3][0][0] > uv[2*i  ]) slim[3][0][0] = uv[2*i  ];
    if (slim[3][1][0] < uv[2*i  ]) slim[3][1][0] = uv[2*i  ];
    if (slim[3][0][1] > uv[2*i+1]) slim[3][0][1] = uv[2*i+1];
    if (slim[3][1][1] < uv[2*i+1]) slim[3][1][1] = uv[2*i+1];
  }
  if (slim[3][0][0] > uv[0]) slim[3][0][0] = uv[0];
  if (slim[3][1][0] < uv[0]) slim[3][1][0] = uv[0];
  if (slim[3][0][1] > uv[1]) slim[3][0][1] = uv[0];
  if (slim[3][1][1] < uv[1]) slim[3][1][1] = uv[0];
  /* check side range vs face range */
  for (i = 0; i < 4; i++) {
    if (((slim[i][1][0]-slim[i][0][0])/(xylim[1][0]-xylim[0][0]) >= edgeTOL) &&
        ((slim[i][1][1]-slim[i][0][1])/(xylim[1][1]-xylim[0][1]) >= edgeTOL)) {
      align = 1;
      break;
    }
  }
  
  /* fill up our 0 to 1 mapping in UV */

  for (i = 0; i < len; i++) indices[i] = i;
  if ((unmap == 0) && (align == 0)) {
    for (j = i = 0; i < elens[0]; i++) {
      uv[2*j  ] = 0.0;
      uv[2*j+1] = 1.0 - i/((double) elens[0]);
      j++;
    }
    for (i = 0; i < elens[1]; i++) {
      uv[2*j  ] = 0.0 + i/((double) elens[1]);
      uv[2*j+1] = 0.0;
      j++;
    }
    for (i = 0; i < elens[2]; i++) {
      uv[2*j  ] = 1.0;
      uv[2*j+1] = 0.0 + i/((double) elens[2]);
      j++;
    }
    for (i = 0; i < elens[3]; i++) {
      uv[2*j  ] = 1.0 - i/((double) elens[3]);
      uv[2*j+1] = 1.0;
      j++;
    }
  }
  for (i = 0; i < 2*len-2; i++) uvx[i] = uv[i];
#ifdef DEBUG
  printf("QuadFill unmap = %d,  align = %d!\n", unmap, align);
#endif

  /* rotate sides to get the biggest delta on side 2 */

  lens[0] = elens[0];
  lens[1] = elens[1];
  lens[2] = elens[2];
  lens[3] = elens[3];
  if (abs(elens[0]-elens[2]) >= abs(elens[1]-elens[3])) {
    if (elens[2] < elens[0]) {
#ifdef DEBUG
      printf("  Side 0  -> Side 2!\n");
#endif
      len = lens[0]+lens[1];
      for (i = 0; i < lens[2]+lens[3]; i++) {
        indices[i] = len+i;
        uvx[2*i  ] = uv[2*(len+i)  ];
        uvx[2*i+1] = uv[2*(len+i)+1];
      }
      len = lens[2]+lens[3];
      for (i = len; i < lens[0]+lens[1]+lens[2]+lens[3]; i++) {
        indices[i] = i-len;
        uvx[2*i  ] = uv[2*(i-len)  ];
        uvx[2*i+1] = uv[2*(i-len)+1];
      }
      lens[2] = elens[0];
      lens[3] = elens[1];
      lens[0] = elens[2];
      lens[1] = elens[3];
    }
  } else {
    if (elens[3] > elens[1]) {
#ifdef DEBUG
      printf("  Side 3  -> Side 2!\n");
#endif
      len = lens[0];
      for (i = 0; i < lens[1]+lens[2]+lens[3]; i++) {
        indices[i] = len+i;
	uvx[2*i  ] = uv[2*(len+i)  ];
	uvx[2*i+1] = uv[2*(len+i)+1];
      }
      len = lens[1]+lens[2]+lens[3];
      for (i = len; i < lens[0]+lens[1]+lens[2]+lens[3]; i++) {
        indices[i] = i-len;
        uvx[2*i  ] = uv[2*(i-len)  ];
        uvx[2*i+1] = uv[2*(i-len)+1];
      }
      lens[3] = elens[0];
      lens[0] = elens[1];
      lens[1] = elens[2];
      lens[2] = elens[3];
    } else {
#ifdef DEBUG
      printf("  Side 1  -> Side 2!\n");
#endif
      len = lens[0]+lens[1]+lens[2];
      for (i = 0; i < lens[3]; i++) {
        indices[i] = len+i;
        uvx[2*i  ] = uv[2*(len+i)  ];
        uvx[2*i+1] = uv[2*(len+i)+1];
      }
      len = lens[3];
      for (i = len; i < lens[0]+lens[1]+lens[2]+lens[3]; i++) {
        indices[i] = i-len;
        uvx[2*i  ] = uv[2*(i-len)  ];
        uvx[2*i+1] = uv[2*(i-len)+1];
      }
      lens[1] = elens[0];
      lens[2] = elens[1];
      lens[3] = elens[2];
      lens[0] = elens[3];
    }
  }

  /* make side 1 bigger than 3 */

  if (lens[1] < lens[3]) {
#ifdef DEBUG
    printf("  Side 1 <-> Side 3!\n");
#endif
    len = lens[0] + lens[1] + lens[2] + lens[3] + 1;
    uvx[2*len-2]   = uvx[0];
    uvx[2*len-1]   = uvx[1];
    indices[len-1] = indices[0];
    for (i = 0; i < len/2; i++) {
      j = len - i - 1;
      sav[0]     = uvx[2*i  ];
      sav[1]     = uvx[2*i+1];
      ret        = indices[i];
      uvx[2*i  ] = uvx[2*j  ];
      uvx[2*i+1] = uvx[2*j+1];
      indices[i] = indices[j];
      uvx[2*j  ] = sav[0];
      uvx[2*j+1] = sav[1];
      indices[j] = ret;
    }
    len--;
    for (j = 0; j < lens[0]; j++) {
      sav[0] = uvx[2*len-2];
      sav[1] = uvx[2*len-1];
      ret    = indices[len-1];
      for(i = len-1; i > 0; i--) {
        uvx[2*i  ] = uvx[2*i-2];
        uvx[2*i+1] = uvx[2*i-1];
        indices[i] = indices[i-1];
      }
      uvx[0]     = sav[0];
      uvx[1]     = sav[1];
      indices[0] = ret;
    }
    i       = lens[3];
    lens[3] = lens[1];
    lens[1] = i;
    flip    = -1.0;
  }

  /* get the template & go */

  len =  lens[0]+lens[1]+lens[2]+lens[3];
  N   =  lens[0];
  M   =  lens[3];
  P   =  lens[1] - M;
  Q   = (lens[2] - N - P)/2;
  if ((N < 3) || (M < 3) || (P < 0) || (Q < 0)) {
    for (i = 0; i < len; i++) {
      j         = len+i+1;
      uv[2*i  ] = uvx[2*j  ];
      uv[2*i+1] = uvx[2*j+1];
    }
    EG_free(uvx);
    EG_free(indices);
    if (outLevel > 0)
      printf(" EGADS Info: Too small ->  %d %d (>3)   %d %d\n", 
             N, M, P, Q);
    return -2;
  }

  if (P == 0) {
    ret = EG_quadFillQ(face, nsp, indices, lens, uvx, npts, uvs);
  } else if (Q == 0) {
    ret = EG_quadFillP(face, nsp, indices, lens, uvx, npts, uvs);
  } else {
    ret = EG_quadFillG(face, nsp, indices, lens, uvx, npts, uvs);
  }
  for (i = 0; i < len; i++) {
    j         = len+i+1;
    uv[2*i  ] = uvx[2*j  ];
    uv[2*i+1] = uvx[2*j+1];
  }
  EG_free(uvx);
  EG_free(indices);

  /* remap back to our UV */

  if ((ret == 0) && (unmap == 0) && (align == 0) && (*uvs != NULL)) {
    ret = EG_dQuadTFI(elens, uv, *npts, *uvs);
    if (ret != 0) {
      EG_free(*uvs);
      EG_free(quads);
      EG_free(vpatch);
      *npts = 0;
      *uvs  = NULL;
    }
  }

  /* fix orientation if flipped direction */

  uvx = *uvs;
  if (uvx == NULL) return -99;
  if ((ret == 0) && (flip < 0.0))
    for (iv = k = 0; k < npatch; k++) {
      nx = patch[k][0];
      for (j = 0; j < patch[k][1]; j++) {
        for (i = 0; i < nx/2; i++) {
          m            = nx - i - 1;
          save         = vpatch[iv+i];
          vpatch[iv+i] = vpatch[iv+m];
          vpatch[iv+m] = save;
        }
        iv += nx;
      }
    }

  /* make sure we are OK */

  if (ret == 0) {
    for (k = i = 0; i < nquad; i++) {
      m    = k;
      uv0  = &uvx[2*quads[i].nodes[0]];
      uv1  = &uvx[2*quads[i].nodes[1]];
      uv2  = &uvx[2*quads[i].nodes[2]];
      dist = AREA2D(uv0, uv1, uv2)*flip;
      if (dist*0.0 != 0.0) k++;		/* special Nan, Ind, ... checker */
      if (dist     <= 0.0) k++;
      if (m != k) continue;
      uv1 = &uvx[2*quads[i].nodes[2]];
      uv2 = &uvx[2*quads[i].nodes[3]];
      dist = AREA2D(uv0, uv1, uv2)*flip;
      if (dist*0.0 != 0.0) k++;
      if (dist     <= 0.0) k++;
    }
    EG_free(quads);
    if (k != 0) {
      if (outLevel > 0)
        printf(" EGADS Info: Bad mapping - %d non-positive of %d quads\n",
               k, nquad);
      EG_free(*uvs);
      EG_free(vpatch);
      *npts = 0;
      *uvs  = NULL;
      return -6;
    }
  }

  if (ret == 0) {
    for (k = 0; k < npatch; k++) {
      pats[2*k  ] = patch[k][0];
      pats[2*k+1] = patch[k][1];
    }
    *npat  = npatch;
    *vpats = vpatch;
  }

  return ret;
}

