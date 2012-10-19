/*
 *	The Web Viewer
 *
 *		WV server-side functions
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
#include <unistd.h>

#include "wsss.h"
#include "libwebsockets.h"


  extern struct libwebsocket_protocols *wv_protocols;



/*@null@*/ /*@out@*/ /*@only@*/ void *
wv_alloc(int nbytes)
{
  if (nbytes <= 0) return NULL;
  return malloc(nbytes);
}


/*@null@*/ /*@only@*/ void *
wv_calloc(int nele, int size)
{
  if (nele*size <= 0) return NULL;
  return calloc(nele, size);
}


/*@null@*/ /*@only@*/ void *
wv_realloc(/*@null@*/ /*@only@*/ /*@returned@*/ void *ptr, int nbytes)
{
  if (nbytes <= 0) return NULL;
  return realloc(ptr, nbytes);
}


void
wv_free(/*@null@*/ /*@only@*/ void *ptr)
{
  if (ptr == NULL) return;
  free(ptr);
}


/*@null@*/ /*@only@*/ char *
wv_strdup(/*@null@*/ const char *str)
{
  int  i, len;
  char *dup;

  if (str == NULL) return NULL;

  len = strlen(str) + 1;
  dup = (char *) wv_alloc(len*sizeof(char));
  if (dup != NULL)
    for (i = 0; i < len; i++) dup[i] = str[i];

  return dup;
}


static void
wv_freeStripe(wvStripe stripe, int num)
{
  if (num != 1) {
    wv_free(stripe.vertices);
    wv_free(stripe.normals);
    wv_free(stripe.colors);
  }
  wv_free(stripe.gIndices);
  wv_free(stripe.sIndice2);
  wv_free(stripe.lIndice2);
  wv_free(stripe.pIndice2);
}


void
wv_freeGPrim(wvGPrim gprim)
{
  int i;

  for (i = 0; i < gprim.nStripe; i++)
    wv_freeStripe(gprim.stripes[i], gprim.nStripe);
  wv_free(gprim.stripes);
  wv_free(gprim.name);
  wv_free(gprim.vertices);
  wv_free(gprim.normals);
  wv_free(gprim.colors);
  wv_free(gprim.indices);
  wv_free(gprim.lIndices);
  wv_free(gprim.pIndices);
}


void 
wv_destroyContext(wvContext **context)
{
  int       i;
  wvContext *cntxt;

  cntxt = *context;
  if (cntxt->gPrims != NULL) {
    for (i = 0; i < cntxt->nGPrim; i++)
      wv_freeGPrim(cntxt->gPrims[i]);
    wv_free(cntxt->gPrims);
  }

  wv_free(cntxt);
  *context = NULL;
}


/*@null@*/ wvContext *
wv_createContext(int bias, float fov, float zNear, float zFar, float *eye,
                 float *center, float *up)
{
  wvContext *context;

  context = (wvContext *) wv_alloc(sizeof(wvContext));
  if (context == NULL) return NULL;

  context->ioAccess   = 0;
  context->dataAccess = 0;
  context->bias       = bias;
  context->fov        = fov;
  context->zNear      = zNear;
  context->zFar       = zFar;
  context->eye[0]     = eye[0];
  context->eye[1]     = eye[1];
  context->eye[2]     = eye[2];
  context->center[0]  = center[0];
  context->center[1]  = center[1];
  context->center[2]  = center[2];
  context->up[0]      = up[0];
  context->up[1]      = up[1];
  context->up[2]      = up[2];
  context->cleanAll   = 0;
  context->nGPrim     = 0;
  context->mGPrim     = 0;
  context->gPrims     = NULL;

  return context;
}


void
wv_removeAll(wvContext *cntxt)
{
  int i;

  if (cntxt->gPrims != NULL)
    for (i = 0; i < cntxt->nGPrim; i++)
      wv_freeGPrim(cntxt->gPrims[i]);
  cntxt->nGPrim   = 0;
  cntxt->cleanAll = 1;
}


int
wv_setData(int type, int len, void *data, int VBOtype, wvData *dstruct)
{
  int            *idata, *iptr, i;
  unsigned short *sdata;
  unsigned char  *cdata, *cptr;
  float          *fdata, *fptr;
  double         *ddata;

  dstruct->dataType = 0;
  dstruct->dataLen  = len;
  dstruct->dataPtr  = NULL;
  dstruct->data[0]  = 0.0;
  dstruct->data[1]  = 0.0;
  dstruct->data[2]  = 0.0;
  if (len <= 0) return -4;
  
  /* single data entry */
  if ((len == 1) &&
      ((VBOtype == WV_COLORS) || (VBOtype == WV_NORMALS) ||
       (VBOtype == WV_PCOLOR) || (VBOtype == WV_LCOLOR)  ||
       (VBOtype == WV_BCOLOR))) {
    switch (type) {
      case WV_UINT8:
        cdata = (unsigned char *) data;
        if (VBOtype == WV_NORMALS) return -2;
        for (i = 0; i < 3; i++) {
          dstruct->data[i]  = cdata[i];
          dstruct->data[i] /= 255.0;
        }
        break;
      case WV_REAL32:
        fdata = (float *) data;
        dstruct->data[0] = fdata[0];
        dstruct->data[1] = fdata[1];
        dstruct->data[2] = fdata[2];
        break;
      case WV_REAL64:
        ddata = (double *) data;
        dstruct->data[0] = ddata[0];
        dstruct->data[1] = ddata[1];
        dstruct->data[2] = ddata[2];
        break;
      default:
        return -3;
    }

    dstruct->dataType = VBOtype;
    return 0;
  }

  /* an array of data */
  fptr = NULL;
  iptr = NULL;
  cptr = NULL;
  switch (VBOtype) {
    case WV_VERTICES:
    case WV_NORMALS:
      fptr = (float *) wv_alloc(3*len*sizeof(float));
      if (fptr == NULL) return -1;
      dstruct->dataPtr = fptr;
      break;
    case WV_INDICES:
    case WV_PINDICES:
    case WV_LINDICES:
      iptr = (int *) wv_alloc(len*sizeof(int));
      if (iptr == NULL) return -1;
      dstruct->dataPtr = iptr;
      break;
    case WV_COLORS:
      cptr = (unsigned char *) wv_alloc(3*len*sizeof(unsigned char));
      if (cptr == NULL) return -1;
      dstruct->dataPtr = cptr;
      break;
    default:
      return -2;
  }

  switch (type) {
    case WV_UINT8:
      cdata = (unsigned char *) data;
      if (VBOtype != WV_COLORS) {
        wv_free(dstruct->dataPtr);
        dstruct->dataPtr = NULL;
        return -2;
      }
      if (cptr == NULL) return -1;
      for (i = 0; i < len; i++) {
        cptr[3*i  ] = cdata[3*i  ];
        cptr[3*i+1] = cdata[3*i+1];
        cptr[3*i+2] = cdata[3*i+2];
      }
      break;
    case WV_UINT16:
      sdata = (unsigned short *) data;
      if ((VBOtype != WV_INDICES) && (VBOtype != WV_PINDICES) &&
          (VBOtype != WV_LINDICES)) {
        wv_free(dstruct->dataPtr);
        dstruct->dataPtr = NULL;
        return -2;
      }
      if (iptr == NULL) return -1;
      for (i = 0; i < len; i++) iptr[i] = sdata[i];
      break;
    case WV_INT32:
      idata = (int *) data;
      if ((VBOtype != WV_INDICES) && (VBOtype != WV_PINDICES) &&
          (VBOtype != WV_LINDICES)) {
        wv_free(dstruct->dataPtr);
        dstruct->dataPtr = NULL;
        return -2;
      }
      if (iptr == NULL) return -1;
      for (i = 0; i < len; i++) iptr[i] = idata[i];
      break;
    case WV_REAL32:
      fdata = (float *) data;
      if ((VBOtype == WV_VERTICES) || (VBOtype == WV_NORMALS)) {
        if (fptr == NULL) return -1;
        for (i = 0; i < len; i++) {
          fptr[3*i  ] = fdata[3*i  ];
          fptr[3*i+1] = fdata[3*i+1];
          fptr[3*i+2] = fdata[3*i+2];
        }
      } else if (VBOtype == WV_COLORS) {
        if (cptr == NULL) return -1;
        for (i = 0; i < len; i++) {
          cptr[3*i  ] = 255.0*fdata[3*i  ];
          cptr[3*i+1] = 255.0*fdata[3*i+1];
          cptr[3*i+2] = 255.0*fdata[3*i+2];
        }
      } else {
        wv_free(dstruct->dataPtr);
        dstruct->dataPtr = NULL;
        return -2;
      }
      break;
    case WV_REAL64:
      ddata = (double *) data;
      if ((VBOtype == WV_VERTICES) || (VBOtype == WV_NORMALS)) {
        if (fptr == NULL) return -1;
        for (i = 0; i < len; i++) {
          fptr[3*i  ] = ddata[3*i  ];
          fptr[3*i+1] = ddata[3*i+1];
          fptr[3*i+2] = ddata[3*i+2];
        }
      } else if (VBOtype == WV_COLORS) {
        if (cptr == NULL) return -1;
        for (i = 0; i < len; i++) {
          cptr[3*i  ] = 255.0*ddata[3*i  ];
          cptr[3*i+1] = 255.0*ddata[3*i+1];
          cptr[3*i+2] = 255.0*ddata[3*i+2];
        }
      } else {
        wv_free(dstruct->dataPtr);
        dstruct->dataPtr = NULL;
        return -2;
      }
      break;
    default:
      return -3;
  }

  dstruct->dataType = VBOtype;
  return 0;
}


void
wv_adjustVerts(wvData *dstruct, float *focus)
{
  int   i;
  float *fptr;

  if (dstruct->dataType != WV_VERTICES) return;
  if (dstruct->dataPtr  == NULL) return;
  
  fptr = (float *) dstruct->dataPtr;
  for (i = 0; i < dstruct->dataLen; i++) {
    fptr[3*i  ] -= focus[0];
    fptr[3*i+1] -= focus[1];
    fptr[3*i+2] -= focus[2];
    fptr[3*i  ] /= focus[3];
    fptr[3*i+1] /= focus[3];
    fptr[3*i+2] /= focus[3];
  }

}


