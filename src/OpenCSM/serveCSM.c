/*
 ************************************************************************
 *                                                                      *
 * serveCSM.c -- server for driving OpenCSM                             *
 *                                                                      *
 *              Written by John Dannenhoffer @ Syracuse University      *
 *                                                                      *
 ************************************************************************
 */

/*
 * Copyright (C) 2012  John F. Dannenhoffer, III (Syracuse University)
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
#include <unistd.h>
#include <math.h>
#include <time.h>

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <winsock2.h>
#else
    #include <X11/Xlib.h>
#endif

#if   defined(GEOM_CAPRI)
    #include "capri.h"

    #define CINT    int
    #define CDOUBLE double
    #define CCHAR   char
#elif defined(GEOM_EGADS)
    #include "egads.h"

    #define CINT    const int
    #define CDOUBLE const double
    #define CCHAR   const char
#else
    #error no_geometry_system_specified
#endif

#include "common.h"
#include "OpenCSM.h"

#include "wsserver.h"

/***********************************************************************/
/*                                                                     */
/* macros (including those that go along with common.h)                */
/*                                                                     */
/***********************************************************************/

#ifdef DEBUG
   #define DOPEN {if (dbg_fp == NULL) dbg_fp = fopen("serveCSM.dbg", "w");}
   static  FILE *dbg_fp = NULL;
#endif

//static void *realloc_temp = NULL;            /* used by RALLOC macro */

#define  RED(COLOR)      (float)(COLOR / 0x10000        ) / (float)(255)
#define  GREEN(COLOR)    (float)(COLOR / 0x00100 % 0x100) / (float)(255)
#define  BLUE(COLOR)     (float)(COLOR           % 0x100) / (float)(255)

#define MAX_EXPR_LEN     128
#define MAX_STR_LEN    32767

/***********************************************************************/
/*                                                                     */
/* structures                                                          */
/*                                                                     */
/***********************************************************************/


/***********************************************************************/
/*                                                                     */
/* global variables                                                    */
/*                                                                     */
/***********************************************************************/

/* global variable holding a MODL */
static void       *modl;               /* pointer to MODL */
static int        outLevel = 1;        /* default output level */

/* global variables associated with graphical user interface (gui) */
static wvContext  *cntxt;              /* context for the WebViewer */
static int         port   = 7681;      /* port number */

/* global variables associated with undo */
#define MAX_UNDOS  100
static int         nundo  = 0;         /* number of undos */
static void       *undo_modl[MAX_UNDOS+1];
static char        undo_text[MAX_UNDOS+1][32];

/* global variables associated with scene graph meta-data */
#define MAX_METADATA_LENGTH 32000
static char        sgMetaData[MAX_METADATA_LENGTH];

/* global variables associated with bodyList */
#define MAX_BODYS        999
static int        nbody    = 0;        /* number of built Bodys */
static int        bodyList[MAX_BODYS]; /* array  of built Bodys */

/* global variables associated with journals */
static FILE       *jrnl_out = NULL;    /* output journal file */

/***********************************************************************/
/*                                                                     */
/* declarations                                                        */
/*                                                                     */
/***********************************************************************/

/* declarations for high-level routines defined below */
#if   defined(GEOM_CAPRI)
       int        CAPRI_MAIN(int argc, char *argv[]);
#elif defined(GEOM_EGADS)
#endif

/* declarations for support routines defined below */
static int        buildBodys(int buildTo, int *builtTo, int *buildStatus);
static int        storeUndo(char *cmd, char *arg);

/* declarations for graphic routines defined below */
static int        buildSceneGraph();
       void       browserMessage(void *wsi, char *text, /*@unused@*/ int lena);
       void       processMessage(char *text, char*response);
static int        getToken(char *text, int nskip, char *token);

/* external functions not declared in an .h file */
#if   defined(GEOM_CAPRI)
    extern void   *gi_alloc(int nbtyes);
    extern void   *gi_reall(void *old, int nbytes);
    extern void   gi_free(void *ptr);
#elif defined(GEOM_EGADS)
#endif


/***********************************************************************/
/*                                                                     */
/*   CAPRI_MAIN - main program                                         */
/*                                                                     */
/***********************************************************************/

#if   defined(GEOM_CAPRI)
    int
    CAPRI_MAIN(int       argc,          /* (in)  number of arguments */
               char      *argv[])       /* (in)  array of arguments */
#elif defined(GEOM_EGADS)
    int
    main(int       argc,                /* (in)  number of arguments */
         char      *argv[])             /* (in)  array of arguments */
