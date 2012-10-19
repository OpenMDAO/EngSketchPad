/*
 ************************************************************************
 *                                                                      *
 * udpWaffle -- udp file to generate 2D waffle                          *
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

#define TWOPI 6.2831853071795862319959269

/* static variables used to hold info between function calls */

/* number of Waffle UDPs */
static int    numUdp   = 0;

/* arrays to hold info for each of the UDPs */
static ego    *ebodys   = NULL;
static double *Depth    = NULL;
static int    *Nseg     = NULL;
static double **Segments= NULL;


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
    ebodys   = (ego    *) EG_alloc(sizeof(ego    ));
    Depth    = (double *) EG_alloc(sizeof(double ));
    Nseg     = (int    *) EG_alloc(sizeof(int    ));
    Segments = (double**) EG_alloc(sizeof(double*));

    if (ebodys == NULL || Depth == NULL || Nseg == NULL || Segments == NULL) {
        return EGADS_MALLOC;
    }

    /* initialize the array elements that will hold the "current" settings */
    ebodys[  0] = NULL;
    Depth[   0] = 1;
    Nseg[    0] = 0;
    Segments[0] = NULL;

    /* set up returns that describe the UDP */
    *nArgs    = 2;
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

    name[0]     = "Depth";
    type[0]     = ATTRREAL;
    idefault[0] = 0;
    ddefault[0] = 1;

    name[1]     = "Segments";
    type[1]     = ATTRSTRING;
    idefault[1] = 0;
    ddefault[1] = 0;

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
        Depth[0] = 1;
        Nseg[ 0] = 0;

        if (Segments[0] != NULL) {
            EG_free(Segments[0]);
            Segments[0] = NULL;
        }

    /* called when closing up */
    } else {
        for (iudp = 0; iudp <= numUdp; iudp++) {
            if (ebodys[iudp] != NULL) {
                EG_deleteObject(ebodys[iudp]);
                ebodys[iudp] = NULL;
            }
            if (Segments[iudp] != NULL) {
                EG_free(Segments[iudp]);
                Segments[iudp] = NULL;
            }
        }

        EG_free(ebodys  );  ebodys    = NULL;
        EG_free(Depth   );  Depth     = NULL;
        EG_free(Nseg    );  Nseg      = NULL;
        EG_free(Segments);  *Segments = NULL;

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
    int  ij, len, icount, jcount;
    char *pEnd, defn[128];

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

    if (strcmp(name, "Depth") == 0) {
        Depth[0] = strtod(value, &pEnd);
        if (Depth[0] <= 0) {
            printf(" udpSet: Depth = %f -- reset to 1\n", Depth[0]);
            Depth[0] = 1;
        }

    } else if (strcmp(name, "Segments") == 0) {

        /* count the number of values */
        icount = 0;
        for (ij = 0; ij < strlen(value); ij++) {
            if (value[ij] == ';') icount++;
        }

        if (icount%4 == 0) {
            Nseg[0] = icount / 4;
        } else {
            printf(" udpSet: Segements must have 4*n values\n");
            return EGADS_NODATA;
        }

        /* allocate necessary array */
        if (Segments[0] != NULL) EG_free(Segments[0]);

        Segments[0] = (double*) EG_alloc(icount*sizeof(double));

        if (Segments[0] == NULL) {
            return EGADS_MALLOC;
        }

        /* extract the data from the string */
        icount = 0;
        for (ij = 0; ij < 4*Nseg[0]; ij++) {
            jcount = 0;

            while (icount < strlen(value)) {
                if (value[icount] == ';') {
                    icount++;
                    break;
                } else {
                    defn[jcount  ] = value[icount];
                    defn[jcount+1] = '\0';
                    icount++;
                    jcount++;
                }
            }

            Segments[0][ij] = strtod(defn, &pEnd);
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
udpExecute(ego context,
           ego *ebody,
           int *nMesh,
           char **string)
{
    int     status = EGADS_SUCCESS;

    int     inode, jnode, nnode, iseg, jseg, jedge, jface, senses[4];
    int     *ibeg=NULL, *iend=NULL;
    int     ia, ib, ic, id, MAXSIZE=1000;
    double  *X=NULL, *Y=NULL;
    double  xyz[20], data[6], xyz_out[20], D, s, t, xx, yy;

    double  EPS06 = 1.0e-6;
    ego     *enodes=NULL, *eedges=NULL, *efaces=NULL, ecurve, echild[4], eloop, eshell;

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* increment number of UDPs */
    numUdp++;

    /* increase the arrays to make room for the new UDP */
    ebodys   = (ego    *) EG_reall(ebodys,   (numUdp+1)*sizeof(ego    ));
    Depth    = (double *) EG_reall(Depth,    (numUdp+1)*sizeof(double ));
    Nseg     = (int    *) EG_reall(Nseg,     (numUdp+1)*sizeof(int    ));
    Segments = (double**) EG_reall(Segments, (numUdp+1)*sizeof(double*));

    if (ebodys == NULL || Depth == NULL || Nseg == NULL || Segments == NULL) {
        return EGADS_MALLOC;
    }

    ebodys[  numUdp] = NULL;

    Segments[numUdp] = (double*) EG_alloc(4*Nseg[0]*sizeof(double));

    if (Segments[numUdp] == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    /* copy in the data */
    Depth[numUdp] = Depth[0];
    Nseg[ numUdp] = Nseg[ 0];

    for (iseg = 0; iseg < Nseg[0]; iseg++) {
        Segments[numUdp][4*iseg  ] = Segments[0][4*iseg  ];
        Segments[numUdp][4*iseg+1] = Segments[0][4*iseg+1];
        Segments[numUdp][4*iseg+2] = Segments[0][4*iseg+2];
        Segments[numUdp][4*iseg+3] = Segments[0][4*iseg+3];
    }

    /* make an indexed table of the segments */
    X    = (double *) EG_alloc(MAXSIZE*sizeof(double));  // probably too big
    Y    = (double *) EG_alloc(MAXSIZE*sizeof(double));
    ibeg = (int    *) EG_alloc(MAXSIZE*sizeof(int   ));
    iend = (int    *) EG_alloc(MAXSIZE*sizeof(int   ));

    if (X == NULL || Y == NULL || ibeg == NULL || iend == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    nnode = 0;

    for (iseg = 0; iseg < Nseg[0]; iseg++) {
        ibeg[iseg] = -1;
        for (jnode = 0; jnode < nnode; jnode++) {
            if (fabs(Segments[0][4*iseg  ]-X[jnode]) < EPS06 &&
                fabs(Segments[0][4*iseg+1]-Y[jnode]) < EPS06   ) {
                ibeg[iseg] = jnode;
                break;
            }
        }

        if (ibeg[iseg] < 0) {
            ibeg[iseg] = nnode;

            X[nnode] = Segments[0][4*iseg  ];
            Y[nnode] = Segments[0][4*iseg+1];
            nnode++;
        }

        iend[iseg] = -1;
        for (jnode = 0; jnode < nnode; jnode++) {
            if (fabs(Segments[0][4*iseg+2]-X[jnode]) < EPS06 &&
                fabs(Segments[0][4*iseg+3]-Y[jnode]) < EPS06   ) {
                iend[iseg] = jnode;
                break;
            }
        }

        if (iend[iseg] < 0) {
            iend[iseg] = nnode;

            X[nnode] = Segments[0][4*iseg+2];
            Y[nnode] = Segments[0][4*iseg+3];
            nnode++;
        }
    }

    /* check for intersections */
    for (jseg = 0; jseg < Nseg[0]; jseg++) {
        for (iseg = jseg+1; iseg < Nseg[0]; iseg++) {
            ia = ibeg[iseg];
            ib = iend[iseg];
            ic = ibeg[jseg];
            id = iend[jseg];

            D = (X[ib] - X[ia]) * (Y[ic] - Y[id]) - (X[ic] - X[id]) * (Y[ib] - Y[ia]);
            if (fabs(D) > EPS06) {
                s = ((X[ic] - X[ia]) * (Y[ic] - Y[id]) - (X[ic] - X[id]) * (Y[ic] - Y[ia])) / D;
                t = ((X[ib] - X[ia]) * (Y[ic] - Y[ia]) - (X[ic] - X[ia]) * (Y[ib] - Y[ia])) / D;

                if (s > -EPS06 && s < 1+EPS06 &&
                    t > -EPS06 && t < 1+EPS06   ) {
                    xx = (1 - s) * X[ia] + s * X[ib];
                    yy = (1 - s) * Y[ia] + s * Y[ib];

                    inode = -1;
                    for (jnode = 0; jnode < nnode; jnode++) {
                        if (fabs(xx-X[jnode]) < EPS06 &&
                            fabs(yy-Y[jnode]) < EPS06   ) {
                            inode = jnode;
                            break;
                        }
                    }

                    if (inode < 0) {
                        inode = nnode;

                        X[nnode] = xx;
                        Y[nnode] = yy;
                        nnode++;
                    }

                    if (ia != inode && ib != inode) {
                        ibeg[Nseg[0]] = inode;
                        iend[Nseg[0]] = ib;
                        Nseg[0]++;

                        iend[iseg] = inode;
                    }

                    if (ic != inode && id != inode) {
                        ibeg[Nseg[0]] = inode;
                        iend[Nseg[0]] = id;
                        Nseg[0]++;

                        iend[jseg] = inode;
                    }
                }
            }
        }
    }

    /* check for degenerate segments */
    for (jseg = 0; jseg < Nseg[0]; jseg++) {
        if (ibeg[jseg] == iend[jseg]) {
            printf(" udpExecute: segment %d is degenerate\n", iseg);
            status = EGADS_DEGEN;
            goto cleanup;
        }
    }

    /* allocate space for the egos */
    enodes = (ego *) EG_alloc((2*nnode          )*sizeof(ego));
    eedges = (ego *) EG_alloc((  nnode+2*Nseg[0])*sizeof(ego));
    efaces = (ego *) EG_alloc((          Nseg[0])*sizeof(ego));

    if (enodes == NULL || eedges == NULL || efaces == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    /* set up enodes at Z=0 and at Z=Depth */
    jnode = 0;

    for (inode = 0; inode < nnode; inode++) {
        xyz[0] = X[inode];
        xyz[1] = Y[inode];
        xyz[2] = 0;

        status = EG_makeTopology(context, NULL, NODE, 0,
                                 xyz, 0, NULL, NULL, &(enodes[jnode]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jnode++;
    }

    for (inode = 0; inode < nnode; inode++) {
        xyz[0] = X[inode];
        xyz[1] = Y[inode];
        xyz[2] = Depth[0];

        status = EG_makeTopology(context, NULL, NODE, 0, xyz, 0,
                                 NULL, NULL, &(enodes[jnode]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jnode++;
    }        

    /* set up the edges on the z=0 plane */
    jedge = 0;

    for (iseg = 0; iseg < Nseg[0]; iseg++) {
        xyz[0] = X[ibeg[iseg]];
        xyz[1] = Y[ibeg[iseg]];
        xyz[2] = 0;
        xyz[3] = X[iend[iseg]] - X[ibeg[iseg]];
        xyz[4] = Y[iend[iseg]] - Y[ibeg[iseg]];
        xyz[5] = 0;

        status = EG_makeGeometry(context, CURVE, LINE, NULL,
                                 NULL, xyz, &ecurve);
        if (status != EGADS_SUCCESS) goto cleanup;

        echild[0] = enodes[ibeg[iseg]];
        echild[1] = enodes[iend[iseg]];

        status = EG_invEvaluate(ecurve, xyz, &(data[0]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        xyz[0] = X[iend[iseg]];
        xyz[1] = Y[iend[iseg]];
        xyz[2] = 0;

        status = EG_invEvaluate(ecurve, xyz, &(data[1]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 data, 2, echild, NULL, &(eedges[jedge]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jedge++;
    }
    
    /* set up the edges on the z=Depth plane */
    for (iseg = 0; iseg < Nseg[0]; iseg++) {
        xyz[0] = X[ibeg[iseg]];
        xyz[1] = Y[ibeg[iseg]];
        xyz[2] = Depth[0];
        xyz[3] = X[iend[iseg]] - X[ibeg[iseg]];
        xyz[4] = Y[iend[iseg]] - Y[ibeg[iseg]];
        xyz[5] = 0;

        status = EG_makeGeometry(context, CURVE, LINE, NULL,
                                 NULL, xyz, &ecurve);
        if (status != EGADS_SUCCESS) goto cleanup;

        echild[0] = enodes[ibeg[iseg]+nnode];
        echild[1] = enodes[iend[iseg]+nnode];

        status = EG_invEvaluate(ecurve, xyz, &(data[0]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        xyz[0] = X[iend[iseg]];
        xyz[1] = Y[iend[iseg]];
        xyz[2] = Depth[0];

        status = EG_invEvaluate(ecurve, xyz, &(data[1]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 data, 2, echild, NULL, &(eedges[jedge]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jedge++;
    }
    
    /* set up the edges between z=0 and Depth */
    for (inode = 0; inode < nnode; inode++) {
        xyz[0] = X[inode];
        xyz[1] = Y[inode];
        xyz[2] = 0;
        xyz[3] = 0;
        xyz[4] = 0;
        xyz[5] = 1;

        status = EG_makeGeometry(context, CURVE, LINE, NULL,
                                 NULL, xyz, &ecurve);
        if (status != EGADS_SUCCESS) goto cleanup;

        echild[0] = enodes[inode      ];
        echild[1] = enodes[inode+nnode];

        status = EG_invEvaluate(ecurve, xyz, &(data[0]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        xyz[0] = X[inode];
        xyz[1] = Y[inode];
        xyz[2] = Depth[0];

        status = EG_invEvaluate(ecurve, xyz, &(data[1]), xyz_out);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeTopology(context, ecurve, EDGE, TWONODE,
                                 data, 2, echild, NULL, &(eedges[jedge]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jedge++;
    }

    /* set up the faces */
    jface = 0;

    for (iseg = 0; iseg < Nseg[0]; iseg++) {
        echild[0] = eedges[iseg];
        echild[1] = eedges[2*Nseg[0]+iend[iseg]];
        echild[2] = eedges[  Nseg[0]+     iseg ];
        echild[3] = eedges[2*Nseg[0]+ibeg[iseg]];

        senses[0] = SFORWARD;
        senses[1] = SFORWARD;
        senses[2] = SREVERSE;
        senses[3] = SREVERSE;

        status = EG_makeTopology(context, NULL, LOOP, CLOSED,
                                 NULL, 4, echild, senses, &eloop);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeFace(eloop, SFORWARD, NULL, &(efaces[jface]));
        if (status != EGADS_SUCCESS) goto cleanup;

        jseg = iseg + 1;
        status = EG_attributeAdd(efaces[jface], "segment", ATTRINT,
                                 1, &jseg, NULL, NULL);
        if (status != EGADS_SUCCESS) goto cleanup;

        jface++;
    }

    /* make the sheet body */
    status = EG_makeTopology(context, NULL, SHELL, OPEN,
                             NULL, jface, efaces, NULL, &eshell);
    if (status != EGADS_SUCCESS) goto cleanup;

    status = EG_makeTopology(context, NULL, BODY, SHEETBODY,
                             NULL, 1, &eshell, NULL, ebody);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* remember this model (body) */
    ebodys[numUdp] = *ebody;

cleanup:
    if (efaces != NULL) EG_free(efaces);
    if (eedges != NULL) EG_free(eedges);
    if (enodes != NULL) EG_free(enodes);

    if (X      != NULL) EG_free(X     );
    if (Y      != NULL) EG_free(Y     );
    if (ibeg   != NULL) EG_free(ibeg  );
    if (iend   != NULL) EG_free(iend  );

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