static void
wv_fixupLineData(wvGPrim *gp, int nstripe, wvStripe *stripes, int *lmark, int bias)
{
  int            *itmp, i, j, k, m, cnt, nslen, nsline;
  float          *ftmp;
  unsigned char  *ctmp;
  unsigned short *stmp;

  for (cnt = i = 0; i < gp->nlIndex/2; i++) if (lmark[i] == -1) cnt++;
  if (cnt == 0) return;
  
  /* need to add these verts and lines to the last stripe */
  nslen = stripes[nstripe-1].nsVerts + 2*cnt;
  if (nslen > 65536) {
    printf(" WV warning: Can not complete last stripe with Lines!\n");
    return;
  }

  ftmp = (float *) wv_realloc(stripes[nstripe-1].vertices, 3*nslen*sizeof(float));
  if (ftmp == NULL) return;
  stripes[nstripe-1].vertices = ftmp;
  itmp = (int *) wv_realloc(stripes[nstripe-1].gIndices, nslen*sizeof(int));
  if (itmp == NULL) return;
  stripes[nstripe-1].gIndices = itmp;
  if (gp->normals != NULL) {
    ftmp = (float *) wv_realloc(stripes[nstripe-1].normals, 3*nslen*sizeof(float));
    if (ftmp == NULL) return;
    stripes[i].normals = ftmp;
  }
  if (gp->colors != NULL) {
    ctmp = (unsigned char *) wv_realloc(stripes[nstripe-1].colors, 
                                        3*nslen*sizeof(unsigned char));
    if (ctmp == NULL) return;
  }
  
  nsline = stripes[nstripe-1].nlIndices + 2*cnt;
  if (stripes[nstripe-1].lIndice2 == NULL) {
    stmp = (unsigned short *) wv_alloc(nsline*sizeof(unsigned short));
  } else {
    stmp = (unsigned short *) wv_realloc(stripes[nstripe-1].lIndice2, 
                                         nsline*sizeof(unsigned short));
  }
  if (stmp == NULL) return;
  stripes[nstripe-1].lIndice2 = stmp;
  
  j = stripes[nstripe-1].nsVerts;
  k = stripes[nstripe-1].nlIndices;
  for (i = 0; i < gp->nlIndex/2; i++) {
    if (lmark[i] != -1) continue;
    m = gp->lIndices[2*i  ]-bias;
    stripes[nstripe-1].gIndices[j]      = m;
    stripes[nstripe-1].vertices[3*j  ]  = gp->vertices[3*m  ];
    stripes[nstripe-1].vertices[3*j+1]  = gp->vertices[3*m+1];
    stripes[nstripe-1].vertices[3*j+2]  = gp->vertices[3*m+2];    
    stripes[nstripe-1].lIndice2[k]      = j;
    if (gp->normals != NULL) {
      stripes[nstripe-1].normals[3*j  ] = gp->normals[3*m  ];
      stripes[nstripe-1].normals[3*j+1] = gp->normals[3*m+1];
      stripes[nstripe-1].normals[3*j+2] = gp->normals[3*m+2]; 
    }
    if (gp->colors != NULL) {
      stripes[nstripe-1].colors[3*j  ]  = gp->colors[3*m  ];
      stripes[nstripe-1].colors[3*j+1]  = gp->colors[3*m+1];
      stripes[nstripe-1].colors[3*j+2]  = gp->colors[3*m+2]; 
    }
    j++;
    k++;
    
    m = gp->lIndices[2*i+1]-bias;
    stripes[nstripe-1].gIndices[j]      = m;
    stripes[nstripe-1].vertices[3*j  ]  = gp->vertices[3*m  ];
    stripes[nstripe-1].vertices[3*j+1]  = gp->vertices[3*m+1];
    stripes[nstripe-1].vertices[3*j+2]  = gp->vertices[3*m+2];    
    stripes[nstripe-1].lIndice2[k]      = j;
    if (gp->normals != NULL) {
      stripes[nstripe-1].normals[3*j  ] = gp->normals[3*m  ];
      stripes[nstripe-1].normals[3*j+1] = gp->normals[3*m+1];
      stripes[nstripe-1].normals[3*j+2] = gp->normals[3*m+2]; 
    }
    if (gp->colors != NULL) {
      stripes[nstripe-1].colors[3*j  ]  = gp->colors[3*m  ];
      stripes[nstripe-1].colors[3*j+1]  = gp->colors[3*m+1];
      stripes[nstripe-1].colors[3*j+2]  = gp->colors[3*m+2]; 
    }
    j++;
    k++;
  }
  
  stripes[nstripe-1].nsVerts   = nslen;
  stripes[nstripe-1].nlIndices = nsline;
}