#endif
{

    int       imajor, iminor, status, i, bias, showUsage=0;
    int       builtTo, buildStatus;
    float     fov, zNear, zFar;
    float     eye[3]    = {0.0, 0.0, 7.0};
    float     center[3] = {0.0, 0.0, 0.0};
    float     up[3]     = {0.0, 1.0, 0.0};
    char      casename[257], filename[257], jrnlname[257], tempname[257];
    char      text[MAX_STR_LEN], response[MAX_STR_LEN], *wv_start;
    FILE      *jrnl_in = NULL;

    #if   defined(GEOM_CAPRI)
        double    box[6];
    #elif defined(GEOM_EGADS)
    #endif

    clock_t   old_time, new_time;

    ROUTINE(MAIN);

    /* --------------------------------------------------------------- */

    DPRINT0("starting serveCSM");

    /* get the flags and casename(s) from the command line */
    casename[0] = '\0';
    jrnlname[0] = '\0';

    for (i = 1; i < argc; i++) {
        if        (strcmp(argv[i], "-port") == 0) {
            if (i < argc-1) {
                sscanf(argv[++i], "%d", &port);
            } else {
                showUsage = 1;
                break;
            }
        } else if (strcmp(argv[i], "-jrnl") == 0) {
            if (i < argc-1) {
                strcpy(jrnlname, argv[++i]);
            } else {
                showUsage = 1;
                break;
            }
        } else if (strcmp(argv[i], "-outLevel") == 0) {
            if (i < argc-1) {
                sscanf(argv[++i], "%d", &outLevel);
                if (outLevel < 0) outLevel = 0;
                if (outLevel > 3) outLevel = 3;
            } else {
                showUsage = 1;
                break;
            }
        } else if (strlen(casename) == 0) {
            strcpy(casename, argv[i]);
        } else {
            SPRINT0(0, "two casenames given");
            showUsage = 1;
            break;
        }
    }

    if (showUsage) {
        SPRINT0(0, "proper usage: 'serveCSM [-port X] [-jrnl jrnlname] [-outLevel X] [casename[.csm]]'");
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* welcome banner */
    (void) ocsmVersion(&imajor, &iminor);

    SPRINT0(1, "**********************************************************");
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "*                    Program serveCSM                    *");
    SPRINT2(1, "*                     version %2d.%02d                      *", imajor, iminor);
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "*        written by John Dannenhoffer, 2010/2012         *");
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "**********************************************************");

    /* set OCSMs output level */
    (void) ocsmSetOutLevel(outLevel);

    /* strip off .csm (which is assumed to be at the end) if present */
    if (strlen(casename) > 0) {
        strcpy(filename, casename);
        if (strstr(casename, ".csm") == NULL) {
            strcat(filename, ".csm");
        }
    } else {
        casename[0] = '\0';
    }

    #if   defined(GEOM_CAPRI)
        /* start CAPRI */
        gi_uRegister();

        status = gi_uStart();
        SPRINT1(1, "--> gi_uStart() -> status=%d", status);

        if (status < SUCCESS) {
            SPRINT0(0, "problem detected while starting CAPRI");
            SPRINT0(0, "STOPPING...\a");
            exit(0);
        }
    #elif defined(GEOM_EGADS)
    #endif

    #if   defined(GEOM_CAPRI)
        /* make a "throw-away" volume so that CAPRI's startup
           message does not get produced during code below */
        box[0] = 0.0; box[1] = 0.0; box[2] = 0.0;
        box[3] = 1.0; box[4] = 1.0; box[5] = 1.0;
        status = gi_gCreateVolume(NULL, "Parasolid", 1, box);
        SPRINT1(1, "--> gi_gCreateVolume(dummy) -> status=%d", status);
    #elif defined(GEOM_EGADS)
    #endif

    /* read the .csm file and create the MODL */
    old_time = clock();
    status   = ocsmLoad(filename, &modl);
    new_time = clock();
    SPRINT3(1, "--> ocsmLoad(%s) -> status=%d (%s)", filename, status, ocsmGetText(status));
    SPRINT1(1, "==> ocsmLoad CPUtime=%9.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < SUCCESS) {
        SPRINT0(0, "problem in ocsmLoad");
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* check that Branches are properly ordered */
    old_time = clock();
    status   = ocsmCheck(modl);
    new_time = clock();
    SPRINT2(0, "--> ocsmCheck() -> status=%d (%s)",
            status, ocsmGetText(status));
    SPRINT1(0, "==> ocsmCheck CPUtime=%10.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < SUCCESS) {
        SPRINT0(0, "problem in ocsmCheck");
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* open the output journal file */
    sprintf(tempname, "port%d.jrnl", port);
    jrnl_out = fopen(tempname, "w");

    /* initialize the scene graph meta data */
    sgMetaData[0] = '\0';

    /* create the WebViewer context */
    bias  =  1;
    fov   = 30.0;
    zNear =  1.0;
    zFar  = 10.0;
    cntxt = wv_createContext(bias, fov, zNear, zFar, eye, center, up);
    if (cntxt == NULL) {
        SPRINT0(0, "failed to create wvContext");
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* build the Bodys from the MODL */
    status = buildBodys(0, &builtTo, &buildStatus);

    if (builtTo < 0) {
        modl_T  *MODL = (modl_T*)modl;
        SPRINT2(0, "build() detected \"%s\" in %s",
                ocsmGetText(buildStatus), MODL->brch[1-builtTo].name);
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    } else if (status != SUCCESS) {
        SPRINT1(0, "build() detected \"%s\"",
                ocsmGetText(buildStatus));
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* process the input journal file if jrnlname exists */
    if (strlen(jrnlname) > 0) {
        SPRINT1(0, "\n==> Opening input journal file \"%s\"\n", jrnlname);

        jrnl_in = fopen(jrnlname, "r");
        if (jrnl_in == NULL) {
            SPRINT0(0, "Journal file cannot be opened");
            SPRINT0(0, "STOPPING...\a");
            exit(0);
        } else {
            while (!feof(jrnl_in)) {
                (void)fgets(text, MAX_STR_LEN, jrnl_in);

                text[strlen(text)-1] = '\0';

                processMessage(text, response);
            }

            fclose(jrnl_in);

            SPRINT0(0, "\n==> Closing input journal file\n");
        }
    }

    /* get the command to start the client (if any) */
    wv_start = getenv("WV_START");

    /* start the server */
    status = SUCCESS;
    if (wv_startServer(port, NULL, NULL, NULL, 0, cntxt) == 0) {

        /* stay alive a long as we have a client */
        while (wv_statusServer(0)) {
            usleep(500000);

            /* start the browser if the first time through this loop */
            if (status == SUCCESS) {
                if (wv_start != NULL) {
                    system(wv_start);
                }

                status++;
            }
        }
    }

    /* cleanup and exit */
    fclose(jrnl_out);

    status = ocsmFree(modl);
    SPRINT2(1, "--> ocsmFree() -> status=%d (%s)", status, ocsmGetText(status));

    wv_cleanupServers();
    status = SUCCESS;

    SPRINT0(0, "==> serveCSM completed successfully");

//cleanup:
    return status;
}


/***********************************************************************/
/*                                                                     */
/*   buildBodys - build Bodys and update scene graph                   */
/*                                                                     */
/***********************************************************************/

static int
buildBodys(int     buildTo,             /* (in)  last Branch to execute */
           int     *builtTo,            /* (out) last Branch successfully executed */
           int     *buildStatus)        /* (out) status returned from build */
{
    int       status = SUCCESS;         /* return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ibody, jbody;
    double    box[6], size, params[3];
    clock_t   old_time, new_time;

    #if   defined(GEOM_CAPRI)
        int       ivol, numvol;
    #elif defined(GEOM_EGADS)
        ego       ebody;
    #endif

    ROUTINE(buildBodys);

    /* --------------------------------------------------------------- */

    /* remove previous Bodys (if they exist) */
    #if   defined(GEOM_CAPRI)
        numvol = gi_uNumVolumes();

        if (numvol <= 0) {
            SPRINT0(0, "--> no volumes to release");
        } else {
            for (ivol = 1; ivol <= numvol; ivol++) {
                status = gi_uStatModel(ivol);
                if (status >= SUCCESS) {
                    status = gi_uRelModel(ivol);
                    SPRINT2(0, "--> gi_uRelModel -> status=%d (%s)",
                            status, ocsmGetText(status));
                }
            }
        }
    #elif defined(GEOM_EGADS)
        if (MODL->context != NULL) {
            status = EG_setOutLevel(MODL->context, 0);
            status = EG_close(MODL->context);
            SPRINT2(0, "--> EG_close() -> status=%d (%s)",
                    status, ocsmGetText(status));

            MODL->context = NULL;
        }
    #endif

    /* if there are no Branches, simply return */
    if (MODL->nbrch <= 0) {
        SPRINT0(1, "--> No Branches, so skipping build");
        *builtTo     = 0;
        *buildStatus = SUCCESS;

    } else {
        /* check that Branches are properly ordered */
        old_time = clock();
        status   = ocsmCheck(modl);
        new_time = clock();
        SPRINT2(0, "--> ocsmCheck() -> status=%d (%s)",
                status, ocsmGetText(status));
        SPRINT1(0, "==> ocsmCheck CPUtime=%10.3f sec",
                (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

        if (status < SUCCESS) {
            goto cleanup;
        }

        /* build the Bodys */
        nbody    = MAX_BODYS;
        old_time = clock();
        status   = ocsmBuild(modl, buildTo, builtTo, &nbody, bodyList);
        new_time = clock();
        SPRINT5(0, "--> ocsmBuild(buildTo=%d) -> status=%d (%s), builtTo=%d, nbody=%d",
                buildTo, status, ocsmGetText(status), *builtTo, nbody);
        SPRINT1(0, "==> ocsmBuild CPUtime=%10.3f sec",
                (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

        *buildStatus = status;

        if (status < SUCCESS) {
            goto cleanup;
        }

        /* tessellate the Bodys */
        old_time = clock();
        for (jbody = 0; jbody < nbody; jbody++) {
            ibody = bodyList[jbody];

            #if   defined(GEOM_CAPRI)
                ivol   = MODL->body[ibody].ivol;
                status = gi_dBox(ivol, box);
            #elif defined(GEOM_EGADS)
                ebody  = MODL->body[ibody].ebody;
                status = EG_getBoundingBox(ebody, box);
            #endif

            size   = sqrt(pow(box[3]-box[0],2) + pow(box[4]-box[1],2)
                                               + pow(box[5]-box[2],2));

            /* vTess parameters */
            params[0] = 0.0250 * size;
            params[1] = 0.0010 * size;
            params[2] = 15.0;

            #if   defined(GEOM_CAPRI)
                SPRINT0(0, "--> default tessellation used");
            #elif defined(GEOM_EGADS)
                ebody = MODL->body[ibody].ebody;

                (void) EG_setOutLevel(MODL->context, 0);
                status = EG_makeTessBody(ebody, params, &(MODL->body[ibody].etess));
                (void) EG_setOutLevel(MODL->context, outLevel);
                SPRINT6(0, "--> EG_makeTessBody(ibody=%4d, params=%10.5f, %10.5f, %10.5f) -> status=%d (%s)",
                        ibody, params[0], params[1], params[2], status, ocsmGetText(status));
            #endif
        }
        new_time = clock();
        SPRINT1(0, "==> EG_makeTessBody CPUtime=%10.3f sec",
                (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));
    }

    /* build the scene graph */
    buildSceneGraph();

cleanup:
    return status;
}


/***********************************************************************/
/*                                                                     */
/*   storeUndo - store an undo for the current command                 */
/*                                                                     */
/***********************************************************************/

static int
storeUndo(char   *cmd,                  /* (in)  current command */
          char   *arg)                  /* (in)  current argument */
{
    int       status = SUCCESS;         /* return status */

    int       iundo;
    char      text[MAX_EXPR_LEN];

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(buildSceneGraph);

    /* --------------------------------------------------------------- */

    /* if the undos are full, discard the most ancient one */
    if (nundo >= MAX_UNDOS) {
        status = ocsmFree(undo_modl[0]);
        CHECK_STATUS(ocsmFree);

        for (iundo = 0; iundo < nundo; iundo++) {
            undo_modl[iundo] = undo_modl[iundo+1];
            (void) strcpy(undo_text[iundo], undo_text[iundo+1]);
        }

        nundo--;
    }

    /* store an undo snapshot */
    sprintf(text, "%s %s", cmd, arg);
    strncpy(undo_text[nundo], text, 31);

    status = ocsmCopy(MODL, &undo_modl[nundo]);
    CHECK_STATUS(ocsmCopy);

    nundo++;

    SPRINT2(1, "~~> ocsmCopy() -> status=%d  (nundo=%d)",
            status, nundo);

cleanup:
    return status;
}


/***********************************************************************/
/*                                                                     */
/*   buildSceneGraph - make a scene graph for wv                       */
/*                                                                     */
/***********************************************************************/

static int
buildSceneGraph()
{
    int       status = SUCCESS;

    int       ibody, jbody, iface, iedge, iattr, nattr;
    int       npnt, ipnt, ntri, itri, igprim, nseg, k, attrs, head;
    int       *segs=NULL, *ivrts=NULL;
    CINT      *ptype, *pindx, *tris, *tric;
    float     focus[4], color[3];
    double    bigbox[6], box[6], size, xyz_dum[3];
    CDOUBLE   *xyz, *uv, *t;
    char      gpname[33];

    #if   defined(GEOM_CAPRI)
        int       ivol;
    #elif defined(GEOM_EGADS)
        ego       ebody, etess, eface;
        int       i, nlist, itype;
        CINT      *tempIlist;
        CDOUBLE   *tempRlist;
        CCHAR     *attrName, *tempClist;
    #endif

    wvData    items[5];

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(buildSceneGraph);

    /* --------------------------------------------------------------- */

    /* remove any graphic primitives that already exist */
    wv_removeAll(cntxt);

    /* find the values needed to adjust the vertices */
    bigbox[0] = bigbox[1] = bigbox[2] = +HUGEQ;
    bigbox[3] = bigbox[4] = bigbox[5] = -HUGEQ;

    for (jbody = 0; jbody < nbody; jbody++) {
        ibody = bodyList[jbody];

        #if   defined(GEOM_CAPRI)
            ivol  = MODL->body[ibody].ivol;
            status = gi_dBox(ivol, box);
        #elif defined(GEOM_EGADS)
            ebody = MODL->body[ibody].ebody;
            etess = MODL->body[ibody].etess;

            status = EG_getBoundingBox(ebody, box);
        #endif

        if (box[0] < bigbox[0]) bigbox[0] = box[0];
        if (box[1] < bigbox[1]) bigbox[1] = box[1];
        if (box[2] < bigbox[2]) bigbox[2] = box[2];
        if (box[3] > bigbox[3]) bigbox[3] = box[3];
        if (box[4] > bigbox[4]) bigbox[4] = box[4];
        if (box[5] > bigbox[5]) bigbox[5] = box[5];
    }

                                    size = bigbox[3] - bigbox[0];
    if (size < bigbox[4]-bigbox[1]) size = bigbox[4] - bigbox[1];
    if (size < bigbox[5]-bigbox[2]) size = bigbox[5] - bigbox[2];

    focus[0] = (bigbox[0] + bigbox[3]) / 2;
    focus[1] = (bigbox[1] + bigbox[4]) / 2;
    focus[2] = (bigbox[2] + bigbox[5]) / 2;
    focus[3] = size;

    /* initialize the scene graph meta data */
    strcpy(sgMetaData, "sgData;{");

    /* special scene graph if nbody<=0 */
    if (nbody <= 0) {
        sprintf(gpname, "Body 0 Face 0");
        attrs = WV_ON;

        xyz_dum[0] = 0;
        xyz_dum[1] = 0;
        xyz_dum[2] = 0;

        status = wv_setData(WV_REAL64, 1, (void*)xyz_dum, WV_VERTICES, &(items[0]));
        wv_adjustVerts(&(items[0]), focus);

        color[0] = 0;
        color[1] = 0;
        color[2] = 0;
        status = wv_setData(WV_REAL32, 1, (void*)color, WV_PCOLOR, &(items[1]));

        igprim = wv_addGPrim(cntxt, gpname, WV_LINE, attrs, 2, items);
    }

    /* loop through the Bodys */
    for (jbody = 0; jbody < nbody; jbody++) {
        ibody = bodyList[jbody];

        #if   defined(GEOM_CAPRI)
            ivol  = MODL->body[ibody].ivol;
        #elif defined(GEOM_EGADS)
            ebody = MODL->body[ibody].ebody;
            etess = MODL->body[ibody].etess;
        #endif

        /* loop through the Faces within each Body */
        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            #if   defined(GEOM_CAPRI)
                status = gi_dTesselFace(ivol, iface, &ntri, &tris, &tric,
                                        &npnt, &xyz, &ptype, &pindx, &uv);
            #elif defined(GEOM_EGADS)
                status = EG_getTessFace(etess, iface,
                                        &npnt, &xyz, &uv, &ptype, &pindx,
                                        &ntri, &tris, &tric);
            #endif

            /* name and attributes */
            sprintf(gpname, "Body %d Face %d", ibody, iface);
            attrs = WV_ON | WV_ORIENTATION;

            /* vertices */
            status = wv_setData(WV_REAL64, npnt, (void*)xyz, WV_VERTICES, &(items[0]));
            wv_adjustVerts(&(items[0]), focus);

            /* triangles */
            status = wv_setData(WV_INT32, 3*ntri, (void*)tris, WV_INDICES, &(items[1]));

            /* triangle colors */
            color[0] = RED(  MODL->body[ibody].face[iface].gratt.color);
            color[1] = GREEN(MODL->body[ibody].face[iface].gratt.color);
            color[2] = BLUE( MODL->body[ibody].face[iface].gratt.color);
            status = wv_setData(WV_REAL32, 1, (void*)color, WV_COLORS, &(items[2]));

            /* triangle sides (segments) */
            nseg = 0;
            for (itri = 0; itri < ntri; itri++) {
                for (k = 0; k < 3; k++) {
                    if (tric[3*itri+k] < itri+1) {
                        nseg++;
                    }
                }
            }

            segs = (int*) malloc(2*nseg*sizeof(int));

            nseg = 0;
            for (itri = 0; itri < ntri; itri++) {
                for (k = 0; k < 3; k++) {
                    if (tric[3*itri+k] < itri+1) {
                        segs[2*nseg  ] = tris[3*itri+(k+1)%3];
                        segs[2*nseg+1] = tris[3*itri+(k+2)%3];
                        nseg++;
                    }
                }
            }

            status = wv_setData(WV_INT32, 2*nseg, (void*)segs, WV_LINDICES, &(items[3]));

            free(segs);

            /* segment colors */
            color[0] = 0.0;
            color[1] = 0.0;
            color[2] = 0.0;
            status = wv_setData(WV_REAL32, 1, (void*)color, WV_LCOLOR, &(items[4]));

            /* make graphic primitive */
            igprim = wv_addGPrim(cntxt, gpname, WV_TRIANGLE, attrs, 5, items);
            if (igprim >= 0) {

                /* make line width 1 */
                cntxt->gPrims[igprim].lWidth = 1.0;
            }

            #if   defined(GEOM_CAPRI)
                nattr = 0;
            #elif defined(GEOM_EGADS)
                eface  = MODL->body[ibody].face[iface].eface;
                status = EG_attributeNum(eface, &nattr);
            #endif

            /* add Face to meta data (if there is room) */
            if (strlen(sgMetaData) < MAX_METADATA_LENGTH-1000) {
                if (nattr > 0) {
                    sprintf(sgMetaData, "%s\"%s\":[",  sgMetaData, gpname);
                } else {
                    sprintf(sgMetaData, "%s\"%s\":[]", sgMetaData, gpname);
                }

                for (iattr = 1; iattr <= nattr; iattr++) {
                    #if   defined(GEOM_CAPRI)
                    #elif defined(GEOM_EGADS)
                        status = EG_attributeGet(eface, iattr, &attrName, &itype, &nlist,
                                                 &tempIlist, &tempRlist, &tempClist);

                        sprintf(sgMetaData, "%s\"%s\",\"", sgMetaData, attrName);

                        if        (itype == ATTRINT) {
                            for (i = 0; i < nlist ; i++) {
                                sprintf(sgMetaData, "%s %d", sgMetaData, tempIlist[i]);
                            }
                        } else if (itype == ATTRREAL) {
                            for (i = 0; i < nlist ; i++) {
                                sprintf(sgMetaData, "%s %f", sgMetaData, tempRlist[i]);
                            }
                        } else if (itype == ATTRSTRING) {
                            sprintf(sgMetaData, "%s %s ", sgMetaData, tempClist);
                        }

                        sprintf(sgMetaData, "%s\",", sgMetaData);
                    #endif
                }
                sgMetaData[strlen(sgMetaData)-1] = '\0';
                sprintf(sgMetaData, "%s],", sgMetaData);
            }
        }

        /* loop through the Edges within each Body */
        for (iedge = 1; iedge <= MODL->body[ibody].nedge; iedge++) {
            #if   defined(GEOM_CAPRI)
                status = gi_dTesselEdge(ivol, iedge, &npnt, &xyz, &t);
            #elif defined(GEOM_EGADS)
                status = EG_getTessEdge(etess, iedge, &npnt, &xyz, &t);
            #endif

            /* name and attributes */
            sprintf(gpname, "Body %d Edge %d", ibody, iedge);
            attrs = WV_ON;

            /* vertices */
            status = wv_setData(WV_REAL64, npnt, (void*)xyz, WV_VERTICES, &(items[0]));
            wv_adjustVerts(&(items[0]), focus);

            /* segments */
            ivrts = (int*) malloc(2*(npnt-1)*sizeof(int));

            for (ipnt = 0; ipnt < npnt-1; ipnt++) {
                ivrts[2*ipnt  ] = ipnt + 1;
                ivrts[2*ipnt+1] = ipnt + 2;
            }

            status = wv_setData(WV_INT32, 2*(npnt-1), (void*)ivrts, WV_INDICES, &(items[1]));

            free(ivrts);

            /* line colors */
            color[0] = RED(  MODL->body[ibody].edge[iedge].gratt.color);
            color[1] = GREEN(MODL->body[ibody].edge[iedge].gratt.color);
            color[2] = BLUE( MODL->body[ibody].edge[iedge].gratt.color);
            status = wv_setData(WV_REAL32, 1, (void*)color, WV_COLORS, &(items[2]));

            /* points */
            ivrts = (int*) malloc(npnt*sizeof(int));

            for (ipnt = 0; ipnt < npnt; ipnt++) {
                ivrts[ipnt] = ipnt + 1;
            }

            status = wv_setData(WV_INT32, npnt, (void*)ivrts, WV_PINDICES, &(items[3]));

            free(ivrts);

            /* point colors */
            color[0] = 0;
            color[1] = 0;
            color[2] = 0;
            status = wv_setData(WV_REAL32, 1, (void*)color, WV_PCOLOR, &(items[4]));

            /* make graphic primitive */
            igprim = wv_addGPrim(cntxt, gpname, WV_LINE, attrs, 5, items);
            if (igprim >= 0) {

                /* make line width 2 */
                cntxt->gPrims[igprim].lWidth = 2.0;

                /* make point size 5 */
                cntxt->gPrims[igprim].pSize  = 5.0;

                /* add arrow heads */
                head = npnt - 1;
                status = wv_addArrowHeads(cntxt, igprim, 0.05, 1, &head);
            }
        }
    }

    /* finish the scene graph meta data */
    sgMetaData[strlen(sgMetaData)-1] = '\0';
    sprintf(sgMetaData, "%s}", sgMetaData);

//cleanup:
    return status;
}


/***********************************************************************/
/*                                                                     */
/*   browserMessage - called when client sends a message to the server */
/*                                                                     */
/***********************************************************************/

void
browserMessage(void    *wsi,
               char    *text,
  /*@unused@*/ int     lena)
{
    char      response[MAX_STR_LEN];

    ROUTINE(browserMessage);

    /* --------------------------------------------------------------- */

    /* process the Message */
    processMessage(text, response);

    /* send the response */
    SPRINT1(2, "response-> %s", response);
    wv_sendText(wsi, response);

    /* send the scene graph meta data if it has not already been sent */
    if (strlen(sgMetaData) > 0) {
        SPRINT1(2, "sgData-> %s", sgMetaData);
        wv_sendText(wsi, sgMetaData);

        /* nullify meta data so that it does not get sent again */
        sgMetaData[0] = '\0';
    }

}


/***********************************************************************/
/*                                                                     */
/*   processMessage - process the message and create the response      */
/*                                                                     */
/***********************************************************************/

void
processMessage(char    *text,
               char    *response)
{
    int       i, status, ibrch, itype, builtTo, buildStatus;
    int       ipmtr, nrow, ncol, irow, icol, index, iattr, actv;
    char      *pEnd;
    char      name[MAX_EXPR_LEN], type[MAX_EXPR_LEN], valu[MAX_EXPR_LEN];
    char      arg1[MAX_EXPR_LEN], arg2[MAX_EXPR_LEN], arg3[MAX_EXPR_LEN];
    char      arg4[MAX_EXPR_LEN], arg5[MAX_EXPR_LEN], arg6[MAX_EXPR_LEN];
    char      arg7[MAX_EXPR_LEN], arg8[MAX_EXPR_LEN], arg9[MAX_EXPR_LEN];

    char      entry[MAX_STR_LEN];

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(processMessage);

    /* --------------------------------------------------------------- */

    SPRINT1(1, "==> processMessage(text=%s)", text);

    /* NO-OP */
    if (strlen(text) == 0) {

    /* "identify;" */
    } else if (strncmp(text, "identify;", 9) == 0) {

        /* build the response */
        sprintf(response, "identify;serveCSM;");

    /* "getPmtrs;" */
    } else if (strncmp(text, "getPmtrs;", 9) == 0) {

        /* build the response in JSON format */
        sprintf(response, "getPmtrs;[");

        for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
            sprintf(entry, "{\"name\":\"%s\",\"type\":%d,\"nrow\":%d,\"ncol\":%d,\"value\":[",
                    MODL->pmtr[ipmtr].name,
                    MODL->pmtr[ipmtr].type,
                    MODL->pmtr[ipmtr].nrow,
                    MODL->pmtr[ipmtr].ncol);
            strcat(response, entry);

            index = 0;
            for (irow = 1; irow <= MODL->pmtr[ipmtr].nrow; irow++) {
                for (icol = 1; icol <= MODL->pmtr[ipmtr].ncol; icol++) {
                    if (irow < MODL->pmtr[ipmtr].nrow || icol < MODL->pmtr[ipmtr].ncol) {
                        sprintf(entry, "%lg,", MODL->pmtr[ipmtr].value[index++]);
                    } else {
                        sprintf(entry, "%lg]", MODL->pmtr[ipmtr].value[index++]);
                    }
                    strcat(response, entry);
                }
            }

            if (ipmtr < MODL->npmtr) {
                strcat(response, "},");
            } else {
                strcat(response, "}]");
            }
        }

    /* "newPmtr;name;nrow;ncol; value1; ..." */
    } else if (strncmp(text, "newPmtr;", 8) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract arguments */
        nrow = 0;
        ncol = 0;

        if (getToken(text,  1, name) == 0) name[0] = '\0';
        if (getToken(text,  2, arg1)     ) nrow = strtol(arg1, &pEnd, 10);
        if (getToken(text,  3, arg2)     ) ncol = strtol(arg2, &pEnd, 10);

        /* store an undo snapshot */
        status = storeUndo("newPmtr", name);

        /* build the response */
        status = ocsmNewPmtr(MODL, name, OCSM_EXTERNAL, nrow, ncol);

        if (status == SUCCESS) {
            ipmtr = MODL->npmtr;

            i = 4;
            for (irow = 1; irow <= nrow; irow++) {
                for (icol = 1; icol <= ncol; icol++) {
                    if (getToken(text, i, arg3)) {
                        (void) ocsmSetValu(MODL, ipmtr, irow, icol, arg3);
                    }

                    i++;
                }
            }

            sprintf(response, "newPmtr;");
        } else {
            sprintf(response, "ERROR:: newPmtr() detected: %s",
                    ocsmGetText(status));
        }

    /* "setPmtr;ipmtr;irow;icol;value1; " */
    } else if (strncmp(text, "setPmtr", 7) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract arguments */
        ipmtr = 0;
        irow  = 0;
        icol  = 0;

        if (getToken(text, 1, arg1)) ipmtr = strtol(arg1, &pEnd, 10);
        if (getToken(text, 2, arg2)) irow  = strtol(arg2, &pEnd, 10);
        if (getToken(text, 3, arg3)) icol  = strtol(arg3, &pEnd, 10);

        /* store an undo snapshot */
        status = storeUndo("setPmtr", MODL->pmtr[ipmtr].name);

        if (getToken(text, 4, arg4)) {
            status = ocsmSetValu(MODL, ipmtr, irow, icol, arg4);
        } else {
            status = -999;
        }

        /* build the response */
        if (status == SUCCESS) {
            sprintf(response, "setPmtr;");
        } else {
            sprintf(response, "ERROR:: setPmtr(%d, %d, %d) detected: %s",
                    ipmtr, irow, icol, ocsmGetText(status));
        }

    /* "getBrchs;" */
    } else if (strncmp(text, "getBrchs;", 9) == 0) {

        /* buildthe response in JSON format */
        sprintf(response, "getBrchs;[");

        for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
            sprintf(entry, "{\"name\":\"%s\",\"type\":\"%s\",\"actv\":%d,\"attrs\":[",
                    MODL->brch[ibrch].name,
        ocsmGetText(MODL->brch[ibrch].type),
                    MODL->brch[ibrch].actv);
            strcat(response, entry);

            for (iattr = 0; iattr < MODL->brch[ibrch].nattr; iattr++) {
                if (iattr < MODL->brch[ibrch].nattr-1) {
                    sprintf(entry, "[\"%s\",\"%s\"],",
                            MODL->brch[ibrch].attr[iattr].name,
                            MODL->brch[ibrch].attr[iattr].value);
                } else {
                    sprintf(entry, "[\"%s\",\"%s\"]",
                            MODL->brch[ibrch].attr[iattr].name,
                            MODL->brch[ibrch].attr[iattr].value);
                }
                strcat(response, entry);
            }

            sprintf(entry, "],\"ileft\":%d,\"irite\":%d,\"ichld\":%d,\"args\":[",
                    MODL->brch[ibrch].ileft,
                    MODL->brch[ibrch].irite,
                    MODL->brch[ibrch].ichld);
            strcat(response, entry);

            if (MODL->brch[ibrch].narg >= 1) {
                sprintf(entry,  "\"%s\"", MODL->brch[ibrch].arg1);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 2) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg2);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 3) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg3);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 4) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg4);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 5) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg5);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 6) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg6);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 7) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg7);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 8) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg8);
                strcat(response, entry);
            }
            if (MODL->brch[ibrch].narg >= 9) {
                sprintf(entry, ",\"%s\"", MODL->brch[ibrch].arg9);
                strcat(response, entry);
            }

            if (ibrch < MODL->nbrch) {
                sprintf(entry, "]},");
            } else {
                sprintf(entry, "]}]");
            }
            strcat(response, entry);
        }

    /* "newBrch;ibrch;type;arg1;arg2;arg3;arg4;arg5;arg6;arg7;arg8;arg9; aname1;avalu1;" */
    } else if (strncmp(text, "newBrch;", 8) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract arguments */
        ibrch = 0;
        itype = 0;

        if (getToken(text,  1, arg1)) ibrch = strtol(arg1, &pEnd, 10);

        if (getToken(text,  2, type) >  0) itype = ocsmGetCode(type);
        if (getToken(text,  3, arg1) == 0) arg1[0] = '\0';
        if (getToken(text,  4, arg2) == 0) arg2[0] = '\0';
        if (getToken(text,  5, arg3) == 0) arg3[0] = '\0';
        if (getToken(text,  6, arg4) == 0) arg4[0] = '\0';
        if (getToken(text,  7, arg5) == 0) arg5[0] = '\0';
        if (getToken(text,  8, arg6) == 0) arg6[0] = '\0';
        if (getToken(text,  9, arg7) == 0) arg7[0] = '\0';
        if (getToken(text, 10, arg8) == 0) arg8[0] = '\0';
        if (getToken(text, 11, arg9) == 0) arg9[0] = '\0';

        /* store an undo snapshot */
        status = storeUndo("newBrch", type);

        /* build the response */
        status = ocsmNewBrch(MODL, ibrch, itype, arg1, arg2, arg3,
                                                 arg4, arg5, arg6,
                                                 arg7, arg8, arg9);
        if (status == SUCCESS) {
            status = ocsmCheck(MODL);

            if (status == SUCCESS) {
                sprintf(response, "newBrch;");
            } else {
                sprintf(response, "ERROR:: newBrch(ibrch=%d) detected: %s",
                        ibrch, ocsmGetText(status));
            }
        } else {
            sprintf(response, "ERROR:: newBrch(ibrch=%d) detected: %s",
                    ibrch, ocsmGetText(status));
        }

    /* "setBrch;ibrch;name;actv;arg1;arg2;arg3;arg4;arg5;arg6;arg7;arg8;arg9; aname1;avalu1; ..." */
    } else if (strncmp(text, "setBrch;", 8) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract arguments */
        ibrch = 0;
        actv  = 0;

        if (getToken(text, 1, arg1)) ibrch = strtol(arg1, &pEnd, 10);

        /* store an undo snapshot */
        status = storeUndo("setBrch", MODL->brch[ibrch].name);

        /* build the response */
        if (ibrch >= 1 && ibrch <= MODL->nbrch) {
            if (getToken(text,  2, name)) (void) ocsmSetName(MODL, ibrch, name);

            if (getToken(text,  3, arg1)) {
                if (strcmp(arg1, "suppressed") == 0) {
                    (void) ocsmSetBrch(MODL, ibrch, OCSM_SUPPRESSED);
                    actv = 1;
                } else {
                    (void) ocsmSetBrch(MODL, ibrch, OCSM_ACTIVE);
                    actv = 1;
                }
            }

            if (getToken(text,  4, arg1)) (void) ocsmSetArg(MODL, ibrch, 1, arg1);
            if (getToken(text,  5, arg2)) (void) ocsmSetArg(MODL, ibrch, 2, arg2);
            if (getToken(text,  6, arg3)) (void) ocsmSetArg(MODL, ibrch, 3, arg3);
            if (getToken(text,  7, arg4)) (void) ocsmSetArg(MODL, ibrch, 4, arg4);
            if (getToken(text,  8, arg5)) (void) ocsmSetArg(MODL, ibrch, 5, arg5);
            if (getToken(text,  9, arg6)) (void) ocsmSetArg(MODL, ibrch, 6, arg6);
            if (getToken(text, 10, arg7)) (void) ocsmSetArg(MODL, ibrch, 7, arg7);
            if (getToken(text, 11, arg8)) (void) ocsmSetArg(MODL, ibrch, 8, arg8);
            if (getToken(text, 12, arg9)) (void) ocsmSetArg(MODL, ibrch, 9, arg9);

            i = 13;
            while (1) {
                if (getToken(text, i++, name) == 0) break;
                if (getToken(text, i++, valu) == 0) break;

                (void) ocsmSetAttr(MODL, ibrch, name, valu);
            }

            if (actv > 0) {
                status = ocsmCheck(MODL);

                if (status >= SUCCESS) {
                    sprintf(response, "setBrch;");
                } else {
                    sprintf(response, "ERROR:: setBrch(ibrch=%d) detected: %s",
                            ibrch, ocsmGetText(status));
                }
            } else {
                sprintf(response, "setBrch;");
            }
        } else {
            status = OCSM_ILLEGAL_BRCH_INDEX;
            sprintf(response, "ERROR: setBrch(%d) detected: %s",
                    ibrch, ocsmGetText(status));
        }

    /* "delBrch;ibrch;" */
    } else if (strncmp(text, "delBrch;", 8) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract argument */
        ibrch = 0;

        if (getToken(text, 1, arg1)) ibrch = strtol(arg1, &pEnd, 10);

        /* store an undo snapshot */
        status = storeUndo("delBrch", MODL->brch[ibrch].name);

        /* delete the Branch */
        status = ocsmDelBrch(MODL, ibrch);

        /* check that the Branches are properly ordered */
        if (status == SUCCESS) {
            status = ocsmCheck(MODL);
        }

        /* build the response */
        if (status == SUCCESS) {
            sprintf(response, "delBrch;");
        } else {
            sprintf(response, "ERROR:: delBrch(%d) detected: %s",
                    ibrch, ocsmGetText(status));
        }

    /* "setAttr;ibrch;aname;avalue;" */
    } else if (strncmp(text, "setAttr;", 8) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract arguments */
        ibrch = 0;

        if (getToken(text, 1, arg1)) ibrch = strtol(arg1, &pEnd, 10);
        getToken(text, 2, arg2);
        getToken(text, 3, arg3);

        /* store an undo snapshot */
        status = storeUndo("setAttr", MODL->brch[ibrch].name);

        /* set the Attribute */
        status = ocsmSetAttr(MODL, ibrch, arg2, arg3);

        /* build the response */
        if (status == SUCCESS) {
            sprintf(response, "setAttr;");
        } else {
            sprintf(response, "ERROR: setAttr(%d, %s, %s) detected: %s",
                    ibrch, arg2, arg3, ocsmGetText(status));
        }

    /* "undo;" */
    } else if (strncmp(text, "undo;", 5) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* build the response */
        if (nundo <= 0) {
            sprintf(response, "ERROR:: there is nothing to undo");

        } else {
            /* remove the current MODL */
            status = ocsmFree(modl);
            if (status < SUCCESS) {
                sprintf(response, "ERROR:: undo(%s) detected: %s",
                        arg1, ocsmGetText(status));
            } else {
                /* repoint MODL to the saved modl */
                modl = undo_modl[--nundo];

                MODL = (modl_T*)modl;

                sprintf(response, "undo;%s;", undo_text[nundo]);
            }
        }

    /* "save;filename" */
    } else if (strncmp(text, "save;", 5) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract argument */
        getToken(text, 1, arg1);

        /* save the file */
        status = ocsmSave(MODL, arg1);

        /* build the response */
        if (status == SUCCESS) {
            sprintf(response, "save;");
        } else {
            sprintf(response, "ERROR:: save(%s) detected: %s",
                    arg1, ocsmGetText(status));
        }

    /* "build;"  */
    } else if (strncmp(text, "build;", 6) == 0) {

        /* write journal entry */
        if (jrnl_out != NULL) {
            fprintf(jrnl_out, "%s\n", text);
        }

        /* extract argument */
        ibrch = 0;

        if (getToken(text, 1, arg1)) ibrch = strtol(arg1, &pEnd, 10);

        /* build the response */
        status = buildBodys(ibrch, &builtTo, &buildStatus);

        if (builtTo < 0) {
            sprintf(response, "ERROR:: build() detected \"%s\" in %s",
                    ocsmGetText(buildStatus), MODL->brch[1-builtTo].name);
        } else if (status == SUCCESS) {
            sprintf(response, "build;%d;%d;", builtTo, nbody);
        } else {
            sprintf(response, "ERROR:: build() detected: %s",
                    ocsmGetText(status));
        }

    }
}


/***********************************************************************/
/*                                                                     */
/*   getToken - get a token from a string                              */
/*                                                                     */
/***********************************************************************/

static int
getToken(char   *text,                  /* (in)  full text */
         int    nskip,                  /* (in)  tokens to skip */
         char   *token)                 /* (out) token */
{
    int    lentok, i, count, iskip;

    token[0] = '\0';
    lentok   = 0;

    /* count the number of semi-colons */
    count = 0;
    for (i = 0; i < strlen(text); i++) {
        if (text[i] == ';') {
            count++;
        }
    }

    if (count < nskip+1) return 0;

    /* skip over nskip tokens */
    i = 0;
    for (iskip = 0; iskip < nskip; iskip++) {
        while (text[i] != ';') {
            i++;
        }
        i++;
    }

    /* extract the token we are looking for */
    while (text[i] != ';') {
        token[lentok++] = text[i++];
        token[lentok  ] = '\0';
    }

    return strlen(token);
}

