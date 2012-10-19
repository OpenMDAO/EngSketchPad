/*
 *	The Web Viewer
 *
 *      	WV server-side structures header
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#ifndef _WSSS_H_

#define _WSSS_H_



/* IO buffer size */
#define BUFLEN     3205696


/* Graphic Primitive Types */

#define WV_POINT         0
#define WV_LINE          1
#define WV_TRIANGLE      2


/* Plotting Attribute Bits */

#define WV_ON            1
#define WV_TRANSPARENT   2
#define WV_SHADING       4
#define WV_ORIENTATION   8
#define WV_POINTS       16
#define WV_LINES        32


/* VBO & single data Bits */

#define WV_VERTICES      1
#define WV_INDICES       2
#define WV_COLORS        4
#define WV_NORMALS       8

#define WV_PINDICES     16
#define WV_LINDICES     32
#define WV_PCOLOR       64
#define WV_LCOLOR      128
#define WV_BCOLOR      256

#define WV_DELETE      512
#define WV_DONE       1024


/* Data Types */

#define WV_UINT8         1
#define WV_UINT16        2
#define WV_INT32         3
#define WV_REAL32        4
#define WV_REAL64        5


typedef struct {
  int    dataType;              /* VBO type */
  int    dataLen;               /* length of data */
  void  *dataPtr;               /* pointer to data */
  float  data[3];               /* size = 1 data (not in ptr) */
} wvData;

typedef struct {
  int            nsVerts;       /* number of vertices */
  int            nsIndices;     /* the number of top-level indices */
  int            nlIndices;     /* the number of line indices */
  int            npIndices;     /* the number of point indices */
  int            *gIndices;     /* global indices for the stripe -- NULL if 1 */
  float          *vertices;     /* the stripes set of vertices (3XnsVerts) */
  float          *normals;      /* the stripes suite of normals (3XnsVerts) */
  unsigned char  *colors;       /* the stripes suite of colors (3XnsVerts) */
  unsigned short *sIndice2;     /* the top-level stripe indices */
  unsigned short *lIndice2;     /* the line indices */
  unsigned short *pIndice2;     /* the point indices */
  /* note that for 1 stripe, vertices, colors and normals are same as wvGPrim */
} wvStripe;

typedef struct {
  int            gtype;         /* graphic primitive type */
  int            updateFlg;     /* update flag */
  int            nStripe;       /* number of stripes */
  int            attrs;         /* initial plotting attribute bits */
  int            nVerts;        /* number of vertices */
  int            nIndex;        /* the number of top-level indices */
  int            nlIndex;       /* the number of line indices */
  int            npIndex;       /* the number of point indices */
  int            nameLen;       /* length of name (modulo 4) */
  float          pSize;         /* point size */
  float          pColor[3];     /* point color */
  float          lWidth;        /* line width */
  float          lColor[3];     /* line color */
  float          fColor[3];     /* triangle foreground color */
  float          bColor[3];     /* triangle background color */
  float          normal[3];     /* constant normal -- planar surfaces only */
  char           *name;         /* gPrim name (nameLen is length+) */
  float          *vertices;     /* the complete set of vertices (3XnVerts) */
  float          *normals;      /* the complete suite of normals (3XnVerts) */
  unsigned char  *colors;       /* the complete suite of colors (3XnVerts) */
  int            *indices;      /* the complete suite of top-level indices */
  int            *lIndices;     /* the complete suite of line indices */
  int            *pIndices;     /* the complete suite of point indices */
  wvStripe       *stripes;      /* stripes */
} wvGPrim;

typedef struct {
  int     ioAccess;             /* IO currently has access */
  int     dataAccess;           /* data routines have access */
  int     bias;                 /* vaule subtracted from all indices */
  float   fov;                  /* angle field of view for perspective */
  float   zNear;                /* Z near value for perspective */
  float   zFar;                 /* Z far value for perspective */
  float   eye[3];               /* eye coordinates for "camera" */
  float   center[3];            /* lookat center coordinates */
  float   up[3];                /* the up direction -- usually Y */
  int     nGPrim;               /* number of graphics primitives */
  int     mGPrim;               /* number of malloc'd primitives */
  int     cleanAll;		/* flag for complete gPrim delete */
  /*@null@*/
  wvGPrim *gPrims;              /* the graphics primitives */
} wvContext;

#endif  /*_WSSS_H_*/