static int
wv_makeStripes(wvGPrim *gp, int bias)
{
  int            i, j, k, m, maxLen, cnt, len, nstripe, *index, *lmark;
  unsigned short *i2, *il2, *ip2;
  wvStripe       *stripes;

  maxLen = 65536;
  if (gp->gtype == WV_TRIANGLE) maxLen = 65535;
  if (gp->nVerts <= maxLen) {

    /* a single stripe */
    i2  = NULL;
    il2 = NULL;
    ip2 = NULL;
    if (gp->indices != NULL) {
      i2 = (unsigned short *) wv_alloc(gp->nIndex*sizeof(unsigned short));
      if (i2 == NULL) return -1;
      for (i = 0; i < gp->nIndex; i++) i2[i] = gp->indices[i] - bias;
    }
    if (gp->lIndices != NULL) {
      il2 = (unsigned short *) wv_alloc(gp->nlIndex*sizeof(unsigned short));
      if (il2 == NULL) {
        wv_free(i2);
        return -1;
      }
      for (i = 0; i < gp->nlIndex; i++) il2[i] = gp->lIndices[i] - bias;
    }
    if (gp->pIndices != NULL) {
      ip2 = (unsigned short *) wv_alloc(gp->npIndex*sizeof(unsigned short));
      if (ip2 == NULL) {
        wv_free(il2);
        wv_free(i2);
        return -1;
      }
      for (i = 0; i < gp->npIndex; i++) ip2[i] = gp->pIndices[i] - bias;
    }

    gp->nStripe = 1;
    gp->stripes = (wvStripe *) wv_alloc(sizeof(wvStripe));
    if (gp->stripes == NULL) {
      wv_free(ip2);
      wv_free(il2);
      wv_free(i2);
      return -1;
    }
    gp->stripes[0].nsVerts   = gp->nVerts;
    gp->stripes[0].nsIndices = gp->nIndex;
    gp->stripes[0].nlIndices = gp->nlIndex;
    gp->stripes[0].npIndices = gp->npIndex;
    gp->stripes[0].gIndices  = NULL;
    gp->stripes[0].vertices  = gp->vertices;
    gp->stripes[0].normals   = gp->normals;
    gp->stripes[0].colors    = gp->colors;
    gp->stripes[0].sIndice2  = i2;
    gp->stripes[0].lIndice2  = il2;
    gp->stripes[0].pIndice2  = ip2;

  } else {

    /* multiple stripes */
    
    if ((gp->indices  == NULL) && (gp->lIndices == NULL) && 
        (gp->pIndices == NULL)) {
        
      /* not indexed */
      gp->nStripe = gp->nVerts/maxLen;
      if (gp->nStripe*maxLen != gp->nVerts) gp->nStripe += 1;
      stripes = (wvStripe *) wv_alloc(gp->nStripe*sizeof(wvStripe));
      if (stripes == NULL) return -1;

      for (j = i = 0; i < gp->nStripe; i++, j+=maxLen) {
        len = maxLen;
        if (i == gp->nStripe-1) len = gp->nVerts - j;
        stripes[i].sIndice2  = NULL;
        stripes[i].lIndice2  = NULL;
        stripes[i].pIndice2  = NULL;
        stripes[i].normals   = NULL;
        stripes[i].colors    = NULL;
        stripes[i].nsIndices = 0;
        stripes[i].nlIndices = 0;
        stripes[i].npIndices = 0;
        stripes[i].nsVerts   = len;
        stripes[i].vertices  = (float *) wv_alloc(3*len*sizeof(float));
        stripes[i].gIndices  = (int *)   wv_alloc(len*sizeof(int));
        if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
          wv_free(stripes[i].vertices);
          wv_free(stripes[i].gIndices);
          for (k = 0; k < i; k++) {
            wv_free(stripes[k].gIndices);
            wv_free(stripes[k].vertices);
            wv_free(stripes[k].normals);
            wv_free(stripes[k].colors);
          }
          wv_free(stripes);
          return -1;
        }
        for (k = 0; k < len; k++) {
          stripes[i].gIndices[k]     = j+k;
          stripes[i].vertices[3*k  ] = gp->vertices[3*(j+k)  ];
          stripes[i].vertices[3*k+1] = gp->vertices[3*(j+k)+1];
          stripes[i].vertices[3*k+2] = gp->vertices[3*(j+k)+2];
        }
        if (gp->colors != NULL) {
          stripes[i].colors = (unsigned char *) 
                              wv_alloc(3*len*sizeof(unsigned char));
          if (stripes[i].colors == NULL) {
            wv_free(stripes[i].vertices);
            wv_free(stripes[i].gIndices);
            for (k = 0; k < i; k++) {
              wv_free(stripes[k].gIndices);
              wv_free(stripes[k].vertices);
              wv_free(stripes[k].normals);
              wv_free(stripes[k].colors);
            }
            wv_free(stripes);
            return -1;
          }
          for (k = 0; k < len; k++) {
            stripes[i].colors[3*k  ] = gp->colors[3*(j+k)  ];
            stripes[i].colors[3*k+1] = gp->colors[3*(j+k)+1];
            stripes[i].colors[3*k+2] = gp->colors[3*(j+k)+2];
          }          
        }
        if (gp->normals != NULL) {
          stripes[i].normals = (float *) wv_alloc(3*len*sizeof(float));
          if (stripes[i].normals == NULL) {
            wv_free(stripes[i].colors);
            wv_free(stripes[i].vertices);
            wv_free(stripes[i].gIndices);
            for (k = 0; k < i; k++) {
              wv_free(stripes[k].gIndices);
              wv_free(stripes[k].vertices);
              wv_free(stripes[k].normals);
              wv_free(stripes[k].colors);
            }
            wv_free(stripes);
            return -1;
          }
          for (k = 0; k < len; k++) {
            stripes[i].normals[3*k  ] = gp->normals[3*(j+k)  ];
            stripes[i].normals[3*k+1] = gp->normals[3*(j+k)+1];
            stripes[i].normals[3*k+2] = gp->normals[3*(j+k)+2];
          }          
        }

      }

    } else {

      /* indexed */
      
      if (gp->gtype == WV_POINT) {
        /* convert to non-indexed */
        nstripe = gp->nIndex/maxLen;
        if (nstripe*maxLen != gp->nIndex) nstripe++;
        stripes = (wvStripe *) wv_alloc(gp->nStripe*sizeof(wvStripe));
        if (stripes == NULL) return -1;
        for (j = i = 0; i < nstripe; i++, j+=maxLen) {
          len = maxLen;
          if (i == nstripe) len = gp->nIndex - j;
          stripes[i].sIndice2  = NULL;
          stripes[i].lIndice2  = NULL;
          stripes[i].pIndice2  = NULL;
          stripes[i].normals   = NULL;
          stripes[i].colors    = NULL;
          stripes[i].nsIndices = 0;
          stripes[i].nlIndices = 0;
          stripes[i].npIndices = 0;
          stripes[i].nsVerts   = len;
          stripes[i].vertices  = (float *) wv_alloc(3*len*sizeof(float));
          stripes[i].gIndices  = (int *)   wv_alloc(len*sizeof(int));
          if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
            wv_free(stripes[i].vertices);
            wv_free(stripes[i].gIndices);
            for (k = 0; k < i; k++) {
              wv_free(stripes[k].gIndices);
              wv_free(stripes[k].vertices);
              wv_free(stripes[k].colors);
            }
            wv_free(stripes);
            return -1;
          }
          for (k = 0; k < len; k++) {
            m = gp->indices[j+k] - bias;
            stripes[i].gIndices[k]     = m;
            stripes[i].vertices[3*k  ] = gp->vertices[3*m  ];
            stripes[i].vertices[3*k+1] = gp->vertices[3*m+1];
            stripes[i].vertices[3*k+2] = gp->vertices[3*m+2];
          }
          if (gp->colors != NULL) {
            stripes[i].colors = (unsigned char *) 
                                wv_alloc(3*len*sizeof(unsigned char));
            if (stripes[i].colors == NULL) {
              wv_free(stripes[i].vertices);
              wv_free(stripes[i].gIndices);
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              return -1;
            }
            for (k = 0; k < len; k++) {
              m = stripes[i].gIndices[k];
              stripes[i].colors[3*k  ] = gp->colors[3*m  ];
              stripes[i].colors[3*k+1] = gp->colors[3*m+1];
              stripes[i].colors[3*k+2] = gp->colors[3*m+2];
            }          
          }
        }
        
      } else if ((gp->gtype == WV_LINE) || (gp->lIndices == NULL)) {
        
        if (gp->indices == NULL) {

          nstripe = gp->nVerts/maxLen;
          if (nstripe*maxLen != gp->nVerts) nstripe++;
          stripes = (wvStripe *) wv_alloc(nstripe*sizeof(wvStripe));
          if (stripes == NULL) return -1;

          for (j = i = 0; i < nstripe; i++, j+=maxLen) {
            len = maxLen;
            if (i == nstripe-1) len = gp->nVerts - j;
            stripes[i].sIndice2  = NULL;
            stripes[i].lIndice2  = NULL;
            stripes[i].pIndice2  = NULL;
            stripes[i].normals   = NULL;
            stripes[i].colors    = NULL;
            stripes[i].nsIndices = 0;
            stripes[i].nlIndices = 0;
            stripes[i].npIndices = 0;
            stripes[i].nsVerts   = len;
            stripes[i].vertices  = (float *) wv_alloc(3*len*sizeof(float));
            stripes[i].gIndices  = (int *)   wv_alloc(len*sizeof(int));
            if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
              wv_free(stripes[i].vertices);
              wv_free(stripes[i].gIndices);
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              return -1;
            }
            for (k = 0; k < len; k++) {
              stripes[i].gIndices[k]     = j+k;
              stripes[i].vertices[3*k  ] = gp->vertices[3*(j+k)  ];
              stripes[i].vertices[3*k+1] = gp->vertices[3*(j+k)+1];
              stripes[i].vertices[3*k+2] = gp->vertices[3*(j+k)+2];
            }
            for (cnt = k = 0; k < gp->npIndex; k++)
              if ((gp->pIndices[k]-bias >= j) &&
                  (gp->pIndices[k]-bias < j+len)) cnt++;
            if (cnt != 0) {
              stripes[i].pIndice2  = (unsigned short *) 
                                     wv_alloc(cnt*sizeof(unsigned short));
              if (stripes[i].pIndice2 == NULL) {
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                return -1;
              }
              for (cnt = k = 0; k < gp->npIndex; k++)
                if ((gp->pIndices[k]-bias >= j) &&
                    (gp->pIndices[k]-bias < j+len)) {
                  stripes[i].pIndice2[cnt] =  gp->pIndices[k]-bias-j;
                  cnt++;
                }
              stripes[i].npIndices = cnt;
            }
            if (gp->normals != NULL) {
              stripes[i].normals = (float *) wv_alloc(3*len*sizeof(float));
              if (stripes[i].normals == NULL) {
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                return -1;
              }
              for (k = 0; k < len; k++) {
                stripes[i].normals[3*k  ] = gp->normals[3*(j+k)  ];
                stripes[i].normals[3*k+1] = gp->normals[3*(j+k)+1];
                stripes[i].normals[3*k+2] = gp->normals[3*(j+k)+2];
              }
            }
            if (gp->colors != NULL) {
              stripes[i].colors = (unsigned char *) 
                                  wv_alloc(3*len*sizeof(unsigned char));
              if (stripes[i].colors == NULL) {
                wv_free(stripes[i].normals);
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                return -1;
              }
              for (k = 0; k < len; k++) {
                stripes[i].colors[3*k  ] = gp->colors[3*(j+k)  ];
                stripes[i].colors[3*k+1] = gp->colors[3*(j+k)+1];
                stripes[i].colors[3*k+2] = gp->colors[3*(j+k)+2];
              }          
            }
          }
        
        } else {
        
          index = (int *) wv_alloc((gp->nVerts+gp->nIndex)*sizeof(int));
          if (index == NULL) return -1;
          nstripe = gp->nIndex/maxLen;
          if (nstripe*maxLen != gp->nIndex) nstripe++;
          stripes = (wvStripe *) wv_alloc(nstripe*sizeof(wvStripe));
          if (stripes == NULL) {
            wv_free(index);
            return -1;
          }
          for (j = i = 0; i < nstripe; i++, j+=maxLen) {
            for (k = 0; k < gp->nVerts; k++) index[k] = -1;
            len = maxLen;
            if (i == nstripe-1) len = gp->nIndex - j;
            for (cnt = k = 0; k < len; k++) {
              m = gp->indices[j+k] - bias;
              if (index[m] == -1) {
                index[m] = cnt;
                index[gp->nVerts+cnt] = m;
                cnt++;
              }
            }
            stripes[i].sIndice2  = (unsigned short *)
                                   wv_alloc(len*sizeof(unsigned short));
            if (stripes[i].sIndice2 == NULL) {
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].sIndice2);
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              wv_free(index);
              return -1;
            }
            for (k = 0; k < len; k++) {
              m = gp->indices[j+k] - bias;
              stripes[i].sIndice2[k] = index[m]; 
            }

            stripes[i].lIndice2  = NULL;
            stripes[i].pIndice2  = NULL;
            stripes[i].normals   = NULL;
            stripes[i].colors    = NULL;
            stripes[i].nsIndices = len;
            stripes[i].nlIndices = 0;
            stripes[i].npIndices = 0;
            stripes[i].nsVerts   = cnt;
            stripes[i].vertices  = (float *) wv_alloc(3*cnt*sizeof(float));
            stripes[i].gIndices  = (int *)   wv_alloc(cnt*sizeof(int));
            if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
              wv_free(stripes[i].vertices);
              wv_free(stripes[i].gIndices);
              wv_free(stripes[i].sIndice2);
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].sIndice2);
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              wv_free(index);
              return -1;
            }
            for (k = 0; k < cnt; k++) {
              m = index[gp->nVerts+k];
              stripes[i].gIndices[k]     = m;
              stripes[i].vertices[3*k  ] = gp->vertices[3*m  ];
              stripes[i].vertices[3*k+1] = gp->vertices[3*m+1];
              stripes[i].vertices[3*k+2] = gp->vertices[3*m+2];
            }
            if (gp->normals != NULL) {
              stripes[i].normals = (float *) wv_alloc(3*cnt*sizeof(float));
              if (stripes[i].normals == NULL) {
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                return -1;
              }
              for (k = 0; k < cnt; k++) {
                m = index[gp->nVerts+k];
                stripes[i].normals[3*k  ] = gp->normals[3*m  ];
                stripes[i].normals[3*k+1] = gp->normals[3*m+1];
                stripes[i].normals[3*k+2] = gp->normals[3*m+2];
              }
            }
            if (gp->colors != NULL) {
              stripes[i].colors = (unsigned char *) 
                                  wv_alloc(3*cnt*sizeof(unsigned char));
              if (stripes[i].colors == NULL) {
                wv_free(stripes[i].normals);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                return -1;
              }
              for (k = 0; k < cnt; k++) {
                m = index[gp->nVerts+k];
                stripes[i].colors[3*k  ] = gp->colors[3*m  ];
                stripes[i].colors[3*k+1] = gp->colors[3*m+1];
                stripes[i].colors[3*k+2] = gp->colors[3*m+2];
              }          
            }
            for (cnt = k = 0; k < gp->npIndex; k++) {
              m = gp->pIndices[k]-bias;
              if (index[m] != -1) cnt++;
            }
            if (cnt != 0) {
              stripes[i].pIndice2  = (unsigned short *) 
                                     wv_alloc(cnt*sizeof(unsigned short));
              if (stripes[i].pIndice2 == NULL) {
                wv_free(stripes[i].colors);
                wv_free(stripes[i].normals);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                return -1;
              }
              for (cnt = k = 0; k < gp->npIndex; k++) {
                m = gp->pIndices[k]-bias;
                if (index[m] != -1) {
                  stripes[i].pIndice2[cnt] = index[m];
                  cnt++;
                }
              }
              stripes[i].npIndices = cnt;
            }
          }          

          wv_free(index);
        }

      } else {
      
        /* triangles with line indices */

        lmark = (int *) wv_alloc((gp->nlIndex/2)*sizeof(int));
        if (lmark == NULL) return -1;
        for (i = 0; i < gp->nlIndex/2; i++) lmark[i] = -1;

        if (gp->indices == NULL) {

          nstripe = gp->nVerts/maxLen;
          if (nstripe*maxLen != gp->nVerts) nstripe++;
          stripes = (wvStripe *) wv_alloc(nstripe*sizeof(wvStripe));
          if (stripes == NULL) {
            wv_free(lmark);
            return -1;
          }

          for (j = i = 0; i < nstripe; i++, j+=maxLen) {
            len = maxLen;
            if (i == nstripe-1) len = gp->nVerts - j;
            stripes[i].sIndice2  = NULL;
            stripes[i].lIndice2  = NULL;
            stripes[i].pIndice2  = NULL;
            stripes[i].normals   = NULL;
            stripes[i].colors    = NULL;
            stripes[i].nsIndices = 0;
            stripes[i].nlIndices = 0;
            stripes[i].npIndices = 0;
            stripes[i].nsVerts   = len;
            stripes[i].vertices  = (float *) wv_alloc(3*len*sizeof(float));
            stripes[i].gIndices  = (int *)   wv_alloc(len*sizeof(int));
            if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
              wv_free(stripes[i].vertices);
              wv_free(stripes[i].gIndices);
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].lIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              wv_free(lmark);
              return -1;
            }
            for (k = 0; k < len; k++) {
              stripes[i].gIndices[k]     = j+k;
              stripes[i].vertices[3*k  ] = gp->vertices[3*(j+k)  ];
              stripes[i].vertices[3*k+1] = gp->vertices[3*(j+k)+1];
              stripes[i].vertices[3*k+2] = gp->vertices[3*(j+k)+2];
            }
            for (cnt = k = 0; k < gp->npIndex; k++)
              if ((gp->pIndices[k]-bias >= j) &&
                  (gp->pIndices[k]-bias < j+len)) cnt++;
            if (cnt != 0) {
              stripes[i].pIndice2  = (unsigned short *) 
                                     wv_alloc(cnt*sizeof(unsigned short));
              if (stripes[i].pIndice2 == NULL) {
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(lmark);
                return -1;
              }
              for (cnt = k = 0; k < gp->npIndex; k++)
                if ((gp->pIndices[k]-bias >= j) &&
                    (gp->pIndices[k]-bias < j+len)) {
                  stripes[i].pIndice2[cnt] =  gp->pIndices[k]-bias-j;
                  cnt++;
                }
            }
            if (gp->normals != NULL) {
              stripes[i].normals = (float *) wv_alloc(3*len*sizeof(float));
              if (stripes[i].normals == NULL) {
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(lmark);
                return -1;
              }
              for (k = 0; k < len; k++) {
                stripes[i].normals[3*k  ] = gp->normals[3*(j+k)  ];
                stripes[i].normals[3*k+1] = gp->normals[3*(j+k)+1];
                stripes[i].normals[3*k+2] = gp->normals[3*(j+k)+2];
              }
            }
            if (gp->colors != NULL) {
              stripes[i].colors = (unsigned char *) 
                                  wv_alloc(3*len*sizeof(unsigned char));
              if (stripes[i].colors == NULL) {
                wv_free(stripes[i].normals);
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(lmark);
                return -1;
              }
              for (k = 0; k < len; k++) {
                stripes[i].colors[3*k  ] = gp->colors[3*(j+k)  ];
                stripes[i].colors[3*k+1] = gp->colors[3*(j+k)+1];
                stripes[i].colors[3*k+2] = gp->colors[3*(j+k)+2];
              }          
            }
            for (cnt = k = 0; k < gp->nlIndex/2; k++) {
              if (lmark[k] != -1) continue;
              if ((gp->lIndices[2*k  ]-bias >= j) &&
                  (gp->lIndices[2*k  ]-bias < j+len) &&
                  (gp->lIndices[2*k+1]-bias >= j) &&
                  (gp->lIndices[2*k+1]-bias < j+len)) cnt++;
            }
            if (cnt != 0) {
              stripes[i].lIndice2  = (unsigned short *) 
                                     wv_alloc(2*cnt*sizeof(unsigned short));
              if (stripes[i].lIndice2 == NULL) {
                wv_free(stripes[i].colors);
                wv_free(stripes[i].normals);
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(lmark);
                return -1;
              }
              for (cnt = k = 0; k < gp->nlIndex/2; k++) {
                if (lmark[k] != -1) continue;
                if ((gp->lIndices[2*k  ]-bias >= j) &&
                    (gp->lIndices[2*k  ]-bias < j+len) &&
                    (gp->lIndices[2*k+1]-bias >= j) &&
                    (gp->lIndices[2*k+1]-bias < j+len)) {
                  stripes[i].lIndice2[2*cnt  ] = gp->lIndices[2*k  ]-bias-j;
                  stripes[i].lIndice2[2*cnt+1] = gp->lIndices[2*k+1]-bias-j;
                  lmark[k] = i;
                  cnt++;
                }
              }
              stripes[i].nlIndices = 2*cnt;
            }
          }
        
        } else {
        
          index = (int *) wv_alloc((gp->nVerts+gp->nIndex)*sizeof(int));
          if (index == NULL) {
            wv_free(lmark);
            return -1;
          }
          nstripe = gp->nIndex/maxLen;
          if (nstripe*maxLen != gp->nIndex) nstripe++;
          stripes = (wvStripe *) wv_alloc(nstripe*sizeof(wvStripe));
          if (stripes == NULL) {
            wv_free(index);
            wv_free(lmark);
            return -1;
          }
          for (j = i = 0; i < nstripe; i++, j+=maxLen) {
            for (k = 0; k < gp->nVerts; k++) index[k] = -1;
            len = maxLen;
            if (i == nstripe-1) len = gp->nIndex - j;
            for (cnt = k = 0; k < len; k++) {
              m = gp->indices[j+k] - bias;
              if (index[m] == -1) {
                index[m] = cnt;
                index[gp->nVerts+cnt] = m;
                cnt++;
              }
            }
            stripes[i].sIndice2  = (unsigned short *)
                                   wv_alloc(len*sizeof(unsigned short));
            if (stripes[i].sIndice2 == NULL) {
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].sIndice2);
                wv_free(stripes[k].lIndice2);
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              wv_free(index);
              wv_free(lmark);
              return -1;
            }
            for (k = 0; k < len; k++) {
              m = gp->indices[j+k] - bias;
              stripes[i].sIndice2[k] = index[m];
            }            
            stripes[i].lIndice2  = NULL;
            stripes[i].pIndice2  = NULL;
            stripes[i].normals   = NULL;
            stripes[i].colors    = NULL;
            stripes[i].nsIndices = len;
            stripes[i].nlIndices = 0;
            stripes[i].npIndices = 0;
            stripes[i].nsVerts   = cnt;
            stripes[i].vertices  = (float *) wv_alloc(3*cnt*sizeof(float));
            stripes[i].gIndices  = (int *)   wv_alloc(cnt*sizeof(int));
            if ((stripes[i].vertices == NULL) || (stripes[i].gIndices == NULL)) {
              wv_free(stripes[i].vertices);
              wv_free(stripes[i].gIndices);
              wv_free(stripes[i].sIndice2);
              for (k = 0; k < i; k++) {
                wv_free(stripes[k].sIndice2);
                wv_free(stripes[k].lIndice2);
                wv_free(stripes[k].pIndice2);
                wv_free(stripes[k].gIndices);
                wv_free(stripes[k].vertices);
                wv_free(stripes[k].normals);
                wv_free(stripes[k].colors);
              }
              wv_free(stripes);
              wv_free(index);
              wv_free(lmark);
              return -1;
            }
            for (k = 0; k < cnt; k++) {
              m = index[gp->nVerts+k];
              stripes[i].gIndices[k]     = m;
              stripes[i].vertices[3*k  ] = gp->vertices[3*m  ];
              stripes[i].vertices[3*k+1] = gp->vertices[3*m+1];
              stripes[i].vertices[3*k+2] = gp->vertices[3*m+2];
            }
            if (gp->normals != NULL) {
              stripes[i].normals = (float *) wv_alloc(3*cnt*sizeof(float));
              if (stripes[i].normals == NULL) {
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                wv_free(lmark);
                return -1;
              }
              for (k = 0; k < cnt; k++) {
                m = index[gp->nVerts+k];
                stripes[i].normals[3*k  ] = gp->normals[3*m  ];
                stripes[i].normals[3*k+1] = gp->normals[3*m+1];
                stripes[i].normals[3*k+2] = gp->normals[3*m+2];
              }
            }
            if (gp->colors != NULL) {
              stripes[i].colors = (unsigned char *) 
                                  wv_alloc(3*cnt*sizeof(unsigned char));
              if (stripes[i].colors == NULL) {
                wv_free(stripes[i].normals);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                wv_free(lmark);
                return -1;
              }
              for (k = 0; k < cnt; k++) {
                m = index[gp->nVerts+k];
                stripes[i].colors[3*k  ] = gp->colors[3*m  ];
                stripes[i].colors[3*k+1] = gp->colors[3*m+1];
                stripes[i].colors[3*k+2] = gp->colors[3*m+2];
              }          
            }
            for (cnt = k = 0; k < gp->npIndex; k++) {
              m = gp->pIndices[k]-bias;
              if (index[m] != -1) cnt++;
            }
            if (cnt != 0) {
              stripes[i].pIndice2  = (unsigned short *) 
                                     wv_alloc(cnt*sizeof(unsigned short));
              if (stripes[i].pIndice2 == NULL) {
                wv_free(stripes[i].colors);
                wv_free(stripes[i].normals);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                wv_free(lmark);
                return -1;
              }
              for (cnt = k = 0; k < gp->npIndex; k++) {
                m = gp->pIndices[k]-bias;
                if (index[m] != -1) {
                  stripes[i].pIndice2[cnt] = index[m];
                  cnt++;
                }
              }
              stripes[i].npIndices = cnt;
            }

            for (cnt = k = 0; k < gp->nlIndex/2; k++) {
              if (lmark[k] != -1) continue;
              m = gp->lIndices[2*k  ]-bias;
              if (index[m] == -1) continue;
              m = gp->lIndices[2*k+1]-bias;
              if (index[m] != -1) cnt++;
            }
            if (cnt != 0) {
              stripes[i].lIndice2  = (unsigned short *) 
                                     wv_alloc(2*cnt*sizeof(unsigned short));
              if (stripes[i].lIndice2 == NULL) {
                wv_free(stripes[i].pIndice2);
                wv_free(stripes[i].colors);
                wv_free(stripes[i].normals);
                wv_free(stripes[i].vertices);
                wv_free(stripes[i].gIndices);
                wv_free(stripes[i].sIndice2);
                for (k = 0; k < i; k++) {
                  wv_free(stripes[k].sIndice2);
                  wv_free(stripes[k].lIndice2);
                  wv_free(stripes[k].pIndice2);
                  wv_free(stripes[k].gIndices);
                  wv_free(stripes[k].vertices);
                  wv_free(stripes[k].normals);
                  wv_free(stripes[k].colors);
                }
                wv_free(stripes);
                wv_free(index);
                wv_free(lmark);
                return -1;
              }
              for (cnt = k = 0; k < gp->nlIndex/2; k++) {
                if (lmark[k] != -1) continue;
                m = gp->lIndices[2*k  ]-bias;
                if (index[m] == -1) continue;
                stripes[i].lIndice2[2*cnt  ] = index[m];
                m = gp->lIndices[2*k+1]-bias;
                if (index[m] == -1) continue;
                stripes[i].lIndice2[2*cnt+1] = index[m];
                lmark[k] = i;
                cnt++;
              }
              stripes[i].nlIndices = 2*cnt;
            }
          }

          wv_free(index);
        }

        wv_fixupLineData(gp, nstripe, stripes, lmark, bias);
        wv_free(lmark);
      }
      
      gp->nStripe = nstripe;
    }
    
    gp->stripes = stripes;
  }

  return 0;
}


