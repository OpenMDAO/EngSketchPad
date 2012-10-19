/*
 ************************************************************************
 *                                                                      *
 * udpFreeform -- udp file to generate a freeform brick                 *
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
static char   *FileName = NULL;

/* number of Freeform UDPs */
static int    numUdp    = 0;

/* arrays to hold info for each of the UDPs */
static ego    *ebodys   = NULL;
static int    *Imax     = NULL;
static int    *Jmax     = NULL;
static int    *Kmax     = NULL;
static double **X       = NULL;
static double **Y       = NULL;
static double **Z       = NULL;

/* routines defined below */
static int spline1d(ego context, int imax,           double *x, double *y, double *z, ego *ecurve);
static int spline2d(ego context, int imax, int jmax, double *x, double *y, double *z, ego *esurf);


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
    Imax   = (int    *) EG_alloc(sizeof(int    ));
    Jmax   = (int    *) EG_alloc(sizeof(int    ));
    Kmax   = (int    *) EG_alloc(sizeof(int    ));
    X      = (double**) EG_alloc(sizeof(double*));
    Y      = (double**) EG_alloc(sizeof(double*));
    Z      = (double**) EG_alloc(sizeof(double*));

    if (ebodys == NULL || Imax == NULL || Jmax == NULL || Kmax == NULL ||
                          X    == NULL || Y    == NULL || Z    == NULL   ) {
        return EGADS_MALLOC;
    }

    /* initialize the array elements that will hold the "current" settings */
    ebodys[0] = NULL;
    Imax[  0] = 1;
    Jmax[  0] = 1;
    Kmax[  0] = 1;
    X[     0] = NULL;
    Y[     0] = NULL;
    Z[     0] = NULL;

    /* set up returns that describe the UDP */
    *nArgs    = 5;
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

    name[1]     = "Imax";
    type[1]     = ATTRINT;
    idefault[1] = 1;
    ddefault[1] = 0;

    name[2]     = "Jmax";
    type[2]     = ATTRINT;
    idefault[2] = 1;
    ddefault[2] = 0;

    name[3]     = "Kmax";
    type[3]     = ATTRINT;
    idefault[3] = 1;
    ddefault[3] = 0;

    name[4]     = "Xyz";
    type[4]     = ATTRSTRING;
    idefault[4] = 0;
    ddefault[4] = 0;

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
        Imax[0] = 1;
        Jmax[0] = 1;
        Kmax[0] = 1;

        if (X[0] != NULL) {
            EG_free(X[0]);
            X[0] = NULL;
        }
        if (Y[0] != NULL) {
            EG_free(Y[0]);
            Y[0] = NULL;
        }
        if (Z[0] != NULL) {
            EG_free(Z[0]);
            Z[0] = NULL;
        }

    /* called when closing up */
    } else {
        for (iudp = 0; iudp <= numUdp; iudp++) {
            if (ebodys[iudp] != NULL) {
                EG_deleteObject(ebodys[iudp]);
                ebodys[iudp] = NULL;
            }
            if (X[iudp] != NULL) {
                EG_free(X[iudp]);
                X[iudp] = NULL;
            }
            if (Y[iudp] != NULL) {
                EG_free(Y[iudp]);
                Y[iudp] = NULL;
            }
            if (Z[iudp] != NULL) {
                EG_free(Z[iudp]);
                Z[iudp] = NULL;
            }
        }

        EG_free(ebodys);  ebodys = NULL;
        EG_free(Imax  );  Imax   = NULL;
        EG_free(Jmax  );  Jmax   = NULL;
        EG_free(Kmax  );  Kmax   = NULL;
        EG_free(X     );  *X     = NULL;
        EG_free(Y     );  *Y     = NULL;
        EG_free(Z     );  *Z     = NULL;

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
    int  i, len, ijk, icount, jcount;
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

    } else if (strcmp(name, "Imax") == 0) {
        Imax[0] = strtol(value, &pEnd, 10);
        if (Imax[0] <= 0) {
            printf(" udpSet: Imax = %d -- reset to 1\n", Imax[0]);
            Imax[0] = 1;
        }

    } else if (strcmp(name, "Jmax") == 0) {
        Jmax[0] = strtol(value, &pEnd, 10);
        if (Jmax[0] <= 0) {
            printf(" udpSet: Jmax = %d -- reset to 1\n", Jmax[0]);
            Jmax[0] = 1;
        }

    } else if (strcmp(name, "Kmax") == 0) {
        Kmax[0] = strtol(value, &pEnd, 10);
        if (Kmax[0] <= 0) {
            printf(" udpSet: Kmax = %d -- reset to 1\n", Kmax[0]);
            Kmax[0] = 1;
        }

    } else if (strcmp(name, "Xyz") == 0) {

        /* allocate necessary arrays */
        if (X[0] != NULL) EG_free(X[0]);
        if (Y[0] != NULL) EG_free(Y[0]);
        if (Z[0] != NULL) EG_free(Z[0]);

        X[0] = (double*) EG_alloc(Imax[0]*Jmax[0]*Kmax[0]*sizeof(double));
        Y[0] = (double*) EG_alloc(Imax[0]*Jmax[0]*Kmax[0]*sizeof(double));
        Z[0] = (double*) EG_alloc(Imax[0]*Jmax[0]*Kmax[0]*sizeof(double));

        if (X[0] == NULL || Y[0] == NULL || Z[0] == NULL) {
            return EGADS_MALLOC;
        }

        /* extract the data from the string */
        icount = 0;
        for (ijk = 0; ijk < 3*Imax[0]*Jmax[0]*Kmax[0]; ijk++) {
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

            if        (ijk%3 == 0) {
                X[0][ijk/3] = strtod(defn, &pEnd);
            } else if (ijk%3 == 1) {
                Y[0][ijk/3] = strtod(defn, &pEnd);
            } else {
                Z[0][ijk/3] = strtod(defn, &pEnd);
            }
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
 *   macros for udpExecute                                              *
 *                                                                      *
 ************************************************************************
 */

#define CREATE_NODE(INODE, I, J, K)                                     \
    printf("        creating Node %3d\n", INODE);                       \
                                                                        \
    /* create data */                                                   \
    data[0] = X[numUdp][(I)+imax*((J)+jmax*(K))];                       \
    data[1] = Y[numUdp][(I)+imax*((J)+jmax*(K))];                       \
    data[2] = Z[numUdp][(I)+imax*((J)+jmax*(K))];                       \
                                                                        \
    /* make Node */                                                     \
    status = EG_makeTopology(context, NULL, NODE, 0,                    \
                             data, 0, NULL, NULL, &(enodes[INODE]));    \
    if (status != EGADS_SUCCESS) goto cleanup;

#define CREATE_EDGE(IEDGE, IBEG, IEND, IMAX)                            \
    printf("        creating Edge %3d\n", IEDGE);                       \
                                                                        \
    /* make spline Curve */                                             \
    status = spline1d(context, IMAX, x2d, y2d, z2d, &(ecurvs[IEDGE]));  \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    /* make Edge */                                                     \
    etemp[0] = enodes[IBEG];                                            \
    etemp[1] = enodes[IEND];                                            \
    tdata[0] = 0;                                                       \
    tdata[1] = IMAX-1;                                                  \
                                                                        \
    status = EG_makeTopology(context, ecurvs[IEDGE], EDGE, TWONODE,     \
                             tdata, 2, etemp, NULL, &(eedges[IEDGE]));  \
    if (status != EGADS_SUCCESS) goto cleanup;

#define CREATE_FACE(IFACE, IS, IE, IN, IW, JMAX, KMAX)                  \
    printf("        creating Face %3d\n", IFACE);                       \
                                                                        \
    /* make spline Surface */                                           \
    status = spline2d(context, KMAX, JMAX, x2d, y2d, z2d,               \
                      &(esurfs[IFACE]));                                \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    tdata[0] = 0; tdata[1] = 0;                                         \
    (void) EG_evaluate(esurfs[IFACE], tdata, data);                     \
                                                                        \
    tdata[0] = KMAX-1; tdata[1] = 0;                                    \
    (void) EG_evaluate(esurfs[IFACE], tdata, data);                     \
                                                                        \
    tdata[0] = 0; tdata[1] = JMAX-1;                                    \
    (void) EG_evaluate(esurfs[IFACE], tdata, data);                     \
                                                                        \
    tdata[0] = KMAX-1; tdata[1] = JMAX-1;                               \
    (void) EG_evaluate(esurfs[IFACE], tdata, data);                     \
                                                                        \
    /* remember Edges that surround the Face */                         \
    etemp[0] = eedges[IS];                                              \
    etemp[1] = eedges[IE];                                              \
    etemp[2] = eedges[IN];                                              \
    etemp[3] = eedges[IW];                                              \
                                                                        \
    /* make PCurves */                                                  \
    data[0] = 0;         data[1] = 0;                                   \
    data[2] = KMAX-1;    data[3] = 0;                                   \
    status = EG_makeGeometry(context, PCURVE, LINE, NULL,               \
                             NULL, data, &(etemp[4]));                  \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    data[0] = KMAX-1;    data[1] = 0;                                   \
    data[2] = 0;         data[3] = JMAX-1;                              \
    status = EG_makeGeometry(context, PCURVE, LINE, NULL,               \
                             NULL, data, &(etemp[5]));                  \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    data[0] = 0;         data[1] = JMAX-1;                              \
    data[2] = KMAX-1;    data[3] = 0;                                   \
    status = EG_makeGeometry(context, PCURVE, LINE, NULL,               \
                             NULL, data, &(etemp[6]));                  \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    data[0] = 0;         data[1] = 0;                                   \
    data[2] = 0;         data[3] = JMAX-1;                              \
    status = EG_makeGeometry(context, PCURVE, LINE, NULL,               \
                             NULL, data , &(etemp[7]));                 \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    /* make the Loop */                                                 \
    senses[0] = SFORWARD; senses[1] = SFORWARD;                         \
    senses[2] = SREVERSE; senses[3] = SREVERSE;                         \
    senses[4] = SFORWARD; senses[5] = SFORWARD;                         \
    senses[6] = SREVERSE; senses[7] = SREVERSE;                         \
                                                                        \
    status = EG_makeTopology(context, esurfs[IFACE], LOOP, CLOSED,      \
                             NULL, 4, etemp, senses, &eloop);           \
    if (status != EGADS_SUCCESS) goto cleanup;                          \
                                                                        \
    /* make the Face */                                                 \
    status = EG_makeTopology(context, esurfs[IFACE], FACE, SFORWARD,    \
                             NULL, 1, &eloop, senses, &(efaces[IFACE])); \
    if (status != EGADS_SUCCESS) goto cleanup;


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

    int     imax, jmax, kmax, i, j, k, ijk, senses[8], periodic;
    double  *x2d=NULL, *y2d=NULL, *z2d=NULL;
    double  data[18], tdata[2];
    FILE    *fp;
    ego                ecurvs[12], esurfs[6];
    ego     enodes[8], eedges[12], efaces[6], etemp[8], eloop, eshell;

    /* default return values */
    *ebody  = NULL;
    *nMesh  = 0;
    *string = NULL;

    /* increment number of UDPs */
    numUdp++;

    /* increase the arrays to make room for the new UDP */
    ebodys = (ego    *) EG_reall(ebodys, (numUdp+1)*sizeof(ego    ));
    Imax   = (int    *) EG_reall(Imax,   (numUdp+1)*sizeof(int    ));
    Jmax   = (int    *) EG_reall(Jmax,   (numUdp+1)*sizeof(int    ));
    Kmax   = (int    *) EG_reall(Kmax,   (numUdp+1)*sizeof(int    ));
    X      = (double**) EG_reall(X,      (numUdp+1)*sizeof(double*));
    Y      = (double**) EG_reall(Y,      (numUdp+1)*sizeof(double*));
    Z      = (double**) EG_reall(Z,      (numUdp+1)*sizeof(double*));

    if (ebodys == NULL || Imax == NULL || Jmax == NULL || Kmax == NULL ||
                          X    == NULL || Y    == NULL || Z    == NULL   ) {
        return EGADS_MALLOC;
    }

    ebodys[numUdp] = NULL;

    /* data is in a file */
    if (FileName != NULL) {

        /* open the file */
        fp = fopen(FileName, "r");
        if (fp == NULL) {
            status = EGADS_NOTFOUND;
            goto cleanup;
        }

        /* read the size of the configuration */
        fscanf(fp, "%d %d %d", &imax, &jmax, &kmax);

        /* save the array size from the file */
        Imax[numUdp] = imax;
        Jmax[numUdp] = jmax;
        Kmax[numUdp] = kmax;

        /* allocate necessary arrays */
        X[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
        Y[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
        Z[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));

        if (X[numUdp] == NULL || Y[numUdp] == NULL || Z[numUdp] == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        /* read the data.  if 3d, only the outside points are read */
        for (k = 0; k < kmax; k++) {
            for (j = 0; j < jmax; j++) {
                for (i = 0; i < imax; i++) {
                    if (i == 0 || i == imax-1 ||
                        j == 0 || j == jmax-1 ||
                        k == 0 || k == kmax-1   ) {
                        ijk = (i) + imax * ((j) + jmax * (k));
                        fscanf(fp, "%lf %lf %lf", &(X[numUdp][ijk]),
                                                  &(Y[numUdp][ijk]),
                                                  &(Z[numUdp][ijk]));
                    }
                }
            }
        }

        /* close the file */
        fclose(fp);

    /* data came in XYZ */
    } else if (X[0] != NULL && Y[0] != NULL && Z[0] != NULL) {
        imax = Imax[0];
        jmax = Jmax[0];
        kmax = Kmax[0];

        Imax[numUdp] = imax;
        Jmax[numUdp] = jmax;
        Kmax[numUdp] = kmax;

        /* allocate necessary arrays */
        X[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
        Y[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
        Z[numUdp] = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));

        if (X[numUdp] == NULL || Y[numUdp] == NULL || Z[numUdp] == NULL) {
            status = EGADS_MALLOC;
            goto cleanup;
        }

        /* copy the data */
        for (ijk = 0; ijk < imax*jmax*kmax; ijk++) {
            X[numUdp][ijk] = X[0][ijk];
            Y[numUdp][ijk] = Y[0][ijk];
            Z[numUdp][ijk] = Z[0][ijk];
        }

        EG_free(X[0]);  X[0] = NULL;
        EG_free(Y[0]);  Y[0] = NULL;
        EG_free(Z[0]);  Z[0] = NULL;

    /* no data specified */
    } else {
        printf(" udpExecute: filename and xyz both null\n");
        return EGADS_NODATA;
    }

    /* allocate necessary (oversized) 2D arrays */
    x2d = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
    y2d = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));
    z2d = (double*) EG_alloc(imax*jmax*kmax*sizeof(double));

    if (x2d == NULL || y2d == NULL || z2d == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    /* create WireBody (since jmax<=1) */
    if (jmax <= 1) {
        printf("    WireBody: (%d)\n", imax);

        /* create the 2 Nodes */
        CREATE_NODE(0, 0,      0, 0);
        CREATE_NODE(1, imax-1, 0, 0);

        /* create the Curve and Edge */
        for (i = 0; i < imax; i++) {
            x2d[i] = X[numUdp][(i)+imax*((0)+jmax*(0))];
            y2d[i] = Y[numUdp][(i)+imax*((0)+jmax*(0))];
            z2d[i] = Z[numUdp][(i)+imax*((0)+jmax*(0))];
        }
        CREATE_EDGE(0, 0, 1, imax);

        /* make a Loop */
        senses[0] = SFORWARD;
        status = EG_makeTopology(context, NULL, LOOP, OPEN,
                                 NULL, 1, eedges, senses, &eloop);
        if (status != EGADS_SUCCESS) goto cleanup;

        /* make a WireBody */
        status = EG_makeTopology(context, NULL, BODY, WIREBODY,
                                 NULL, 1, &eloop, NULL, ebody);
        if (status != EGADS_SUCCESS) goto cleanup;

        /* remember this model (body) */
        ebodys[numUdp] = *ebody;

    /* create FaceBody (since kmax<=1) */
    } else if (kmax <= 1) {
        printf("    FaceBody: (%d*%d)\n", imax, jmax);

        /* make the cubic spline */
        status = spline2d(context, imax, jmax, X[numUdp], Y[numUdp], Z[numUdp], &(esurfs[0]));
        if (status != EGADS_SUCCESS) goto cleanup;

        /* make a Face */
        status = EG_getRange(esurfs[0], data, &periodic);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeFace(esurfs[0], SFORWARD, data, &(efaces[0]));
        if (status != EGADS_SUCCESS) goto cleanup;

        /* make a FaceBody */
        senses[0] = SFORWARD;
        status = EG_makeTopology(context, NULL, BODY, FACEBODY,
                                 NULL, 1, efaces, senses, ebody);
        if (status != EGADS_SUCCESS) goto cleanup;

        /* inform the user that there is 1 surface meshes */
        *nMesh = 1;

        /* remember this model (body) */
        ebodys[numUdp] = *ebody;

    /* create a SolidBody */
    } else {
        printf("    SolidBody: (%d*%d*%d)\n", imax, jmax, kmax);

        /* create the 8 Nodes */
        CREATE_NODE(0, 0,      0,      0     );
        CREATE_NODE(1, imax-1, 0,      0     );
        CREATE_NODE(2, 0,      jmax-1, 0     );
        CREATE_NODE(3, imax-1, jmax-1, 0     );
        CREATE_NODE(4, 0,      0,      kmax-1);
        CREATE_NODE(5, imax-1, 0,      kmax-1);
        CREATE_NODE(6, 0,      jmax-1, kmax-1);
        CREATE_NODE(7, imax-1, jmax-1, kmax-1);

        /* create the 12 Curves and Edges */
        for (i = 0; i < imax; i++) {
            x2d[i] = X[numUdp][(i     )+imax*((0     )+jmax*(0     ))];
            y2d[i] = Y[numUdp][(i     )+imax*((0     )+jmax*(0     ))];
            z2d[i] = Z[numUdp][(i     )+imax*((0     )+jmax*(0     ))];
        }
        CREATE_EDGE( 0, 0, 1, imax);

        for (i = 0; i < imax; i++) {
            x2d[i] = X[numUdp][(i     )+imax*((jmax-1)+jmax*(0     ))];
            y2d[i] = Y[numUdp][(i     )+imax*((jmax-1)+jmax*(0     ))];
            z2d[i] = Z[numUdp][(i     )+imax*((jmax-1)+jmax*(0     ))];
        }
        CREATE_EDGE( 1, 2, 3, imax);

        for (i = 0; i < imax; i++) {
            x2d[i] = X[numUdp][(i     )+imax*((0     )+jmax*(kmax-1))];
            y2d[i] = Y[numUdp][(i     )+imax*((0     )+jmax*(kmax-1))];
            z2d[i] = Z[numUdp][(i     )+imax*((0     )+jmax*(kmax-1))];
        }
        CREATE_EDGE( 2, 4, 5, imax);

        for (i = 0; i < imax; i++) {
            x2d[i] = X[numUdp][(i     )+imax*((jmax-1)+jmax*(kmax-1))];
            y2d[i] = Y[numUdp][(i     )+imax*((jmax-1)+jmax*(kmax-1))];
            z2d[i] = Z[numUdp][(i     )+imax*((jmax-1)+jmax*(kmax-1))];
        }
        CREATE_EDGE( 3, 6, 7, imax);

        for (j = 0; j < jmax; j++) {
            x2d[j] = X[numUdp][(0     )+imax*((j     )+jmax*(0     ))];
            y2d[j] = Y[numUdp][(0     )+imax*((j     )+jmax*(0     ))];
            z2d[j] = Z[numUdp][(0     )+imax*((j     )+jmax*(0     ))];
        }
        CREATE_EDGE( 4, 0, 2, jmax);

        for (j = 0; j < jmax; j++) {
            x2d[j] = X[numUdp][(0     )+imax*((j     )+jmax*(kmax-1))];
            y2d[j] = Y[numUdp][(0     )+imax*((j     )+jmax*(kmax-1))];
            z2d[j] = Z[numUdp][(0     )+imax*((j     )+jmax*(kmax-1))];
        }
        CREATE_EDGE( 5, 4, 6, jmax);

        for (j = 0; j < jmax; j++) {
            x2d[j] = X[numUdp][(imax-1)+imax*((j     )+jmax*(0     ))];
            y2d[j] = Y[numUdp][(imax-1)+imax*((j     )+jmax*(0     ))];
            z2d[j] = Z[numUdp][(imax-1)+imax*((j     )+jmax*(0     ))];
        }
        CREATE_EDGE( 6, 1, 3, jmax);

        for (j = 0; j < jmax; j++) {
            x2d[j] = X[numUdp][(imax-1)+imax*((j     )+jmax*(kmax-1))];
            y2d[j] = Y[numUdp][(imax-1)+imax*((j     )+jmax*(kmax-1))];
            z2d[j] = Z[numUdp][(imax-1)+imax*((j     )+jmax*(kmax-1))];
        }
        CREATE_EDGE( 7, 5, 7, jmax);

        for (k = 0; k < kmax; k++) {
            x2d[k] = X[numUdp][(0     )+imax*((0     )+jmax*(k     ))];
            y2d[k] = Y[numUdp][(0     )+imax*((0     )+jmax*(k     ))];
            z2d[k] = Z[numUdp][(0     )+imax*((0     )+jmax*(k     ))];
        }
        CREATE_EDGE( 8, 0, 4, kmax);

        for (k = 0; k < kmax; k++) {
            x2d[k] = X[numUdp][(imax-1)+imax*((0     )+jmax*(k     ))];
            y2d[k] = Y[numUdp][(imax-1)+imax*((0     )+jmax*(k     ))];
            z2d[k] = Z[numUdp][(imax-1)+imax*((0     )+jmax*(k     ))];
        }
        CREATE_EDGE( 9, 1, 5, kmax);

        for (k = 0; k < kmax; k++) {
            x2d[k] = X[numUdp][(0     )+imax*((jmax-1)+jmax*(k     ))];
            y2d[k] = Y[numUdp][(0     )+imax*((jmax-1)+jmax*(k     ))];
            z2d[k] = Z[numUdp][(0     )+imax*((jmax-1)+jmax*(k     ))];
        }
        CREATE_EDGE(10, 2, 6, kmax);

        for (k = 0; k < kmax; k++) {
            x2d[k] = X[numUdp][(imax-1)+imax*((jmax-1)+jmax*(k     ))];
            y2d[k] = Y[numUdp][(imax-1)+imax*((jmax-1)+jmax*(k     ))];
            z2d[k] = Z[numUdp][(imax-1)+imax*((jmax-1)+jmax*(k     ))];
        }
        CREATE_EDGE(11, 3, 7, kmax);

        /* create the 6 Surfaces, Pcurves, Loops, and Edges */
        for (j = 0; j < jmax; j++) {
            for (k = 0; k < kmax; k++) {
                x2d[k+j*kmax] = X[numUdp][(0     )+imax*((j     )+jmax*(k     ))];
                y2d[k+j*kmax] = Y[numUdp][(0     )+imax*((j     )+jmax*(k     ))];
                z2d[k+j*kmax] = Z[numUdp][(0     )+imax*((j     )+jmax*(k     ))];
            }
        }
        CREATE_FACE(0,  8,  5, 10,  4, jmax, kmax);

        for (k = 0; k < kmax; k++) {
            for (j = 0; j < jmax; j++) {
                x2d[j+k*jmax] = X[numUdp][(imax-1)+imax*((j     )+jmax*(k     ))];
                y2d[j+k*jmax] = Y[numUdp][(imax-1)+imax*((j     )+jmax*(k     ))];
                z2d[j+k*jmax] = Z[numUdp][(imax-1)+imax*((j     )+jmax*(k     ))];
            }
        }
        CREATE_FACE(1,  6, 11,  7,  9, kmax, jmax);

        for (k = 0; k < kmax; k++) {
            for (i = 0; i < imax; i++) {
                x2d[i+k*imax] = X[numUdp][(i     )+imax*((0     )+jmax*(k     ))];
                y2d[i+k*imax] = Y[numUdp][(i     )+imax*((0     )+jmax*(k     ))];
                z2d[i+k*imax] = Z[numUdp][(i     )+imax*((0     )+jmax*(k     ))];
            }
        }
        CREATE_FACE(2,  0,  9,  2,  8, kmax, imax);

        for (i = 0; i < imax; i++) {
            for (k = 0; k < kmax; k++) {
                x2d[k+i*kmax] = X[numUdp][(i     )+imax*((jmax-1)+jmax*(k     ))];
                y2d[k+i*kmax] = Y[numUdp][(i     )+imax*((jmax-1)+jmax*(k     ))];
                z2d[k+i*kmax] = Z[numUdp][(i     )+imax*((jmax-1)+jmax*(k     ))];
            }
        }
        CREATE_FACE(3, 10,  3, 11,  1, imax, kmax);

        for (i = 0; i < imax; i++) {
            for (j = 0; j < jmax; j++) {
                x2d[j+i*jmax] = X[numUdp][(i     )+imax*((j     )+jmax*(0     ))];
                y2d[j+i*jmax] = Y[numUdp][(i     )+imax*((j     )+jmax*(0     ))];
                z2d[j+i*jmax] = Z[numUdp][(i     )+imax*((j     )+jmax*(0     ))];
            }
        }
        CREATE_FACE(4,  4,  1,  6,  0, imax, jmax);

        for (j = 0; j < jmax; j++) {
            for (i = 0; i < imax; i++) {
                x2d[i+j*imax] = X[numUdp][(i     )+imax*((j     )+jmax*(kmax-1))];
                y2d[i+j*imax] = Y[numUdp][(i     )+imax*((j     )+jmax*(kmax-1))];
                z2d[i+j*imax] = Z[numUdp][(i     )+imax*((j     )+jmax*(kmax-1))];
            }
        }
        CREATE_FACE(5,  2,  7,  3,  5, jmax, imax);

        /* make the Shell and SolidBody */
        printf("        creating Shell\n");
        status = EG_makeTopology(context, NULL, SHELL, CLOSED,
                                 NULL, 6, efaces, NULL, &eshell);
        if (status != EGADS_SUCCESS) goto cleanup;

        printf("        creating SolidBody\n");
        status = EG_makeTopology(context, NULL, BODY, SOLIDBODY,
                                 NULL, 1, &eshell, NULL, ebody);
        if (status != EGADS_SUCCESS) goto cleanup;

        /* inform the user that there are 6 surface meshes */
        *nMesh = 6;

        /* remember this model (body) */
        ebodys[numUdp] = *ebody;
    }

cleanup:
    if (z2d != NULL) EG_free(z2d);
    if (y2d != NULL) EG_free(y2d);
    if (z2d != NULL) EG_free(x2d);

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
    int    iudp, judp, i2d, j2d, i3d, j3d, k3d;
    double *Mesh=NULL;

    *imax = 0;
    *jmax = 0;
    *kmax = 0;
    *mesh = Mesh;

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

    /* if a FaceBody, return the mesh */
    if (Imax[iudp] > 1 && Jmax[iudp] > 1 && Kmax[iudp] == 1) {

        /* mesh 1: i3d = i2d; j3d = j2d; k3d = 0 */
        if (iMesh == 1) {
            Mesh = (double *) EG_alloc(3*Imax[iudp]*Jmax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;

            *imax = Imax[iudp];
            *jmax = Jmax[iudp];
            *kmax = Kmax[iudp];
            *mesh = Mesh;

            for (j3d = 0; j3d < Jmax[iudp]; j3d++) {
                for (i3d = 0; i3d < Imax[iudp]; i3d++) {
                    i2d = i3d;
                    j2d = j3d;

                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*j3d];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*j3d];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*j3d];
                }
            }
        } else {
            *imax = 0;
            *jmax = 0;
            *kmax = 0;
            *mesh = NULL;
            
            return EGADS_INDEXERR;
        }

    /* if a SolidBody, return one of the meshes */
    } else if (Imax[iudp] > 1 && Jmax[iudp] > 1 && Kmax[iudp] > 1) {

        /* mesh 1: i3d = 0; j3d = j2d; k3d = i2d */
        if        (iMesh == 1) {
            Mesh = (double *) EG_alloc(3*Jmax[iudp]*Kmax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Kmax[iudp];
            *jmax = Jmax[iudp];
            *kmax = 1;
            *mesh = Mesh;

            for (k3d = 0; k3d < Kmax[iudp]; k3d++) {
                for (j3d = 0; j3d < Jmax[iudp]; j3d++) {
                    i3d = 0;

                    i2d = k3d;
                    j2d = j3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }

        /* mesh 2: i3d = Imax-1; j3d = i2d; k3d = j2d */
        } else if (iMesh == 2) {
            Mesh = (double *) EG_alloc(3*Jmax[iudp]*Kmax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Jmax[iudp];
            *jmax = Kmax[iudp];
            *kmax = 1;
            *mesh = Mesh;
            
            for (k3d = 0; k3d < Kmax[iudp]; k3d++) {
                for (j3d = 0; j3d < Jmax[iudp]; j3d++) {
                    i3d = Imax[iudp] - 1;

                    i2d = j3d;
                    j2d = k3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }

        /* mesh 3: i3d = i2d; j3d = 0; k3d = j2d */
        } else if (iMesh == 3) {
            Mesh = (double *) EG_alloc(3*Kmax[iudp]*Imax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Imax[iudp];
            *jmax = Kmax[iudp];
            *kmax = 1;
            *mesh = Mesh;
            
            for (i3d = 0; i3d < Imax[iudp]; i3d++) {
                for (k3d = 0; k3d < Kmax[iudp]; k3d++) {
                    j3d = 0;

                    i2d = i3d;
                    j2d = k3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }

        /* mesh 4: i3d = j2d; j3d = Jmax-1; k3d = i2d */
        } else if (iMesh == 4) {
            Mesh = (double *) EG_alloc(3*Kmax[iudp]*Imax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Kmax[iudp];
            *jmax = Imax[iudp];
            *kmax = 1;
            *mesh = Mesh;
            
            for (i3d = 0; i3d < Imax[iudp]; i3d++) {
                for (k3d = 0; k3d < Kmax[iudp]; k3d++) {
                    j3d = Jmax[iudp] - 1;

                    i2d = k3d;
                    j2d = i3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }

        /* mesh 5: i3d = j2d; j3d = i2d; k3d = 0 */
        } else if (iMesh == 5) {
            Mesh = (double *) EG_alloc(3*Imax[iudp]*Jmax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Jmax[iudp];
            *jmax = Imax[iudp];
            *kmax = 1;
            *mesh = Mesh;
            
            for (j3d = 0; j3d < Jmax[iudp]; j3d++) {
                for (i3d = 0; i3d < Imax[iudp]; i3d++) {
                    k3d = 0;

                    i2d = j3d;
                    j2d = i3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }

        /* mesh 6: i3d = i2d; j3d = j2d; k3d = Kmax-1 */
        } else if (iMesh == 6) {
            Mesh = (double *) EG_alloc(3*Imax[iudp]*Jmax[iudp]*sizeof(double));
            if (Mesh == NULL) return EGADS_MALLOC;
            
            *imax = Imax[iudp];
            *jmax = Jmax[iudp];
            *kmax = 1;
            *mesh = Mesh;
            
            for (j3d = 0; j3d < Jmax[iudp]; j3d++) {
                for (i3d = 0; i3d < Imax[iudp]; i3d++) {
                    k3d = Kmax[iudp] - 1;

                    i2d = i3d;
                    j2d = j3d;
                    
                    Mesh[3*(i2d+j2d*(*imax))  ] = X[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+1] = Y[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                    Mesh[3*(i2d+j2d*(*imax))+2] = Z[iudp][i3d+Imax[iudp]*(j3d+Jmax[iudp]*k3d)];
                }
            }
        } else {
            *imax = 0;
            *jmax = 0;
            *kmax = 0;
            *mesh = NULL;
            
            return EGADS_INDEXERR;
        }

    /* neither FaceBody nor SolidBody were loaded */
    } else {
        *imax = 0;
        *jmax = 0;
        *kmax = 0;
        *mesh = NULL;
            
        return EGADS_NOTBODY;
    }

    return EGADS_SUCCESS;
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


/*
 ************************************************************************
 *                                                                      *
 *   spline1d - create 1d cubic spline (uniform spacing, fixed ends)    *
 *                                                                      *
 ************************************************************************
 */

static int
spline1d(ego    context,
         int    imax,
         double *x,
         double *y,
         double *z,
         ego    *ecurv)
{
    int    status = EGADS_SUCCESS;

    int    i, kk, iknot, icp, iter, niter, header[4];
    double *cp=NULL, dx, dy, dz, data[18], dxyzmax;
    double dxyztol = 1.0e-7;

    icp   = imax + 2;
    iknot = imax + 6;

    cp     = (double*) EG_alloc((iknot+3*icp)*sizeof(double));

    if (cp == NULL) {
        status = EGADS_MALLOC;
        goto cleanup;
    }

    /* create spline curve */
    header[0] = 0;
    header[1] = 3;
    header[2] = icp;
    header[3] = iknot;

    kk = 0;

    /* knots (equally spaced) */
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;

    for (i = 1; i < imax; i++) {
        cp[kk++] = i;
    }

    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;

    /* initial control point */
    cp[kk++] = x[0];
    cp[kk++] = y[0];
    cp[kk++] = z[0];

    /* initial interior control point (for slope) */
    cp[kk++] = (3 * x[0] + x[1]) / 4;
    cp[kk++] = (3 * y[0] + y[1]) / 4;
    cp[kk++] = (3 * z[0] + z[1]) / 4;

    /* interior control points */
    for (i = 1; i < imax-1; i++) {
        cp[kk++] = x[i];
        cp[kk++] = y[i];
        cp[kk++] = z[i];
    }

    /* penultimate interior control point (for slope) */
    cp[kk++] = (3 * x[imax-1] + x[imax-2]) / 4;
    cp[kk++] = (3 * y[imax-1] + y[imax-2]) / 4;
    cp[kk++] = (3 * z[imax-1] + z[imax-2]) / 4;

    /* final control point */
    cp[kk++] = x[imax-1];
    cp[kk++] = y[imax-1];
    cp[kk++] = z[imax-1];

    /* make the original BSPLINE (based upon the assumed control points) */
    status = EG_makeGeometry(context, CURVE, BSPLINE, NULL,
                             header, cp, ecurv);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* iterate to have knot evaluations match data points */
    niter = 10000;
    for (iter = 0; iter < niter; iter++) {
        dxyzmax = 0;

        /* match interior spline points */
        for (i = 1; i < imax-1; i++) {
            status = EG_evaluate(*ecurv, &(cp[i+3]), data);
            if (status != EGADS_SUCCESS) goto cleanup;

            dx = x[i] - data[0];
            dy = y[i] - data[1];
            dz = z[i] - data[2];

            if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
            if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
            if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

            cp[iknot+3*i+3] += dx;
            cp[iknot+3*i+4] += dy;
            cp[iknot+3*i+5] += dz;
        }

        /* convergence check */
        if (dxyzmax < dxyztol) break;

        /* make the new curve (after deleting old one) */
        status = EG_deleteObject(*ecurv);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeGeometry(context, CURVE, BSPLINE, NULL,
                                 header, cp, ecurv);
        if (status != EGADS_SUCCESS) goto cleanup;
    }

cleanup:
        EG_free(cp);

        return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   spline2d - create 2d cubic spline (uniform spacing, fixed ends)    *
 *                                                                      *
 ************************************************************************
 */

static int
spline2d(ego    context,
         int    imax,
         int    jmax,
         double *x,
         double *y,
         double *z,
         ego    *esurf)
{
    int    status = EGADS_SUCCESS;

    int    i, j, kk, iknot, jknot, icp, jcp, iter, niter, header[7];
    double *cp=NULL, dx, dy, dz, data[18], dxyzmax, parms[2];
    double dxyztol = 1.0e-7;

    icp   = imax + 2;
    iknot = imax + 6;
    jcp   = jmax + 2;
    jknot = jmax + 6;

    cp = (double*) EG_alloc((iknot+jknot+3*icp*jcp)*sizeof(double));

    if (cp == NULL) {
        status  = EGADS_MALLOC;
        goto cleanup;
    }

    /* create spline surface */
    header[0] = 0;
    header[1] = 3;
    header[2] = icp;
    header[3] = iknot;
    header[4] = 3;
    header[5] = jcp;
    header[6] = jknot;

    kk = 0;

    /* knots in i-direction (equally spaced) */
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;

    for (i = 1; i < imax; i++) {
        cp[kk++] = i;
    }

    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;

    /* knots in j-direction (equally spaced) */
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;
    cp[kk++] = 0;

    for (j = 1; j < jmax; j++) {
        cp[kk++] = j;
    }

    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;
    cp[kk] = cp[kk-1]; kk++;

    /* map of point ID for imax=9 and jmax=5 (used in comments below)

             nw O  n  n  n  n  n  n  n  P ne
             J  K  L  L  L  L  L  L  L  M  N
             w  H  *  *  *  *  *  *  *  I  e
             w  H  *  *  *  *  *  *  *  I  e
             w  H  *  *  *  *  *  *  *  I  e
             C  D  E  E  E  E  E  E  E  F  G
             sw A  s  s  s  s  s  s  s  B se
    */

    /* southwest control point */
    cp[kk++] = x[(0)+(0)*imax];
    cp[kk++] = y[(0)+(0)*imax];
    cp[kk++] = z[(0)+(0)*imax];

    /* point A */
    cp[kk++] = (3 * x[(0)+(0)*imax] + x[(1)+(0)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(0)*imax] + y[(1)+(0)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(0)*imax] + z[(1)+(0)*imax]) / 4;

    /* south control points */
    for (i = 1; i < imax-1; i++) {
        cp[kk++] = x[(i)+(0)*imax];
        cp[kk++] = y[(i)+(0)*imax];
        cp[kk++] = z[(i)+(0)*imax];
    }

    /* point B */
    cp[kk++] = (3 * x[(imax-1)+(0)*imax] + x[(imax-2)+(0)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(0)*imax] + y[(imax-2)+(0)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(0)*imax] + z[(imax-2)+(0)*imax]) / 4;

    /* southeast control point */
    cp[kk++] = x[(imax-1)+(0)*imax];
    cp[kk++] = y[(imax-1)+(0)*imax];
    cp[kk++] = z[(imax-1)+(0)*imax];

    /* point C */
    cp[kk++] = (3 * x[(0)+(0)*imax] + x[(0)+(1)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(0)*imax] + y[(0)+(1)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(0)*imax] + z[(0)+(1)*imax]) / 4;

    /* point D */
    cp[kk++] = (3 * x[(0)+(0)*imax] + x[(1)+(1)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(0)*imax] + y[(1)+(1)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(0)*imax] + z[(1)+(1)*imax]) / 4;

    /* points E */
    for (i = 1; i < imax-1; i++) {
        cp[kk++] = (3 * x[(i)+(0)*imax] + x[(i)+(1)*imax]) / 4;
        cp[kk++] = (3 * y[(i)+(0)*imax] + y[(i)+(1)*imax]) / 4;
        cp[kk++] = (3 * z[(i)+(0)*imax] + z[(i)+(1)*imax]) / 4;
    }

    /* point F */
    cp[kk++] = (3 * x[(imax-1)+(0)*imax] + x[(imax-2)+(1)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(0)*imax] + y[(imax-2)+(1)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(0)*imax] + z[(imax-2)+(1)*imax]) / 4;

    /* point G */
    cp[kk++] = (3 * x[(imax-1)+(0)*imax] + x[(imax-1)+(1)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(0)*imax] + y[(imax-1)+(1)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(0)*imax] + z[(imax-1)+(1)*imax]) / 4;

    /* loop through interior j lines */
    for (j = 1; j < jmax-1; j++) {

        /* west control point */
        cp[kk++] = x[(0)+(j)*imax];
        cp[kk++] = y[(0)+(j)*imax];
        cp[kk++] = z[(0)+(j)*imax];

        /* point H */
        cp[kk++] = (3 * x[(0)+(j)*imax] + x[(1)+(j)*imax]) / 4;
        cp[kk++] = (3 * y[(0)+(j)*imax] + y[(1)+(j)*imax]) / 4;
        cp[kk++] = (3 * z[(0)+(j)*imax] + z[(1)+(j)*imax]) / 4;

        /* interior points */
        for (i = 1; i < imax-1; i++) {
            cp[kk++] = x[(i)+(j)*imax];
            cp[kk++] = y[(i)+(j)*imax];
            cp[kk++] = z[(i)+(j)*imax];
        }

        /* point I */
        cp[kk++] = (3 * x[(imax-1)+(j)*imax] + x[(imax-2)+(j)*imax]) / 4;
        cp[kk++] = (3 * y[(imax-1)+(j)*imax] + y[(imax-2)+(j)*imax]) / 4;
        cp[kk++] = (3 * z[(imax-1)+(j)*imax] + z[(imax-2)+(j)*imax]) / 4;

        /* east control point */
        cp[kk++] = x[(imax-1)+(j)*imax];
        cp[kk++] = y[(imax-1)+(j)*imax];
        cp[kk++] = z[(imax-1)+(j)*imax];
    }

    /* point J */
    cp[kk++] = (3 * x[(0)+(jmax-1)*imax] + x[(0)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(jmax-1)*imax] + y[(0)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(jmax-1)*imax] + z[(0)+(jmax-2)*imax]) / 4;

    /* point K */
    cp[kk++] = (3 * x[(0)+(jmax-1)*imax] + x[(1)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(jmax-1)*imax] + y[(1)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(jmax-1)*imax] + z[(1)+(jmax-2)*imax]) / 4;

    /* points L */
    for (i = 1; i < imax-1; i++) {
        cp[kk++] = (3 * x[(i)+(jmax-1)*imax] + x[(i)+(jmax-2)*imax]) / 4;
        cp[kk++] = (3 * y[(i)+(jmax-1)*imax] + y[(i)+(jmax-2)*imax]) / 4;
        cp[kk++] = (3 * z[(i)+(jmax-1)*imax] + z[(i)+(jmax-2)*imax]) / 4;
    }

    /* point M */
    cp[kk++] = (3 * x[(imax-1)+(jmax-1)*imax] + x[(imax-2)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(jmax-1)*imax] + y[(imax-2)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(jmax-1)*imax] + z[(imax-2)+(jmax-2)*imax]) / 4;

    /* point N */
    cp[kk++] = (3 * x[(imax-1)+(jmax-1)*imax] + x[(imax-1)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(jmax-1)*imax] + y[(imax-1)+(jmax-2)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(jmax-1)*imax] + z[(imax-1)+(jmax-2)*imax]) / 4;

    /* northwest control point */
    cp[kk++] = x[(0)+(jmax-1)*imax];
    cp[kk++] = y[(0)+(jmax-1)*imax];
    cp[kk++] = z[(0)+(jmax-1)*imax];

    /* point O */
    cp[kk++] = (3 * x[(0)+(jmax-1)*imax] + x[(1)+(jmax-1)*imax]) / 4;
    cp[kk++] = (3 * y[(0)+(jmax-1)*imax] + y[(1)+(jmax-1)*imax]) / 4;
    cp[kk++] = (3 * z[(0)+(jmax-1)*imax] + z[(1)+(jmax-1)*imax]) / 4;

    /* north control points */
    for (i = 1; i < imax-1; i++) {
        cp[kk++] = x[(i)+(jmax-1)*imax];
        cp[kk++] = y[(i)+(jmax-1)*imax];
        cp[kk++] = z[(i)+(jmax-1)*imax];
    }

    /* point P */
    cp[kk++] = (3 * x[(imax-1)+(jmax-1)*imax] + x[(imax-2)+(jmax-1)*imax]) / 4;
    cp[kk++] = (3 * y[(imax-1)+(jmax-1)*imax] + y[(imax-2)+(jmax-1)*imax]) / 4;
    cp[kk++] = (3 * z[(imax-1)+(jmax-1)*imax] + z[(imax-2)+(jmax-1)*imax]) / 4;

    /* northeast control point */
    cp[kk++] = x[(imax-1)+(jmax-1)*imax];
    cp[kk++] = y[(imax-1)+(jmax-1)*imax];
    cp[kk++] = z[(imax-1)+(jmax-1)*imax];

    /* make the original BSPLINE (based upon the assumed control points) */
    status = EG_makeGeometry(context, SURFACE, BSPLINE, NULL,
                             header, cp, esurf);
    if (status != EGADS_SUCCESS) goto cleanup;

    /* iterate to have know evaluations match data points */
    niter = 10000;
    for (iter = 0; iter < niter; iter++) {
        dxyzmax = 0;

        /* match interior spline points */
        for (j = 1; j < jmax-1; j++) {
            for (i = 1; i < imax-1; i++) {
                parms[0] = cp[      i+3];
                parms[1] = cp[iknot+j+3];

                status = EG_evaluate(*esurf, parms, data);
                if (status != EGADS_SUCCESS) goto cleanup;

                dx = x[(i)+(j)*imax] - data[0];
                dy = y[(i)+(j)*imax] - data[1];
                dz = z[(i)+(j)*imax] - data[2];

                if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
                if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
                if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

                cp[iknot+jknot+3*((i+1)+(j+1)*(imax+2))  ] += dx;
                cp[iknot+jknot+3*((i+1)+(j+1)*(imax+2))+1] += dy;
                cp[iknot+jknot+3*((i+1)+(j+1)*(imax+2))+2] += dz;
            }
        }

        /* match south points */
        for (i = 1; i < imax-1; i++) {
            parms[0] = cp[i      +3];
            parms[1] = cp[iknot  +3];

            status = EG_evaluate(*esurf, parms, data);
            if (status != EGADS_SUCCESS) goto cleanup;

            dx = x[(i)+(0)*imax] - data[0];
            dy = y[(i)+(0)*imax] - data[1];
            dz = z[(i)+(0)*imax] - data[2];

            if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
            if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
            if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

            cp[iknot+jknot+3*((i+1)+(0)*(imax+2))  ] += dx;
            cp[iknot+jknot+3*((i+1)+(0)*(imax+2))+1] += dy;
            cp[iknot+jknot+3*((i+1)+(0)*(imax+2))+2] += dz;
        }

        /* match north points */
        for (i = 1; i < imax-1; i++) {
            parms[0] = cp[i         +3];
            parms[1] = cp[iknot+jmax+2];

            status = EG_evaluate(*esurf, parms, data);
            if (status != EGADS_SUCCESS) goto cleanup;

            dx = x[(i)+(jmax-1)*imax] - data[0];
            dy = y[(i)+(jmax-1)*imax] - data[1];
            dz = z[(i)+(jmax-1)*imax] - data[2];

            if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
            if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
            if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

            cp[iknot+jknot+3*((i+1)+(jmax+1)*(imax+2))  ] += dx;
            cp[iknot+jknot+3*((i+1)+(jmax+1)*(imax+2))+1] += dy;
            cp[iknot+jknot+3*((i+1)+(jmax+1)*(imax+2))+2] += dz;
        }

        /* match west points */
        for (j = 1; j < jmax-1; j++) {
            parms[0] = cp[       +3];
            parms[1] = cp[iknot+j+3];

            status = EG_evaluate(*esurf, parms, data);
            if (status != EGADS_SUCCESS) goto cleanup;

            dx = x[(0)+(j)*imax] - data[0];
            dy = y[(0)+(j)*imax] - data[1];
            dz = z[(0)+(j)*imax] - data[2];

            if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
            if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
            if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

            cp[iknot+jknot+3*((0)+(j+1)*(imax+2))  ] += dx;
            cp[iknot+jknot+3*((0)+(j+1)*(imax+2))+1] += dy;
            cp[iknot+jknot+3*((0)+(j+1)*(imax+2))+2] += dz;
        }

        /* match east points */
        for (j = 1; j < jmax-1; j++) {
            parms[0] = cp[imax   +2];
            parms[1] = cp[iknot+j+3];

            status = EG_evaluate(*esurf, parms, data);
            if (status != EGADS_SUCCESS) goto cleanup;

            dx = x[(imax-1)+(j)*imax] - data[0];
            dy = y[(imax-1)+(j)*imax] - data[1];
            dz = z[(imax-1)+(j)*imax] - data[2];

            if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
            if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
            if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

            cp[iknot+jknot+3*((i+2)+(j+1)*(imax+2))  ] += dx;
            cp[iknot+jknot+3*((i+2)+(j+1)*(imax+2))+1] += dy;
            cp[iknot+jknot+3*((i+2)+(j+1)*(imax+2))+2] += dz;
        }

        /* convergence check */
        if (dxyzmax < dxyztol) break;

        /* make the new curve (after deleting old one) */
        status = EG_deleteObject(*esurf);
        if (status != EGADS_SUCCESS) goto cleanup;

        status = EG_makeGeometry(context, SURFACE, BSPLINE, NULL,
                                 header, cp, esurf);
        if (status != EGADS_SUCCESS) goto cleanup;
    }

cleanup:
    if (cp != NULL) EG_free(cp);

    return status;
}

#endif
