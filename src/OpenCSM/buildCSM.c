/*
 ************************************************************************
 *                                                                      *
 * buildCSM.c -- use OpenCSM code to build Bodys                        *
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
#include <math.h>
#include <time.h>

#ifdef WIN32
    #include <windows.h>
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

#include "gv.h"
#include "Graphics.h"

#if   defined(GRAFIC)
    #include "grafic.h"
#endif

/***********************************************************************/
/*                                                                     */
/* macros (including those that go along with common.h)                */
/*                                                                     */
/***********************************************************************/

#ifdef DEBUG
   #define DOPEN {if (dbg_fp == NULL) dbg_fp = fopen("buildCSM.dbg", "w");}
   static  FILE *dbg_fp = NULL;
#endif

static int outLevel = 1;

//static void *realloc_temp = NULL;            /* used by RALLOC macro */

#define  RED(COLOR)      (float)(COLOR / 0x10000        ) / (float)(255)
#define  GREEN(COLOR)    (float)(COLOR / 0x00100 % 0x100) / (float)(255)
#define  BLUE(COLOR)     (float)(COLOR           % 0x100) / (float)(255)

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
static void      *modl;                /* pointer to MODL */

/* global variables associated with graphical user interface (gui) */
static int        ngrobj   = 0;        /* number of GvGraphic objects */
static GvGraphic  **grobj;             /* list   of GvGraphic objects */
static int        new_data = 1;        /* =1 means image needs to be updated */
static FILE       *script  = NULL;     /* pointer to script file (if it exists) */
static int        numarg   = 0;        /* numeric argument */
static double     bigbox[6];           /* bounding box of config */
static int        builtTo  = 0;        /* last branch built to */

/* global variables associated with paste buffer */
#define MAX_PASTE         10
#define MAX_EXPR_LEN     128

static int        npaste   = 0;                         /* number of Branches in paste buffer */
static int        type_paste[MAX_PASTE];                /* type for Branches in paste buffer */
static char       name_paste[MAX_PASTE][MAX_EXPR_LEN];  /* name for Branches in paste buffer */
static char       str1_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str1 for Branches in paste buffer */
static char       str2_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str2 for Branches in paste buffer */
static char       str3_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str3 for Branches in paste buffer */
static char       str4_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str4 for Branches in paste buffer */
static char       str5_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str5 for Branches in paste buffer */
static char       str6_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str6 for Branches in paste buffer */
static char       str7_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str7 for Branches in paste buffer */
static char       str8_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str8 for Branches in paste buffer */
static char       str9_paste[MAX_PASTE][MAX_EXPR_LEN];  /* str9 for Branches in paste buffer */

/* global variables associated with bodyList */
#define MAX_BODYS         99
static int        nbody    = 0;        /* number of built Bodys */
static int        bodyList[MAX_BODYS]; /* array  of built Bodys */

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

/* declarations for graphic routines defined below */
       int        gvupdate();
       void       gvdata(int ngraphics, GvGraphic *graphic[]);
       int        gvscalar(int key, GvGraphic *graphic, int len, float *scalar);
       void       gvevent(int *win, int *type, int *xpix, int *ypix, int *state);
       void       transform(double xform[3][4], double *point, float *out);
static int        pickObject(int *utype);
static int        getInt(char *prompt);
static double     getDbl(char *prompt);
static void       getStr(char *prompt, char *string);

#ifdef GRAFIC
static void       plotGrid(int*, void*, void*, void*, void*, void*, void*,
                           void*, void*, void*, void*, float*, char*, int);
#endif

/* external functions not declared in an .h file */
#if   defined(GEOM_CAPRI)
    extern void    *gi_alloc(int nbtyes);
    extern void    *gi_reall(void *old, int nbytes);
    extern void    gi_free(void *ptr);
#elif defined(GEOM_EGADS)
#endif

/* functions and variables associated with picking */
extern GvGraphic  *gv_picked;
extern int        gv_pickon;
extern int        gv_pickmask;
extern void       PickGraphic(float x, float y, int flag);

/* functions and variables associated with background ccolor, etc. */
#ifndef WIN32
extern Display    *gr_dspl;
#endif
extern GWinAtt    gv_wAux, gv_wDial, gv_wKey, gv_w3d, gv_w2d;
extern float      gv_black[3], gv_white[3];
extern float      gv_xform[4][4];
extern void       GraphicGCSetFB(GID gc, float *fg, float *bg);
extern void       GraphicBGColor(GID win, float *bg);

/* window defines */
#define           DataBase        1
#define           TwoD            2
#define           ThreeD          3
#define           Dials           4
#define           Key             5

/* event types */
#define           KeyPress        2
#define           KeyRelease      3
#define           ButtonPress     4
#define           ButtonRelease   5
#define           Expose          12
#define           NoExpose        14
#define           ClientMessage   33