static void
wv_computeNormals(int bias, int nVerts, float *vertices, int nIndex, 
                  /*@null@*/ int *indices, float *norm, /*@null@*/ int *cnt)
{
  int   i, i1, i2, i3;
  float dis, xnor, ynor, znor, x1, y1, z1, x2, y2, z2, x3, y3, z3;
  
  if ((indices == NULL) || (cnt == NULL)) {
  
    /* triangle facetted only */
    for (i = 0; i < nVerts; i+=3) {
      x1   = vertices[3*i  ];
      y1   = vertices[3*i+1];
      z1   = vertices[3*i+2];
      x2   = vertices[3*i+3];
      y2   = vertices[3*i+4];
      z2   = vertices[3*i+5];
      x3   = vertices[3*i+6];
      y3   = vertices[3*i+7];
      z3   = vertices[3*i+8];
      xnor = (y3-y1)*(z2-z1)-(y2-y1)*(z3-z1);
      ynor = (z3-z1)*(x2-x1)-(x3-x1)*(z2-z1);
      znor = (x3-x1)*(y2-y1)-(y3-y1)*(x2-x1);
      dis  = sqrtf(xnor*xnor + ynor*ynor + znor*znor);
      if (dis != 0.0f) {
        norm[3*i  ] = xnor/dis;
        norm[3*i+1] = ynor/dis;
        norm[3*i+2] = znor/dis;
        norm[3*i+3] = xnor/dis;
        norm[3*i+4] = ynor/dis;
        norm[3*i+5] = znor/dis;
        norm[3*i+6] = xnor/dis;
        norm[3*i+7] = ynor/dis;
        norm[3*i+8] = znor/dis;
      } else {
        norm[3*i  ] = 0.0;
        norm[3*i+1] = 0.0;
        norm[3*i+2] = 0.0;
        norm[3*i+3] = 0.0;
        norm[3*i+4] = 0.0;
        norm[3*i+5] = 0.0;
        norm[3*i+6] = 0.0;
        norm[3*i+7] = 0.0;
        norm[3*i+8] = 0.0;
      }
    }
    
    return;
  }
  
  /* zero the storage */

  for (i = 0; i < nVerts; i++) {
    cnt[i]      = 0;
    norm[3*i  ] = 0.0;
    norm[3*i+1] = 0.0;
    norm[3*i+2] = 0.0;
  }
  
  for (i = 0; i < nIndex; i+=3) {
    i1   = indices[i  ] - bias;
    i2   = indices[i+1] - bias;
    i3   = indices[i+2] - bias;
    x1   = vertices[3*i1  ];
    y1   = vertices[3*i1+1];
    z1   = vertices[3*i1+2];
    x2   = vertices[3*i2  ];
    y2   = vertices[3*i2+1];
    z2   = vertices[3*i2+2];
    x3   = vertices[3*i3  ];
    y3   = vertices[3*i3+1];
    z3   = vertices[3*i3+2];
    xnor = (y3-y1)*(z2-z1)-(y2-y1)*(z3-z1);
    ynor = (z3-z1)*(x2-x1)-(x3-x1)*(z2-z1);
    znor = (x3-x1)*(y2-y1)-(y3-y1)*(x2-x1);
    dis  = sqrtf(xnor*xnor + ynor*ynor + znor*znor);
    if (dis != 0.0f) {
      norm[3*i1  ] += xnor/dis;
      norm[3*i1+1] += ynor/dis;
      norm[3*i1+2] += znor/dis;
      norm[3*i2  ] += xnor/dis;
      norm[3*i2+1] += ynor/dis;
      norm[3*i2+2] += znor/dis;
      norm[3*i3  ] += xnor/dis;
      norm[3*i3+1] += ynor/dis;
      norm[3*i3+2] += znor/dis;
      cnt[i1]++;
      cnt[i2]++;
      cnt[i3]++;
    }
  }
  
  /* renormalize */
  for (i = 0; i < nVerts; i++) {
    if (cnt[i] <= 1) continue;
    dis  = cnt[i];
    xnor = norm[3*i  ] / dis;
    ynor = norm[3*i+1] / dis;
    znor = norm[3*i+2] / dis;
    dis  = sqrtf(xnor*xnor + ynor*ynor + znor*znor);
    norm[3*i  ] = xnor/dis;
    norm[3*i+1] = ynor/dis;
    norm[3*i+2] = znor/dis;
  }

}


