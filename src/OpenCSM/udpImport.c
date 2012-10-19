/*
 ************************************************************************
 *                                                                      *
 * udpImport -- udp file to import from a STEP file                     *
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
static char   *FileName   = NULL;

/* number of Import UDPs */
static int    numUdp      = 0;

/* arrays to hold info for each of the UDPs */
static ego    *ebodys     = NULL;
static int    *BodyNumber = NULL;


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
    ebodys     = (ego *) EG_alloc(sizeof(ego));
    BodyNumber = (int *) EG_alloc(sizeof(int));

    if (ebodys == NULL || BodyNumber == NULL) {
        return EGADS_MALLOC;
    }

    /* initialize the array elements that will hold the "current" settings */
    ebodys[    0] = NULL;
    BodyNumber[0] = 1;

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

    name[0]     = "FileName";
    type[0]     = ATTRSTRING;
    idefault[0] = 0;
    ddefault[0] = 0.0;

    name[1]     = "BodyNumber";
    type[1]     = ATTRINT;
    idefault[1] = 1;
    ddefault[1] = 0.0;

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

    if (FileName != NULL) {
        EG_free(FileName);
        FileName = NULL;
    }

    /* reset the "current" settings */
    if (flag == 0) {
        BodyNumber[0] = 1;

    /* called when closing up */
    } else {
        for (iudp = 0; iudp <= numUdp; iudp++) {
            if (ebodys[iudp] != NULL) {
                EG_deleteObject(ebodys[iudp]);
                ebodys[iudp] = NULL;
            }
        }

        EG_free(ebodys    ); ebodys     = NULL;
        EG_free(BodyNumber); BodyNumber = NULL;

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
    int  i, len;
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

    if (strcmp(name, "FileName") == 0) {
        if (FileName != NULL) {
            EG_free(FileName);
            FileName = NULL;
        }

        FileName = (char *) EG_alloc((len+1)*sizeof(char));
        if (FileName == NULL) {
            return EGADS_MALLOC;
        }

        for (i = 0; i <= len; i++) {
            FileName[i] = value[i];
        }

    } else if (strcmp(name, "BodyNumber") == 0) {
        BodyNumber[0] = strtol(value, &pEnd, 10);
        if (BodyNumber[0] <= 0) {
            printf(" udpSet: BodyNumber = %d -- reset to 1\n", BodyNumber[0]);
            BodyNumber[0] = 1;
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

    int     oclass, mtype, nbody, *senses;
    ego     geom, *bodies, emodel;

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* increment number of UDPs */
    numUdp++;

    /* increase the arrays to make room for the new UDP */
    ebodys     = (ego *) EG_reall(ebodys,     (numUdp+1)*sizeof(ego));
    BodyNumber = (int *) EG_reall(BodyNumber, (numUdp+1)*sizeof(int));

    if (ebodys == NULL || BodyNumber == NULL) {
        return EGADS_MALLOC;
    }

    ebodys[    numUdp] = NULL;
    BodyNumber[numUdp] = BodyNumber[0];

    /* load the model */
    status = EG_loadModel(context, 0, FileName, &emodel);
    if ((status != EGADS_SUCCESS) || (emodel == NULL)) {
        *string = udpErrorStr(status);
        goto cleanup;
    }

    /* extract the ebody */
    status = EG_getTopology(emodel, &geom, &oclass, &mtype, NULL, &nbody,
                          &bodies, &senses);
    if (status != EGADS_SUCCESS) {
        *string = udpErrorStr(status);
        EG_deleteObject(emodel);
        emodel = NULL;
        goto cleanup;
    }

    if (BodyNumber[0] > nbody) {
        printf(" udpExecute: BodyNumber %d > %d -- set to 1\n", BodyNumber[0], nbody);
        *ebody = bodies[0];
    } else {
        *ebody = bodies[BodyNumber[0]-1];
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
