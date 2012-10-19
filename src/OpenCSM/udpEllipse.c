/*
 ************************************************************************
 *                                                                      *
 * udpEllipse -- udp file to generate an ellipse                        *
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

/* number of Ellipse UDPs */
static int    numUdp  = 0;

/* arrays to hold info for each of the UDPs */
static ego    *ebodys   = NULL;
static double *Rx       = NULL;
static double *Ry       = NULL;
static double *Rz       = NULL;


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
    Rx     = (double *) EG_alloc(sizeof(double ));
    Ry     = (double *) EG_alloc(sizeof(double ));
    Rz     = (double *) EG_alloc(sizeof(double ));

    if (ebodys == NULL || Rx == NULL || Ry == NULL || Rz == NULL) {
        return EGADS_MALLOC;
    }

    /* initialize the array elements that will hold the "current" settings */
    ebodys[0] = NULL;
    Rx[    0] = 0;
    Ry[    0] = 0;
    Rz[    0] = 0;

    /* set up returns that describe the UDP */
    *nArgs    = 3;
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

    name[0]     = "rx";
    type[0]     = ATTRREAL;
    idefault[0] = 0.0;
    ddefault[0] = 0.0;

    name[1]     = "ry";
    type[1]     = ATTRREAL;
    idefault[1] = 0.0;
    ddefault[1] = 0.0;

    name[2]     = "rz";
    type[2]     = ATTRREAL;
    idefault[2] = 0.0;
    ddefault[2] = 0.0;

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
        Rx[0] = 0;
        Ry[0] = 0;
        Rz[0] = 0;

    /* called when closing up */
    } else {
        for (iudp = 0; iudp <= numUdp; iudp++) {
            if (ebodys[iudp] != NULL) {
                EG_deleteObject(ebodys[iudp]);
                ebodys[iudp] = NULL;
            }
        }

        EG_free(ebodys); ebodys = NULL;
        EG_free(Rx    ); Rx     = NULL;
        EG_free(Ry    ); Ry     = NULL;
        EG_free(Rz    ); Rz     = NULL;

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

    if (strcmp(name, "rx") == 0) {
        Rx[0] = strtod(value, &pEnd);
        if (Rx[0] < 0) {
            printf(" udpSet: rx = %f -- reset to 0.0\n", Rx[0]);
            Rx[0] = 0.0;
        }

    } else if (strcmp(name, "ry") == 0) {
        Ry[0] = strtod(value, &pEnd);
        if (Ry[0] < 0) {
            printf(" udpSet: ry = %f -- reset to 0.0\n", Ry[0]);
            Ry[0] = 0.0;
        }

    } else if (strcmp(name, "rz") == 0) {
        Rz[0] = strtod(value, &pEnd);
        if (Rz[0] < 0) {
            printf(" udpSet: rz = %f -- reset to 0.0\n", Rz[0]);
            Rz[0] = 0.0;
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

    int     periodic, senses[2];
    double  params[11], data[18], trange[4];
    ego     enodes[3], ecurve, eedges[2], eloop, eface;

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* increment number of UDPs */
    numUdp++;

    /* increase the arrays to make room for the new UDP */
    ebodys = (ego    *) EG_reall(ebodys, (numUdp+1)*sizeof(ego    ));
    Rx     = (double *) EG_reall(Rx,     (numUdp+1)*sizeof(double ));
    Ry     = (double *) EG_reall(Ry,     (numUdp+1)*sizeof(double ));
    Rz     = (double *) EG_reall(Rz,     (numUdp+1)*sizeof(double ));

    if (ebodys == NULL || Rx == NULL || Ry == NULL || Rz == NULL) {
        return EGADS_MALLOC;
    }

    Rx[numUdp] = Rx[0];
    Ry[numUdp] = Ry[0];
    Rz[numUdp] = Rz[0];

    /* ellipses are centered at origin */
    params[0] = 0.0;
    params[1] = 0.0;
    params[2] = 0.0;

    /* ellipse in y-z plane */
    if (Rx[0] == 0.0 && Ry[0] > 0.0 && Rz[0] > 0.0) {
        if (Ry[0] >= Rz[0]) {
            params[ 3] = 0.0;
            params[ 4] = 1.0;
            params[ 5] = 0.0;

            params[ 6] = 0.0;
            params[ 7] = 0.0;
            params[ 8] = 1.0;

            params[ 9] = Ry[0];
            params[10] = Rz[0];
        } else {
            params[ 3] = 0.0;
            params[ 4] = 0.0;
            params[ 5] = 1.0;

            params[ 6] = 0.0;
            params[ 7] = 1.0;
            params[ 8] = 0.0;

            params[ 9] = Rz[0];
            params[10] = Ry[0];
        }

    /* ellipse in z-x plane */
    } else if (Ry[0] == 0.0 && Rz[0] > 0.0 && Rx[0] > 0.0) {
        if (Rz[0] >= Rx[0]) {
            params[ 3] = 0.0;
            params[ 4] = 0.0;
            params[ 5] = 1.0;

            params[ 6] = 1.0;
            params[ 7] = 0.0;
            params[ 8] = 0.0;

            params[ 9] = Rz[0];
            params[10] = Rx[0];
        } else {
            params[ 3] = 1.0;
            params[ 4] = 0.0;
            params[ 5] = 0.0;

            params[ 6] = 0.0;
            params[ 7] = 0.0;
            params[ 8] = 1.0;

            params[ 9] = Rx[0];
            params[10] = Rz[0];
        }

    /* ellipse in x-y plane */
    } else if (Rz[0] == 0.0 && Rx[0] > 0.0 && Ry[0] > 0.0) {
        if (Rx[0] >= Ry[0]) {
            params[ 3] = 1.0;
            params[ 4] = 0.0;
            params[ 5] = 0.0;

            params[ 6] = 0.0;
            params[ 7] = 1.0;
            params[ 8] = 0.0;

            params[ 9] = Rx[0];
            params[10] = Ry[0];
        } else {
            params[ 3] = 0.0;
            params[ 4] = 1.0;
            params[ 5] = 0.0;

            params[ 6] = 1.0;
            params[ 7] = 0.0;
            params[ 8] = 0.0;

            params[ 9] = Ry[0];
            params[10] = Rx[0];
        }

    /* illegal combination of Rx, Ry, and Rz */
    } else {
        status = EGADS_GEOMERR;
        *string = udpErrorStr(status);
        goto cleanup;
    }

    /* make the Curve */
    status = EG_makeGeometry(context, CURVE, ELLIPSE, NULL, NULL, params, &ecurve);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    /* get the parameter ranges */
    status = EG_getRange(ecurve, trange, &periodic);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }
    trange[2] =  trange[1];
    trange[1] = (trange[0] + trange[2]) / 2;

    /* make two Nodes (and a copy) */
    status = EG_evaluate(ecurve, &(trange[0]), data);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    status = EG_makeTopology(context, NULL, NODE, 0, data, 0, NULL, NULL, &(enodes[0]));
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }


    status = EG_evaluate(ecurve, &(trange[1]), data);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    status = EG_makeTopology(context, NULL, NODE, 0, data, 0, NULL, NULL, &(enodes[1]));
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }


    enodes[2] = enodes[0];

    /* make the Edges */
    status = EG_makeTopology(context, ecurve, EDGE, TWONODE, &(trange[0]), 2, &(enodes[0]), NULL, &(eedges[0]));
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    status = EG_makeTopology(context, ecurve, EDGE, TWONODE, &(trange[1]), 2, &(enodes[1]), NULL, &(eedges[1]));
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    /* make Loop from this Edge */
    senses[0] = SFORWARD;
    senses[1] = SFORWARD;

    status = EG_makeTopology(context, NULL, LOOP, CLOSED, NULL, 2, eedges, senses, &eloop);
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
    status = EG_makeTopology(context, NULL, BODY, FACEBODY, NULL, 1, &eface, senses, ebody);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    /* remember this model (body) */
    ebodys[numUdp] = *ebody;

cleanup:
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