void
wv_printGPrim(wvContext *cntxt, int index)
{
  int i;

  if (cntxt == NULL) return;
  if (cntxt->gPrims == NULL) return;

  printf("\n GPrim: %s  GType = %d  Attrs = %x\n", 
         cntxt->gPrims[index].name, cntxt->gPrims[index].gtype,
         cntxt->gPrims[index].attrs);
  printf("    Point data: %f  %f %f %f\n",
         cntxt->gPrims[index].pSize,     cntxt->gPrims[index].pColor[0],
         cntxt->gPrims[index].pColor[1], cntxt->gPrims[index].pColor[2]);
  if (cntxt->gPrims[index].gtype > 0) {
    printf("    Line  data: %f  %f %f %f\n",
           cntxt->gPrims[index].lWidth,    cntxt->gPrims[index].lColor[0],
           cntxt->gPrims[index].lColor[1], cntxt->gPrims[index].lColor[2]);
    printf("      f/bcolor:  %f %f %f  %f %f %f\n",
           cntxt->gPrims[index].bColor[0], cntxt->gPrims[index].bColor[1], 
           cntxt->gPrims[index].bColor[2], cntxt->gPrims[index].fColor[0],
           cntxt->gPrims[index].fColor[1], cntxt->gPrims[index].fColor[2]);
  }
  if (cntxt->gPrims[index].gtype > 1) {
    printf("    Tri   data: colors  %f %f %f  %f %f %f\n",
           cntxt->gPrims[index].bColor[0], cntxt->gPrims[index].bColor[1], 
           cntxt->gPrims[index].bColor[2], cntxt->gPrims[index].fColor[0],
           cntxt->gPrims[index].fColor[1], cntxt->gPrims[index].fColor[2]);
    printf("                normal  %f %f %f\n",
           cntxt->gPrims[index].normal[0], cntxt->gPrims[index].normal[1], 
           cntxt->gPrims[index].normal[2]);
  }
  printf("    %d Vertices:\n", cntxt->gPrims[index].nVerts);
  for (i = 0; i < cntxt->gPrims[index].nVerts; i++)
    printf("           %f %f %f\n", cntxt->gPrims[index].vertices[3*i  ],
                                    cntxt->gPrims[index].vertices[3*i+1], 
                                    cntxt->gPrims[index].vertices[3*i+2]);
  if (cntxt->gPrims[index].colors != NULL) {
    printf("    %d Colors:\n", cntxt->gPrims[index].nVerts);
    for (i = 0; i < cntxt->gPrims[index].nVerts; i++)
      printf("           %d %d %d\n", cntxt->gPrims[index].colors[3*i  ],
                                      cntxt->gPrims[index].colors[3*i+1], 
                                      cntxt->gPrims[index].colors[3*i+2]);
  }
  if (cntxt->gPrims[index].gtype == 1) {
    if (cntxt->gPrims[index].normals != NULL) {
      printf("    %d tVerts:\n", cntxt->gPrims[index].nlIndex/2);
      for (i = 0; i < cntxt->gPrims[index].nlIndex/2; i++)
        printf("           %f %f %f\n", cntxt->gPrims[index].normals[3*i  ],
                                        cntxt->gPrims[index].normals[3*i+1], 
                                        cntxt->gPrims[index].normals[3*i+2]);
      printf("    %d normals:\n", cntxt->gPrims[index].nlIndex/2);
      for (i = cntxt->gPrims[index].nlIndex/2; i < cntxt->gPrims[index].nlIndex; i++)
        printf("           %f %f %f\n", cntxt->gPrims[index].normals[3*i  ],
                                        cntxt->gPrims[index].normals[3*i+1], 
                                        cntxt->gPrims[index].normals[3*i+2]);
    }
  } else {
    if (cntxt->gPrims[index].normals != NULL) {
      printf("    %d normals:\n", cntxt->gPrims[index].nVerts);
      for (i = 0; i < cntxt->gPrims[index].nVerts; i++)
        printf("           %f %f %f\n", cntxt->gPrims[index].normals[3*i  ],
                                        cntxt->gPrims[index].normals[3*i+1], 
                                        cntxt->gPrims[index].normals[3*i+2]);
    }
  }
  if (cntxt->gPrims[index].indices != NULL) {
    printf("    %d Indices:", cntxt->gPrims[index].nIndex);
    for (i = 0; i < cntxt->gPrims[index].nIndex; i++)
      printf(" %d", cntxt->gPrims[index].indices[i]);
    printf("\n");
  }
  if (cntxt->gPrims[index].lIndices != NULL) {
    printf("    %d lIndices:", cntxt->gPrims[index].nlIndex);
    for (i = 0; i < cntxt->gPrims[index].nlIndex; i++)
      printf(" %d", cntxt->gPrims[index].lIndices[i]);
    printf("\n");
  }
  if (cntxt->gPrims[index].pIndices != NULL) {
    printf("    %d pIndices:", cntxt->gPrims[index].npIndex);
    for (i = 0; i < cntxt->gPrims[index].npIndex; i++)
      printf(" %d", cntxt->gPrims[index].pIndices[i]);
    printf("\n");
  }
         
  printf("\n");
}


int
wv_indexGPrim(wvContext *cntxt, char *name)
{
  int i;

  if (name == NULL) return -3;
  
  if (cntxt->gPrims != NULL)
    for (i = 0; i < cntxt->nGPrim; i++)
      if (strcmp(name, cntxt->gPrims[i].name) == 0) return i;
      
  return -2;
}


int
wv_addGPrim(wvContext *cntxt, char *name, int gtype, int attrs, 
            int nItems, wvData *items)
{
  int     i, nameLen, type, *cnt;
  char    *nam;
  float   *norm;
  wvGPrim *gp;

  if (name == NULL) return -3;
  nameLen = strlen(name);
  if (nameLen == 0) return -3;

  if (cntxt->gPrims != NULL)
    for (i = 0; i < cntxt->nGPrim; i++)
      if (strcmp(name, cntxt->gPrims[i].name) == 0) return -2;

  nameLen += 4 - nameLen%4;
  nam = (char *) wv_alloc(nameLen*sizeof(char));
  if (nam == NULL) return -1;
  for (i = 0; i < nameLen; i++) nam[i] = 0;
  for (i = 0; i < nameLen; i++) {
    if (name[i] == 0) break;
    nam[i] = name[i];
  }

  while (cntxt->ioAccess != 0) usleep(10000);
  cntxt->dataAccess = 1;
  if (cntxt->nGPrim == cntxt->mGPrim) {
    if (cntxt->nGPrim == 0) {
      gp = (wvGPrim *) wv_alloc(sizeof(wvGPrim));
    } else {
      gp = (wvGPrim *) wv_realloc( cntxt->gPrims,
                                  (cntxt->mGPrim+1)*sizeof(wvGPrim));
    }
    if (gp == NULL) {
      wv_free(nam);
      return -1;
    }
    cntxt->mGPrim += 1;
    cntxt->gPrims  = gp;
  }
  if (cntxt->gPrims == NULL) {
    wv_free(nam);
    return -1;
  }
  gp = &cntxt->gPrims[cntxt->nGPrim];
  cntxt->dataAccess = 0;

  gp->gtype     = gtype;
  gp->updateFlg = WV_PCOLOR;            /* all sub-data types (new) */
  gp->attrs     = attrs;
  gp->nVerts    = 0;
  gp->nIndex    = 0;
  gp->nlIndex   = 0;
  gp->npIndex   = 0;
  gp->pSize     = 3;                    /* not touched by items */
  gp->pColor[0] = 0.0;
  gp->pColor[1] = 0.0;
  gp->pColor[2] = 0.0;
  gp->lWidth    = gtype;                /* not touched by items */
  gp->lColor[0] = 0.2;
  gp->lColor[1] = 0.2;
  gp->lColor[2] = 0.2;
  gp->fColor[0] = 1.0;
  gp->fColor[1] = 0.0;
  gp->fColor[2] = 0.0;
  gp->bColor[0] = 0.5;
  gp->bColor[1] = 0.5;
  gp->bColor[2] = 0.5;
  gp->normal[0] = 0.0;
  gp->normal[1] = 0.0;
  gp->normal[2] = 0.0;
  gp->vertices  = NULL;
  gp->colors    = NULL;
  gp->normals   = NULL;
  gp->indices   = NULL;
  gp->lIndices  = NULL;
  gp->pIndices  = NULL;
  gp->stripes   = NULL;
  
  /* parse through the data items and store away */
  for (i = 0; i < nItems; i++) {
    type = items[i].dataType;
    switch (type) {
      case WV_VERTICES:
        if (gp->nVerts == 0) {
          gp->nVerts = items[i].dataLen;
        } else {
          if (gp->nVerts != items[i].dataLen) {
            wv_free(nam);
            return -4;
          }
        }
        gp->vertices = (float *) items[i].dataPtr;
        break;
      case WV_INDICES:
        gp->nIndex  = items[i].dataLen;
        gp->indices = (int *) items[i].dataPtr;
        break;
      case WV_COLORS:
        if (items[i].dataLen == 1) {
          if (gtype == WV_POINT) {
            gp->pColor[0] = items[i].data[0];
            gp->pColor[1] = items[i].data[1];
            gp->pColor[2] = items[i].data[2];            
          } else if (gtype == WV_LINE) {
            gp->lColor[0] = items[i].data[0];
            gp->lColor[1] = items[i].data[1];
            gp->lColor[2] = items[i].data[2];
            gp->fColor[0] = items[i].data[0];
            gp->fColor[1] = items[i].data[1];
            gp->fColor[2] = items[i].data[2];
          } else {
            gp->fColor[0] = items[i].data[0];
            gp->fColor[1] = items[i].data[1];
            gp->fColor[2] = items[i].data[2];  
          } 
        } else {
          if (gp->nVerts == 0) {
            gp->nVerts = items[i].dataLen;
          } else {
            if (gp->nVerts != items[i].dataLen) {
              wv_free(nam);
              return -4;
            }
          }
          gp->colors = (unsigned char *) items[i].dataPtr;
        }
        break;
      case WV_NORMALS:
        if (items[i].dataLen == 1) {
          gp->normal[0] = items[i].data[0];
          gp->normal[1] = items[i].data[1];
          gp->normal[2] = items[i].data[2];  
        } else {
          if (gp->nVerts == 0) {
            gp->nVerts = items[i].dataLen;
          } else {
            if (gp->nVerts != items[i].dataLen) {
              wv_free(nam);
              return -4;
            }
          }
          gp->normals = (float *) items[i].dataPtr;
        }
        break;
      case WV_PINDICES:
        gp->npIndex  = items[i].dataLen;
        gp->pIndices = (int *) items[i].dataPtr;
        break;
      case WV_LINDICES:
        gp->nlIndex  = items[i].dataLen;
        gp->lIndices = (int *) items[i].dataPtr;
        break;
      case WV_PCOLOR:
        gp->pColor[0] = items[i].data[0];
        gp->pColor[1] = items[i].data[1];
        gp->pColor[2] = items[i].data[2];
        break;
      case WV_LCOLOR:
        gp->lColor[0] = items[i].data[0];
        gp->lColor[1] = items[i].data[1];
        gp->lColor[2] = items[i].data[2];
        break;
      case WV_BCOLOR:
        gp->bColor[0] = items[i].data[0];
        gp->bColor[1] = items[i].data[1];
        gp->bColor[2] = items[i].data[2];
        break;
    }
  }
  /* do we have anything? */
  if ((gp->nVerts == 0) || (gp->vertices == NULL)) {
    wv_free(nam);
    return -5;
  }

  /* do we need to compute normals? */
  norm = NULL;
  if ((gp->gtype == WV_TRIANGLE) && (gp->normals == NULL) &&
      (sqrtf(gp->normal[0]*gp->normal[0] + gp->normal[1]*gp->normal[1] +
             gp->normal[2]*gp->normal[2]) == 0.0)) {
    norm = (float *) wv_alloc(3*gp->nVerts*sizeof(float));
    if (norm == NULL) {
      wv_free(nam);
      return -1;
    }
    cnt = NULL;
    if (gp->indices != NULL) {
      cnt = (int *) wv_alloc(gp->nVerts*sizeof(int));
      if (cnt == NULL) {
        wv_free(norm);
        wv_free(nam);
        return -1;
      }
    }
    wv_computeNormals(cntxt->bias, gp->nVerts, gp->vertices, 
                      gp->nIndex, gp->indices, norm, cnt);
    if (cnt != NULL) wv_free(cnt);
    gp->normals = norm;
  }
  
  /* make the stripes */
  i = wv_makeStripes(gp, cntxt->bias);
  if (i != 0) {
    if (norm != NULL) wv_free(gp->normals);
    wv_free(nam);
    return i;
  }

  gp->name    = nam;
  gp->nameLen = nameLen;
  /* clean up our used items */
  for (i = 0; i < nItems; i++) {
    items[i].dataType = 0;
    items[i].dataLen  = 0;
    items[i].dataPtr  = NULL;
  }
  
  while (cntxt->ioAccess != 0) usleep(10000);
  cntxt->dataAccess = 1;
  cntxt->nGPrim += 1;
  cntxt->dataAccess = 0;

  return cntxt->nGPrim-1;
}


