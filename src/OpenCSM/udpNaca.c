/*
 ************************************************************************
 *                                                                      *
 * udpNaca -- udp file to generate a NACAmptt airfoil                   *
 *                                                                      *
 *            Written by John Dannenhoffer @ Syracuse University        *
 *            Patterned after code written by Bob Haimes  @ MIT         *
 *                                                                      *
 ************************************************************************
 */

#ifdef GEOM_EGADS

/*
 * Copyright (C) 2011  John F. Dannenhoffer, III (Syracuse University)
 *
 * This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 *    License along with this library; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *     MA  02110-1301  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef WIN32
    #define snprintf _snprintf
#endif

#include "egads.h"

/* ADDITIONAL ATTRIBUTE TYPE */
#define ATTRREALSEN 4

#define PI    3.1415926535897931159979635
#define TWOPI 6.2831853071795862319959269

/* static variables used to hold info between function calls */

/* number of Naca UDPs */
static int    numUdp  = 0;

/* arrays to hold info for each of the UDPs */
static ego    *ebodys   = NULL;
static int    *Series   = NULL;


/*
 ************************************************************************
 *                                                                      *
 *   udpErrorStr - print error string                                   *
 *                                                                      *
 ************************************************************************
 */

static /*@null@*/ char *
udpErrorStr(int stat)
{
    char *string;

    string = EG_alloc(25*sizeof(char));
    if (string == NULL) {
        return string;
    }
    snprintf(string, 25, "EGADS status = %d", stat);

    return string;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpInitialize - initialize and get info about the list of arguments*
 *                                                                      *
 ************************************************************************
 */

int
udpInitialize(int    *nArgs,
              char   ***namex,
              int    **typex,
              int    **idefaulx,
              double **ddefaulx)
{
    int    *type, *idefault;
    char   **name;
    double *ddefault;

    /* make the arrays */
    ebodys = (ego    *) EG_alloc(sizeof(ego    ));
    Series = (int    *) EG_alloc(sizeof(int    ));

    if (ebodys == NULL || Series == NULL) {
        return EGADS_MALLOC;
    }

    /* initialize the array elements that will hold the "current" settings */
    ebodys[0] = NULL;
    Series[0] = 0012;

    /* set up returns that describe the UDP */
    *nArgs    = 1;
    *namex    = NULL;
    *typex    = NULL;
    *idefaulx = NULL;
    *ddefaulx = NULL;

    *namex = name = (char **) EG_alloc((*nArgs)*sizeof(char *));
    if (*namex == NULL) {
        return EGADS_MALLOC;
    }

    *typex = type = (int *) EG_alloc((*nArgs)*sizeof(int));
    if (*typex == NULL) {
        EG_free(*namex);
        return EGADS_MALLOC;
    }

    *idefaulx = idefault = (int *) EG_alloc((*nArgs)*sizeof(int));
    if (*idefaulx == NULL) {
        EG_free(*typex);
        EG_free(*namex);
        return EGADS_MALLOC;
    }

    *ddefaulx = ddefault = (double *) EG_alloc((*nArgs)*sizeof(double));
    if (*ddefaulx == NULL) {
        EG_free(*idefaulx);
        EG_free(*typex);
        EG_free(*namex);
        return EGADS_MALLOC;
    }

    name[0]     = "Series";
    type[0]     = ATTRINT;
    idefault[0] = 0012;
    ddefault[0] = 0.0;

    return EGADS_SUCCESS;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpReset - reset the arguments to their defaults                   *
 *                                                                      *
 ************************************************************************
 */

int
udpReset(int flag)
{
    int   iudp;

    /* reset the "current" settings */
    if (flag == 0) {
        Series[0] = 0012;

    /* called when closing up */
    } else {
        for (iudp = 0; iudp <= numUdp; iudp++) {
            if (ebodys[iudp] != NULL) {
                EG_deleteObject(ebodys[iudp]);
                ebodys[iudp] = NULL;
            }
        }

        EG_free(ebodys); ebodys = NULL;
        EG_free(Series); Series = NULL;

        numUdp = 0;
    }

    return EGADS_SUCCESS;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpSet - set an argument                                           *
 *                                                                      *
 ************************************************************************
 */

int
udpSet(char *name,
       char *value)
{
    int  len;
    char *pEnd;

    if (name  == NULL) {
        return EGADS_NONAME;
    }
    if (value == NULL) {
        return EGADS_NULLOBJ;
    }

    len = strlen(value);
    if (len == 0) {
        return EGADS_NODATA;
    }

    if (strcmp(name, "Series") == 0) {
        Series[0] = strtol(value, &pEnd, 10);
        if (Series[0] <= 0) {
            printf(" udpSet: Series = %d -- reset to 0012\n", Series[0]);
            Series[0] = 12;
        }

    } else {
        printf(" udpSet: Parameter %s not known\n", name);
        return EGADS_INDEXERR;

    }

    return EGADS_SUCCESS;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpExecute - execute the primitive                                 *
 *                                                                      *
 ************************************************************************
 */

int
udpExecute(ego  context,
           ego  *ebody,
           int  *nMesh,
           char **string)
{
    int     status = EGADS_SUCCESS;

    int     npts, i, ii, jj, iter, niter, sense[3], header[4];
    double  m, p, t, zeta, xx, yt, yc, theta, *pts=NULL, *cp=NULL;
    double  data[18], tdata[2], dxymax, dx, dy, du, result[3], area;
    double  dxytol = 1.0e-6;
    ego     enodes[4], eedges[3], ecurve, eline, eloop, eface;

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* increment number of UDPs */
    numUdp++;

    /* increase the arrays to make room for the new UDP */
    ebodys = (ego    *) EG_reall(ebodys, (numUdp+1)*sizeof(ego    ));
    Series = (int    *) EG_reall(Series, (numUdp+1)*sizeof(int    ));

    if (ebodys == NULL || Series == NULL) {
        return EGADS_MALLOC;
    }

    Series[numUdp] = Series[0];

    /* break Series into camber (m), locn of max camber (p), and thickness (t) */
    m = (int)( Series[0]           / 1000);
    p = (int)((Series[0] - 1000*m) /  100);
    t =        Series[0]           %  100;

    if (p == 0) {
        p = 4;
    }

    m /= 100;
    p /=  10;
    t /= 100;

    /* if t > 0, generate FaceBody for profile */
    if (t > 0) {

        /* mallocs required by Windows compiler */
        npts = 101;
        pts = (double*)malloc((2*npts    )*sizeof(double));
        cp  = (double*)malloc((4*npts+100)*sizeof(double));

        if (pts == NULL || cp == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        /* points around airfoil (upper and then lower) */
        for (i = 0; i < npts; i++) {
            zeta = TWOPI * i / (npts-1);
            xx   = (1 + cos(zeta)) / 2;
            yt   = t/0.20 * ( 0.2969 * sqrt(xx)
                            - 0.1260 * xx
                            - 0.3516 * xx*xx
                            + 0.2843 * xx*xx*xx
                            - 0.1015 * xx*xx*xx*xx);

            if (xx < p) {
                yc    =      m / (  p)/(  p) * (          2*p*xx -   xx*xx);
                theta = atan(m / (  p)/(  p) * (          2*p    - 2*xx   ));
            } else {
                yc    =      m / (1-p)/(1-p) * ((1-2*p) + 2*p*xx -   xx*xx);
                theta = atan(m / (1-p)/(1-p) * (          2*p    - 2*xx   ));
            }

            if (i < npts/2) {
                pts[2*i  ] = xx - yt * sin(theta);
                pts[2*i+1] = yc + yt * cos(theta);
            } else if (i == npts/2) {
                pts[2*i  ] = 0;
                pts[2*i+1] = 0;
            } else {
                pts[2*i  ] = xx + yt * sin(theta);
                pts[2*i+1] = yc - yt * cos(theta);
            }
        }

        /* create Node at upper trailing edge */
        data[0] = pts[0];
        data[1] = pts[1];
        data[2] = 0;
        status = EG_makeTopology(context, NULL, NODE, 0,
                                 data, 0, NULL, NULL, &(enodes[0]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create Node at leading edge */
        data[0] = pts[npts-1];
        data[1] = pts[npts  ];
        data[2] = 0;
        status = EG_makeTopology(context, NULL, NODE, 0,
                                 data, 0, NULL, NULL, &(enodes[1]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create Node at lower trailing edge */
        data[0] = pts[2*npts-2];
        data[1] = pts[2*npts-1];
        data[2] = 0;
        status = EG_makeTopology(context, NULL, NODE, 0,
                                 data, 0, NULL, NULL, &(enodes[2]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        enodes[3] = enodes[0];

        /* create spline curve from upper TE, to LE, to lower TE */
        header[0] = 0;
        header[1] = 3;
        header[2] = npts + 2;
        header[3] = npts + 6;

        /* knots (which are arc-length spaced) */
        jj = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;

        for (ii = 1; ii < npts; ii++) {
            cp[jj] = cp[jj-1] + sqrt(pow(pts[2*ii  ]-pts[2*ii-2],2)
                                    +pow(pts[2*ii+1]-pts[2*ii-1],2));
            jj++;
        }

        cp[jj] = cp[jj-1]; jj++;
        cp[jj] = cp[jj-1]; jj++;
        cp[jj] = cp[jj-1]; jj++;

        for (ii = 0; ii < header[3]; ii++) {
            cp[ii] /= cp[header[3]-1];
        }

        /* initial control point */
        cp[jj++] = pts[0];
        cp[jj++] = pts[1];
        cp[jj++] = 0;

        /* initial interior control point (for slope) */
        cp[jj++] = (3 * pts[0] + pts[2]) / 4;
        cp[jj++] = (3 * pts[1] + pts[3]) / 4;
        cp[jj++] = 0;

        /* interior control points */
        for (ii = 1; ii < npts-1; ii++) {
            cp[jj++] = pts[2*ii  ];
            cp[jj++] = pts[2*ii+1];
            cp[jj++] = 0;
        }

        /* penultimate interior control point (for slope) */
        cp[jj++] = (3 * pts[2*npts-2] + pts[2*npts-4]) / 4;
        cp[jj++] = (3 * pts[2*npts-1] + pts[2*npts-3]) / 4;
        cp[jj++] = 0;

        /* final control point */
        cp[jj++] = pts[2*npts-2];
        cp[jj++] = pts[2*npts-1];
        cp[jj++] = 0;

        /* make the original BSPLINE (based upon the assumed control points) */
        status = EG_makeGeometry(context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* iterate to have knot evaluations match data points */
        niter = 250;
        for (iter = 0; iter < niter; iter++) {
            dxymax = 0;

            /* natural end condition at beginning */
            status = EG_evaluate(ecurve, &(cp[0]), data);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            /* note: the 0.01 is needed to make this work, buts its
               "theoreical justification" is unknown (perhap under-relaxation?) */
            du = cp[4] - cp[3];
            dx = 0.01 * du * du * data[6];
            dy = 0.01 * du * du * data[7];

            if (fabs(dx/du) > dxymax) dxymax = fabs(dx/du);
            if (fabs(dy/du) > dxymax) dxymax = fabs(dy/du);

            cp[header[3]+3] += dx;
            cp[header[3]+4] += dy;

            /* match interior spline points */
            for (ii = 1; ii < npts-1; ii++) {
                status = EG_evaluate(ecurve, &(cp[ii+3]), data);
                if (status != EGADS_SUCCESS) {
                    *string = udpErrorStr(status);
                    goto cleanup;
                }

                dx = pts[2*ii  ] - data[0];
                dy = pts[2*ii+1] - data[1];

                if (fabs(dx) > dxymax) dxymax = fabs(dx);
                if (fabs(dy) > dxymax) dxymax = fabs(dy);

                cp[header[3]+3*ii+3] += dx;
                cp[header[3]+3*ii+4] += dy;
            }

            /* natural end condition at end */
            status = EG_evaluate(ecurve, &(cp[npts+3]), data);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            du = cp[npts+2] - cp[npts+1];
            dx = 0.01 * du * du * data[6];
            dy = 0.01 * du * du * data[7];

            if (fabs(dx/du) > dxymax) dxymax = fabs(dx/du);
            if (fabs(dy/du) > dxymax) dxymax = fabs(dy/du);

            cp[header[3]+3*npts  ] += dx;
            cp[header[3]+3*npts+1] += dy;

            if (dxymax < dxytol) break;

            /* make the new curve */
            status = EG_deleteObject(ecurve);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            status = EG_makeGeometry(context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }
        }

        /* make Edge for upper surface */
        data[0] = pts[0];
        data[1] = pts[1];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[0]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        data[0] = pts[npts-1];
        data[1] = pts[npts  ];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[1]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 tdata, 2, &(enodes[0]), NULL, &(eedges[0]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* make Edge for lower surface */
        data[0] = pts[npts-1];
        data[1] = pts[npts  ];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[0]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        data[0] = pts[2*npts-2];
        data[1] = pts[2*npts-1];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[1]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 tdata, 2, &(enodes[1]), NULL, &(eedges[1]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create line segment at trailing edge */
        data[0] = pts[2*npts-2];
        data[1] = pts[2*npts-1];
        data[2] = 0;
        data[3] = pts[0] - pts[2*npts-2];
        data[4] = pts[1] - pts[2*npts-1];
        data[5] = 0;
        status = EG_makeGeometry(context, CURVE, LINE, NULL, NULL, data, &eline);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* make Edge for this line */

        status = EG_invEvaluate(eline, data, &(tdata[0]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        data[0] = pts[0];
        data[1] = pts[1];
        data[2] = 0;
        status = EG_invEvaluate(eline, data, &(tdata[1]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        status = EG_makeTopology(context, eline, EDGE, TWONODE,
                                 tdata, 2, &(enodes[2]), NULL, &(eedges[2]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create loop of the three Edges */
        sense[0] = SFORWARD;
        sense[1] = SFORWARD;
        sense[2] = SFORWARD;

        status = EG_makeTopology(context, NULL, LOOP, CLOSED,
                                 NULL, 3, eedges, sense, &eloop);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        status = EG_getArea(eloop, NULL, &area);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* make Face from the loop */
        status = EG_makeFace(eloop, SFORWARD, NULL, &eface);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create the FaceBody (which will be returned) */
        status = EG_makeTopology(context, NULL, BODY, FACEBODY,
                                 NULL, 1, &eface, sense, ebody);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

    /* t == 0, so create WireBody of the meanline */
    } else {

        /* mallocs required by Windows compiler */
        npts = 51;
        pts = (double*)malloc((2*npts    )*sizeof(double));
        cp  = (double*)malloc((4*npts+100)*sizeof(double));

        if (pts == NULL || cp == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        /* points along meanline (lading edge to trailing edge) */
        for (i = 0; i < npts; i++) {
            zeta = PI * i / (npts-1);
            xx   = (1 - cos(zeta)) / 2;

            if (xx < p) {
                yc =  m / (  p)/(  p) * (          2*p*xx - xx*xx);
            } else {
                yc =  m / (1-p)/(1-p) * ((1-2*p) + 2*p*xx - xx*xx);
            }

            pts[2*i  ] = xx;
            pts[2*i+1] = yc;
        }

        /* create Node at leading edge */
        data[0] = pts[0];
        data[1] = pts[1];
        data[2] = 0;
        status = EG_makeTopology(context, NULL, NODE, 0,
                                 data, 0, NULL, NULL, &(enodes[0]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create Node at trailing edge */
        data[0] = pts[2*npts-2];
        data[1] = pts[2*npts-1];
        data[2] = 0;
        status = EG_makeTopology(context, NULL, NODE, 0,
                                 data, 0, NULL, NULL, &(enodes[1]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create spline curve from LE to TE */
        header[0] = 0;
        header[1] = 3;
        header[2] = npts + 2;
        header[3] = npts + 6;

        /* knots (which are arc-length spaced) */
        jj = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;
        cp[jj++] = 0;

        for (ii = 1; ii < npts; ii++) {
            cp[jj] = cp[jj-1] + sqrt(pow(pts[2*ii  ]-pts[2*ii-2],2)
                                    +pow(pts[2*ii+1]-pts[2*ii-1],2));
            jj++;
        }

        cp[jj] = cp[jj-1]; jj++;
        cp[jj] = cp[jj-1]; jj++;
        cp[jj] = cp[jj-1]; jj++;

        for (ii = 0; ii < header[3]; ii++) {
            cp[ii] /= cp[header[3]-1];
        }

        /* initial control point */
        cp[jj++] = pts[0];
        cp[jj++] = pts[1];
        cp[jj++] = 0;

        /* initial interior control point (for slope) */
        cp[jj++] = (3 * pts[0] + pts[2]) / 4;
        cp[jj++] = (3 * pts[1] + pts[3]) / 4;
        cp[jj++] = 0;

        /* interior control points */
        for (ii = 1; ii < npts-1; ii++) {
            cp[jj++] = pts[2*ii  ];
            cp[jj++] = pts[2*ii+1];
            cp[jj++] = 0;
        }

        /* penultimate interior control point (for slope) */
        cp[jj++] = (3 * pts[2*npts-2] + pts[2*npts-4]) / 4;
        cp[jj++] = (3 * pts[2*npts-1] + pts[2*npts-3]) / 4;
        cp[jj++] = 0;

        /* final control point */
        cp[jj++] = pts[2*npts-2];
        cp[jj++] = pts[2*npts-1];
        cp[jj++] = 0;

        /* make the original BSPLINE (based upon the assumed control points) */
        status = EG_makeGeometry(context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* iterate to have knot evaluations match data points */
        niter = 250;
        for (iter = 0; iter < niter; iter++) {
            dxymax = 0;

            /* natural end condition at beginning */
            status = EG_evaluate(ecurve, &(cp[0]), data);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            /* note: the 0.01 is needed to make this work, buts its
               "theoreical justification" is unknown (perhap under-relaxation?) */
            du = cp[4] - cp[3];
            dx = 0.01 * du * du * data[6];
            dy = 0.01 * du * du * data[7];

            if (fabs(dx/du) > dxymax) dxymax = fabs(dx/du);
            if (fabs(dy/du) > dxymax) dxymax = fabs(dy/du);

            cp[header[3]+3] += dx;
            cp[header[3]+4] += dy;

            /* match interior spline points */
            for (ii = 1; ii < npts-1; ii++) {
                status = EG_evaluate(ecurve, &(cp[ii+3]), data);
                if (status != EGADS_SUCCESS) {
                    *string = udpErrorStr(status);
                    goto cleanup;
                }

                dx = pts[2*ii  ] - data[0];
                dy = pts[2*ii+1] - data[1];

                if (fabs(dx) > dxymax) dxymax = fabs(dx);
                if (fabs(dy) > dxymax) dxymax = fabs(dy);

                cp[header[3]+3*ii+3] += dx;
                cp[header[3]+3*ii+4] += dy;
            }

            /* natural end condition at end */
            status = EG_evaluate(ecurve, &(cp[npts+3]), data);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            du = cp[npts+2] - cp[npts+1];
            dx = 0.01 * du * du * data[6];
            dy = 0.01 * du * du * data[7];

            if (fabs(dx/du) > dxymax) dxymax = fabs(dx/du);
            if (fabs(dy/du) > dxymax) dxymax = fabs(dy/du);

            cp[header[3]+3*npts  ] += dx;
            cp[header[3]+3*npts+1] += dy;

            if (dxymax < dxytol) break;

            /* make the new curve */
            status = EG_deleteObject(ecurve);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }

            status = EG_makeGeometry(context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
            if (status != EGADS_SUCCESS) {
                *string = udpErrorStr(status);
                goto cleanup;
            }
        }

        /* make Edge for camberline */
        data[0] = pts[0];
        data[1] = pts[1];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[0]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        data[0] = pts[2*npts-2];
        data[1] = pts[2*npts-1];
        data[2] = 0;
        status = EG_invEvaluate(ecurve, data, &(tdata[1]), result);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 tdata, 2, &(enodes[0]), NULL, &(eedges[0]));
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create loop of the Edges */
        sense[0] = SFORWARD;

        status = EG_makeTopology(context, NULL, LOOP, OPEN,
                                 NULL, 1, eedges, sense, &eloop);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }

        /* create the WireBody (which will be returned) */
        status = EG_makeTopology(context, NULL, BODY, WIREBODY,
                                 NULL, 1, &eloop, NULL, ebody);
        if (status != EGADS_SUCCESS) {
            *string = udpErrorStr(status);
            goto cleanup;
        }
    }

    /* remember this model (body) */
    ebodys[numUdp] = *ebody;

cleanup:
    if (pts != NULL) free(pts);
    if (cp  != NULL) free(cp);

    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
    }

    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpMesh - return mesh associated with primitive                    *
 *                                                                      *
 ************************************************************************
 */

int
udpMesh(ego    ebody,
        int    iMesh,
        int    *imax,
        int    *jmax,
        int    *kmax,
        double **mesh)
{
    int    iudp, judp;

    *imax = 0;
    *jmax = 0;
    *kmax = 0;
    *mesh = NULL;

    /* check that ebody matches one of the ebodys */
    iudp = 0;
    for (judp = 1; judp <= numUdp; judp++) {
        if (ebody == ebodys[judp]) {
            iudp = judp;
            break;
        }
    }
    if (iudp <= 0) {
        return EGADS_NOTMODEL;
    }

    if (iMesh != 0) {
        printf(" udpMesh: iMesh %d != 0\n", iMesh);
    }

    return EGADS_NOLOAD;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpSensitivity - return sensitivity derivatives for the "real" argument *
 *                                                                      *
 ************************************************************************
 */

int
udpSensitivity(ego    ebody,
               char   *vname,
  /*@unused@*/ int    npts,
  /*@unused@*/ int    *fIndices,
  /*@unused@*/ double *uvs,
  /*@unused@*/ double *dxdname)
{
    int iudp, judp;

    /* check that ebody matches one of the ebodys */
    iudp = 0;
    for (judp = 1; judp <= numUdp; judp++) {
        if (ebody == ebodys[judp]) {
            iudp = judp;
            break;
        }
    }
    if (iudp <= 0) {
        return EGADS_NOTMODEL;
    }

    if (vname  == NULL) {
        return EGADS_NONAME;
    }

    /* this routine is not written yet */
    return EGADS_NOLOAD;
}


/*
 ************************************************************************
 *                                                                      *
 *   udpStepSize - return sensitivity step size of finite differencing  *
 *                                                                      *
 ************************************************************************
 */

int
udpStepSize(ego    ebody,
            char   *vname,
            double *delta)
{
    int iudp, judp;

    *delta = 0.0;

    /* check that ebody matches one of the ebodys */
    iudp = 0;
    for (judp = 1; judp <= numUdp; judp++) {
        if (ebody == ebodys[judp]) {
            iudp = judp;
            break;
        }
    }
    if (iudp <= 0) {
        return EGADS_NOTMODEL;
    }

    if (vname  == NULL) {
        return EGADS_NONAME;
    }

    /* this routine is not written yet */
    return EGADS_NOLOAD;
}

#endif