/* error codes */
/*                user by CAPRI             -1 to  -99 */
/*                used by OpenCSM         -201 to -199 */


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

    int       status, mtflag = -1, noviz, readonly, i, ibody, jbody;
    int       imajor, iminor, buildTo, nbrch, npmtr, showUsage=0;
    float     focus[4];
    double    box[6];
    char      casename[255], filename[255];
    clock_t   old_time, new_time;

    modl_T    *MODL;
    void      *orig_modl;

    #if   defined(GEOM_CAPRI)
    int       ivol;
    #elif defined(GEOM_EGADS)
        double    size, params[3];
        ego       ebody;
    #endif

    int       nkeys      = 0;
    int       keys[2]    = {'U',             'V'             };
    int       types[2]   = {GV_SURF,         GV_SURF         };
    char      titles[32] =  " u Parameter    v Parameter    " ;
    float     lims[4]    = {0.,1.,           0.,1.           };

    ROUTINE(MAIN);

    /* --------------------------------------------------------------- */

    DPRINT0("starting buildCSM");

    /* get the flags and casename(s) from the command line */
    casename[0] = '\0';
    noviz       = 0;
    readonly    = 0;

    for (i = 1; i < argc; i++) {
        if        (strcmp(argv[i], "-noviz") == 0) {
            noviz = 1;
        } else if (strcmp(argv[i], "-readonly") == 0) {
            readonly = 1;
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
        SPRINT0(0, "proper usage: 'buildCSM [-noviz] [-readonly] [-outLevel X] casename]'");
        SPRINT0(0, "STOPPING...\a");
        exit(0);
    }

    /* welcome banner */
    (void) ocsmVersion(&imajor, &iminor);

    SPRINT0(1, "**********************************************************");
    SPRINT0(1, "*                                                        *");
    SPRINT0(1, "*                    Program buildCSM                    *");
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
        filename[0] = '\0';
    }

    #if   defined(GEOM_CAPRI)
        /* start CAPRI */
        gi_uRegister();

        status = gi_uStart();
        SPRINT1(1, "--> gi_uStart() -> status=%d", status);

        if (status < SUCCESS) {
            SPRINT0(0, "problem starting CAPRI\nSTOPPING...\a");
            exit(0);
        }
    #elif defined(GEOM_EGADS)
    #endif

    #if   defined(GEOM_CAPRI)
        /* make a "throw-away" volume so that CAPRI's startup
           message does not get produced during code below */
        bigbox[0] = 0.0; bigbox[1] = 0.0; bigbox[2] = 0.0;
        bigbox[3] = 1.0; bigbox[4] = 1.0; bigbox[5] = 1.0;
        status = gi_gCreateVolume(NULL, "Parasolid", 1, bigbox);
        SPRINT1(1, "--> gi_gCreateVolume(dummy) -> status=%d", status);
    #elif defined(GEOM_EGADS)
    #endif

    /* read the .csm file and create the MODL */
    old_time = clock();
    status   = ocsmLoad(filename, &orig_modl);
    new_time = clock();
    SPRINT3(1, "--> ocsmLoad(%s) -> status=%d (%s)", filename, status, ocsmGetText(status));
    SPRINT1(1, "==> ocsmLoad CPUtime=%9.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < 0) exit(0);

    /* make a copy of the MODL */
    old_time = clock();
    status   = ocsmCopy(orig_modl, &modl);
    new_time = clock();
    SPRINT2(1, "--> ocsmCopy() -> status=%d (%s)", status, ocsmGetText(status));
    SPRINT1(1, "==> ocsmCopy CPUtime=%9.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < 0) exit(0);

    /* delete the original MODL */
    old_time = clock();
    status   = ocsmFree(orig_modl);
    new_time = clock();
    SPRINT2(1, "--> ocsmFree() -> status=%d (%s)", status, ocsmGetText(status));
    SPRINT1(1, "==> ocsmFree CPUtime=%9.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < 0) exit(0);

    /* check that Branches are properly ordered */
    old_time = clock();
    status   = ocsmCheck(modl);
    new_time = clock();
    SPRINT2(0, "--> ocsmCheck()) -> status=%d (%s)",
            status, ocsmGetText(status));
    SPRINT1(0, "==> ocsmCheck CPUtime=%10.3f sec",
            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

    if (status < 0) exit(0);

    MODL = (modl_T*)modl;

    /* print out the Parameters and Branches */
    SPRINT0(1, "External Parameter(s):");
    if (outLevel > 0) {
        status = ocsmPrintPmtrs(modl, stdout);
    }

    SPRINT0(1, "Branch(es):");
    if (outLevel > 0) {
        status = ocsmPrintBrchs(modl, stdout);
    }

    (void)ocsmInfo(modl, &nbrch, &npmtr, &nbody);

    /* skip ocsmBuild and EG_makeTessBody if readonly==1 */
    if (readonly == 1 || nbrch == 0) {
        SPRINT0(0, "WARNING:: ocsmBuild and EG_makeTessBody skipped");

    /* build the Bodys from the MODL */
    } else {
        buildTo = 0; /* all */

        /* build the Bodys */
        nbody    = MAX_BODYS;
        old_time = clock();
        status   = ocsmBuild(modl, buildTo, &builtTo, &nbody, bodyList);
        new_time = clock();
        SPRINT5(1, "--> ocsmBuild(buildTo=%d) -> status=%d (%s), builtTo=%d, nbody=%d",
                buildTo, status, ocsmGetText(status), builtTo, nbody);
        SPRINT1(1, "==> ocsmBuild CPUtime=%9.3f sec",
                (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

        if (status < 0) exit(0);

        /* tessellate the Bodys */
        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            old_time = clock();
            for (jbody = 0; jbody < nbody; jbody++) {
                ibody = bodyList[jbody];
                ebody = MODL->body[ibody].ebody;

                status = EG_getBoundingBox(ebody, box);
                size   = sqrt(pow(box[3]-box[0],2) + pow(box[4]-box[1],2)
                                                   + pow(box[5]-box[2],2));

                /* vTess parameters */
                params[0] = 0.0250 * size;
                params[1] = 0.0010 * size;
                params[2] = 15.0;

                ebody = MODL->body[ibody].ebody;
                (void) EG_setOutLevel(MODL->context, 0);
                status   = EG_makeTessBody(ebody, params, &(MODL->body[ibody].etess));
                (void) EG_setOutLevel(MODL->context, outLevel);
                SPRINT5(1, "--> EG_makeTessBody(ibody=%4d, params=%10.5f, %10.5f, %10.5f) -> status=%d",
                        ibody, params[0], params[1], params[2], status);

                if (status < 0) exit(0);
            }
            new_time = clock();
            SPRINT1(1, "==> EG_makeTessBody CPUtime=%9.3f sec",
                    (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));
        #endif

        /* print out the Bodys */
        SPRINT0(1, "Body(s):");
        if (outLevel > 0) {
            status = ocsmPrintBodys(modl, stdout);
        }
    }

    /* start GV */
    if (noviz == 0) {
        if (nbody > 0) {
            bigbox[0] = +HUGEQ;
            bigbox[1] = +HUGEQ;
            bigbox[2] = +HUGEQ;
            bigbox[3] = -HUGEQ;
            bigbox[4] = -HUGEQ;
            bigbox[5] = -HUGEQ;

            for (jbody = 0; jbody < nbody; jbody++) {
                ibody  = bodyList[jbody];
                #if   defined(GEOM_CAPRI)
                    ivol   = MODL->body[ibody].ivol;
                    status = gi_dBox(ivol, box);
                #elif defined(GEOM_EGADS)
                    ebody  = MODL->body[ibody].ebody;
                    status = EG_getBoundingBox(ebody, box);
                #endif

                bigbox[0] = MIN(bigbox[0], box[0]);
                bigbox[1] = MIN(bigbox[1], box[1]);
                bigbox[2] = MIN(bigbox[2], box[2]);
                bigbox[3] = MAX(bigbox[3], box[3]);
                bigbox[4] = MAX(bigbox[4], box[4]);
                bigbox[5] = MAX(bigbox[5], box[5]);
            }
        } else {
            bigbox[0] = -1;
            bigbox[1] = -1;
            bigbox[2] = -1;
            bigbox[3] = +1;
            bigbox[4] = +1;
            bigbox[5] = +1;
        }

        focus[0] = (bigbox[0] + bigbox[3]) / 2;
        focus[1] = (bigbox[1] + bigbox[4]) / 2;
        focus[2] = (bigbox[2] + bigbox[5]) / 2;
        focus[3] = sqrt(  pow(bigbox[3] - bigbox[0], 2)
                        + pow(bigbox[4] - bigbox[1], 2)
                        + pow(bigbox[5] - bigbox[2], 2));

        gv_black[0] = gv_black[1] = gv_black[2] = 1.0;
        gv_white[0] = gv_white[1] = gv_white[2] = 0.0;

        status = gv_init("                buildCSM     ", mtflag,
                         nkeys, keys, types, lims,
                         titles, focus);
        SPRINT1(1, "--> gv_init() -> status=%d", status);
    }

    /* stop CAPRI/EGADS and clean up GvGraphic objects and other allocated memory */
    #if   defined(GEOM_CAPRI)
        status = gi_uStop(0);
        SPRINT1(1, "--> gi_uStop(0) -> status=%d", status);
    #elif defined(GEOM_EGADS)
        status = EG_setOutLevel(MODL->context, 0);
        status = EG_close(MODL->context);
        SPRINT2(1, "--> EG_close() -> status=%d (%s)", status, ocsmGetText(status));
    #endif

        if (status < 0) exit(0);

    for (i = 0; i < ngrobj; i++) {
        gv_free(grobj[i], 2);
    }
    ngrobj = 0;

    /* free up the modl */
    old_time = clock();
    status = ocsmFree(modl);
    SPRINT2(1, "--> ocsmFree() -> status=%d (%s)", status, ocsmGetText(status));

    if (status < 0) exit(0);

    /* if builtTo is non-positive, then report that an error was found */
    if (builtTo <= 0) {
        SPRINT0(0, "ERROR:: build not completed because an error was detected");
    } else {
        SPRINT0(1, "==> buildCSM completed successfully");
    }

//cleanup:
    return SUCCESS;
}


/***********************************************************************/
/*                                                                     */
/*   gvupdate - called by gv to allow the changing of data             */
/*                                                                     */
/***********************************************************************/

int
gvupdate( )
{
    int       nobj;                     /* =0   no data has changed */
                                        /* >0  new number of graphic objects */

    int        i, ibody, jbody;
    static int init = 0;
    char       *family_name;

    modl_T    *MODL = (modl_T*)modl;

//    ROUTINE(gvupdate);

    /* --------------------------------------------------------------- */

//$$$    SPRINT0(2, "enter gvupdate()");

    /* one-time initialization */
    if (init == 0) {
        GraphicGCSetFB(gv_wAux.GCs,  gv_white, gv_black);
        GraphicBGColor(gv_wAux.wid,  gv_black);

        GraphicGCSetFB(gv_wDial.GCs, gv_white, gv_black);
        GraphicBGColor(gv_wDial.wid, gv_black);

        #ifdef WIN32
            ShowWindow((HWND) gv_wDial.wid, SW_FORCEMINIMIZE);
            ShowWindow((HWND) gv_w2d.wid,   SW_FORCEMINIMIZE);
//$$$            ShowWindow((HWND) gv_wKey.wid,  SW_FORCEMINIMIZE);
        #else
            XIconifyWindow(gr_dspl, gv_wDial.wid, DefaultScreen(gr_dspl));
            XIconifyWindow(gr_dspl, gv_w2d.wid,   DefaultScreen(gr_dspl));
//$$$            XIconifyWindow(gr_dspl, gv_wKey.wid,  DefaultScreen(gr_dspl));
        #endif

        init++;
    }

    /* simply return if no new data */
    if (new_data == 0) {
        return 0;
    }

    /* remove any previous families */
    for (i = gv_numfamily()-1; i >= 0; i--) {
        (void) gv_returnfamily(0, &family_name);
        (void) gv_freefamily(family_name);
    }

    /* remove any previous graphic objects */
    for (i = 0; i < ngrobj; i++) {
        gv_free(grobj[i], 2);
    }
    ngrobj = 0;

    /* get the new number of Edges and Faces */
    nobj = 3;
    for (jbody = 0; jbody < nbody; jbody++) {
        ibody  = bodyList[jbody];

        nobj += (MODL->body[ibody].nedge + MODL->body[ibody].nface);
    }

    /* return the number of graphic objects */
    new_data = 0;

//cleanup:
    return nobj;
}


/***********************************************************************/
/*                                                                     */
/*   gvdata - called by gv to (re)set the graphic objects              */
/*                                                                     */
/***********************************************************************/

void
gvdata(int       ngraphics,             /* (in)  number of objects to fill */
       GvGraphic *graphic[])            /* (out) graphic objects to fill */
{
    GvColor   color;
    GvObject  *object;
    int       i, j, npnt, ntri;
    int       utype, iedge, iface, mask;
    int       ibody, jbody, nedge, nface, attr;
    CINT      *tris, *tric, *ptype, *pindx;
    CDOUBLE   *xyz, *uv;
    char      title[16], bodyName[16];

    #if   defined(GEOM_CAPRI)
    int       ivol;
        double    xform[3][4];
    #elif defined(GEOM_EGADS)
        ego       etess;
    #endif

    modl_T    *MODL = (modl_T*)modl;

//    ROUTINE(gvdata);

    /* --------------------------------------------------------------- */

//$$$    SPRINT1(2, "enter gvdata(ngraphics=%d)",
//$$$           ngraphics);

    /* create the graphic objects */
    grobj  = graphic;
    ngrobj = ngraphics;
    i      = 0;

    /* if the family does not exist, create it */
    if (gv_getfamily(  "Axes", 1, &attr) == -1) {
        gv_allocfamily("Axes");
    }

    /* x-axis */
    mask = GV_FOREGROUND | GV_FORWARD | GV_ORIENTATION;

    color.red   = 1.0;
    color.green = 0.0;
    color.blue  = 0.0;

    sprintf(title, "X axis");
    utype = 999;
    graphic[i] = gv_alloc(GV_NONINDEXED, GV_POLYLINES, mask, color, title, utype, 0);
    if (graphic[i] != NULL) {
        graphic[i]->number    = 1;
        graphic[i]->lineWidth = 3;
        #if   defined(GEOM_CAPRI)
            graphic[i]->fdata = (float*) gi_alloc(6*sizeof(float));
            graphic[i]->fdata[0] = 0;
            graphic[i]->fdata[1] = 0;
            graphic[i]->fdata[2] = 0;
            graphic[i]->fdata[3] = 1;
            graphic[i]->fdata[4] = 0;
            graphic[i]->fdata[5] = 0;
        #elif defined(GEOM_EGADS)
            graphic[i]->ddata = (double*)  malloc(6*sizeof(double));
            graphic[i]->ddata[0] = 0;
            graphic[i]->ddata[1] = 0;
            graphic[i]->ddata[2] = 0;
            graphic[i]->ddata[3] = 1;
            graphic[i]->ddata[4] = 0;
            graphic[i]->ddata[5] = 0;
        #endif
        graphic[i]->object->length = 1;
        #if   defined(GEOM_CAPRI)
            graphic[i]->object->type.plines.len = (int*) gi_alloc(sizeof(int));
        #elif defined(GEOM_EGADS)
            graphic[i]->object->type.plines.len = (int*)   malloc(sizeof(int));
        #endif
        graphic[i]->object->type.plines.len[0] = 2;

        gv_adopt("Axes", graphic[i]);

        i++;
    }

    /* y-axis */
    mask = GV_FOREGROUND | GV_FORWARD | GV_ORIENTATION;

    color.red   = 0.0;
    color.green = 1.0;
    color.blue  = 0.0;

    sprintf(title, "Y axis");
    utype = 999;
    graphic[i] = gv_alloc(GV_NONINDEXED, GV_POLYLINES, mask, color, title, utype, 0);
    if (graphic[i] != NULL) {
        graphic[i]->number    = 1;
        graphic[i]->lineWidth = 3;
        #if   defined(GEOM_CAPRI)
            graphic[i]->fdata = (float*) gi_alloc(6*sizeof(float));
            graphic[i]->fdata[0] = 0;
            graphic[i]->fdata[1] = 0;
            graphic[i]->fdata[2] = 0;
            graphic[i]->fdata[3] = 0;
            graphic[i]->fdata[4] = 1;
            graphic[i]->fdata[5] = 0;
        #elif defined(GEOM_EGADS)
            graphic[i]->ddata = (double*)  malloc(6*sizeof(double));
            graphic[i]->ddata[0] = 0;
            graphic[i]->ddata[1] = 0;
            graphic[i]->ddata[2] = 0;
            graphic[i]->ddata[3] = 0;
            graphic[i]->ddata[4] = 1;
            graphic[i]->ddata[5] = 0;
        #endif
        graphic[i]->object->length = 1;
        #if   defined(GEOM_CAPRI)
            graphic[i]->object->type.plines.len = (int*) gi_alloc(sizeof(int));
        #elif defined(GEOM_EGADS)
            graphic[i]->object->type.plines.len = (int*)   malloc(sizeof(int));
        #endif
        graphic[i]->object->type.plines.len[0] = 2;

        gv_adopt("Axes", graphic[i]);

        i++;
    }

    /* z-axis */
    mask = GV_FOREGROUND | GV_FORWARD | GV_ORIENTATION;

    color.red   = 0.0;
    color.green = 0.0;
    color.blue  = 1.0;

    sprintf(title, "Z axis");
    utype = 999;
    graphic[i] = gv_alloc(GV_NONINDEXED, GV_POLYLINES, mask, color, title, utype, 0);
    if (graphic[i] != NULL) {
        graphic[i]->number    = 1;
        graphic[i]->lineWidth = 3;
        #if   defined(GEOM_CAPRI)
            graphic[i]->fdata = (float*) gi_alloc(6*sizeof(float));
            graphic[i]->fdata[0] = 0;
            graphic[i]->fdata[1] = 0;
            graphic[i]->fdata[2] = 0;
            graphic[i]->fdata[3] = 0;
            graphic[i]->fdata[4] = 0;
            graphic[i]->fdata[5] = 1;
        #elif defined(GEOM_EGADS)
            graphic[i]->ddata = (double*)  malloc(6*sizeof(double));
            graphic[i]->ddata[0] = 0;
            graphic[i]->ddata[1] = 0;
            graphic[i]->ddata[2] = 0;
            graphic[i]->ddata[3] = 0;
            graphic[i]->ddata[4] = 0;
            graphic[i]->ddata[5] = 1;
        #endif
        graphic[i]->object->length = 1;
        #if   defined(GEOM_CAPRI)
            graphic[i]->object->type.plines.len = (int*) gi_alloc(sizeof(int));
        #elif defined(GEOM_EGADS)
            graphic[i]->object->type.plines.len = (int*)   malloc(sizeof(int));
        #endif
        graphic[i]->object->type.plines.len[0] = 2;

        gv_adopt("Axes", graphic[i]);

        i++;
    }

    /* Bodys */
    for (jbody = 0; jbody < nbody; jbody++) {
        ibody = bodyList[jbody];
        nedge = MODL->body[ibody].nedge;
        nface = MODL->body[ibody].nface;

        #if   defined(GEOM_CAPRI)
            ivol = MODL->body[ibody].ivol;
            gi_iGetDisplace(ivol, (double*)xform);
        #elif defined(GEOM_EGADS)
        #endif

        /* if the family does not exist, create it */
        sprintf(bodyName, "Body %d", ibody);

        if (gv_getfamily( bodyName, 1, &attr) == -1){
            gv_allocfamily(bodyName);
        }

        /* create a graphic object for each Edge (in ivol) */
        for (iedge = 1; iedge <= nedge; iedge++) {

            /* get the Edge info */
            #if   defined(GEOM_CAPRI)
                (void) gi_dTesselEdge(ivol, iedge, &npnt, &xyz, &uv);
            #elif defined(GEOM_EGADS)
                etess = MODL->body[ibody].etess;
                (void) EG_getTessEdge(etess, iedge, &npnt, &xyz, &uv);
            #endif

            /* set up the new graphic object */
            mask = MODL->body[ibody].edge[iedge].gratt.render;
            color.red   = RED(  MODL->body[ibody].edge[iedge].gratt.color);
            color.green = GREEN(MODL->body[ibody].edge[iedge].gratt.color);
            color.blue  = BLUE( MODL->body[ibody].edge[iedge].gratt.color);

            sprintf(title, "Edge %d", iedge);
            utype = 1 + 10 * ibody;
            graphic[i] = gv_alloc(GV_NONINDEXED, GV_POLYLINES, mask, color, title, utype, iedge);

            if (graphic[i] != NULL) {
                graphic[i]->number     = 1;
                graphic[i]->lineWidth  = MODL->body[ibody].edge[iedge].gratt.lwidth;
                graphic[i]->pointSize  = 3;
                graphic[i]->mesh.red   = 0;
                graphic[i]->mesh.green = 0;
                graphic[i]->mesh.blue  = 0;

                /* load the data */
                #if   defined(GEOM_CAPRI)
                    graphic[i]->fdata = (float*) gi_alloc(3*npnt*sizeof(float));
                    for (j = 0; j < npnt; j++) {
                        transform(xform, &xyz[3*j], &graphic[i]->fdata[3*j]);
                    }
                #elif defined(GEOM_EGADS)
                    graphic[i]->ddata = (double*)  malloc(3*npnt*sizeof(double));
                    for (j = 0; j < npnt; j++) {
                        graphic[i]->ddata[3*j  ] = xyz[3*j  ];
                        graphic[i]->ddata[3*j+1] = xyz[3*j+1];
                        graphic[i]->ddata[3*j+2] = xyz[3*j+2];
                    }
                #endif

                object = graphic[i]->object;
                object->length = 1;
                #if   defined(GEOM_CAPRI)
                    object->type.plines.len = (int *) gi_alloc(sizeof(int));
                #elif defined(GEOM_EGADS)
                    object->type.plines.len = (int *)   malloc(sizeof(int));
                #endif
                object->type.plines.len[0] = npnt;

                gv_adopt(bodyName, graphic[i]);
            }
            i++;
        }

        /* create a graphic object for each Face (in ivol) */
        for (iface = 1; iface <= nface; iface++) {

            /* get the Face info */
            #if   defined(GEOM_CAPRI)
                (void) gi_dTesselFace(ivol, iface, &ntri, &tris, &tric,
                                      &npnt, &xyz, &ptype, &pindx, &uv);
            #elif defined(GEOM_EGADS)
                etess = MODL->body[ibody].etess;
                (void) EG_getTessFace(etess, iface, &npnt, &xyz, &uv, &ptype, &pindx,
                                      &ntri, &tris, &tric);
            #endif

            /* set up new grafic object */
            mask = MODL->body[ibody].face[iface].gratt.render;
            color.red   = RED(  MODL->body[ibody].face[iface].gratt.color);
            color.green = GREEN(MODL->body[ibody].face[iface].gratt.color);
            color.blue  = BLUE( MODL->body[ibody].face[iface].gratt.color);

            sprintf(title, "Face %d ", iface);
            utype = 2 + 10 * ibody;
            graphic[i] = gv_alloc(GV_INDEXED, GV_DISJOINTTRIANGLES, mask, color, title, utype, iface);

            if (graphic[i] != NULL) {
                graphic[i]->back.red   = RED(  MODL->body[ibody].face[iface].gratt.bcolor);
                graphic[i]->back.green = GREEN(MODL->body[ibody].face[iface].gratt.bcolor);
                graphic[i]->back.blue  = BLUE( MODL->body[ibody].face[iface].gratt.bcolor);

                graphic[i]->mesh.red   = RED(  MODL->body[ibody].face[iface].gratt.mcolor);
                graphic[i]->mesh.green = GREEN(MODL->body[ibody].face[iface].gratt.mcolor);
                graphic[i]->mesh.blue  = BLUE( MODL->body[ibody].face[iface].gratt.mcolor);

                graphic[i]->number    = 1;
                graphic[i]->lineWidth = MODL->body[ibody].face[iface].gratt.lwidth;

                /* load the data */
                #if   defined(GEOM_CAPRI)
                    graphic[i]->fdata = (float*) gi_alloc(3*npnt*sizeof(float));
                    for (j = 0; j < npnt; j++) {
                        transform(xform, &xyz[3*j], &graphic[i]->fdata[3*j]);
                    }
                #elif defined(GEOM_EGADS)
                    graphic[i]->ddata = (double*)  malloc(3*npnt*sizeof(double));
                    for (j = 0; j < npnt; j++) {
                        graphic[i]->ddata[3*j  ] = xyz[3*j  ];
                        graphic[i]->ddata[3*j+1] = xyz[3*j+1];
                        graphic[i]->ddata[3*j+2] = xyz[3*j+2];
                    }
                #endif


                object = graphic[i]->object;
                object->length = ntri;
                #if   defined(GEOM_CAPRI)
                    object->type.distris.index = (int *) gi_alloc(3*ntri*sizeof(int));
                #elif defined(GEOM_EGADS)
                    object->type.distris.index = (int *)   malloc(3*ntri*sizeof(int));
                #endif

                for (j = 0; j < 3*ntri; j++) {
                    object->type.distris.index[j] = tris[j]-1;
                }

                gv_adopt(bodyName, graphic[i]);
            }
            i++;
        }
    }

//cleanup:
    return;
}


/***********************************************************************/
/*                                                                     */
/*   gvscalar - called by gv for color rendering of graphic objects    */
/*                                                                     */
/***********************************************************************/

int
gvscalar(int       key,                 /* (in)  scalar index (from gv_init) */
         GvGraphic *graphic,            /* (in)  GvGraphic structure for scalar fill */
/*@unused@*/int    len,                 /* (in)  length of scalar to be filled */
         float     *scalar)             /* (out) scalar to be filled */
{
    int       answer;                   /* return value */

    int       utype, iface, i, ibody;
    int       ntri, npnt;
    CINT      *tris, *tric, *ptype, *pindx;
    double    umin, umax, vmin, vmax;
    CDOUBLE   *xyz, *uv;

    #if   defined(GEOM_CAPRI)
    int       ivol, nnode, nedge, nface, nbound;
        char      *name;
    #elif defined(GEOM_EGADS)
        ego       etess;
    #endif

    modl_T    *MODL = (modl_T*)modl;

//    ROUTINE(gvscalar);

    /* --------------------------------------------------------------- */

//$$$    SPRINT3(2, "enter gvscalar(key=%d, len=%d, scalar=%f",
//$$$           key, len, *scalar);

    utype  = graphic->utype;
    iface  = graphic->uindex;

    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        if (MODL->body[ibody].botype == OCSM_SOLID_BODY) {
            #if   defined(GEOM_CAPRI)
                ivol = MODL->body[ibody].ivol;
                (void) gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            #elif defined(GEOM_EGADS)
            #endif

            /* Face */
            if (utype%10 == 2) {
                 #if   defined(GEOM_CAPRI)
                     (void) gi_dTesselFace(ivol, iface, &ntri, &tris, &tric,
                                          &npnt, &xyz, &ptype, &pindx, &uv);
                 #elif defined(GEOM_EGADS)
                     etess = MODL->body[ibody].etess;
                     (void) EG_getTessFace(etess, iface, &npnt, &xyz, &uv, &ptype, &pindx,
                                           &ntri, &tris, &tric);
                 #endif

                 if (key == 0) {
                     umin = uv[0];
                     umax = uv[0];
                     for (i = 0; i < npnt; i++) {
                         umin = MIN(umin, uv[2*i]);
                         umax = MAX(umax, uv[2*i]);
                     }

                     for (i = 0; i < npnt; i++) {
                         scalar[i] = (uv[2*i] - umin) / (umax - umin);
                     }
                 } else if (key == 1) {
                     vmin = uv[1];
                     vmax = uv[1];
                     for (i = 0; i < npnt; i++) {
                         vmin = MIN(vmin, uv[2*i+1]);
                         vmax = MAX(vmax, uv[2*i+1]);
                     }

                     for (i = 0; i < npnt; i++) {
                         scalar[i] = (uv[2*i+1] - vmin) / (vmax - vmin);
                     }
                 } else {
                     for (i = 0; i < npnt; i++) {
                         scalar[i] = 0.;
                     }
                 }
            }
        }
    }

//cleanup:
    answer = 1;
    return answer;
}


/***********************************************************************/
/*                                                                     */
/*   gvevent - called by gv calls to process callbacks                 */
/*                                                                     */
/***********************************************************************/

void
gvevent(int       *win,                 /* (in)  window of event */
        int       *type,                /* (in)  type of event */
/*@unused@*/int   *xpix,                /* (in)  x-pixel location of event */
/*@unused@*/int   *ypix,                /* (in)  y-pixel location of event */
        int       *state)               /* (in)  aditional event info */
{
    FILE      *fp=NULL;

    int       uindex, utype, iedge, iface, ibrch, ipmtr, irow, icol, nrow, ncol;
    int       itype, found, status, idum, ibody, ipaste, dum1, dum2;
    int       iclass, iactv, ichld, ileft, irite, narg, iarg, nattr, iattr;
    double    value, dum;
    char      jnlName[255], tempName[255], fileName[255];
    char      pmtrName[255], editName[255], defn[255], brchName[255];
    char      aName[255], aValue[255];

    static int utype_save = 0, uindex_save = 0;

    #if   defined(GEOM_CAPRI)
        int     ivol, numvol;
    #elif defined(GEOM_EGADS)
        int     jbody, i, nlist;
        CINT    *tempIlist;
        CDOUBLE *tempRlist;
        CCHAR   *attrName;
        CCHAR   *tempClist;
    #endif

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(gvevent);

    /* --------------------------------------------------------------- */

//$$$    SPRINT5(2, "enter gvevent(*win=%d, *type=%d, *state=%d (%c), script=%d)",
//$$$            *win, *type, *state, *state, (int)script);

    /* repeat as long as we are reading a script (or once if
       not reading a script) */
    do {

        /* get the next script line if we are reading a script (and insert
           a '$' if we have detected an EOF) */

        if (script != NULL) {
            if (fscanf(script, "%1s", (char*)state) != 1) {
                *state = '$';
            }
            *win  = ThreeD;
            *type = KeyPress;
        }

        if ((*win == ThreeD) && (*type == KeyPress)) {
            if (*state == '\0') {

                /* these calls should never be made */
                idum = getInt("Dummy call to use getInt");
                dum  = getDbl("Dummy call to use getDbl");
                printf("idum=%d   dum=%f\n", idum, dum);

            /* 'a' - add Parameter */
            } else if (*state == 'a') {
                SPRINT0(0, "--> Option 'a' chosen (add Parameter)");

                getStr("Enter Parameter name: ", pmtrName);
                nrow = getInt("Enter number of rows: ");
                ncol = getInt("Enter number of cols: ");

                status = ocsmNewPmtr(modl, pmtrName, OCSM_EXTERNAL, nrow, ncol);
                SPRINT5(0, "--> ocsmNewPmtr(name=%s, nrow=%d, ncol=%d) -> status=%d (%s)",
                        pmtrName, nrow, ncol, status, ocsmGetText(status));

                status = ocsmInfo(modl, &dum1, &ipmtr, &dum2);

                for (icol = 1; icol <= ncol; icol++) {
                    for (irow = 1; irow <= nrow; irow++) {
                        SPRINT1x(0, "Enter value for %s", pmtrName);
                        SPRINT1x(0, "[%d,",               irow    );
                        SPRINT1x(0, "%d]",                icol    );
                        getStr(": ", defn);

                        status = ocsmSetValu(modl, ipmtr, irow, icol, defn);
                        SPRINT5(0, "--> ocsmSetValu(irow=%d, icol=%d, defn=%s) -> status=%d (%s)",
                                irow, icol, defn, status, ocsmGetText(status));
                    }
                }

            /* 'A' - add Branch */
            } else if (*state == 'A') {
                char str1[257], str2[257], str3[257], str4[257], str5[257];
                char str6[257], str7[257], str8[257], str9[257];
                SPRINT0(0, "--> Option 'A' chosen (add Branch)");

                SPRINT0(0, "1 box        11 extrude    31 intersect  51 set   ");
                SPRINT0(0, "2 sphere     12 loft       32 subtract   52 macbeg");
                SPRINT0(0, "3 cone       13 revolve    33 union      53 macend");
                SPRINT0(0, "4 cylinder                               54 recall");
                SPRINT0(0, "5 torus      21 fillet     41 translate  55 patbeg");
                SPRINT0(0, "6 import     22 chamfer    42 rotatex    56 patend");
                SPRINT0(0, "7 udprim                   43 rotatey    57 mark  ");
                SPRINT0(0, "                           44 rotatez    58 dump  ");
                SPRINT0(0, "                           45 scale               ");
                itype = getInt("Enter type to add: ");

                strcpy(str1, "$");
                strcpy(str2, "$");
                strcpy(str3, "$");
                strcpy(str4, "$");
                strcpy(str5, "$");
                strcpy(str6, "$");
                strcpy(str7, "$");
                strcpy(str8, "$");
                strcpy(str9, "$");

                if        (itype ==  1) {
                    itype = OCSM_BOX;
                    getStr("Enter xbase : ",  str1   );
                    getStr("Enter ybase : ",  str2   );
                    getStr("Enter zbase : ",  str3   );
                    getStr("Enter dx    : ",  str4   );
                    getStr("Enter dy    : ",  str5   );
                    getStr("Enter dz    : ",  str6   );
                } else if (itype ==  2) {
                    itype = OCSM_SPHERE;
                    getStr("Enter xcent : ",  str1   );
                    getStr("Enter ycent : ",  str2   );
                    getStr("Enter zcent : ",  str3   );
                    getStr("Enter radius: ",  str4   );
                } else if (itype ==  3) {
                    itype = OCSM_CONE;
                    getStr("Enter xvrtx : ",  str1   );
                    getStr("Enter yvrtx : ",  str2   );
                    getStr("Enter zvrtx : ",  str3   );
                    getStr("Enter xbase : ",  str4   );
                    getStr("Enter ybase : ",  str5   );
                    getStr("Enter zbase : ",  str6   );
                    getStr("Enter radius: ",  str7   );
                } else if (itype ==  4) {
                    itype = OCSM_CYLINDER;
                    getStr("Enter xbeg  : ",  str1   );
                    getStr("Enter ybeg  : ",  str2   );
                    getStr("Enter zbeg  : ",  str3   );
                    getStr("Enter xend  : ",  str4   );
                    getStr("Enter yend  : ",  str5   );
                    getStr("Enter zend  : ",  str6   );
                    getStr("Enter radius: ",  str7   );
                } else if (itype ==  5) {
                    itype = OCSM_TORUS;
                    getStr("Enter xcent : ",  str1   );
                    getStr("Enter ycent : ",  str2   );
                    getStr("Enter zcent : ",  str3   );
                    getStr("Enter dxaxis: ",  str4   );
                    getStr("Enter dyaxis: ",  str5   );
                    getStr("Enter dzaxis: ",  str6   );
                    getStr("Enter majrad: ",  str7   );
                    getStr("Enter minrad: ",  str8   );
                } else if (itype ==  6) {
                    itype = OCSM_IMPORT;
                    getStr("Enter filNam: ", &str1[1]);
                } else if (itype ==  7) {
                    itype = OCSM_UDPRIM;
                    getStr("Enter ptype : ", &str1[1]);
                    getStr("Enter name1 : ", &str2[1]);
                    getStr("Enter value1: ", &str3[1]);
                    getStr("Enter name2 : ", &str4[1]);
                    getStr("Enter value2: ", &str5[1]);
                    getStr("Enter name3 : ", &str6[1]);
                    getStr("Enter value3: ", &str7[1]);
                    getStr("Enter name4 : ", &str8[1]);
                    getStr("Enter value4: ", &str9[1]);
                } else if (itype == 11) {
                    itype = OCSM_EXTRUDE;
                    getStr("Enter dx    : ",  str1   );
                    getStr("Enter dy    : ",  str2   );
                    getStr("Enter dz    : ",  str3   );
                } else if (itype == 12) {
                    itype = OCSM_LOFT;
                    getStr("Enter smooth: ",  str1   );
                } else if (itype == 13) {
                    itype = OCSM_REVOLVE;
                    getStr("Enter xorig : ",  str1   );
                    getStr("Enter yorig : ",  str2   );
                    getStr("Enter zorig : ",  str3   );
                    getStr("Enter dxaxis: ",  str4   );
                    getStr("Enter dyaxis: ",  str5   );
                    getStr("Enter dzaxis: ",  str6   );
                    getStr("Enter angDeg: ",  str7   );
                } else if (itype == 21) {
                    itype = OCSM_FILLET;
                    getStr("Enter radius: ",  str1   );
                    getStr("Enter iford1: ",  str2   );
                    getStr("Enter iford2: ",  str3   );
                } else if (itype == 22) {
                    itype = OCSM_CHAMFER;
                    getStr("Enter radius: ",  str1   );
                    getStr("Enter iford1: ",  str2   );
                    getStr("Enter iford2: ",  str3   );
                } else if (itype == 31) {
                    itype = OCSM_INTERSECT;
                    getStr("Enter order : ", &str1[1]);
                    getStr("Enter index : ",  str2   );
                } else if (itype == 32) {
                    itype = OCSM_SUBTRACT;
                    getStr("Enter order : ", &str1[1]);
                    getStr("Enter index : ",  str2   );
                } else if (itype == 33) {
                    itype = OCSM_UNION;
                } else if (itype == 41) {
                    itype = OCSM_TRANSLATE;
                    getStr("Enter dx    : ",  str1   );
                    getStr("Enter dy    : ",  str2   );
                    getStr("Enter dz    : ",  str3   );
                } else if (itype == 42) {
                    itype = OCSM_ROTATEX;
                    getStr("Enter angDeg: ",  str1   );
                    getStr("Enter yaxis : ",  str2   );
                    getStr("Enter zaxis : ",  str3   );
                } else if (itype == 43) {
                    itype = OCSM_ROTATEY;
                    getStr("Enter angDeg: ",  str1   );
                    getStr("Enter zaxis : ",  str2   );
                    getStr("Enter xaxis : ",  str3   );
                } else if (itype == 44) {
                    itype = OCSM_ROTATEZ;
                    getStr("Enter angDeg: ",  str1   );
                    getStr("Enter xaxis : ",  str2   );
                    getStr("Enter yaxish: ",  str3   );
                } else if (itype == 45) {
                    itype = OCSM_SCALE;
                    getStr("Enter fact  : ",  str1   );
                } else if (itype == 51) {
                    itype = OCSM_SET;
                    getStr("Enter pname : ", &str1[1]);
                    getStr("Enter defn  : ",  str2   );
                } else if (itype == 52) {
                    itype = OCSM_MACBEG;
                    getStr("Enter istore: ",  str1   );
                } else if (itype == 53) {
                    itype = OCSM_MACEND;
                } else if (itype == 54) {
                    itype = OCSM_RECALL;
                    getStr("Enter istore: ",  str1   );
                } else if (itype == 55) {
                    itype = OCSM_PATBEG;
                    getStr("Enter pname : ", &str1[1]);
                    getStr("Enter ncopy : ",  str2   );
                } else if (itype == 56) {
                    itype = OCSM_PATEND;
                } else if (itype == 57) {
                    itype = OCSM_MARK;
                } else if (itype == 58) {
                    itype = OCSM_DUMP;
                    getStr("Enter filNam: ", &str1[1]);
                    getStr("Enter remove: ",  str2   );
                } else {
                    SPRINT1(0, "Illegal type (%d)", itype);
                    goto next_option;
                }

                status = ocsmNewBrch(MODL, MODL->nbrch, itype,
                                     str1, str2, str3, str4, str5,
                                     str6, str7, str8, str9);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsnNewBrch(ibrch=%d) -> status=%d (%s)",
                            MODL->nbrch, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT1(0, "Branch %d has been added", MODL->nbrch);
                SPRINT0(0, "Use 'B' to rebuild");

            /* 'b' - undefined option */
            } else if (*state == 'b') {
                SPRINT0(0, "--> Option 'b' (undefined)");

            /* 'B' - build to Branch */
            } else if (*state == 'B') {
                int     buildTo;
                clock_t old_time, new_time;
                #if   defined(GEOM_CAPRI)
                #elif defined(GEOM_EGADS)
                    double size, box[6], params[3];
                    ego    ebody;
                #endif
                SPRINT0(0, "--> Option 'B' chosen (build to Branch)");

                if (numarg > 0) {
                    buildTo = numarg;
                    numarg  = 0;
                } else {
                    buildTo = 0;
                }

                #if   defined(GEOM_CAPRI)
                   numvol = gi_uNumVolumes();

                   if (numvol <= 0) {
                       SPRINT0(0, "--> no volumes to release");
                   } else {
                       for (ivol = 1; ivol <= numvol; ivol++) {
                           status = gi_uStatModel(ivol);
                           if (status >= 0) {
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

                old_time = clock();
                status   = ocsmCheck(modl);
                new_time = clock();
                SPRINT2(0, "--> ocsmCheck() -> status=%d (%s)",
                        status, ocsmGetText(status));
                SPRINT1(0, "==> ocsmCheck CPUtime=%10.3f sec",
                        (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

                if (status < SUCCESS) goto next_option;

                nbody    = MAX_BODYS;
                old_time = clock();
                status   = ocsmBuild(modl, buildTo, &builtTo, &nbody, bodyList);
                new_time = clock();
                SPRINT5(0, "--> ocsmBuild(buildTo=%d) -> status=%d (%s), builtTo=%d, nbody=%d",
                        buildTo, status, ocsmGetText(status), builtTo, nbody);
                SPRINT1(0, "==> ocsmBuild CPUtime=%10.3f sec",
                        (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));

                if (status < SUCCESS) goto next_option;

                #if   defined(GEOM_CAPRI)
                #elif defined(GEOM_EGADS)
                    old_time = clock();
                    for (jbody = 0; jbody < nbody; jbody++) {
                        ibody = bodyList[jbody];
                        ebody = MODL->body[ibody].ebody;

                        status = EG_getBoundingBox(ebody, box);
                        size   = sqrt(pow(box[3]-box[0],2) + pow(box[4]-box[1],2)
                                                           + pow(box[5]-box[2],2));

                        /* vTess parameters */
                        params[0] = 0.0250 * size;
                        params[1] = 0.0010 * size;
                        params[2] = 15.0;

                        ebody = MODL->body[ibody].ebody;
                        (void) EG_setOutLevel(MODL->context, 0);
                        status = EG_makeTessBody(ebody, params, &(MODL->body[ibody].etess));
                        (void) EG_setOutLevel(MODL->context, outLevel);
                        SPRINT6(0, "--> EG_makeTessBody(ibody=%4d, params=%10.5f, %10.5f, %10.5f) -> status=%d (%s)",
                                ibody, params[0], params[1], params[2], status, ocsmGetText(status));
                    }
                    new_time = clock();
                    SPRINT1(0, "==> EG_makeTessBody CPUtime=%10.3f sec",
                            (double)(new_time-old_time) / (double)(CLOCKS_PER_SEC));
                #endif

                if (status >= 0) {
                    new_data = 1;
                }

            /* 'c' - undefined option */
            } else if (*state == 'c') {
                SPRINT0(0, "--> Option 'c' (undefined)");

            /* 'C' - undefined option */
            } else if (*state == 'C') {
                SPRINT0(0, "--> Option 'C' (undefined)");

            /* 'd' - undefined option */
            } else if (*state == 'd') {
                SPRINT0(0, "--> Option 'd' (undefined)");

            /* 'D' - delete Branch */
            } else if (*state == 'D') {
                SPRINT0(0, "--> Option 'D' (delete Branch)");

                status = ocsmDelBrch(MODL, MODL->nbrch);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmDelBrch(ibrch=%d) -> status=%d (%s)",
                            MODL->nbrch, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT0(0, "Branch deleted");
                SPRINT0(0, "Use 'B' to rebuild");

            /* 'e' - edit Parameter */
            } else if (*state == 'e') {
                SPRINT0(0, "--> Option 'e' chosen (edit Parameter)");

                getStr("Enter parameter name: ", editName);

                found = 0;
                for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                    status = ocsmGetPmtr(modl, ipmtr, &itype, &nrow, &ncol, pmtrName);
                    if (status == SUCCESS && strcmp(editName, pmtrName) == 0) {
                        if (itype != OCSM_EXTERNAL) {
                            SPRINT1(0, "Parameter '%s' cannot be edited", editName);
                            goto next_option;
                        }

                        if (nrow > 1) {
                            irow = getInt("Enter row    number: ");
                        } else {
                            irow = 1;
                        }
                        if (ncol > 1) {
                            icol = getInt("Enter column number: ");
                        } else {
                            icol = 1;
                        }

                        status = ocsmGetValu(modl, ipmtr, irow, icol, &value);
                        SPRINT1(0, "Old       definition: %f", value);
                        getStr( "Enter new definition: ",   defn);

                        status = ocsmSetValu(modl, ipmtr, irow, icol, defn);
                        if (status != SUCCESS) {
                            SPRINT4(0, "**> ocsmSetPmtr(ipmtr=%d, defn=%s) -> status=%d (%s)",
                                    ipmtr, defn, status, ocsmGetText(status));
                            goto next_option;
                        }
                        found = 1;
                        break;
                    }
                }

                if (found == 0) {
                    SPRINT1(0, "Parameter '%s' not found", editName);
                    goto next_option;
                }

                SPRINT1(0, "Parameter %s has been redefined", editName);
                SPRINT0(0, "Use 'B' to rebuild");

            /* 'E' - edit Branch */
            } else if (*state == 'E') {
                SPRINT0(0, "--> Option 'E' chosen (edit Branch)");

                if (numarg > 0) {
                    ibrch  = numarg;
                    numarg = 0;
                } else {
                    ibrch = getInt("Enter Branch to edit: ");
                }

                if (ibrch < 1 || ibrch > MODL->nbrch) {
                    SPRINT2(0, "Illegal ibrch=%d (should be between 1 and %d)", ibrch, MODL->nbrch);
                    goto next_option;
                }

                status = ocsmGetBrch(modl, ibrch, &itype, &iclass, &iactv,
                                     &ichld, &ileft, &irite, &narg, &nattr);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmGetBranch(ibrch=%d) -> status=%d (%s)",
                            ibrch, status, ocsmGetText(status));
                    goto next_option;
                }

                for (iarg = 1; iarg <= narg; iarg++) {
                    status = ocsmGetArg(modl, ibrch, iarg, defn, &value);
                    if (status != SUCCESS) {
                        SPRINT4(0, "**> ocsmGetArg(ibrch=%d, iarg=%d) -> status=%d (%s)",
                                ibrch, iarg, status, ocsmGetText(status));
                        goto next_option;
                    }

                    SPRINT2(0, "Old       definition for arg %d: %s", iarg, defn);
                    getStr( "Enter new definition ('.' to unchange): ",  defn);

                    if (strcmp(defn, ".") == 0) {
                        SPRINT0(0, "Definition unchanged");
                    } else {
                        status = ocsmSetArg(modl, ibrch, iarg, defn);
                        if (status != SUCCESS) {
                            SPRINT5(0, "**> ocsmSetArg(ibrch=%d, iarg=%d, defn=%s) -> status=%d (%s)",
                                    ibrch, iarg, defn, status, ocsmGetText(status));
                            goto next_option;
                        }

                        SPRINT2(0, "New       definition for arg %d: %s", iarg,  defn);
                    }
                }

                SPRINT0(0, "Use 'B' to rebuild");

            /* 'f' - undefined option */
            } else if (*state == 'f') {
                SPRINT0(0, "--> Option 'f' (undefined)");

            /* 'F' - undefined option */
            } else if (*state == 'F') {
                SPRINT0(0, "--> Option 'F' (undefined)");

            /* 'g' - undefined option */
            } else if (*state == 'g') {
                SPRINT0(0, "--> Option 'g' (undefined)");

            /* 'G' - undefined option */
            } else if (*state == 'G') {
                SPRINT0(0, "--> Option 'G' (undefined)");

            /* 'h' - hide Edge or Face at cursor */
            } else if (*state == 'h') {
                uindex = pickObject(&utype);

                if (utype%10 == 1) {
                    ibody = utype / 10;
                    iedge = uindex;

                    MODL->body[ibody].edge[iedge].gratt.render = 0;
                    SPRINT2(0, "Hiding Edge %d (body %d)", iedge, ibody);

                    new_data    = 1;
                    utype_save  = utype;
                    uindex_save = uindex;
                } else if (utype%10 == 2) {
                    ibody = utype / 10;
                    iface = uindex;

                    MODL->body[ibody].face[iface].gratt.render = 0;
                    SPRINT2(0, "Hiding Face %d (body %d)", iface, ibody);

                    new_data    = 1;
                    utype_save  = utype;
                    uindex_save = uindex;
                } else {
                    SPRINT0(0, "nothing to hide");
                }

            /* 'H' - undefined option */
            } else if (*state == 'H') {
                SPRINT0(0, "--> Option 'H' (undefined)");

            /* 'i' - undefined option */
            } else if (*state == 'i') {
                SPRINT0(0, "--> Option 'i' (undefined)");

            /* 'I' - undefined option */
            } else if (*state == 'I') {
                SPRINT0(0, "--> Option 'I' (undefined)");

            /* 'j' - undefined option */
            } else if (*state == 'j') {
                SPRINT0(0, "--> Option 'j' (undefined)");

            /* 'J' - undefined option */
            } else if (*state == 'J') {
                SPRINT0(0, "--> Option 'J' (undefined)");

            /* 'k' - undefined option */
            } else if (*state == 'k') {
                SPRINT0(0, "--> Option 'k' (undefined)");

            /* 'K' - undefined option */
            } else if (*state == 'K') {
                SPRINT0(0, "--> Option 'K' (undefined)");

            /* 'l' - list Parameters */
            } else if (*state == 'l') {
                SPRINT0(0, "--> Option 'l' chosen (list Parameters)");

                status = ocsmPrintPmtrs(modl, stdout);
                SPRINT2(0, "--> ocsmPrintPmtrs() -> status=%d (%s)",
                        status, ocsmGetText(status));

            /* 'L' - list Branches */
            } else if (*state == 'L') {
                SPRINT0(0, "--> Option 'L' chosen (list Branches)");

                status = ocsmPrintBrchs(modl, stdout);
                SPRINT2(0, "--> ocsmPrintBrchs() -> status=%d (%s)",
                        status, ocsmGetText(status));

            /* 'm' - undefined option */
            } else if (*state == 'm') {
                SPRINT0(0, "--> Option 'm' (undefined)");

            /* 'M' - undefined option */
            } else if (*state == 'M') {
                SPRINT0(0, "--> Option 'M' (undefined)");

            /* 'n' - undefined option */
            } else if (*state == 'n') {
                SPRINT0(0, "--> Option 'n' (undefined)");

            /* 'N' - name Branch */
            } else if (*state == 'N') {
                SPRINT0(0, "--> Option 'N' chosen (name Branch)");

                if (numarg > 0) {
                    ibrch  = numarg;
                    numarg = 0;
                } else {
                    ibrch = getInt("Enter Branch to rename: ");
                }

                if (ibrch < 1 || ibrch > MODL->nbrch) {
                    SPRINT2(0, "Illegal ibrch=%d (should be between 1 and %d)", ibrch, MODL->nbrch);
                    goto next_option;
                }

                status = ocsmGetName(modl, ibrch, brchName);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmGetName(ibrch=%d) -> status=%d (%s)",
                            ibrch, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT2(0, "--> Name of Branch %d is %s", ibrch, brchName);

                getStr("Enter new Branch name (. for none): ", brchName);
                if (strcmp(brchName, ".") == 0) {
                    SPRINT1(0, "Branch %4d has not been renamed", ibrch);
                    goto next_option;
                }

                status = ocsmSetName(modl, ibrch, brchName);
                if (status != SUCCESS) {
                    SPRINT4(0, "**> ocsmSetName(ibrch=%d, name=%s) -> status=%d (%s)",
                            ibrch, brchName, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT1(0, "Branch %4d has been renamed", ibrch);

            /* 'o' - undefined option */
            } else if (*state == 'o') {
                SPRINT0(0, "--> Option 'o' (undefined)");

            /* 'O' - undefined option */
            } else if (*state == 'O') {
                SPRINT0(0, "--> Option 'O' (undefined)");

            /* 'p' - undefined option */
            } else if (*state == 'p') {
                SPRINT0(0, "--> Option 'p' (undefined)");

            /* 'P' - undefined option */
            } else if (*state == 'P') {
                SPRINT0(0, "--> Option 'P' (undefined)");

            /* 'q' - query Edge/Face at cursor */
            } else if (*state == 'q') {
                #if   defined(GEOM_CAPRI)
                #elif defined(GEOM_EGADS)
                    ego eedge, eface;
                #endif
                SPRINT0(0, "--> Option q chosen (query Edge/Face at cursor) ");

                uindex = pickObject(&utype);

                /* Edge found */
                if        (utype%10 == 1) {
                    ibody = utype / 10;
                    iedge = uindex;

                    SPRINT2(0, "Body %4d Edge %4d:", ibody, iedge);

                    #if   defined(GEOM_CAPRI)
                    #elif defined(GEOM_EGADS)
                        eedge  = MODL->body[ibody].edge[iedge].eedge;
                        status = EG_attributeNum(eedge, &nattr);
                    #endif

                    for (iattr = 1; iattr <= nattr; iattr++) {
                        #if   defined(GEOM_CAPRI)
                        #elif defined(GEOM_EGADS)
                            status = EG_attributeGet(eedge, iattr, &attrName, &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            SPRINT1x(0, "                     %-20s =", attrName);

                            if        (itype == ATTRINT) {
                                for (i = 0; i < nlist ; i++) {
                                    SPRINT1x(0, "%5d ", tempIlist[i]);
                                }
                                SPRINT0(0, " ");
                            } else if (itype == ATTRREAL) {
                                for (i = 0; i < nlist ; i++) {
                                    SPRINT1x(0, "%11.5f ", tempRlist[i]);
                                }
                                SPRINT0(0, " ");
                            } else if (itype == ATTRSTRING) {
                                SPRINT1(0, "%s", tempClist);
                            }
                        #endif
                    }

                /* Face found */
                } else if (utype%10 == 2) {
                    ibody = utype / 10;
                    iface = uindex;

                    SPRINT2(0, "Body %4d Face %4d:", ibody, iface);

                    #if   defined(GEOM_CAPRI)
                    #elif defined(GEOM_EGADS)
                        eface  = MODL->body[ibody].face[iface].eface;
                        status = EG_attributeNum(eface, &nattr);
                    #endif

                    for (iattr = 1; iattr <= nattr; iattr++) {
                        #if   defined(GEOM_CAPRI)
                        #elif defined(GEOM_EGADS)
                            status = EG_attributeGet(eface, iattr, &attrName, &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            SPRINT1x(0, "                     %-20s =", attrName);

                            if        (itype == ATTRINT) {
                                for (i = 0; i < nlist ; i++) {
                                    SPRINT1x(0, "%5d ", tempIlist[i]);
                                }
                                SPRINT0(0, " ");
                            } else if (itype == ATTRREAL) {
                                for (i = 0; i < nlist ; i++) {
                                    SPRINT1x(0, "%11.5f ", tempRlist[i]);
                                }
                                SPRINT0(0, " ");
                            } else if (itype == ATTRSTRING) {
                                SPRINT1(0, "%s", tempClist);
                            }
                        #endif
                    }

                /* other (Axis) found */
                } else {
                    SPRINT0(0, "Nothing found");
                }

                numarg = 0;

            /* 'Q' - undefined option */
            } else if (*state == 'Q') {
                SPRINT0(0, "--> Option 'Q' (undefined)");

            /* 'r' - undefined option */
            } else if (*state == 'r') {
                SPRINT0(0, "--> Option 'r' (undefined)");

            /* 'R' - resume a Branch */
            } else if (*state == 'R') {
                SPRINT0(0, "--> Option 'R' chosen (resume a Branch)");

                if (numarg > 0) {
                    ibrch  = numarg;
                    numarg = 0;
                } else {
                    ibrch = getInt("Enter Branch to resume (9999 for all): ");
                }

                if (ibrch == 9999) {
                    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
                        status = ocsmSetBrch(modl, ibrch, OCSM_ACTIVE);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmSetBrch(ibrch=%d) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    }

                    SPRINT0(0, "All Branches have been resumed");
                    goto next_option;
                }

                if (ibrch < 1 || ibrch > MODL->nbrch) {
                    SPRINT2(0, "Illegal ibrch=%d (should be between 1 and %d)", ibrch, MODL->nbrch);
                    goto next_option;
                }

                status = ocsmSetBrch(modl, ibrch, OCSM_ACTIVE);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmSetBrch(ibrch=%d) -> status=%d (%s)",
                            ibrch, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT1(0, "Brch %4d has been resumed", ibrch);
                SPRINT0(0, "Use 'B' to rebuild");

            /* 's' - undefined option */
            } else if (*state == 's') {
                SPRINT0(0, "--> Option 's' (undefined)");

            /* 'S' - suppress a Branch */
            } else if (*state == 'S') {
                SPRINT0(0, "--> Option 'S' chosen (suppress a Branch)");

                if (numarg > 0) {
                    ibrch  = numarg;
                    numarg = 0;
                } else {
                    ibrch = getInt("Enter Branch to suppress: ");
                }

                if (ibrch < 1 || ibrch > MODL->nbrch) {
                    SPRINT2(0, "Illegal ibrch=%d (should be between 1 and %d)", ibrch, MODL->nbrch);
                    goto next_option;
                }

                status = ocsmSetBrch(modl, ibrch, OCSM_SUPPRESSED);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmSetBrch(ibrch=%d) -> status=%d (%s)",
                            ibrch, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT1(0, "Branch %4d has been suppressed", ibrch);
                SPRINT0(0, "Use 'B' to rebuild");

            /* 't' - undefined option */
            } else if (*state == 't') {
                SPRINT0(0, "--> Option 't' (undefined)");

            /* 'T' - attribute Branch */
            } else if (*state == 'T') {
                SPRINT0(0, "--> Option 'T' (attribute Branch)");

                if (numarg > 0) {
                    ibrch  = numarg;
                    numarg = 0;
                } else {
                    ibrch = getInt("Enter Branch to attribute: ");
                }

                if (ibrch < 1 || ibrch > MODL->nbrch) {
                    SPRINT2(0, "Illegal ibrch=%d (should be between 1 and %d)", ibrch, MODL->nbrch);
                    goto next_option;
                }

                status = ocsmGetBrch(modl, ibrch, &itype, &iclass, &iactv,
                                     &ichld, &ileft, &irite, &narg, &nattr);
                if (status != SUCCESS) {
                    SPRINT3(0, "**> ocsmGetBrch(ibrch=%d) -> status=%d (%s)",
                            ibrch, status, ocsmGetText(status));
                    goto next_option;
                }

                for (iattr = 1; iattr <= nattr; iattr++) {
                    status = ocsmRetAttr(modl, ibrch, iattr, aName, aValue);
                    if (status != SUCCESS) {
                        SPRINT4(0, "**> ocsmRetAttr(ibrch=%d, iattr=%d) -> status=%d (%s)",
                                ibrch, iattr, status, ocsmGetText(status));
                        goto next_option;
                    }

                    SPRINT2(0, "   %-24s=%s", aName, aValue);
                }

                getStr("Enter Attribute name (. for none): ", aName );

                if (strcmp(aName, ".") == 0) {
                    SPRINT0(0, "Attribute has not been saved");
                    goto next_option;
                }

                getStr("Enter Attribute value            : ", aValue);
                status = ocsmSetAttr(modl, ibrch, aName, aValue);
                if (status != SUCCESS) {
                    SPRINT4(0, "**> ocsmSetAttr(ibrch=%d, aName=%s) -> status=%d (%s)",
                            ibrch, aName, status, ocsmGetText(status));
                    goto next_option;
                }

                SPRINT1(0, "Attribute '%s' has been saved", aName);

            /* 'u' - unhide last hidden */
            } else if (*state == 'u') {

                if        (utype_save == 0) {
                    SPRINT0(0, "nothing to unhide");
                } else if (utype_save%10 == 1) {
                    ibody = utype_save / 10;
                    iedge = uindex_save;

                    MODL->body[ibody].edge[iedge].gratt.render = 2 + 64;
                    SPRINT2(0, "Unhiding Edge %d (body %d)", iedge, ibody);

                    new_data = 1;
                } else if (utype_save%10 == 2) {
                    ibody = utype_save / 10;
                    iface = uindex_save;

                    MODL->body[ibody].face[iface].gratt.render = 2 + 4 + 64;
                    SPRINT2(0, "Unhiding Face %d (body %d)", iface, ibody);

                    new_data = 1;
                } else {
                    SPRINT0(0, "nothing to unhide");
                }

                utype_save = 0;
                utype_save = 0;

            /* 'U' - undefined option */
            } else if (*state == 'U') {
                SPRINT0(0, "--> Option 'U' (undefined)");

            /* 'v' - undefined option */
            } else if (*state == 'v') {
                SPRINT0(0, "--> Option 'v' (undefined)");

            /* 'V' - paste Branches */
            } else if (*state == 'V') {
                SPRINT0(0, "--> Option 'V' (paste Branches)");

                if (npaste <= 0) {
                    SPRINT0(0, "Nothing to paste");
                    goto next_option;
                }

                for (ipaste = npaste-1; ipaste >= 0; ipaste--) {
                    status = ocsmNewBrch(MODL, MODL->nbrch,
                                               type_paste[ipaste],
                                               str1_paste[ipaste],
                                               str2_paste[ipaste],
                                               str3_paste[ipaste],
                                               str4_paste[ipaste],
                                               str5_paste[ipaste],
                                               str6_paste[ipaste],
                                               str7_paste[ipaste],
                                               str8_paste[ipaste],
                                               str9_paste[ipaste]);
                    if (status != SUCCESS) {
                        SPRINT3(0, "**> ocsmNewBrch(type=%d) -> status=%d (%s)",
                                type_paste[ipaste], status, ocsmGetText(status));
                        goto next_option;
                    }

                    if (strncmp(name_paste[ipaste], "Brch_", 5) != 0) {
                        status = ocsmSetName(MODL, MODL->nbrch, name_paste[ipaste]);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmSetName(ibrch=%d) -> status=%d (%s)",
                                    MODL->nbrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    }

                    SPRINT1(0, "New Branch (%d) added from paste buffer", type_paste[ipaste]);
                }

                SPRINT0(0, "Use 'B' to rebuild");

            /* 'w' - undefined option */
            } else if (*state == 'w') {
                SPRINT0(0, "--> Option 'w' (undefined)");

            /* 'W' - write .csm file */
            } else if (*state == 'W') {
                SPRINT0(0, "--> Option 'W' chosen (write .csm file)");

                getStr("Enter filename: ", fileName);
                if (strstr(fileName, ".csm") == NULL) {
                    strcat(fileName, ".csm");
                }

                status = ocsmSave(MODL, fileName);
                SPRINT3(0, "--> ocsmSave(%s) -> status=%d (%s)",
                        fileName, status, ocsmGetText(status));

            /* 'x' - look from +X direction */
            } else if (*state == 'x') {
                double size;
                SPRINT0(0, "--> Option 'x' chosen (look from +X direction)");

                size = 0.5 * sqrt(  pow(bigbox[3] - bigbox[0], 2)
                                  + pow(bigbox[4] - bigbox[1], 2)
                                  + pow(bigbox[5] - bigbox[2], 2));

                gv_xform[0][0] =  0;
                gv_xform[1][0] =  0;
                gv_xform[2][0] = -1 / size;
                gv_xform[3][0] = +(bigbox[2] + bigbox[5]) / 2 / size;
                gv_xform[0][1] =  0;
                gv_xform[1][1] = +1 / size;
                gv_xform[2][1] =  0;
                gv_xform[3][1] = -(bigbox[1] + bigbox[4]) / 2 / size;
                gv_xform[0][2] = +1 / size;
                gv_xform[1][2] =  0;
                gv_xform[2][2] =  0;
                gv_xform[3][2] = -(bigbox[0] + bigbox[3]) / 2 / size;
                gv_xform[0][3] =  0;
                gv_xform[1][3] =  0;
                gv_xform[2][3] =  0;
                gv_xform[3][3] =  1;

                numarg   = 0;
                new_data = 1;

            /* 'X' - cut Branches */
            } else if (*state == 'X') {
                SPRINT0(0, "--> Option 'X' (cut Branches)");

                /* remove previous contents from paste buffer */
                npaste = 0;

                if (numarg > 0) {
                    npaste = numarg;
                    numarg = 0;
                } else {
                    npaste = getInt("Enter number of Branches to cut: ");
                }

                if (npaste > MAX_PASTE) {
                    SPRINT2(0, "Illegal npaste=%d (should be between 1 and %d)", npaste, MAX_PASTE);
                    npaste = 0;
                    goto next_option;
                }

                if (npaste < 1 || npaste > MODL->nbrch) {
                    SPRINT2(0, "Illegal npaste=%d (should be between 1 and %d)", npaste, MODL->nbrch);
                    npaste = 0;
                    goto next_option;
                }

                for (ipaste = 0; ipaste < npaste; ipaste++) {
                    ibrch = MODL->nbrch;

                    status = ocsmGetBrch(MODL, ibrch, &type_paste[ipaste], &iclass, &iactv,
                                         &ichld, &ileft, &irite, &narg, &nattr);
                    if (status != SUCCESS) {
                        SPRINT3(0, "**> ocsmGetBrch(ibrch=%d) -> status=%d (%s)",
                                ibrch, status, ocsmGetText(status));
                        goto next_option;
                    }

                    status = ocsmGetName(MODL, ibrch, name_paste[ipaste]);
                    if (status != SUCCESS) {
                        SPRINT3(0, "**> ocsmGetName(ibrch=%d) => status=%d (%s)",
                                ibrch, status, ocsmGetText(status));
                        goto next_option;
                    }

                    if (narg >= 1) {
                        status = ocsmGetArg(modl, ibrch, 1, str1_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=1) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str1_paste[ipaste], "");
                    }
                    if (narg >= 2) {
                        status = ocsmGetArg(modl, ibrch, 2, str2_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=2) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str2_paste[ipaste], "");
                    }
                    if (narg >= 3) {
                        status = ocsmGetArg(modl, ibrch, 3, str3_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=3) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str3_paste[ipaste], "");
                    }
                    if (narg >= 4) {
                        status = ocsmGetArg(modl, ibrch, 4, str4_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=4) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str4_paste[ipaste], "");
                    }
                    if (narg >= 5) {
                        status = ocsmGetArg(modl, ibrch, 5, str5_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=5) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str5_paste[ipaste], "");
                    }
                    if (narg >= 6) {
                        status = ocsmGetArg(modl, ibrch, 6, str6_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=6) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str6_paste[ipaste], "");
                    }
                    if (narg >= 7) {
                        status = ocsmGetArg(modl, ibrch, 7, str7_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=7) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str7_paste[ipaste], "");
                    }
                    if (narg >= 8) {
                        status = ocsmGetArg(modl, ibrch, 8, str8_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=8) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str8_paste[ipaste], "");
                    }
                    if (narg >= 9) {
                        status = ocsmGetArg(modl, ibrch, 9, str9_paste[ipaste], &value);
                        if (status != SUCCESS) {
                            SPRINT3(0, "**> ocsmGetArg(ibrch=%d, iarg=9) -> status=%d (%s)",
                                    ibrch, status, ocsmGetText(status));
                            goto next_option;
                        }
                    } else {
                        strcpy(str9_paste[ipaste], "");
                    }

                    status = ocsmDelBrch(MODL, MODL->nbrch);
                    if (status != SUCCESS) {
                        SPRINT3(0, "**> ocsmDelBrch(ibrch=%d) -> status=%d (%s)",
                                MODL->nbrch, status, ocsmGetText(status));
                        goto next_option;
                    }

                    SPRINT1(0, "Old Branch (%d) deleted", type_paste[ipaste]);
                }

                SPRINT0(0, "Use 'B' to rebuild");

            /* 'y' - look from +Y direction */
            } else if (*state == 'y') {
                double size;
                SPRINT0(0, "--> Option 'y' chosen (look from +Y direction)");

                size = 0.5 * sqrt(  pow(bigbox[3] - bigbox[0], 2)
                                  + pow(bigbox[4] - bigbox[1], 2)
                                  + pow(bigbox[5] - bigbox[2], 2));

                gv_xform[0][0] = +1 / size;
                gv_xform[1][0] =  0;
                gv_xform[2][0] =  0;
                gv_xform[3][0] = -(bigbox[0] + bigbox[3]) / 2 / size;
                gv_xform[0][1] =  0;
                gv_xform[1][1] =  0;
                gv_xform[2][1] = -1 / size;
                gv_xform[3][1] = +(bigbox[2] + bigbox[5]) / 2 / size;
                gv_xform[0][2] =  0;
                gv_xform[1][2] = +1 / size;
                gv_xform[2][2] =  0;
                gv_xform[3][2] = -(bigbox[1] + bigbox[4]) / 2 / size;
                gv_xform[0][3] =  0;
                gv_xform[1][3] =  0;
                gv_xform[2][3] =  0;
                gv_xform[3][3] =  1;

                numarg   = 0;
                new_data = 1;

            /* 'Y' - undefined option */
            } else if (*state == 'Y') {
                SPRINT0(0, "--> Option 'Y' (undefined)");

            /* 'z' - look from +Z direction */
            } else if (*state == 'z') {
                double size;
                SPRINT0(0, "--> Option 'z' chosen (look from +Z direction)");

                size = 0.5 * sqrt(  pow(bigbox[3] - bigbox[0], 2)
                                  + pow(bigbox[4] - bigbox[1], 2)
                                  + pow(bigbox[5] - bigbox[2], 2));

                gv_xform[0][0] = +1 / size;
                gv_xform[1][0] =  0;
                gv_xform[2][0] =  0;
                gv_xform[3][0] = -(bigbox[0] + bigbox[3]) / 2 / size;
                gv_xform[0][1] =  0;
                gv_xform[1][1] = +1 / size;
                gv_xform[2][1] =  0;
                gv_xform[3][1] = -(bigbox[1] + bigbox[4]) / 2 / size;
                gv_xform[0][2] =  0;
                gv_xform[1][2] =  0;
                gv_xform[2][2] = +1 / size;
                gv_xform[3][2] = -(bigbox[2] + bigbox[5]) / 2 / size;
                gv_xform[0][3] =  0;
                gv_xform[1][3] =  0;
                gv_xform[2][3] =  0;
                gv_xform[3][3] =  1;

                numarg   = 0;
                new_data = 1;

            /* 'Z' - undefined option */
            } else if (*state == 'Z') {
                SPRINT0(0, "--> Option 'Z' (undefined)");

            /* '0' - append "0" to numarg */
            } else if (*state == '0') {
                numarg = 0 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '1' - append "1" to numarg */
            } else if (*state == '1') {
                numarg = 1 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '2' - append "2" to numarg */
            } else if (*state == '2') {
                numarg = 2 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '3' - append "3" to numarg */
            } else if (*state == '3') {
                numarg = 3 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '4' - append "4" to numarg */
            } else if (*state == '4') {
                numarg = 4 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '5' - append "5" to numarg */
            } else if (*state == '5') {
                numarg = 5 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '6' - append "6" to numarg */
            } else if (*state == '6') {
                numarg = 6 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '7' - append "7" to numarg */
            } else if (*state == '7') {
                numarg = 7 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '8' - append "8" to numarg */
            } else if (*state == '8') {
                numarg = 8 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '9' - append "9" to numarg */
            } else if (*state == '9') {
                numarg = 9 + numarg * 10;
                SPRINT1(0, "numarg = %d", numarg);

           /* 'bksp' - erase last digit of numarg */
            } else if (*state == 65288) {
                numarg = numarg / 10;
                SPRINT1(0, "numarg = %d", numarg);

            /* '>' - write viewpoint */
            } else if (*state == '>') {
                sprintf(tempName, "ViewMatrix%d.dat", numarg);
                fp = fopen(tempName, "w");
                fprintf(fp, "%f %f %f %f\n", gv_xform[0][0], gv_xform[1][0],
                                             gv_xform[2][0], gv_xform[3][0]);
                fprintf(fp, "%f %f %f %f\n", gv_xform[0][1], gv_xform[1][1],
                                             gv_xform[2][1], gv_xform[3][1]);
                fprintf(fp, "%f %f %f %f\n", gv_xform[0][2], gv_xform[1][2],
                                             gv_xform[2][2], gv_xform[3][2]);
                fprintf(fp, "%f %f %f %f\n", gv_xform[0][3], gv_xform[1][3],
                                             gv_xform[2][3], gv_xform[3][3]);
                fclose(fp);

                SPRINT1(0, "%s has been saved", tempName);

                numarg = 0;

            /* '<' - read viewpoint */
            } else if (*state == '<') {
                sprintf(tempName, "ViewMatrix%d.dat", numarg);
                fp = fopen(tempName, "r");
                if (fp != NULL) {
                    SPRINT1(0, "resetting to %s", tempName);

                    fscanf(fp, "%f%f%f%f", &(gv_xform[0][0]), &(gv_xform[1][0]),
                                           &(gv_xform[2][0]), &(gv_xform[3][0]));
                    fscanf(fp, "%f%f%f%f", &(gv_xform[0][1]), &(gv_xform[1][1]),
                                           &(gv_xform[2][1]), &(gv_xform[3][1]));
                    fscanf(fp, "%f%f%f%f", &(gv_xform[0][2]), &(gv_xform[1][2]),
                                           &(gv_xform[2][2]), &(gv_xform[3][2]));
                    fscanf(fp, "%f%f%f%f", &(gv_xform[0][3]), &(gv_xform[1][3]),
                                           &(gv_xform[2][3]), &(gv_xform[3][3]));
                    fclose(fp);
                } else {
                    SPRINT1(0, "%s does not exist", tempName);
                }

                numarg   = 0;
                new_data = 1;

            /* '$' - read journal file */
            } else if (*state == '$') {
                SPRINT0(0, "--> Option $ chosen (read journal file)");

                if (script == NULL) {
                    SPRINT0(0, "Enter journal filename: ");
                    scanf("%s", jnlName);

                    SPRINT1x(0, "Opening journal file \"%s\" ...", jnlName);

                    script = fopen(jnlName, "r");
                    if (script != NULL) {
                        SPRINT0(0, "okay");
                    } else {
                        SPRINT0(0, "ERROR detected");
                    }
                } else {
                    fclose(script);
                    SPRINT0(0, "Closing journal file");

                    script = NULL;
                    *win   =    0;
                }

            /* '?' - help */
            } else if (*state == '?') {
                SPRINT0(0, "===========================   ===========================   ===========================");
                SPRINT0(0, "                              3D Window - special options                              ");
                SPRINT0(0, "===========================   ===========================   ===========================");
                SPRINT0(0, "L list     Branches           l list Parameters           0-9 build numeric arg (#)    ");
                SPRINT0(0, "E edit     Branch (#)         e edit Parameter           BKSP edit  numeric arg (#)    ");
                SPRINT0(0, "A add      Branch             a add  Parameter                                         ");
                SPRINT0(0, "N name     Branch                                           x view from +x direction   ");
                SPRINT0(0, "T attrib.  Branch             h hide Edge/Face  at cursor   y view from +y direction   ");
                SPRINT0(0, "S suppress Branch (#)         u unhide last hidden          z view from +z direction   ");
                SPRINT0(0, "R resume   Branch (#)         q query Edge/Face at cursor   > write viewpoint (#)      ");
                SPRINT0(0, "D delete   Branch                                           < read  viewpoint (#)      ");
                SPRINT0(0, "X cut      Branches (#)       B build to Branch (#)         $ read journal file        ");
                SPRINT0(0, "V paste    Branches           W write .csm file             ? help                     ");
                SPRINT0(0, "                                                          ESC exit                     ");

            /* 'ESC' - exit program */
            } else if (*state ==65307) {
                SPRINT0(1, "--> Exiting buildCSM");
            }

        next_option:
            continue;
        }

        /* repeat as long as we are in a script */
    } while (script != NULL);

//cleanup:
    return;
}


/***********************************************************************/
/*                                                                     */
/*   transform - perform graphic transformation                        */
/*                                                                     */
/***********************************************************************/

void
transform(double    xform[3][4],        /* (in)  transformation matrix */
          double    *point,             /* (in)  input Point */
          float     *out)               /* (out) output Point */
{
    out[0] = xform[0][0]*point[0] + xform[0][1]*point[1]
           + xform[0][2]*point[2] + xform[0][3];
    out[1] = xform[1][0]*point[0] + xform[1][1]*point[1]
           + xform[1][2]*point[2] + xform[1][3];
    out[2] = xform[2][0]*point[0] + xform[2][1]*point[1]
           + xform[2][2]*point[2] + xform[2][3];
}


/***********************************************************************/
/*                                                                     */
/*  pickObject - return the objected pointed to by user                */
/*                                                                     */
/***********************************************************************/

int
pickObject(int       *utype)            /* (out) type of object picked */
{
    int       uindex;                   /* index of object picked */

    int       xpix, ypix, saved_pickmask;
    float     xc, yc;

    (void) GraphicCurrentPointer(&xpix, &ypix);

    xc   =  (2.0 * xpix) / (gv_w3d.xsize - 1.0) - 1.0;
    yc   =  (2.0 * ypix) / (gv_w3d.ysize - 1.0) - 1.0;

    saved_pickmask = gv_pickmask;
    gv_pickmask = -1;
    PickGraphic(xc, -yc, 0);
    gv_pickmask = saved_pickmask;
    if (gv_picked == NULL) {
        *utype = 0;
        uindex = 0;
    } else {
        *utype = gv_picked->utype;
        uindex = gv_picked->uindex;
    }

    return uindex;
}


/***********************************************************************/
/*                                                                     */
/*   getInt - get an int from the user or from a script                */
/*                                                                     */
/***********************************************************************/

static int
getInt(char      *prompt)               /* (in)  prompt string */
{
    int       answer;                   /* integer to return */

    /* integer from screen */
    if (script == NULL) {
        SPRINT1x(0, "%s", prompt); fflush(0);
        scanf("%d", &answer);

    /* integer from script (journal file) */
    } else {
        fscanf(script, "%d", &answer);
        SPRINT2(0, "%s %d", prompt, answer);
    }

    return answer;
}


/***********************************************************************/
/*                                                                     */
/*   getDbl - get a double from the user or from a script              */
/*                                                                     */
/***********************************************************************/

static double
getDbl(char      *prompt)               /* (in)  prompt string */
{
    double    answer;                   /* double to return */

    /* double from screen */
    if (script == NULL) {
        SPRINT1x(0, "%s", prompt); fflush(0);
        scanf("%lf", &answer);

    /* double from script (journal file) */
    } else {
        fscanf(script, "%lf", &answer);
        SPRINT2(0, "%s %f", prompt, answer);
    }

    return answer;
}


/***********************************************************************/
/*                                                                     */
/*   getStr - get a string from the user or from a script              */
/*                                                                     */
/***********************************************************************/

static void
getStr(char      *prompt,               /* (in)  prompt string */
       char      *string)               /* (out) string */
{

    /* double from screen */
    if (script == NULL) {
        SPRINT1x(0, "%s", prompt); fflush(0);
        scanf("%254s", string);

    /* double from script (journal file) */
    } else {
        fscanf(script, "%s", string);
        SPRINT2(0, "%s %s", prompt, string);
    }
}


/***********************************************************************/
/*                                                                     */
/* plotGrid - GRAFIC level 3 plotting routine                          */
/*                                                                     */
/***********************************************************************/

#ifdef GRAFIC
static void
plotGrid(int   *ifunct,                 /* (in)  GRAFIC function indicator */
         void  *igridP,                 /* (in)  Grid number */
         void  *nlistP,                 /* (in)  number of Faces */
         void  *ilistP,                 /* (in)  list   of Faces */
         void  *a3,                     /* (in)  dummy GRAFIC argument */
         void  *a4,                     /* (in)  dummy GRAFIC argument */
         void  *a5,                     /* (in)  dummy GRAFIC argument */
         void  *a6,                     /* (in)  dummy GRAFIC argument */
         void  *a7,                     /* (in)  dummy GRAFIC argument */
         void  *a8,                     /* (in)  dummy GRAFIC argument */
         void  *a9,                     /* (in)  dummy GRAFIC argument */
         float *scale,                  /* (out) array of scales */
         char  *text,                   /* (out) help text */
         int   textlen)                 /* (in)  length of text */
{
    int       *igrid = (int    *) igridP;
    int       *nlist = (int    *) nlistP;
    int       *ilist = (int    *) ilistP;

    int       iface, nloop, *loops, *edges;
    int       itri, ip0, ip1, ip2;
    int       i, j, ijk, imax, jmax, kmax;
    int       ntri, *tris, *tric, npnt, *ptype, *pindx;
    float     u[4], v[4];
    double    uvrange[4], *xyz, *uv;

    /* link to grafic.h */
    int       iblack  = GR_BLACK;
    int       ired    = GR_RED;
    int       igreen  = GR_GREEN;
    int       iblue   = GR_BLUE;
    int       icircle = GR_CIRCLE;

    /* --------------------------------------------------------------- */

    /* ---------- return scales ----------*/
    if (*ifunct == 0) {
        scale[0] = +HUGEQ;
        scale[1] = -HUGEQ;
        scale[2] = +HUGEQ;
        scale[3] = -HUGEQ;

        for (j = 0; j < *nlist; j++) {
            iface = ilist[j];

            #if   defined(GEOM_CAPRI)
                gi_dGetFace(tree.ivol, iface, uvrange, &nloop, &loops, &edges);
            #elif defined(GEOM_EGADS)
            #endif

            if (uvrange[0] < scale[0]) scale[0] = uvrange[0];
            if (uvrange[2] > scale[1]) scale[1] = uvrange[2];
            if (uvrange[1] < scale[2]) scale[2] = uvrange[1];
            if (uvrange[3] > scale[3]) scale[3] = uvrange[3];
        }

        strncpy(text, " ", textlen-1);

    /* ---------- plot image ---------- */
    } else if (*ifunct == 1) {

        /* Grid  */
        grcolr_(&igreen);

        imax = grid[*igrid].imax;
        jmax = grid[*igrid].jmax;
        kmax = grid[*igrid].kmax;

        for (j = 1; j <= jmax; j++) {
            for (i = 2; i <= imax; i++) {
                ijk = (i-1) + (j-1) * imax;
                if (grid[*igrid].L[ijk-1] != HOLE &&
                    grid[*igrid].L[ijk  ] != HOLE   ) {
                    u[0] = grid[*igrid].u[ijk-1];
                    v[0] = grid[*igrid].v[ijk-1];
                    grmov2_(u, v);
                    u[0] = grid[*igrid].u[ijk  ];
                    v[0] = grid[*igrid].v[ijk  ];
                    grdrw2_(u, v);
                }
            }
        }

        for (i = 1; i <= imax; i++) {
            for (j = 2; j <= jmax; j++) {
                ijk = (i-1) + (j-1) * imax;
                if (grid[*igrid].L[ijk-imax] != HOLE &&
                    grid[*igrid].L[ijk     ] != HOLE   ) {
                    u[0] = grid[*igrid].u[ijk-imax];
                    v[0] = grid[*igrid].v[ijk-imax];
                    grmov2_(u, v);
                    u[0] = grid[*igrid].u[ijk     ];
                    v[0] = grid[*igrid].v[ijk     ];
                    grdrw2_(u, v);
                }
            }
        }

        /* FRINGE Points */
        grcolr_(&ired);

        for (ijk = 0; ijk < imax*jmax; ijk++) {
            if (grid[*igrid].L[ijk] == FRINGE) {
                u[0] = grid[*igrid].u[ijk];
                v[0] = grid[*igrid].v[ijk];
                grmov2_(u, v);
                grsymb_(&icircle);
            }
        }

        /* boundaries of Faces in ilist */
        grcolr_(&iblue);

        for (j = 0; j < *nlist; j++) {
            iface = ilist[j];
            SPRINT1(0, "iface %5d", iface);

            #if   defined(GEOM_CAPRI)
                gi_dTesselFace(tree.ivol, iface, &ntri, &tris, &tric,
                               &npnt, &xyz, &ptype, &pindx, &uv);
            #elif defined(GEOM_EGADS)
            #endif

            for (itri = 0; itri < ntri; itri++) {
                ip0 = tris[3*itri  ] - 1;
                ip1 = tris[3*itri+1] - 1;
                ip2 = tris[3*itri+2] - 1;

                u[0] = (float)uv[2*ip0  ];
                u[1] = (float)uv[2*ip1  ];
                u[2] = (float)uv[2*ip2  ];

                v[0] = (float)uv[2*ip0+1];
                v[1] = (float)uv[2*ip1+1];
                v[2] = (float)uv[2*ip2+1];

                if (tric[3*itri  ] < 0) {
                    grmov2_(&u[1], &v[1]);
                    grdrw2_(&u[2], &v[2]);
                }

                if (tric[3*itri+1] < 0) {
                    grmov2_(&u[2], &v[2]);
                    grdrw2_(&u[0], &v[0]);
                }

                if (tric[3*itri+2] < 0) {
                    grmov2_(&u[0], &v[0]);
                    grdrw2_(&u[1], &v[1]);
                }
            }
        }

        grcolr_(&iblack);

    /* ---------- "C" option ---------- */
    } else if (*ifunct == -3) {
        SPRINT0(0, "   'C' option in plotGrid");

    /* ---------- "E" option ---------- */
    } else if (*ifunct == -5) {
        SPRINT0(0, "   'E' option in plotGrid");

    /* ---------- "G" option ---------- */
    } else if (*ifunct == -7) {
        SPRINT0(0, "   'G' option in plotGrid");

    /* ---------- "L" option ---------- */
    } else if (*ifunct == -12) {
        SPRINT0(0, "   'L' option in plotGrid");

    /* ---------- "N" option ---------- */
    } else if (*ifunct == -14) {
        SPRINT0(0, "   'N' option in plotGrid");

    /* ---------- "S" option ---------- */
    } else if (*ifunct == -19) {
        SPRINT0(0, "   'S' option in plotGrid");

    }

    return;
}
#endif