static void
wv_triNorms(float *verts, float *norms)
{
  float dis, xnor, ynor, znor, x1, y1, z1, x2, y2, z2, x3, y3, z3;

  x1   = verts[0];
  y1   = verts[1];
  z1   = verts[2];
  x2   = verts[3];
  y2   = verts[4];
  z2   = verts[5];
  x3   = verts[6];
  y3   = verts[7];
  z3   = verts[8];
  xnor = (y3-y1)*(z2-z1)-(y2-y1)*(z3-z1);
  ynor = (z3-z1)*(x2-x1)-(x3-x1)*(z2-z1);
  znor = (x3-x1)*(y2-y1)-(y3-y1)*(x2-x1);
  dis  = sqrtf(xnor*xnor + ynor*ynor + znor*znor);
  if (dis != 0.0f) {
    norms[0] = xnor/dis;
    norms[1] = ynor/dis;
    norms[2] = znor/dis;
    norms[3] = xnor/dis;
    norms[4] = ynor/dis;
    norms[5] = znor/dis;
    norms[6] = xnor/dis;
    norms[7] = ynor/dis;
    norms[8] = znor/dis;
  } else {
    norms[0] = 0.0;
    norms[1] = 0.0;
    norms[2] = 0.0;
    norms[3] = 0.0;
    norms[4] = 0.0;
    norms[5] = 0.0;
    norms[6] = 0.0;
    norms[7] = 0.0;
    norms[8] = 0.0;
  }
}


int
wv_addArrowHeads(wvContext *cntxt, int index, float size, 
                 int nHeads, int *heads)
{
  int     i, j, nh;
  float   hpt[3], tpt[3], sprd, spread = 0.20;
  float   axn[3], ayn[3], azn[3], base[3], dis, *norm;
  wvGPrim *gp;

  if ((index >= cntxt->nGPrim) || (index < 0)) return -3;
  if (cntxt->gPrims == NULL)  return -3;
  if (nHeads        <= 0)     return -3;
  gp = &cntxt->gPrims[index];
  if (gp->gtype   != WV_LINE) return -4;
  if (gp->normals != NULL)    return -4;
  
  if (gp->indices == NULL) {
    for (i = 0; i < nHeads; i++)
      if ((heads[i] == 0) || (abs(heads[i]) > gp->nVerts/2)) return -3;
  } else {
    for (i = 0; i < nHeads; i++)
      if ((heads[i] == 0) || (abs(heads[i]) > gp->nIndex/2)) return -3;
  }
  
  norm = (float *) wv_alloc(12*6*nHeads*sizeof(float));
  if (norm == NULL) return -1;
  
  for (nh = i = 0; i < nHeads; i++) {
    for (j = 0; j < 36; j++) norm[36*i+j] = norm[36*i+j+36*nHeads] = 0.0;
    if (gp->indices == NULL) {
      if (heads[i] > 0) {
        tpt[0] = gp->vertices[6*( heads[i]-1)  ];
        tpt[1] = gp->vertices[6*( heads[i]-1)+1];
        tpt[2] = gp->vertices[6*( heads[i]-1)+2];
        hpt[0] = gp->vertices[6*( heads[i]-1)+3];
        hpt[1] = gp->vertices[6*( heads[i]-1)+4];
        hpt[2] = gp->vertices[6*( heads[i]-1)+5];
      } else {
        hpt[0] = gp->vertices[6*(-heads[i]-1)  ];
        hpt[1] = gp->vertices[6*(-heads[i]-1)+1];
        hpt[2] = gp->vertices[6*(-heads[i]-1)+2];
        tpt[0] = gp->vertices[6*(-heads[i]-1)+3];
        tpt[1] = gp->vertices[6*(-heads[i]-1)+4];
        tpt[2] = gp->vertices[6*(-heads[i]-1)+5];
      }
    } else {
      if (heads[i] > 0) {
        j      = gp->indices[2*heads[i]-2] - cntxt->bias;
        tpt[0] = gp->vertices[3*j  ];
        tpt[1] = gp->vertices[3*j+1];
        tpt[2] = gp->vertices[3*j+2];
        j      = gp->indices[2*heads[i]-1] - cntxt->bias;
        hpt[0] = gp->vertices[3*j  ];
        hpt[1] = gp->vertices[3*j+1];
        hpt[2] = gp->vertices[3*j+2];
      } else {
        j      = gp->indices[-2*heads[i]-2] - cntxt->bias;
        hpt[0] = gp->vertices[3*j  ];
        hpt[1] = gp->vertices[3*j+1];
        hpt[2] = gp->vertices[3*j+2];
        j      = gp->indices[-2*heads[i]-1] - cntxt->bias;
        tpt[0] = gp->vertices[3*j  ];
        tpt[1] = gp->vertices[3*j+1];
        tpt[2] = gp->vertices[3*j+2];
      }
    }  
    azn[0] = hpt[0] - tpt[0];
    azn[1] = hpt[1] - tpt[1];
    azn[2] = hpt[2] - tpt[2];
    dis = sqrtf(azn[0]*azn[0] + azn[1]*azn[1] + azn[2]*azn[2]);
    if (dis == 0.0f) continue;
    azn[0] = azn[0]/dis;
    azn[1] = azn[1]/dis;
    azn[2] = azn[2]/dis;

    base[0] = hpt[0] - size*azn[0];
    base[1] = hpt[1] - size*azn[1];
    base[2] = hpt[2] - size*azn[2];

    ayn[0] = 1.0;
    ayn[1] = ayn[2] = 0.0;
    if ((azn[0] < -0.65) || (azn[0] > 0.65)) {
      ayn[0] = 0.0;
      ayn[1] = 1.0;
    }
    axn[0] = ayn[1]*azn[2] - azn[1]*ayn[2];
    axn[1] = ayn[2]*azn[0] - azn[2]*ayn[0];
    axn[2] = ayn[0]*azn[1] - azn[0]*ayn[1];
    ayn[0] = axn[1]*azn[2] - azn[1]*axn[2];
    ayn[1] = axn[2]*azn[0] - azn[2]*axn[0];
    ayn[2] = axn[0]*azn[1] - azn[0]*axn[1];

    sprd = size*spread;
    
    norm[36*i   ] = hpt[0];
    norm[36*i+ 1] = hpt[1];
    norm[36*i+ 2] = hpt[2];
    norm[36*i+ 3] = base[0] - sprd*axn[0];
    norm[36*i+ 4] = base[1] - sprd*axn[1];
    norm[36*i+ 5] = base[2] - sprd*axn[2];
    norm[36*i+ 6] = base[0] + sprd*ayn[0];
    norm[36*i+ 7] = base[1] + sprd*ayn[1];
    norm[36*i+ 8] = base[2] + sprd*ayn[2];
    wv_triNorms(&norm[36*i  ], &norm[36*i+36*nHeads   ]);
    norm[36*i+ 9] = hpt[0];
    norm[36*i+10] = hpt[1];
    norm[36*i+11] = hpt[2];
    norm[36*i+12] = base[0] + sprd*ayn[0];
    norm[36*i+13] = base[1] + sprd*ayn[1];
    norm[36*i+14] = base[2] + sprd*ayn[2];
    norm[36*i+15] = base[0] + sprd*axn[0];
    norm[36*i+16] = base[1] + sprd*axn[1];
    norm[36*i+17] = base[2] + sprd*axn[2];
    wv_triNorms(&norm[36*i+ 9], &norm[36*i+36*nHeads+ 9]);
    norm[36*i+18] = hpt[0];
    norm[36*i+19] = hpt[1];
    norm[36*i+20] = hpt[2];
    norm[36*i+21] = base[0] + sprd*axn[0];
    norm[36*i+22] = base[1] + sprd*axn[1];
    norm[36*i+23] = base[2] + sprd*axn[2];
    norm[36*i+24] = base[0] - sprd*ayn[0];
    norm[36*i+25] = base[1] - sprd*ayn[1];
    norm[36*i+26] = base[2] - sprd*ayn[2];
    wv_triNorms(&norm[36*i+18], &norm[36*i+36*nHeads+18]);
    norm[36*i+27] = hpt[0];
    norm[36*i+28] = hpt[1];
    norm[36*i+29] = hpt[2];
    norm[36*i+30] = base[0] - sprd*ayn[0];
    norm[36*i+31] = base[1] - sprd*ayn[1];
    norm[36*i+32] = base[2] - sprd*ayn[2];
    norm[36*i+33] = base[0] - sprd*axn[0];
    norm[36*i+34] = base[1] - sprd*axn[1];
    norm[36*i+35] = base[2] - sprd*axn[2];
    wv_triNorms(&norm[36*i+27], &norm[36*i+36*nHeads+27]);

  }

  gp->normals = norm;
  if (gp->updateFlg != WV_PCOLOR) gp->updateFlg |= WV_NORMALS;
  gp->nlIndex = 12*2*nHeads;
  return 0;
}


int
wv_modGPrim(wvContext *cntxt, int index, int nItems, wvData *items)
{
  int     i, type, vlen, *cnt, norm = 0;
  float   *normals;
  wvGPrim *gp;

  if ((index >= cntxt->nGPrim) || (index < 0)) return -3;
  if (cntxt->gPrims == NULL) return -3;
  gp   = &cntxt->gPrims[index];
  vlen = -1;
  for (i = 0; i < nItems; i++)
    if (items[i].dataType == WV_VERTICES) vlen = items[i].dataLen;
  if (vlen == -1) {
    vlen = gp->nVerts;
    if (gp->normals != NULL) norm = 1;
  } else {
    /* if we had stuff it better be replaced! */
    if (gp->colors != NULL) {
      for (i = 0; i < nItems; i++)
        if (items[i].dataType == WV_COLORS) {
          if (items[i].dataLen != vlen) return -4;
          break;
        }
      if (i == nItems) return -4;
    }
    if (gp->normals != NULL) {
      for (i = 0; i < nItems; i++)
        if (items[i].dataType == WV_NORMALS) {
          if (items[i].dataLen != vlen) return -4;
          norm = 1;
          break;
        }
    }
    if (gp->colors  != NULL) wv_free(gp->colors);
    if (gp->normals != NULL) wv_free(gp->normals);
    gp->colors  = NULL;
    gp->normals = NULL;
  }
  
  while (cntxt->ioAccess != 0) usleep(10000);
  cntxt->dataAccess = 1;

  gp->updateFlg = 0;
  for (i = 0; i < nItems; i++) {
    if (items[i].dataLen == 1) continue;
    type = items[i].dataType;
    gp->updateFlg |= type;
    switch (type) {
      case WV_VERTICES:
        wv_free(gp->vertices);
        gp->nVerts   = vlen;
        gp->vertices = (float *) items[i].dataPtr;
        break;
      case WV_INDICES:
        wv_free(gp->indices);
        gp->nIndex  = items[i].dataLen;
        gp->indices = (int *) items[i].dataPtr;
        break;
      case WV_COLORS:
        gp->colors = (unsigned char *) items[i].dataPtr;
        break;
      case WV_NORMALS:
        gp->normals = (float *) items[i].dataPtr;
        break;
      case WV_PINDICES:
        wv_free(gp->pIndices);
        gp->npIndex  = items[i].dataLen;
        gp->pIndices = (int *) items[i].dataPtr;
        break;
      case WV_LINDICES:
        wv_free(gp->lIndices);
        gp->nlIndex  = items[i].dataLen;
        gp->lIndices = (int *) items[i].dataPtr;
        break;
    }
  }
  
  /* compute new normals? */
  if ((gp->gtype == WV_TRIANGLE) && ((gp->updateFlg&WV_VERTICES) != 0) &&
      (sqrtf(gp->normal[0]*gp->normal[0] + gp->normal[1]*gp->normal[1] +
             gp->normal[2]*gp->normal[2]) == 0.0) && (norm == 0)) {
    normals = (float *) wv_alloc(3*gp->nVerts*sizeof(float));
    if (normals == NULL) return -1;
    cnt = NULL;
    if (gp->indices != NULL) {
      cnt = (int *) wv_alloc(gp->nVerts*sizeof(int));
      if (cnt == NULL) {
        wv_free(normals);
        return -1;
      }
    }
    wv_computeNormals(cntxt->bias, gp->nVerts, gp->vertices, 
                      gp->nIndex, gp->indices, normals, cnt);
    if (cnt != NULL) wv_free(cnt);
    gp->updateFlg |= WV_NORMALS;
    gp->normals    = normals;
  }

  /* remake the stripes */
  for (i = 0; i < gp->nStripe; i++)
    wv_freeStripe(gp->stripes[i], gp->nStripe);
  wv_free(gp->stripes);

  i = wv_makeStripes(gp, cntxt->bias);
  if (i != 0) return i;
  
  /* clean up our used items */
  for (i = 0; i < nItems; i++) {
    items[i].dataType = 0;
    items[i].dataLen  = 0;
    items[i].dataPtr  = NULL;
  }
  cntxt->dataAccess = 0;

  return index;
}


void
wv_removeGPrim(wvContext *cntxt, int index)
{
  if ((index >= cntxt->nGPrim) || (index < 0)) return;
  if (cntxt->gPrims == NULL) return;
  
  while (cntxt->ioAccess != 0) usleep(10000);
  cntxt->dataAccess = 1;
  cntxt->gPrims[index].updateFlg = WV_DELETE;
  cntxt->dataAccess = 0;
}


static void
wv_writeBuf(struct libwebsocket *wsi, unsigned char *buf, 
            int npack, int *iBuf)
{
  if (*iBuf+npack <= BUFLEN-4) return;
  
  printf("  iBuf = %d, BUFLEN = %d\n", *iBuf, BUFLEN);
  buf[*iBuf  ] = 0;
  buf[*iBuf+1] = 0;
  buf[*iBuf+2] = 0;
  buf[*iBuf+3] = 0;             /* continue opcode */
  *iBuf += 4;
  if (libwebsocket_write(wsi, buf, *iBuf, LWS_WRITE_BINARY) < 0) 
    fprintf(stderr, "ERROR writing to socket");   
  *iBuf = 0;
}


static void
wv_writeGPrim(wvGPrim *gp, struct libwebsocket *wsi, unsigned char *buf, 
              int *iBuf)
{
  int            i, j, n, npack, i4;
  unsigned char  vflag;
  unsigned char  *c1 = (unsigned char *)  &i4;  
  
  for (i = 0; i < gp->nStripe; i++) {
    npack = 12+gp->nameLen;
    vflag = WV_VERTICES;
    if ((gp->stripes[i].nsVerts  == 0) || 
        (gp->stripes[i].vertices == NULL)) continue;
    npack += 3*4*gp->stripes[i].nsVerts;
    if ((gp->stripes[i].nsIndices != 0) && 
        (gp->stripes[i].sIndice2   != NULL)) {
      npack += 2*gp->stripes[i].nsIndices + 4;
      if ((gp->stripes[i].nsIndices%2) != 0) npack += 2;
      vflag |= WV_INDICES;
    }
    if (gp->stripes[i].colors != NULL) {
      npack += 3*gp->stripes[i].nsVerts + 4;
      if (((3*gp->stripes[i].nsVerts)%4) != 0)
         npack += 4 - (3*gp->stripes[i].nsVerts)%4;
      vflag |= WV_COLORS;
    }
    if (gp->stripes[i].normals != NULL) {
      npack += 3*4*gp->stripes[i].nsVerts + 4;
      vflag |= WV_NORMALS;
    }
    if ((gp->gtype == WV_LINE) && (gp->normals != NULL) && (i == 0)) {
      npack += 3*4*gp->nlIndex + 4;
      vflag |= WV_NORMALS;
    }
    wv_writeBuf(wsi, buf, npack, iBuf);
    if (npack > BUFLEN) {
      printf(" Oops! npack = %d  BUFLEN = %d\n", npack, BUFLEN);
      exit(1);
    }

    n     = *iBuf;
    i4    = i;
    c1[3] = 3;                          /* new data opcode */
    memcpy(&buf[n], c1, 4);
    n    += 4;
    i4    = gp->nameLen;
    c1[2] = vflag;
    c1[3] = gp->gtype;
    memcpy(&buf[n], c1, 4);
    memcpy(&buf[n+4], gp->name, gp->nameLen);
    n += 4+gp->nameLen;
    i4 = 3*gp->stripes[i].nsVerts;
    memcpy(&buf[n], &i4, 4);
    n += 4;
    memcpy(&buf[n], gp->stripes[i].vertices, 3*4*gp->stripes[i].nsVerts);
    n += 3*4*gp->stripes[i].nsVerts;
    if ((gp->stripes[i].nsIndices != 0) && 
        (gp->stripes[i].sIndice2  != NULL)) {
      i4 = gp->stripes[i].nsIndices;
      memcpy(&buf[n], &i4, 4);
      n += 4;      
      memcpy(&buf[n], gp->stripes[i].sIndice2, 2*gp->stripes[i].nsIndices);
      n += 2*gp->stripes[i].nsIndices;
      if ((gp->stripes[i].nsIndices%2) != 0) {
        i4 = 0;
        memcpy(&buf[n], &i4, 2);
        n += 2;
      }
    }
    if (gp->stripes[i].colors != NULL) {
      i4 = 3*gp->stripes[i].nsVerts;
      memcpy(&buf[n], &i4, 4);
      n += 4;
      memcpy(&buf[n], gp->stripes[i].colors, 3*gp->stripes[i].nsVerts);
      n += 3*gp->stripes[i].nsVerts;
      if (((3*gp->stripes[i].nsVerts)%4) != 0) {
        j  = 4 - (3*gp->stripes[i].nsVerts)%4;
        i4 = 0;
        memcpy(&buf[n], &i4, j);
        n += j;
      }
    }
    if (gp->stripes[i].normals != NULL) {
      i4 = 3*gp->stripes[i].nsVerts;
      memcpy(&buf[n], &i4, 4);
      n += 4;
      memcpy(&buf[n], gp->stripes[i].normals, 3*4*gp->stripes[i].nsVerts);
      n += 3*4*gp->stripes[i].nsVerts;
    }
    /* line decorations -- no stripes */
    if ((gp->gtype == WV_LINE) && (gp->normals != NULL) && (i == 0)) {
      i4 = 3*gp->nlIndex;
      memcpy(&buf[n], &i4, 4);
      n += 4;
      memcpy(&buf[n], gp->normals, 3*4*gp->nlIndex);
      n += 3*4*gp->nlIndex;
    }    
    *iBuf += npack;
    
    if ((gp->stripes[i].npIndices != 0) &&
        (gp->stripes[i].pIndice2  != NULL)) {
      npack = 12+gp->nameLen + 2*gp->stripes[i].npIndices;
      if ((gp->stripes[i].npIndices%2) != 0) npack += 2;
      wv_writeBuf(wsi, buf, npack, iBuf);
      n     = *iBuf;
      i4    = i;
      c1[3] = 3;                        /* new data opcode */
      memcpy(&buf[n], c1, 4);
      n    += 4;
      i4    = gp->nameLen;
      c1[2] = WV_INDICES;
      c1[3] = 0;                        /* local gtype */
      memcpy(&buf[n], c1, 4);
      memcpy(&buf[n+4], gp->name, gp->nameLen);
      n += 4+gp->nameLen;
      i4 = gp->stripes[i].npIndices;
      memcpy(&buf[n], &i4, 4);
      n += 4; 
      memcpy(&buf[n], gp->stripes[i].pIndice2, 2*gp->stripes[i].npIndices);
      if ((gp->stripes[i].npIndices%2) != 0) {
        i4 = 0;
        memcpy(&buf[n], &i4, 2);
        n += 2;
      }
      *iBuf += npack;
    }

    if ((gp->stripes[i].nlIndices != 0) &&
        (gp->stripes[i].lIndice2  != NULL)) {
      npack = 12+gp->nameLen + 2*gp->stripes[i].nlIndices;
      if ((gp->stripes[i].nlIndices%2) != 0) npack += 2;
      wv_writeBuf(wsi, buf, npack, iBuf);
      n     = *iBuf;
      i4    = i;
      c1[3] = 3;                        /* new data opcode */
      memcpy(&buf[n], c1, 4);
      n    += 4;
      i4    = gp->nameLen;
      c1[2] = WV_INDICES;
      c1[3] = 1;                        /* local gtype */
      memcpy(&buf[n], c1, 4);
      memcpy(&buf[n+4], gp->name, gp->nameLen);
      n += 4+gp->nameLen;
      i4 = gp->stripes[i].nlIndices;
      memcpy(&buf[n], &i4, 4);
      n += 4; 
      memcpy(&buf[n], gp->stripes[i].lIndice2, 2*gp->stripes[i].nlIndices);
      if ((gp->stripes[i].nlIndices%2) != 0) {
        i4 = 0;
        memcpy(&buf[n], &i4, 2);
        n += 2;
      }
      *iBuf += npack;
    }

  }
}


void
wv_sendGPrim(struct libwebsocket *wsi, wvContext *cntxt, 
             unsigned char *xbuf, int flag)
{
  int            i, j, k, iBuf, npack, i4;
  unsigned short *s2 = (unsigned short *) &i4;
  unsigned char  *c1 = (unsigned char *)  &i4;
  wvGPrim        *gp;
  unsigned char  *buf = &xbuf[LWS_SEND_BUFFER_PRE_PADDING];
  
  /* init message */
  if (flag == 1) {
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 8;                         /* init opcode */
    memcpy(&buf[ 4], &cntxt->fov,     4);
    memcpy(&buf[ 8], &cntxt->zNear,   4);
    memcpy(&buf[12], &cntxt->zFar,    4);
    memcpy(&buf[16],  cntxt->eye,    12);
    memcpy(&buf[28],  cntxt->center, 12);
    memcpy(&buf[40],  cntxt->up,     12);
    /* end of frame marker */
    buf[52] = 0;
    buf[53] = 0;
    buf[54] = 0;
    buf[55] = 7;                        /* eof opcode */
    if (libwebsocket_write(wsi, buf, 56, LWS_WRITE_BINARY) < 0) 
      fprintf(stderr, "ERROR writing to socket");
    return;
  }
  if (cntxt->gPrims == NULL) return;

  /* any changes? */
  if ((flag == 0) && (cntxt->cleanAll == 0)) {
    for (i = 0; i < cntxt->nGPrim; i++) {
      gp = &cntxt->gPrims[i];
      if (gp->updateFlg != 0) break;
    }
    if (i == cntxt->nGPrim) return;
  }
  
  /* put out the new data*/

  iBuf = 0;
  if (cntxt->cleanAll != 0) {
    npack = 8;
    wv_writeBuf(wsi, buf, npack, &iBuf);
    buf[iBuf  ] = 0;
    buf[iBuf+1] = 0;
    buf[iBuf+2] = 0;
    buf[iBuf+3] = 2;		/* delete opcode for all */
    buf[iBuf+4] = 0;
    buf[iBuf+5] = 0;
    buf[iBuf+6] = 0;
    buf[iBuf+7] = 0;
    iBuf += npack;
    cntxt->cleanAll = 0;
  }

  for (i = 0; i < cntxt->nGPrim; i++) {
    gp = &cntxt->gPrims[i];
    if ((gp->updateFlg == 0) && (flag != -1)) continue;  
    if ((gp->updateFlg == WV_DELETE) && (flag == -1)) continue;
/*  printf(" flag = %d,  %d  gp->updateFlg = %d\n", 
           flag, i, gp->updateFlg); */
    if  (gp->updateFlg == WV_DELETE) {
    
      /* delete the gPrim */
      npack = 8 + gp->nameLen;
      wv_writeBuf(wsi, buf, npack, &iBuf);
      buf[iBuf  ] = 0;
      buf[iBuf+1] = 0;
      buf[iBuf+2] = 0;
      buf[iBuf+3] = 2;                  /* delete opcode */
      s2[0]       = gp->nameLen;
      s2[1]       = 0;
      memcpy(&buf[iBuf+4], s2,       4);
      memcpy(&buf[iBuf+8], gp->name, gp->nameLen);
      iBuf += npack;
      gp->updateFlg |= WV_DONE;

    } else if ((gp->updateFlg == WV_PCOLOR) || (flag == -1)) {
    
      /* new gPrim */
      npack = 8 + gp->nameLen + 16;
      if (gp->gtype > 0) npack += 16;
      if (gp->gtype > 1) npack += 36;
      wv_writeBuf(wsi, buf, npack, &iBuf);
      i4    = gp->nStripe;
      c1[3] = 1;                        /* new opcode */
      memcpy(&buf[iBuf  ], c1, 4);
      i4    = gp->nameLen;
      c1[2] = 0;
      c1[3] = gp->gtype;
      memcpy(&buf[iBuf+4], c1, 4);
      memcpy(&buf[iBuf+8], gp->name, gp->nameLen);
      iBuf += 8 + gp->nameLen;
      memcpy(&buf[iBuf  ], &gp->attrs,   4);
      iBuf += 4;
      memcpy(&buf[iBuf  ], &gp->pSize,   4);
      memcpy(&buf[iBuf+4],  gp->pColor, 12);
      iBuf += 16;
      if (gp->gtype > 0) {
        memcpy(&buf[iBuf   ], &gp->lWidth,  4);
        memcpy(&buf[iBuf+ 4],  gp->lColor, 12);
        memcpy(&buf[iBuf+16],  gp->fColor, 12);
        memcpy(&buf[iBuf+28],  gp->bColor, 12);
        iBuf += 40;
      }
      if (gp->gtype > 1) {
        memcpy(&buf[iBuf], gp->normal, 12);
        iBuf += 12;
      }
      wv_writeGPrim(gp, wsi, buf, &iBuf); 

    } else {
    
      /* updated gPrim */

      if ((gp->updateFlg&WV_VERTICES) != 0)
        for (j = 0; j < gp->nStripe; j++) {
          if ((gp->stripes[j].nsVerts  == 0) || 
              (gp->stripes[j].vertices == NULL)) continue;
          npack = 12 + gp->nameLen + 3*4*gp->stripes[j].nsVerts;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = j;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_VERTICES;
          c1[3] = gp->gtype;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = 3*gp->stripes[j].nsVerts;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].vertices,
                 3*4*gp->stripes[j].nsVerts);
          iBuf += npack;
        }

      if ((gp->updateFlg&WV_INDICES) != 0)
        for (j = 0; j < gp->nStripe; j++) {
          if ((gp->stripes[j].nsIndices == 0) || 
              (gp->stripes[j].sIndice2  == NULL)) continue;
          npack = 12 + gp->nameLen + 2*gp->stripes[j].nsIndices;
          if ((gp->stripes[j].nsIndices%2) != 0) npack += 2;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = j;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_INDICES;
          c1[3] = gp->gtype;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = gp->stripes[j].nsIndices;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].sIndice2,
                 2*gp->stripes[j].nsIndices);
          if ((gp->stripes[j].nsIndices%2) != 0) {
            i4 = 0;
            memcpy(&buf[iBuf+npack-2], &i4, 2);
          }
          iBuf += npack;
        }

      if ((gp->updateFlg&WV_COLORS) != 0)
        for (j = 0; j < gp->nStripe; j++) {
          if ((gp->stripes[j].nsVerts == 0) || 
              (gp->stripes[j].colors  == NULL)) continue;
          npack = 12 + gp->nameLen + 3*gp->stripes[j].nsVerts;
          if (((3*gp->stripes[j].nsVerts)%4) != 0)
            npack += 4 - (3*gp->stripes[j].nsVerts)%4;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = j;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_COLORS;
          c1[3] = gp->gtype;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = 3*gp->stripes[j].nsVerts;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].colors,
                 3*gp->stripes[j].nsVerts);
          if (((3*gp->stripes[j].nsVerts)%4) != 0) {
            k  = 4 - (3*gp->stripes[j].nsVerts)%4;
            i4 = 0;
            memcpy(&buf[iBuf+npack-k], &i4, k);
          }
          iBuf += npack;
        }
        
      if ((gp->updateFlg&WV_NORMALS) != 0)
        if (gp->gtype == WV_TRIANGLE) {
          for (j = 0; j < gp->nStripe; j++) {
            if ((gp->stripes[j].nsVerts  == 0) || 
                (gp->stripes[j].vertices == NULL)) continue;
            npack = 12 + gp->nameLen + 3*4*gp->stripes[j].nsVerts;
            wv_writeBuf(wsi, buf, npack, &iBuf);
            i4    = j;
            c1[3] = 4;                      /* edit opcode */
            memcpy(&buf[iBuf   ], c1, 4);
            i4    = gp->nameLen;
            c1[2] = WV_NORMALS;
            c1[3] = gp->gtype;
            memcpy(&buf[iBuf+ 4], c1, 4);
            memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
            i4    = 3*gp->stripes[j].nsVerts;
            memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
            memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].normals,
                   3*4*gp->stripes[j].nsVerts);
            iBuf += npack;
          }
        } else if (gp->gtype == WV_LINE) {
          /* line decorations */
          npack = 12 + gp->nameLen + 3*4*gp->nlIndex;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = 0;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_NORMALS;
          c1[3] = gp->gtype;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = 3*gp->nlIndex;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->normals, 3*4*gp->nlIndex);
          iBuf += npack;
        }
        
      if ((gp->updateFlg&WV_PINDICES) != 0)
        for (j = 0; j < gp->nStripe; j++) {
          if ((gp->stripes[j].npIndices == 0) || 
              (gp->stripes[j].pIndice2  == NULL)) continue;
          npack = 12 + gp->nameLen + 2*gp->stripes[j].npIndices;
          if ((gp->stripes[j].npIndices%2) != 0) npack += 2;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = j;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_INDICES;
          c1[3] = 0;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = gp->stripes[j].npIndices;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].pIndice2,
                 2*gp->stripes[j].npIndices);
          if ((gp->stripes[j].npIndices%2) != 0) {
            i4 = 0;
            memcpy(&buf[iBuf+npack-2], &i4, 2);
          }
          iBuf += npack;
        }

      if ((gp->updateFlg&WV_LINDICES) != 0)
        for (j = 0; j < gp->nStripe; j++) {
          if ((gp->stripes[j].nlIndices == 0) || 
              (gp->stripes[j].lIndice2  == NULL)) continue;
          npack = 12 + gp->nameLen + 2*gp->stripes[j].nlIndices;
          if ((gp->stripes[j].nlIndices%2) != 0) npack += 2;
          wv_writeBuf(wsi, buf, npack, &iBuf);
          i4    = j;
          c1[3] = 4;                        /* edit opcode */
          memcpy(&buf[iBuf   ], c1, 4);
          i4    = gp->nameLen;
          c1[2] = WV_INDICES;
          c1[3] = 1;
          memcpy(&buf[iBuf+ 4], c1, 4);
          memcpy(&buf[iBuf+ 8], gp->name, gp->nameLen);
          i4    = gp->stripes[j].nlIndices;
          memcpy(&buf[iBuf+ 8+gp->nameLen], &i4, 4);
          memcpy(&buf[iBuf+12+gp->nameLen], gp->stripes[j].lIndice2,
                 2*gp->stripes[j].nlIndices);
          if ((gp->stripes[j].nlIndices%2) != 0) {
            i4 = 0;
            memcpy(&buf[iBuf+npack-2], &i4, 2);
          }
          iBuf += npack;
        }

    }

  }
  /* end of frame marker */
  buf[iBuf  ] = 0;
  buf[iBuf+1] = 0;
  buf[iBuf+2] = 0;
  buf[iBuf+3] = 7;                      /* eof opcode */
  iBuf += 4;
  if (libwebsocket_write(wsi, buf, iBuf, LWS_WRITE_BINARY) < 0) 
    fprintf(stderr, "ERROR writing to socket");   

}
