/*
 ************************************************************************
 *                                                                      *
 * OpenCSM -- an open-source constructive solid modeler                 *
 *                                                                      *
 *            Written by John Dannenhoffer @ Syracuse University        *
 *                                                                      *
 ************************************************************************
 */

/*
 * Copyright (C) 2010/2012  John F. Dannenhoffer, III (Syracuse University)
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
#include <ctype.h>
#include <math.h>
#include <time.h>

//$$$#define SHOW_SPLINES

#if   defined(GEOM_CAPRI)
    #include "capri.h"
    #include "capri_rev3.h"

    #define CINT    int
    #define CDOUBLE double
    #define CCHAR   char

    #define BOX         1      /* used in gi_gCreateVolume */
    #define SPHERE      2
    #define CONE        3
    #define CYLINDER    4
    #define TORUS       5
#elif defined(GEOM_EGADS)
    #include "egads.h"
    #include "udp.h"

    #define CINT    const int
    #define CDOUBLE const double
    #define CCHAR   const char
#else
    #error no_geometry_system_specified
#endif

#include "common.h"
#include "OpenCSM.h"

#if   defined(SHOW_SPLINES)
    #include "grafic.h"
#elif defined(PLOT_FACES)
    #include "grafic.h"
#endif

#define MAX_NAME_LEN       32           /* maximum chars in name */
#define MAX_EXPR_LEN      128           /* maximum chars in expression (change %127s also)*/
#define MAX_LINE_LEN     2048           /* maximum chars in line in input file */
#define MAX_STR_LEN      4096           /* maximum chars in any string */
#define MAX_STACK_SIZE    128           /* maximum size of stack */
#define MAX_SKETCH_SIZE  1024           /* maximum points in sketch */
#define MAX_SOLVER_SIZE   256           /* maximum variable in solver */
#define MAX_NUM_SKETCHES  100           /* maximum number of sketches in loft */
#define MAX_NUM_PATTERNS   10           /* maximum number of nested patterns */
#define MAX_NUM_MACROS    100           /* maximum number of storage locations */

#define OCSM_MAGIC        4433340       /* magic number */

/*
 ************************************************************************
 *                                                                      *
 * Structures                                                           *
 *                                                                      *
 ************************************************************************
 */

/* "Rpn" contains information associated with Rpn (pseudo-code) */
typedef struct {
    int           type;                 /* type (see below) */
    char          text[MAX_NAME_LEN];   /* associated text */
    struct rpn_T  *next;                /* next Rpn token */
} rpn_T;

/* "Patn" contains information associated with a patbeg/patend or
          a macbeg/macend pair */
typedef struct {
    int    ipatbeg;                    /* Branch number of patbeg or recall */
    int    ipatend;                    /* Branch number of patend or recall */
    int    ncopy;                      /* total number of copies */
    int    icopy;                      /* current instance number (1->ncopy) */
    int    ipmtr;                      /* Parameter index of iterator */
} patn_T;

/* "Skpt" is a sketch point */
typedef struct {
    int    itype;                      /* point type */
    int    ibrch;                      /* Branch index (1-nbrch) */
    double x;                          /* X-coordinate */
    double y;                          /* Y-coordinate */
    double z;                          /* Z-coordinate */
} skpt_T;

/*
 ************************************************************************
 *                                                                      *
 * Definitions (for structures above)                                   *
 *                                                                      *
 ************************************************************************
 */

#define           PARSE_NOP         0   /* no operation */
#define           PARSE_OP1         1   /* either add "+" or subtract "-" */
#define           PARSE_OP2         2   /* either multiply "*" or divide "/" */
#define           PARSE_OP3         3   /* exponentiation "^" */
#define           PARSE_OPENP       4   /* open  parenthesis "(" */
#define           PARSE_CLOSEP      5   /* close parenthesis ")" */
#define           PARSE_OPENB       6   /* open  bracket "[" */
#define           PARSE_CLOSEB      7   /* close bracket "]" */
#define           PARSE_COMMA       8   /* comma "," */
#define           PARSE_NAME        9   /* variable name */
#define           PARSE_ARRAY      10   /* array name */
#define           PARSE_FUNC       11   /* function name */
#define           PARSE_NUMBER     12   /* number */
#define           PARSE_STRING     13   /* string */
#define           PARSE_END        14   /* end of Rpn-code */

/*
 ************************************************************************
 *                                                                      *
 * Macros (including those that go along with common.h)                 *
 *                                                                      *
 ************************************************************************
 */

#if   defined DEBUG
    #define DOPEN {if (dbg_fp == NULL) dbg_fp = fopen("OpenCSM.dbg", "w");}
    static  FILE *dbg_fp = NULL;
#endif

static void *realloc_temp = NULL;            /* used by RALLOC macro */

/*
 ************************************************************************
 *                                                                      *
 * Declarations for support routines defined below                      *
 *                                                                      *
 ************************************************************************
 */

static int buildApplied(  modl_T *modl, int ibrch, int *nstack, int stack[], int npatn, patn_T patn[]);
static int buildBoolean(  modl_T *modl, int ibrch, int *nstack, int stack[]);
static int buildGrown(    modl_T *modl, int ibrch, int *nstack, int stack[], int npatn, patn_T patn[]);
static int buildPrimitive(modl_T *modl, int ibrch, int *nstack, int stack[], int npatn, patn_T patn[]);
static int buildSketch(   modl_T *modl, int ibrch, int *nstack, int stack[], int npatn, patn_T patn[],
                          int *nskpt, skpt_T skpt[]);
static int buildSolver(   modl_T *modl, int ibrch, int *ncon, int solcons[]);
static int buildTransform(modl_T *modl, int ibrch, int *nstack, int stack[]);
static int newBody(modl_T *modl, int ibrch, int brtype, int ileft, int irite,
                   double args[], int botype, int *ibody);
static int setFaceAttribute(modl_T *modl, int ibody, int iface, int iford, int npatn, patn_T *patn);
static int finishBody(modl_T *modl, int ibody);
static int printBodyAttributes(modl_T *modl, int ibody);
static int setupAtPmtrs(modl_T *modl);
#if   defined(GEOM_CAPRI)
#elif defined(GEOM_EGADS)
    static int faceContains(ego eface, double xx, double yy, double zz);
    static int selectBody(ego emodel, char *order, int index);
    static int getBodyTolerance(ego ebody, double *toler);
#endif
static int matsol(double A[], double b[], int n, double x[]);
static int str2rpn(char str[], rpn_T *rpn);
static int evalRpn(rpn_T *rpn, modl_T *modl, double *val);
static int str2val(char str[], modl_T *modl, double *val);

/*
 ************************************************************************
 *                                                                      *
 * Global variables                                                     *
 *                                                                      *
 ************************************************************************
 */

static int outLevel = 1;


/*
 ************************************************************************
 *                                                                      *
 *   ocsmVersion - return current version                               *
 *                                                                      *
 ************************************************************************
 */

int
ocsmVersion(int    *imajor,             /* (out) major version number */
            int    *iminor)             /* (out) minor version number */
{
    int       status = SUCCESS;         /* (out) return status */

    ROUTINE(ocsmVersion);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    *imajor = OCSM_MAJOR_VERSION;
    *iminor = OCSM_MINOR_VERSION;

//cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetOutLevel - set output level                                 *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSetOutLevel(int    ilevel)      /* (in)  output level: */
                                    /*       =0 warnings and errors only */
                                    /*       =1 nominal (default) */
                                    /*       =2 debug */
{
    int       status = SUCCESS;         /* (out) return status */

    ROUTINE(ocsmSetOutLevel);
    DPRINT2("%s(ilevel=%d) {",
            routine, ilevel);

    /* --------------------------------------------------------------- */

    outLevel = ilevel;

//cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmLoad - create a MODL by reading a .csm file                    *
 *                                                                      *
 ************************************************************************
 */

int
ocsmLoad(char   filename[],             /* (in)  file to be read (with .csm) */
         void   **modl)                 /* (out) pointer to MODL */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL;

    /* sketch contains linsegs, cirarcs, and splines */
    int       nskpt;

    /* number of open patterns */
    int       npatn = 0;

    int       i, j, inquote, narg, nrow, ncol, irow, icol;
    int       ibrch, ipmtr, jpmtr, icount, jcount, insolver;
    double    rows, cols, despmtr;
    char      templine[MAX_LINE_LEN], nextline[MAX_LINE_LEN];
    char      command[MAX_EXPR_LEN], bigstr[MAX_STR_LEN];
    char      str1[MAX_EXPR_LEN], str2[MAX_EXPR_LEN], str3[MAX_EXPR_LEN];
    char      str4[MAX_EXPR_LEN], str5[MAX_EXPR_LEN], str6[MAX_EXPR_LEN];
    char      str7[MAX_EXPR_LEN], str8[MAX_EXPR_LEN], str9[MAX_EXPR_LEN];
    char      pmtrName[MAX_EXPR_LEN], row[MAX_EXPR_LEN], col[MAX_EXPR_LEN];
    char      defn[MAX_EXPR_LEN];

    FILE      *csm_file = NULL;

    ROUTINE(ocsmLoad);
    DPRINT2("%s(filename=%s) {",
            routine, filename);

    /* --------------------------------------------------------------- */

    SPRINT1(1, "--> enter ocsmLoad(filename=%s)", filename);

    /* default return value */
    *modl = NULL;

    /* open the .csm file */
    if (strlen(filename) > 0) {
        csm_file = fopen(filename, "r");
        if (csm_file == NULL) {
            DPRINT0("csm_file = NULL");
        status = OCSM_FILE_NOT_FOUND;
        goto cleanup;
        }
    }

    /* make a new MODL and initialize it */
    MALLOC(MODL, modl_T, 1);

    MODL->magic   = OCSM_MAGIC;
    MODL->checked = 0;
    MODL->nextseq = 1;

    for (i = 0; i < 24; i++) {
        MODL->atPmtrs[i] = 0;
    }

    MODL->nbrch = 0;
    MODL->mbrch = 0;
    MODL->brch  = NULL;

    MODL->npmtr = 0;
    MODL->mpmtr = 0;
    MODL->pmtr  = NULL;

    MODL->nbody = 0;
    MODL->mbody = 0;
    MODL->body  = NULL;

    MODL->context = NULL;

    /* return value */
    *modl = MODL;

    if (strlen(filename) == 0) {
        goto cleanup;
    }

    /* initialize the number of active sketch points and patterns */
    nskpt = 0;
    npatn = 0;

    insolver = 0;

    /* read commands from .csm file until the end of file */
    while (!feof(csm_file)) {

        /* read the next line and:
           - ignore anything after a #
           - ignore anything after a \ and concatenate next line
           - remove any spaces between double quotes (") so that a user
             could include spaces within an expression as long as
             it is surrounded by double quotes */
         (void)fgets(templine, MAX_LINE_LEN, csm_file);

         if (templine[0] != '#') {
             inquote = 0;
             j       = 0;
             for (i = 0; i < strlen(templine); i++) {
                 if (templine[i] == '#') {
                     nextline[j++] = '\0';
                     break;
                 } else if (templine[i] == '\\') {
                     (void)fgets(templine, MAX_LINE_LEN, csm_file);
                     i = -1;
                 } else if (templine[i] == '"') {
                     inquote = 1 - inquote;
                 } else if (templine[i] == ' ' || templine[i] == '\t' || templine[i] == '\n') {
                     if (inquote == 0 && j > 0) {
                         nextline[j++] = templine[i];
                     }
                 } else {
                     nextline[j++] = templine[i];
                 }

                 if (j >= MAX_LINE_LEN-2) {
                     status = OCSM_ILLEGAL_STATEMENT;
                     CHECK_STATUS(line_too_long);
                 }
             }
         } else {
             strcpy(nextline, templine);
             j = strlen(nextline);
         }
         nextline[j++] = '\0';

         /* strip white spaces from end of nextline */
         for (i = j-2; i >= 0; i--) {
             if (nextline[i] == ' ' || nextline[i] == '\t' || nextline[i] == '\n'   ) {
                 nextline[i] = '\0';
             } else {
                 break;
             }
         }
         SPRINT2(1, "    nextline [%4d]: %s", MODL->nbrch+1, nextline);
         if (strlen(nextline) <= 1) continue;

        /* get the command from the next input line */
        sscanf(nextline, "%127s", command);

        /* input is: "# comment" */
        if (strncmp(command, "#", 1) == 0) {
            /* nothing to do */

        /* input is: "dimension pmtrName nrow ncol despmtr" */
        } else if (strcmp(command, "dimension") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(dimension);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(dimension);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4);
            if (narg < 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(dimension);
            }

            /* do not allow pmtrName to start with '@' */
            if (str1[0] == '@') {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(dimension);
            }

            status = str2val(str2, MODL, &rows);
            CHECK_STATUS(str2val:rows);

            status = str2val(str3, MODL, &cols);
            CHECK_STATUS(str2val:cols);

            if (narg == 4) {
                status = str2val(str4, MODL, &despmtr);
                CHECK_STATUS(str2val:despmtr);
            } else {
                despmtr = 0;
            }

            if (NINT(despmtr) == 0) {
                status = ocsmNewPmtr(MODL, str1, OCSM_INTERNAL, NINT(rows), NINT(cols));
                CHECK_STATUS(ocsmNewPmtr);
            } else {
                status = ocsmNewPmtr(MODL, str1, OCSM_EXTERNAL, NINT(rows), NINT(cols));
                CHECK_STATUS(ocsmNewPmtr);
            }

        /* input is: "despmtr pmtrName expression" */
        } else if (strcmp(command, "despmtr") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(despmtr);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(despmtr);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          str1, str2);
            if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(despmtr);
            }

            /* do not allow pmtrName to start with '@' */
            if (str1[0] == '@') {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(dimension);
            }

            /* determine if a single- or multi-set (dependent on whether str2
               contains a semi-colon) */
            if (strstr(str2, ";") == NULL) {

                /* break str1 into name[irow,icol] */
                pmtrName[0] = '\0';
                row[0]      = '\0';
                col[0]      = '\0';
                icount      = 0;
                jcount      = 0;

                for (i = 0; i < strlen(str1); i++) {
                    if (icount == 0) {
                        if (str1[i] != '[') {
                            pmtrName[jcount  ] = str1[i];
                            pmtrName[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            jcount = 0;
                        }
                    } else if (icount == 1) {
                        if (str1[i] != ',') {
                            row[jcount  ] = str1[i];
                            row[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            jcount = 0;
                        }
                    } else {
                        if (str1[i] != ']') {
                            col[jcount  ] = str1[i];
                            col[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            break;
                        }
                    }
                }

                if (icount != 0 && icount != 3) {
                    status = OCSM_ILLEGAL_PMTR_NAME;
                    CHECK_STATUS(despmtr);
                }

                if (strlen(row) == 0) sprintf(row, "1");
                if (strlen(col) == 0) sprintf(col, "1");

                /* look for current Parameter */
                jpmtr = 0;
                for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                    if (strcmp(MODL->pmtr[ipmtr].name, pmtrName) == 0) {
                        if (MODL->pmtr[ipmtr].type != OCSM_INTERNAL) {
                            MODL->pmtr[ipmtr].type =  OCSM_EXTERNAL;
                        } else {
                            status = OCSM_PMTR_IS_INTERNAL;
                            CHECK_STATUS(despmtr);
                        }

                        jpmtr = ipmtr;
                        break;
                    }
                }

                /* create new Parameter (if needed) */
                if (jpmtr == 0) {
                    nrow = 1;
                    ncol = 1;

                    status = ocsmNewPmtr(MODL, pmtrName, OCSM_EXTERNAL, nrow, ncol);
                    CHECK_STATUS(ocsmNewPmtr);

                    jpmtr = MODL->npmtr;
                }

                /* store the value */
                status = str2val(row, MODL, &rows);
                CHECK_STATUS(str2val:row);

                status = str2val(col, MODL, &cols);
                CHECK_STATUS(str2val:col);

                status = ocsmSetValu(MODL, jpmtr, NINT(rows), NINT(cols), str2);
                CHECK_STATUS(ocsmSetValu);

            /* multi-set mode */
            } else {
                narg = sscanf(nextline, "%*s %127s %4095s\n",
                          str1, bigstr);

                /* look for current Parameter */
                jpmtr = 0;
                for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                    if (strcmp(MODL->pmtr[ipmtr].name, str1) == 0) {
                        jpmtr = ipmtr;
                        break;
                    }
                }

                /* Parameter must have been defined (in a dimension statement) */
                if (jpmtr <= 0) {
                    status = OCSM_NAME_NOT_FOUND;
                    CHECK_STATUS(despmtr);
                } else if (MODL->pmtr[ipmtr].type != OCSM_EXTERNAL) {
                    status = OCSM_PMTR_IS_INTERNAL;
                    CHECK_STATUS(despmtr);
                }

                /* store the values */
                icount = 0;
                for (irow = 1; irow <= MODL->pmtr[jpmtr].nrow; irow++) {
                    for (icol = 1; icol <= MODL->pmtr[jpmtr].ncol; icol++) {
                        jcount = 0;

                        while (icount < strlen(bigstr)) {
                            if (bigstr[icount] == ';') {
                                icount++;
                                break;
                            } else {
                                defn[jcount  ] = bigstr[icount];
                                defn[jcount+1] = '\0';
                                icount++;
                                jcount++;
                            }
                        }

                        if (strcmp(defn, "") != 0) {
                            status = ocsmSetValu(MODL, jpmtr, irow, icol, defn);
                            CHECK_STATUS(ocsmSetValu);
                        }
                    }
                }
            }

        /* input is: "box xbase ybase zbase dx dy dz" */
        } else if (strcmp(command, "box") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(box);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(box);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6);
            if (narg != 6) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(box);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_BOX,
                                 str1, str2, str3, str4, str5, str6, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "sphere xcent ycent zcent radius" */
        } else if (strcmp(command, "sphere") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(sphere);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(sphere);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4);
            if (narg != 4) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(sphere);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SPHERE,
                                 str1, str2, str3, str4, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "cone xvrtx yvrtx zvrtx xbase ybase zbase radius" */
        } else if (strcmp(command, "cone") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(cone);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(cone);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6, str7);
            if (narg != 7) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(cone);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_CONE,
                                 str1, str2, str3, str4, str5, str6, str7, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "cylinder xbeg ybeg zbeg xend yend zend radius" */
        } else if (strcmp(command, "cylinder") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(cylinder);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(cylinder);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6, str7);
            if (narg != 7) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(cylinder);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_CYLINDER,
                                 str1, str2, str3, str4, str5, str6, str7, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "torus xcent ycent zcent dxaxis dyaxis dzaxis majorRad minorRad" */
        } else if (strcmp(command, "torus") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(torus);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(torus);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6, str7, str8);
            if (narg != 8) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(torus);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_TORUS,
                                 str1, str2, str3, str4, str5, str6, str7, str8, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "import filename" */
        } else if (strcmp(command, "import") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(import);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(import);
            }

            /* extract argument */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s\n",
                          &str1[1]);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(import);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_IMPORT,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "udprim primtype argName1 argValue1 argName2 argValue2 argName3 argValue3 argName4 argValue4" */
        } else if (strcmp(command, "udprim") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(udprim);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(udprim);
            }

            /* extract arguments */
            str1[0] = '$';  str2[0] = '$';  str3[0] = '$';  str4[0] = '$';  str5[0] = '$';
            str6[0] = '$',  str7[0] = '$';  str8[0] = '$';  str9[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s %127s %127s\n",
                          &str1[1], &str2[1], &str3[1], &str4[1], &str5[1],
                          &str6[1], &str7[1], &str8[1], &str9[1]);
            if (narg < 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(udprim);
            } else if (narg%2 != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(udprim);
            }

            /* create the new Branch */
            if        (narg < 3) {
                status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UDPRIM,
                                     str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            } else if (narg < 5) {
                status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UDPRIM,
                                     str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            } else if (narg < 7) {
                status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UDPRIM,
                                     str1, str2, str3, str4, str5, NULL, NULL, NULL, NULL);
            } else if (narg < 9) {
                status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UDPRIM,
                                     str1, str2, str3, str4, str5, str6, str7, NULL, NULL);
            } else {
                status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UDPRIM,
                                     str1, str2, str3, str4, str5, str6, str7, str8, str9);
            }
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "extrude dx dy dz" */
        } else if (strcmp(command, "extrude") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(extrude);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(extrude);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(extrude);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_EXTRUDE,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "loft smooth" */
        } else if (strcmp(command, "loft") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(loft);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(loft);
            }

            /* extract argument */
            narg = sscanf(nextline, "%*s %127s\n",
                          str1);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(loft);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_LOFT,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "revolve xorig yorig zorig dxaxis dyaxis dzaxis angDeg" */
        } else if (strcmp(command, "revolve") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(revolve);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(revolve);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6, str7);
            if (narg != 7) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(revolve);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_REVOLVE,
                                 str1, str2, str3, str4, str5, str6, str7, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "fillet radius edgeList=0" */
        } else if (strcmp(command, "fillet") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(fillet);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(fillet);
            }

            /* extract arguments */
            str2[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          str1, &str2[1]);
            if (narg == 1) {
                sprintf(str2, "$0");
            } else if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(fillet);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_FILLET,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "chamfer radius edgeList=0" */
        } else if (strcmp(command, "chamfer") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(chamfer);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(chamfer);
            }

            /* extract arguments */
            str2[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          str1, &str2[1]);
            if (narg == 1) {
                sprintf(str2, "$0");
            } else if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(chamfer);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_CHAMFER,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "hollow thick iface1=0 iface2=0 iface3=0 iface4=0 iface5=0 iface6=0 */
        } else if (strcmp(command, "hollow") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(hollow);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(hollow);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6, str7);
            if        (narg <= 0) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(hollow);
            } else if (narg == 1) {
                strcpy(str2, "0");
                strcpy(str3, "0");
                strcpy(str4, "0");
                strcpy(str5, "0");
                strcpy(str6, "0");
                strcpy(str7, "0");
            } else if (narg == 2) {
                strcpy(str3, "0");
                strcpy(str4, "0");
                strcpy(str5, "0");
                strcpy(str6, "0");
                strcpy(str7, "0");
            } else if (narg == 3) {
                strcpy(str4, "0");
                strcpy(str5, "0");
                strcpy(str6, "0");
                strcpy(str7, "0");
            } else if (narg == 4) {
                strcpy(str5, "0");
                strcpy(str6, "0");
                strcpy(str7, "0");
            } else if (narg == 5) {
                strcpy(str6, "0");
                strcpy(str7, "0");
            } else if (narg == 6) {
                strcpy(str7, "0");
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_HOLLOW,
                                 str1, str2, str3, str4, str5, str6, str7, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "intersect order=none index=1" */
        } else if (strcmp(command, "intersect") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(intersect);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(intersect);
            }

            /* extract arguments */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          &str1[1], str2);
            if        (narg <= 0) {
                strcpy(str1, "$none");
                strcpy(str2, "1");
            } else if (narg == 1) {
                strcpy(str2, "1");
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_INTERSECT,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "subtract order=none index=1" */
        } else if (strcmp(command, "subtract") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(subtract);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(subtract);
            }

            /* extract arguments */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          &str1[1], str2);
            if        (narg <= 0) {
                strcpy(str1, "$none");
                strcpy(str2, "1");
            } else if (narg == 1) {
                strcpy(str2, "1");
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SUBTRACT,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "union" */
        } else if (strcmp(command, "union") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(union);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(union);
            }

            /* no arguments */

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_UNION,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "translate dx dy dz" */
        } else if (strcmp(command, "translate") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(translate);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(translate);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(translate);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_TRANSLATE,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "rotatex angDeg yaxis zaxis" */
        } else if (strcmp(command, "rotatex") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(rotatex);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(rotatex);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(rotatex);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_ROTATEX,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "rotatey angDeg zaxis xaxis" */
        } else if (strcmp(command, "rotatey") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(rotatey);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(rotatey);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(rotatey);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_ROTATEY,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "rotatez angDeg xaxis yaxis" */
        } else if (strcmp(command, "rotatez") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(rotatez);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(rotatez);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(rotatez);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_ROTATEZ,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "scale fact" */
        } else if (strcmp(command, "scale") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(scale);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(scale);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s\n",
                          str1);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(scale);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SCALE,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "skbeg x y z" */
        } else if (strcmp(command, "skbeg") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(skbeg);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(skbeg);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(skbeg);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SKBEG,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            /* increment the number of sketch points */
            nskpt++;

        /* input is: "linseg x y z" */
        } else if (strcmp(command, "linseg") == 0) {
            if (nskpt == 0) {
                status = OCSM_SKETCHER_IS_NOT_OPEN;
                CHECK_STATUS(linseg);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(linseg);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(linseg);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_LINSEG,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            /* increment the number of sketch points */
            nskpt++;

        /* input is: "cirarc xon yon zon xend yend zend" */
        } else if (strcmp(command, "cirarc") == 0) {
            if (nskpt == 0) {
                status = OCSM_SKETCHER_IS_NOT_OPEN;
                CHECK_STATUS(cirarc);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(cirarc);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s %127s %127s %127s\n",
                          str1, str2, str3, str4, str5, str6);
            if (narg != 6) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(cirarc);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_CIRARC,
                                 str1, str2, str3, str4, str5, str6, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            /* increment the number of sketch points */
            nskpt++; nskpt++;

        /* input is: "spline x y z" */
        } else if (strcmp(command, "spline") == 0) {
            if (nskpt == 0) {
                status = OCSM_SKETCHER_IS_NOT_OPEN;
                CHECK_STATUS(spline);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(spline);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s %127s\n",
                          str1, str2, str3);
            if (narg != 3) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(spline);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SPLINE,
                                 str1, str2, str3, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            /* increment the number of sketch points */
            nskpt++;

        /* input is: "skend" */
        } else if (strcmp(command, "skend") == 0) {
            if (nskpt < 1) {
                status = OCSM_COLINEAR_SKETCH_POINTS;
                CHECK_STATUS(skend);
            } else if (nskpt > MAX_SKETCH_SIZE) {
                status = OCSM_TOO_MANY_SKETCH_POINTS;
                CHECK_STATUS(skend);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(skend);
            }

            /* no arguments */

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SKEND,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            /* reset the number of sketch points */
            nskpt = 0;

        /* input is: "solbeg" */
        } else if (strcmp(command, "solbeg") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(solbeg);
            }

            /* extract arguments */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s\n",
                          &str1[1]);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(solbeg);
            }

            /* remember that we are in a solver block */
            insolver = 1;

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SOLBEG,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "solcon expr" */
        } else if (strcmp(command, "solcon") == 0) {
            if (insolver != 1) {
                status = OCSM_SOLVER_IS_NOT_OPEN;
                CHECK_STATUS(solcon);
            }

            /* extract arguments */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s\n",
                          &str1[1]);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(solcon);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SOLCON,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "solend" */
        } else if (strcmp(command, "solend") == 0) {
            if (insolver != 1) {
                status = OCSM_SOLVER_IS_NOT_OPEN;
                CHECK_STATUS(solend);
            }

            /* no arguments */

            /* close the solver */
            insolver = 0;

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SOLEND,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "set pmtrName defn" */
        } else if (strcmp(command, "set") == 0) {
            if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(set);
            }

            /* extract arguments */
            str1[0] = '$';
            str2[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          &str1[1], &str2[1]);
            if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(set);
            }

            /* do not allow pmtrName to start with '@' */
            if (str1[1] == '@') {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(dimension);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_SET,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "macbeg imacro" */
        } else if (strcmp(command, "macbeg") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(macbeg);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(macbeg);
            }

            /* extract argument */
            narg = sscanf(nextline, "%*s %127s\n",
                          str1);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(macbeg);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_MACBEG,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "macend" */
        } else if (strcmp(command, "macend") == 0) {
            if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(macend);
            }

            /* no arguments */

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_MACEND,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "recall imacro" */
        } else if (strcmp(command, "recall") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(recall);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(recall);
            }

            /* extract argument and save*/
            narg = sscanf(nextline, "%*s %127s\n",
                          str1);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(recall);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_RECALL,
                                 str1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "patbeg pmtrName ncopy" */
        } else if (strcmp(command, "patbeg") == 0) {
            if (npatn >= MAX_NUM_PATTERNS) {
                status = OCSM_PATTERNS_NESTED_TOO_DEEPLY;
                CHECK_STATUS(patbeg);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(patbeg);
            } else {
                npatn++;
            }

            /* extract arguments */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          &str1[1], str2);
            if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(patbeg);
            }

            /* do not allow pmtrName to start with '@' */
            if (str1[1] == '@') {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(dimension);
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_PATBEG,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "patend" */
        } else if (strcmp(command, "patend") == 0) {
            if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(patend);
            }

            /* find the matching patbeg */
            i = 1;
            for (ibrch = MODL->nbrch; ibrch > 0; ibrch--) {
                if        (MODL->brch[ibrch].type == OCSM_PATEND) {
                    i++;
                } else if (MODL->brch[ibrch].type == OCSM_PATBEG) {
                    i--;
                    if (i == 0) {
                        break;
                    }
                }
            }
            if (ibrch <= 0) {
                status = OCSM_PATEND_WITHOUT_PATBEG;
                CHECK_STATUS(patend);
            }

            /* no arguments */

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_PATEND,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

            npatn--;

        /* input is: "mark" */
        } else if (strcmp(command, "mark") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(mark);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(mark);
            }

            /* no arguments */

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_MARK,
                                 NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "dump filename remove=0" */
        } else if (strcmp(command, "dump") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(dump);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(dump);
            }

            /* extract argument */
            str1[0] = '$';
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          &str1[1], str2);
            if (narg < 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(dump);
            } else if (narg == 1) {
                strcpy(str2, "0");
            }

            /* create the new Branch */
            status = ocsmNewBrch(MODL, MODL->nbrch, OCSM_DUMP,
                                 str1, str2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            CHECK_STATUS(ocsmNewBrch);

        /* input is: "name branchName" */
        } else if (strcmp(command, "name") == 0) {

            /* previous Branch will be named */
            ibrch = MODL->nbrch;
            if (ibrch < 1) {
                status = OCSM_ILLEGAL_BRCH_INDEX;
                CHECK_STATUS(name);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s\n",
                          str1);
            if (narg != 1) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(name);
            }

            /* set the Branch's name */
            status = ocsmSetName(MODL, ibrch, str1);
            CHECK_STATUS(ocsmSetName);

        /* input is: "attribute attrName attrValue */
        } else if (strcmp(command, "attribute") == 0) {

            /* previous Branch will be attributed */
            ibrch = MODL->nbrch;
            if (ibrch < 1) {
                status = OCSM_ILLEGAL_BRCH_INDEX;
                CHECK_STATUS(attribute);
            }

            /* extract arguments */
            narg = sscanf(nextline, "%*s %127s %127s\n",
                          str1, str2);
            if (narg != 2) {
                status = OCSM_NOT_ENOUGH_ARGS;
                CHECK_STATUS(attribute);
            }

            /* set the Branch's attribute */
            status = ocsmSetAttr(MODL, ibrch, str1, str2);
            CHECK_STATUS(ocsmSetAttr);

        /* input is: "end" */
        } else if (strcmp(command, "end") == 0) {
            if (nskpt > 0) {
                status = OCSM_SKETCHER_IS_OPEN;
                CHECK_STATUS(end);
            } else if (insolver != 0) {
                status = OCSM_SOLVER_IS_OPEN;
                CHECK_STATUS(end);
            }

            break;

        /* illegal command */
        } else {
            status = OCSM_ILLEGAL_STATEMENT;
            goto cleanup;
        }
    }

    /* close the .csm file */
    if (csm_file != NULL) fclose(csm_file);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSave - save a MODL to a file                                   *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSave(void   *modl,                  /* (in)  pointer to MODL */
         char   filename[])             /* (in)  file to be written (with .csm) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ipmtr, ibrch, iattr, irow, icol, index;
    FILE      *csm_file;

    ROUTINE(ocsmSave);
    DPRINT2("%s(filename=%s) {",
            routine, filename);

    /* --------------------------------------------------------------- */

    SPRINT1(1, "--> enter ocsmSave(filename=%s)", filename);

    /* open file */
    csm_file = fopen(filename, "w");
    if (csm_file == NULL) {
        status = OCSM_FILE_NOT_FOUND;
        goto cleanup;
    }

    /* write header */
    fprintf(csm_file, "# %s written by ocsmSave\n", filename);

    /* write the design (external) Parameters */
    fprintf(csm_file, "\n# Design Parameters:\n");
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        if (MODL->pmtr[ipmtr].nrow > 1 ||
            MODL->pmtr[ipmtr].ncol > 1   ) {
            if (MODL->pmtr[ipmtr].type == OCSM_EXTERNAL) {
                fprintf(csm_file, "dimension   %s   %d   %d   1\n",
                        MODL->pmtr[ipmtr].name,
                        MODL->pmtr[ipmtr].nrow,
                        MODL->pmtr[ipmtr].ncol);
            } else {
                fprintf(csm_file, "dimension   %s   %d   %d   0\n",
                        MODL->pmtr[ipmtr].name,
                        MODL->pmtr[ipmtr].nrow,
                        MODL->pmtr[ipmtr].ncol);
            }
        }

        if (MODL->pmtr[ipmtr].type == OCSM_EXTERNAL) {
            index = 0;
            for (icol = 1; icol <= MODL->pmtr[ipmtr].ncol; icol++) {
                for (irow = 1; irow <= MODL->pmtr[ipmtr].nrow; irow++) {
                    fprintf(csm_file, "despmtr   %s[%d,%d]   %11.5f\n",
                            MODL->pmtr[ipmtr].name, irow, icol,
                            MODL->pmtr[ipmtr].value[index]);
                    index++;
                }
            }
        }
    }

    /* write the Branches */
    fprintf(csm_file, "\n# Branches:\n");
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        if        (MODL->brch[ibrch].type == OCSM_BOX) {
            fprintf(csm_file, "box       %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6);
        } else if (MODL->brch[ibrch].type == OCSM_SPHERE) {
            fprintf(csm_file, "sphere    %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4);
        } else if (MODL->brch[ibrch].type == OCSM_CONE) {
            fprintf(csm_file, "cone      %s   %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6,
                    MODL->brch[ibrch].arg7);
        } else if (MODL->brch[ibrch].type == OCSM_CYLINDER) {
            fprintf(csm_file, "cylinder  %s   %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6,
                    MODL->brch[ibrch].arg7);
        } else if (MODL->brch[ibrch].type == OCSM_TORUS) {
            fprintf(csm_file, "torus     %s   %s   %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6,
                    MODL->brch[ibrch].arg7,
                    MODL->brch[ibrch].arg8);
        } else if (MODL->brch[ibrch].type == OCSM_IMPORT) {
            fprintf(csm_file, "import    %s\n",
                  &(MODL->brch[ibrch].arg1[1]));
        } else if (MODL->brch[ibrch].type == OCSM_UDPRIM) {
            fprintf(csm_file, "udprim    %s",
                  &(MODL->brch[ibrch].arg1[1]));
            if (MODL->brch[ibrch].narg >= 3) {
                fprintf(csm_file, "   %s   %s",
                        &(MODL->brch[ibrch].arg2[1]),
                        &(MODL->brch[ibrch].arg3[1]));
            }
            if (MODL->brch[ibrch].narg >= 5) {
                fprintf(csm_file, "   %s   %s",
                        &(MODL->brch[ibrch].arg4[1]),
                        &(MODL->brch[ibrch].arg5[1]));
            }
            if (MODL->brch[ibrch].narg >= 7) {
                fprintf(csm_file, "   %s   %s",
                        &(MODL->brch[ibrch].arg6[1]),
                        &(MODL->brch[ibrch].arg7[1]));
            }
            if (MODL->brch[ibrch].narg >= 9) {
                fprintf(csm_file, "   %s   %s",
                        &(MODL->brch[ibrch].arg8[1]),
                        &(MODL->brch[ibrch].arg9[1]));
            }
            fprintf(csm_file, "\n");
        } else if (MODL->brch[ibrch].type == OCSM_EXTRUDE) {
            fprintf(csm_file, "extrude   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_LOFT) {
            fprintf(csm_file, "loft      %s\n",
                    MODL->brch[ibrch].arg1);
        } else if (MODL->brch[ibrch].type == OCSM_REVOLVE) {
            fprintf(csm_file, "revolve   %s   %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6,
                    MODL->brch[ibrch].arg7);
        } else if (MODL->brch[ibrch].type == OCSM_FILLET) {
            fprintf(csm_file, "fillet    %s   %s\n",
                    MODL->brch[ibrch].arg1,
                  &(MODL->brch[ibrch].arg2[1]));
        } else if (MODL->brch[ibrch].type == OCSM_CHAMFER) {
            fprintf(csm_file, "chamfer   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                  &(MODL->brch[ibrch].arg2[1]));
        } else if (MODL->brch[ibrch].type == OCSM_HOLLOW) {
            fprintf(csm_file, "hollow    %s   %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6,
                    MODL->brch[ibrch].arg7);
        } else if (MODL->brch[ibrch].type == OCSM_INTERSECT) {
            fprintf(csm_file, "intersect %s   %s\n",
                  &(MODL->brch[ibrch].arg1[1]),
                    MODL->brch[ibrch].arg2    );
        } else if (MODL->brch[ibrch].type == OCSM_SUBTRACT) {
            fprintf(csm_file, "subtract %s   %s\n",
                  &(MODL->brch[ibrch].arg1[1]),
                    MODL->brch[ibrch].arg2    );
        } else if (MODL->brch[ibrch].type == OCSM_UNION) {
            fprintf(csm_file, "union\n");
        } else if (MODL->brch[ibrch].type == OCSM_TRANSLATE) {
            fprintf(csm_file, "translate %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_ROTATEX) {
            fprintf(csm_file, "rotatex   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_ROTATEY) {
            fprintf(csm_file, "rotatey   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_ROTATEZ) {
            fprintf(csm_file, "rotatez   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_SCALE) {
            fprintf(csm_file, "scale     %s\n",
                    MODL->brch[ibrch].arg1);
        } else if (MODL->brch[ibrch].type == OCSM_SKBEG) {
            fprintf(csm_file, "skbeg     %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_LINSEG) {
            fprintf(csm_file, "linseg    %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_CIRARC) {
            fprintf(csm_file, "cirarc    %s   %s   %s   %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3,
                    MODL->brch[ibrch].arg4,
                    MODL->brch[ibrch].arg5,
                    MODL->brch[ibrch].arg6);
        } else if (MODL->brch[ibrch].type == OCSM_SPLINE) {
            fprintf(csm_file, "spline    %s   %s   %s\n",
                    MODL->brch[ibrch].arg1,
                    MODL->brch[ibrch].arg2,
                    MODL->brch[ibrch].arg3);
        } else if (MODL->brch[ibrch].type == OCSM_SKEND) {
            fprintf(csm_file, "skend\n");
        } else if (MODL->brch[ibrch].type == OCSM_SET) {
            fprintf(csm_file, "set       %s %s\n",
                  &(MODL->brch[ibrch].arg1[1]),
                  &(MODL->brch[ibrch].arg2[1]));
        } else if (MODL->brch[ibrch].type == OCSM_MACBEG) {
            fprintf(csm_file, "macbeg    %s\n",
                    MODL->brch[ibrch].arg1);
        } else if (MODL->brch[ibrch].type == OCSM_MACEND) {
            fprintf(csm_file, "macend\n");
        } else if (MODL->brch[ibrch].type == OCSM_RECALL) {
            fprintf(csm_file, "recall    %s\n",
                    MODL->brch[ibrch].arg1);
        } else if (MODL->brch[ibrch].type == OCSM_PATBEG) {
            fprintf(csm_file, "patbeg    %s %s\n",
                  &(MODL->brch[ibrch].arg1[1]),
                    MODL->brch[ibrch].arg2    );
        } else if (MODL->brch[ibrch].type == OCSM_PATEND) {
            fprintf(csm_file, "patend\n");
        } else if (MODL->brch[ibrch].type == OCSM_MARK) {
            fprintf(csm_file, "mark\n");
        } else if (MODL->brch[ibrch].type == OCSM_DUMP) {
            fprintf(csm_file, "dump      %s   %s\n",
                    &(MODL->brch[ibrch].arg1[1]),
                      MODL->brch[ibrch].arg2    );
        }

        /* write the name of the Branch (if not the default name) */
        if (strncmp(MODL->brch[ibrch].name, "Brch_", 5) != 0) {
            fprintf(csm_file, "name      %s\n",
                    MODL->brch[ibrch].name);
        }

        /* write the Attributes for the Branch */
        for (iattr = 0; iattr < MODL->brch[ibrch].nattr; iattr++) {
            fprintf(csm_file, "attribute %s   %s\n",
                    MODL->brch[ibrch].attr[iattr].name,
                    MODL->brch[ibrch].attr[iattr].value);
        }
    }

    /* close the file */
    fprintf(csm_file, "\nend\n");
    fclose(csm_file);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmCopy - copy a MODL                                             *
 *                                                                      *
 ************************************************************************
 */

int
ocsmCopy(void   *srcModl,               /* (in)  pointer to source MODL */
         void   **newModl)              /* (out) pointer to new    MODL */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *SRC_MODL = (modl_T*)srcModl;
    modl_T    *NEW_MODL;

    int       ibrch, iattr, ipmtr, irow, icol, index, i;

    ROUTINE(ocsmCopy);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    /* default return value */
    *newModl = NULL;

    /* check magic number */
    if (SRC_MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (SRC_MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* make a new MODL and initialize it */
    MALLOC(NEW_MODL, modl_T, 1);

    NEW_MODL->magic   = OCSM_MAGIC;
    NEW_MODL->checked = 0;
    NEW_MODL->nextseq = SRC_MODL->nextseq;

    for (i = 0; i < 24; i++) {
        NEW_MODL->atPmtrs[i] = SRC_MODL->atPmtrs[i];
    }

    NEW_MODL->nbrch = 0;
    NEW_MODL->mbrch = 0;
    NEW_MODL->brch  = NULL;

    NEW_MODL->npmtr = 0;
    NEW_MODL->mpmtr = 0;
    NEW_MODL->pmtr  = NULL;

    NEW_MODL->nbody = 0;
    NEW_MODL->mbody = 0;
    NEW_MODL->body  = NULL;

    NEW_MODL->context = NULL;

    /* return value */
    *newModl = NEW_MODL;

    /* copy the Parameter table (done before Branch table so that
       new Parameters do not get created by a OCSM_SET or OCSM_PATBEG) */
    for (ipmtr = 1; ipmtr <= SRC_MODL->npmtr; ipmtr++) {
        status = ocsmNewPmtr(NEW_MODL,
                             SRC_MODL->pmtr[ipmtr].name,
                             SRC_MODL->pmtr[ipmtr].type,
                             SRC_MODL->pmtr[ipmtr].nrow,
                             SRC_MODL->pmtr[ipmtr].ncol);
        CHECK_STATUS(ocsmNewPmtr);

        index = 0;
        for (irow = 1; irow <= SRC_MODL->pmtr[ipmtr].nrow; irow++) {
            for (icol = 1; icol <= SRC_MODL->pmtr[ipmtr].ncol; icol++) {
                NEW_MODL->pmtr[ipmtr].value[index] = SRC_MODL->pmtr[ipmtr].value[index];
                index++;
            }
        }
    }

    /* copy the Branch table */
    for (ibrch = 1; ibrch <= SRC_MODL->nbrch; ibrch++) {
        status = ocsmNewBrch(NEW_MODL,
                             NEW_MODL->nbrch,
                             SRC_MODL->brch[ibrch].type,
                             SRC_MODL->brch[ibrch].arg1,
                             SRC_MODL->brch[ibrch].arg2,
                             SRC_MODL->brch[ibrch].arg3,
                             SRC_MODL->brch[ibrch].arg4,
                             SRC_MODL->brch[ibrch].arg5,
                             SRC_MODL->brch[ibrch].arg6,
                             SRC_MODL->brch[ibrch].arg7,
                             SRC_MODL->brch[ibrch].arg8,
                             SRC_MODL->brch[ibrch].arg9);
        CHECK_STATUS(ocsmNewBrch);

        /* copy the name and attributes */
        FREE(  NEW_MODL->brch[ibrch].name);
        MALLOC(NEW_MODL->brch[ibrch].name, char, (int)(strlen(SRC_MODL->brch[ibrch].name)+1));
        strcpy(NEW_MODL->brch[ibrch].name,                    SRC_MODL->brch[ibrch].name    );

        for (iattr = 0; iattr < SRC_MODL->brch[ibrch].nattr; iattr++) {
            status = ocsmSetAttr(NEW_MODL, ibrch,
                                 SRC_MODL->brch[ibrch].attr[iattr].name,
                                 SRC_MODL->brch[ibrch].attr[iattr].value);
        }
    }

    /* set the Branch's activity and link to the other Branches */
    for (ibrch = 1; ibrch <= SRC_MODL->nbrch; ibrch++) {
        NEW_MODL->brch[ibrch].actv  = SRC_MODL->brch[ibrch].actv;
        NEW_MODL->brch[ibrch].ileft = SRC_MODL->brch[ibrch].ileft;
        NEW_MODL->brch[ibrch].irite = SRC_MODL->brch[ibrch].irite;
        NEW_MODL->brch[ibrch].ichld = SRC_MODL->brch[ibrch].ichld;
    }

    /* do NOT copy the Body table */

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmFree - free all storage associated with a MODL                 *
 *                                                                      *
 ************************************************************************
 */

int
ocsmFree(void   *modl)                  /* (in)  pointer to MODL */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ibrch, iattr, ipmtr, ibody, iface;

    ROUTINE(ocsmFree);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* free up any storage associated with the UDPs */
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        udp_cleanupAll();
    #endif

    /* free up the Branch table */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        for (iattr = 0; iattr < MODL->brch[ibrch].nattr; iattr++) {
            FREE(MODL->brch[ibrch].attr[iattr].name );
            FREE(MODL->brch[ibrch].attr[iattr].value);
        }

        FREE(MODL->brch[ibrch].name);
        FREE(MODL->brch[ibrch].attr);
        FREE(MODL->brch[ibrch].arg1);
        FREE(MODL->brch[ibrch].arg2);
        FREE(MODL->brch[ibrch].arg3);
        FREE(MODL->brch[ibrch].arg4);
        FREE(MODL->brch[ibrch].arg5);
        FREE(MODL->brch[ibrch].arg6);
        FREE(MODL->brch[ibrch].arg7);
        FREE(MODL->brch[ibrch].arg8);
        FREE(MODL->brch[ibrch].arg9);
    }

    FREE(MODL->brch);

    /* free up the Parameter table */
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        FREE(MODL->pmtr[ipmtr].name);
        FREE(MODL->pmtr[ipmtr].value);
    }

    FREE(MODL->pmtr);

    /* free up the Body table */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            FREE(MODL->body[ibody].face[iface].ibody);
            FREE(MODL->body[ibody].face[iface].iford);
        }

        FREE(MODL->body[ibody].node);
        FREE(MODL->body[ibody].edge);
        FREE(MODL->body[ibody].face);
    }

    FREE(MODL->body);

    /* free up the MODL structure */
    FREE(MODL);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmInfo - get info about a MODL                                   *
 *                                                                      *
 ************************************************************************
 */

int
ocsmInfo(void   *modl,                  /* (in)  pointer to MODL */
         int    *nbrch,                 /* (out) number of Branches */
         int    *npmtr,                 /* (out) number of Parameters */
         int    *nbody)                 /* (out) number of Bodys */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmInfo);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    /* default return values */
    *nbrch = -1;
    *npmtr = -1;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* return values */
    *nbrch = MODL->nbrch;
    *npmtr = MODL->npmtr;
    *nbody = MODL->nbody;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmCheck - check that Branches are properly ordered               *
 *                                                                      *
 ************************************************************************
 */

int
ocsmCheck(void   *modl)                 /* (in)  pointer to MODL */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ibrch, ibrchl, ibrchr, jbrch, type, class;
    int       ipass, imark, nsketch, nmacro;
    int       ileft, ichld;

    ROUTINE(ocsmCheck);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    SPRINT0(1, "--> enter ocsmCheck()");

    /* initially there are no errors */
    ipass = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* initialize all parent/child repationships */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        MODL->brch[ibrch].ileft = -1;
        MODL->brch[ibrch].irite = -1;
        MODL->brch[ibrch].ichld = -1;
    }

    /* set up linkages to other Branches */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        type  = MODL->brch[ibrch].type;
        class = MODL->brch[ibrch].class;

        /* PRIMITIVEs have no parents */
        if        (class == OCSM_PRIMITIVE) {
            MODL->brch[ibrch ].ichld = 0;

        /* LOFTs must have a previous MARK. left parent is first branch
              (typically a SKEND or UDPRIM) without a child */
        } else if (type == OCSM_LOFT) {
            imark   = 0;
            nsketch = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].type == OCSM_MARK) {
                    imark = 1;
                    break;
                } else if (MODL->brch[jbrch].ichld == 0) {
                    nsketch++;

                    MODL->brch[ibrch].ileft = jbrch;
                    MODL->brch[jbrch].ichld = ibrch;

                    MODL->brch[ibrch].ichld = 0;
                }
            }

            if (imark == 0) {
                status = OCSM_LOFT_WITHOUT_MARK;
                goto cleanup;
            } else if (nsketch < 1) {
                status = OCSM_EXPECTING_NLOFT_SKETCHES;
                goto cleanup;
            } else if (nsketch >= MAX_NUM_SKETCHES) {
                status = OCSM_TOO_MANY_SKETCHES_IN_LOFT;
                goto cleanup;
            }

        /* GROWNs (except LOFT) have a left parent (typically a
               SKEND or UDPRIM) */
        } else if (class == OCSM_GROWN) {  /* except OCSM_LOFT */
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].ichld == 0) {
                    ibrchl = jbrch;
                    break;
                }
            }

            if (ibrchl == 0) {
                status = OCSM_EXPECTING_ONE_SKETCH;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* APPLIEDs have a Body as its left parent */
        } else if (class == OCSM_APPLIED) {
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].ichld == 0) {
                    if (MODL->brch[jbrch].type != OCSM_SKEND) {
                        ibrchl = jbrch;
                        break;
                    } else {
                        status = OCSM_EXPECTING_ONE_BODY;
                        goto cleanup;
                    }
                }
            }

            if (ibrchl == 0) {
                status = OCSM_EXPECTING_ONE_BODY;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* BOOLEANs have two Bodys as parents */
        } else if (class == OCSM_BOOLEAN) {
            ibrchl = 0;
            ibrchr = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].ichld == 0) {
                    if        (ibrchr == 0) {
                        ibrchr = jbrch;
                    } else if (ibrchl == 0) {
                        ibrchl = jbrch;
                        break;
                    }
                }
            }

            if (ibrchl == 0 || ibrchr == 0) {
                status = OCSM_EXPECTING_TWO_BODYS;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].irite = ibrchr;
            MODL->brch[ibrchr].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* TRANSFORMs has a Body or SKEND as parent */
        } else if (class == OCSM_TRANSFORM) {
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].ichld == 0) {
                    if (ibrchl == 0) {
                        ibrchl = jbrch;
                        break;
                    }
                }
            }

            if (ibrchl == 0) {
                status = OCSM_EXPECTING_ONE_BODY;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* SKBEGs have no parents */
        } else if (type == OCSM_SKBEG) {
            MODL->brch[ibrch ].ichld = 0;

        /* SKETCHs (except SKBEGs) are linked to previous Branch */
        } else if (class == OCSM_SKETCH) {
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if        (MODL->brch[jbrch].type == OCSM_SKEND) {
                    status = OCSM_SKETCHER_IS_OPEN;
                    goto cleanup;
                } else if (MODL->brch[jbrch].type == OCSM_SOLEND) {
                    status = OCSM_SOLVER_IS_NOT_OPEN;
                    goto cleanup;
                } else if (MODL->brch[jbrch].type == OCSM_SKBEG) {
                    ibrchl = jbrch;
                    break;
                }
            }

            if (ibrchl == 0) {
                status = OCSM_SKETCHER_IS_NOT_OPEN;
                goto cleanup;
            }

            ibrchl = ibrch - 1;

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* SOLBEGs have no parents */
        } else if (type == OCSM_SOLBEG) {
            MODL->brch[ibrch ].ichld = 0;

        /* SOLVERs (except SOLBEGs) have the SOLBEG as its left parent */
        } else if (class == OCSM_SOLVER) {
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if        (MODL->brch[jbrch].type == OCSM_SOLEND) {
                    status = OCSM_SOLVER_IS_OPEN;
                    goto cleanup;
                } else if (MODL->brch[jbrch].type == OCSM_SKEND) {
                    status = OCSM_SKETCHER_IS_NOT_OPEN;
                    goto cleanup;
                } else if (MODL->brch[jbrch].type == OCSM_SOLBEG) {
                    ibrchl = jbrch;
                    break;
                }
            }

            if (ibrchl == 0) {
                status = OCSM_SOLVER_IS_NOT_OPEN;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        /* MACBEGs have no parent or children */
        } else if (type == OCSM_MACEND) {
            MODL->brch[ibrch].ileft = 0;
            MODL->brch[ibrch].irite = 0;
            MODL->brch[ibrch].ichld = 0;

            nmacro = 1;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if        (MODL->brch[jbrch].type == OCSM_MACEND) {
                    nmacro++;
                } else if (MODL->brch[jbrch].type == OCSM_MACBEG) {
                    nmacro--;
                }
                if (MODL->brch[jbrch].ichld == 0) {
                    MODL->brch[jbrch].ichld = ibrch;
                }
                if (nmacro <= 0) {
                    MODL->brch[ibrch].ileft = jbrch;
                    MODL->brch[ibrch].irite = ibrch;
                    MODL->brch[ibrch].ichld = -1;
                    break;
                }
            }

        } else if (type == OCSM_RECALL) {
            MODL->brch[ibrch ].ileft = 0;
            MODL->brch[ibrch ].irite = 0;
            MODL->brch[ibrch ].ichld = 0;

        } else if (type == OCSM_DUMP) {
            ibrchl = 0;
            for (jbrch = ibrch-1; jbrch > 0; jbrch--) {
                if (MODL->brch[jbrch].ichld == 0 &&
                    MODL->brch[jbrch].type  != OCSM_MACEND) {
                    if (ibrchl == 0) {
                        ibrchl = jbrch;
                        break;
                    }
                }
            }

            if (ibrchl == 0) {
                status = OCSM_EXPECTING_ONE_BODY;
                goto cleanup;
            }

            MODL->brch[ibrch ].ileft = ibrchl;
            MODL->brch[ibrchl].ichld = ibrch;

            MODL->brch[ibrch ].ichld = 0;

        } else if (class == OCSM_UTILITY) { /* except OCSM_MACEND, OCSM_RECALL, and OCSM_DUMP */
            /* no linkages */

        } else {
            status = OCSM_ILLEGAL_TYPE;
        }

        /* remember that this Branch passed the test */
        ipass = ibrch;
    }

    /* activate all Branches that are not suppressed */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        if (MODL->brch[ibrch].actv != OCSM_SUPPRESSED) {
            MODL->brch[ibrch].actv =  OCSM_ACTIVE;
        }
    }

    /* propagate inactivity from suppressed Branches */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        if (MODL->brch[ibrch].actv == OCSM_SUPPRESSED) {

            /* from a LOFT, inactivate left parents until next MARK */
            if (MODL->brch[ibrch].type == OCSM_LOFT) {
                ileft = MODL->brch[ibrch].ileft;
                while (ileft > 0) {
                    if (MODL->brch[ileft].actv == OCSM_ACTIVE) {
                        MODL->brch[ileft].actv =  OCSM_INACTIVE;
                    }

                    if (MODL->brch[ileft].type == OCSM_MARK) {
                        break;
                    } else {
                        ileft = MODL->brch[ileft].ileft;
                    }
                }

            /* from a GROWN (except LOFT), inactivate left parents
               until next SKBEG */
            } else if (MODL->brch[ibrch].class == OCSM_GROWN) {
                ileft = MODL->brch[ibrch].ileft;
                while (ileft > 0) {
                    if (MODL->brch[ileft].actv == OCSM_ACTIVE) {
                        MODL->brch[ileft].actv =  OCSM_INACTIVE;
                    }

                    if (MODL->brch[ileft].type == OCSM_SKBEG) {
                        break;
                    } else {
                        ileft = MODL->brch[ileft].ileft;
                    }
                }
            }

            /* if a PRIMITIVE or GROWN, inactivate children until
               next BOOLEAN */
            if (MODL->brch[ibrch].class == OCSM_PRIMITIVE ||
                MODL->brch[ibrch].class == OCSM_GROWN       ) {
                ichld = MODL->brch[ibrch].ichld;
                while (ichld > 0) {
                    if (MODL->brch[ichld].actv == OCSM_ACTIVE) {
                        MODL->brch[ichld].actv =  OCSM_INACTIVE;
                    }

                    if (MODL->brch[ichld].class == OCSM_BOOLEAN) {

                        /* inactivate any APPLIEDs associated with the BOOLEAN */
                        ichld = MODL->brch[ichld].ichld;
                        while (ichld > 0) {
                            if (MODL->brch[ichld].class == OCSM_APPLIED) {
                                if (MODL->brch[ichld].actv == OCSM_ACTIVE) {
                                    MODL->brch[ichld].actv =  OCSM_INACTIVE;
                                }
                                ichld = MODL->brch[ichld].ichld;
                            } else {
                                break;
                            }
                        }

                        break;
                    } else {
                        ichld = MODL->brch[ichld].ichld;
                    }
                }
            }
        }
    }

    /* defer all active Branches within macro definitions */
    nmacro = 0;
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        if (MODL->brch[ibrch].actv == OCSM_ACTIVE && nmacro > 0) {
            MODL->brch[ibrch].actv = OCSM_DEFERRED;
        }
        if        (MODL->brch[ibrch].type == OCSM_MACBEG) {
            nmacro++;
        } else if (MODL->brch[ibrch].type == OCSM_MACEND) {
            nmacro--;
        }
    }

    /* MODL has passed all checks */
    MODL->checked = 1;

cleanup:
    if (MODL->checked == 1) {
        SPRINT0(1, "--> checks passed");
    } else {
        SPRINT0(1, "--> checks failed");

        /* mark the Branch after the last one that passed as the Branch with the error */
        if (ipass < MODL->nbrch) {
            MODL->brch[ipass+1].ileft = -2;
        } else {
            status = OCSM_ILLEGAL_BRCH_INDEX;
        }
    }

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmBuild - build Bodys by executing the MODL up to a given Branch *
 *                                                                      *
 ************************************************************************
 */

int
ocsmBuild(void   *modl,                 /* (in)  pointer to MODL */
          int    buildTo,               /* (in)  last Branch to execute (or 0 for all) */
          int    *builtTo,              /* (out) last Branch executed successfully */
          int    *nbody,                /* (in)  number of entries allocated in body[] */
                                        /* (out) number of Bodys on the stack */
          int    body[])                /* (out) array  of Bodys on the stack (LIFO)
                                                 (at least *nbody long) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    /* stack contains Bodys */
    int       nstack, nstack_save, stack[MAX_STACK_SIZE];

    /* sketch contains linsegs, cirarcs, and splines */
    int       nskpt;
    skpt_T    skpt[MAX_SKETCH_SIZE];

    /* storage loaction information */
    int        imacro, macros[MAX_NUM_MACROS+1];

    /* pattern information */
    int        npatn;
    patn_T     patn[MAX_NUM_PATTERNS];
    char       newValue[6];

    /* solver information */
    int        nsolcon;
    int        solcons[MAX_SOLVER_SIZE];

    int        ibrch, type, ibrchl, i, j, iface, nbodyMax, ncatch=0, buildStatus=SUCCESS;
    int        ipmtr, jpmtr, icount, jcount;
    int        ibody, ibodyl, nrow, ncol, irow, icol;
    double     args[10];
    double     rows, cols, toler, value;
    char       pmtrName[MAX_EXPR_LEN];
    char       row[MAX_EXPR_LEN], col[MAX_EXPR_LEN], defn[MAX_EXPR_LEN];

    #if   defined(GEOM_CAPRI)
        int        ivol, numvol, nsketch=0;
    #elif defined(GEOM_EGADS)
        int        imajor, iminor;
        char       dumpfile[MAX_EXPR_LEN];
        ego        ebody, ebodyl, emodel, etemp;
    #endif

    ROUTINE(ocsmBuild);
    DPRINT2("%s(buildTo=%d) {",
            routine, buildTo);

    /* macro to catch bad status and then continue to finalizing */
#define CATCH_STATUS(X)                                                 \
    if (status < SUCCESS) {                                             \
        nstack = nstack_save;                                           \
        ncatch++;                                                       \
        printf( "WARNING:: build terminated early due to BAD STATUS = %d from %s (called from %s:%d)\n", status, #X, routine, __LINE__); \
        goto finalize;                                                  \
    }

    /* --------------------------------------------------------------- */

    SPRINT1(1, "--> enter ocsmBuild(buildTo=%d)", buildTo);

    *builtTo = 0;
    nbodyMax = *nbody;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* make sure that the MODL has been checked */
    if (MODL->checked != 1) {
//$$$        status = OCSM_MODL_NOT_CHECKED;
//$$$        goto cleanup;
        if (MODL->nbrch > 0) {
            status = ocsmCheck(MODL);
            CHECK_STATUS(ocsmCheck);
        }
    }

    #if   defined(GEOM_CAPRI)
        /* remove all previous CAPRI volumes */
        status = gi_uNumVolumes();
        CHECK_STATUS(gi_uNumVolumes);

        numvol = status;
        if (numvol <= 0) {
            SPRINT0(1, "    no volumes to release");
        } else {
            SPRINT1(1, "    releasing %d volumes", numvol);

            for (ivol = 1; ivol <= numvol; ivol++) {
                status = gi_uStatModel(ivol);
                if (status >= 0) {
                    status = gi_uRelModel(ivol);
                    CHECK_STATUS(gi_uRelModel);
                }
            }
        }
    #elif defined(GEOM_EGADS)
        EG_revision(&imajor, &iminor);
        SPRINT2(1, "\nEGADS version %2d.%02d\n", imajor, iminor);

        /* create a new EGADS context (if not already set) */
        if (MODL->context == NULL) {
            status = EG_open(&(MODL->context));
            CHECK_STATUS(EG_open);
        }

        status = EG_setOutLevel(MODL->context, 1);
        CHECK_STATUS(EG_setOutLevel);
    #endif

    /* free up any previous Bodys */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        for (iface = 1; iface <= MODL->body[ibody].nface; iface++) {
            FREE(MODL->body[ibody].face[iface].ibody);
            FREE(MODL->body[ibody].face[iface].iford);
        }

        FREE(MODL->body[ibody].node);
        FREE(MODL->body[ibody].edge);
        FREE(MODL->body[ibody].face);
    }

    FREE(MODL->body);
    MODL->nbody = 0;
    MODL->mbody = 0;

    /* initialize the stack */
    nstack      = 0;
    nstack_save = 0;

    /* initialize the number of active sketcher points (and constraints) */
    nskpt  = 0;

    /* initiailze the number of nested patterns */
    npatn = 0;

    /* initialize the number of solver constrains */
    nsolcon = 0;

    /* initialize the storage storage locations */
    for (imacro = 1; imacro <= MAX_NUM_MACROS; imacro++) {
        macros[imacro] = -1;
    }

    /* if buildTo was set to zero, reset it (locally) to be greater
       then MODL->nbrch */
    if (buildTo == 0) {
        buildTo = MODL->nbrch + 1;
    }

    /* loop through and process all the Branches (up to buildTo) */
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        nstack_save = nstack;

        type = MODL->brch[ibrch].type;

        /* exit if we exceeded buildTo */
        if (ibrch > buildTo) {
            break;
        }

        /* if Branch is deferred, mark it as active but skip it for now */
        if (MODL->brch[ibrch].actv == OCSM_DEFERRED) {
            MODL->brch[ibrch].actv =  OCSM_ACTIVE;
            SPRINT1(1, "    deferring [%4d]:", ibrch);
            continue;
        }

        /* skip this Branch if it is not active */
        if (MODL->brch[ibrch].actv != OCSM_ACTIVE) {
            SPRINT1(1, "    skipping  [%4d]:", ibrch);
            continue;
        }

        /* get the values for the arguments */
        if (MODL->brch[ibrch].narg >= 1) {
            status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
            CATCH_STATUS(str2val:val1);
        } else {
            args[1] = 0;
        }
        if (MODL->brch[ibrch].narg >= 2) {
            status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
            CATCH_STATUS(str2val:val2);
        } else {
            args[2] = 0;
        }
        if (MODL->brch[ibrch].narg >= 3) {
            status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
            CATCH_STATUS(str2val:val3);
        } else {
            args[3] = 0;
        }
        if (MODL->brch[ibrch].narg >= 4) {
            status = str2val(MODL->brch[ibrch].arg4, MODL, &args[4]);
            CATCH_STATUS(str2val:val4);
        } else {
            args[4] = 0;
        }
        if (MODL->brch[ibrch].narg >= 5) {
            status = str2val(MODL->brch[ibrch].arg5, MODL, &args[5]);
            CATCH_STATUS(str2val:val5);
        } else {
            args[5] = 0;
        }
        if (MODL->brch[ibrch].narg >= 6) {
            status = str2val(MODL->brch[ibrch].arg6, MODL, &args[6]);
            CATCH_STATUS(str2val:val6);
        } else {
            args[6] = 0;
        }
        if (MODL->brch[ibrch].narg >= 7) {
            status = str2val(MODL->brch[ibrch].arg7, MODL, &args[7]);
            CATCH_STATUS(str2val:val7);
        } else {
            args[7] = 0;
        }
        if (MODL->brch[ibrch].narg >= 8) {
            status = str2val(MODL->brch[ibrch].arg8, MODL, &args[8]);
            CATCH_STATUS(str2val:val8);
        } else {
            args[8] = 0;
        }
        if (MODL->brch[ibrch].narg >= 9) {
            status = str2val(MODL->brch[ibrch].arg9, MODL, &args[9]);
            CATCH_STATUS(str2val:val8);
        } else {
            args[9] = 0;
        }

        /* execute Branch ibrch */
        if        (MODL->brch[ibrch].class == OCSM_PRIMITIVE) {
            status = buildPrimitive(MODL, ibrch, &nstack, stack, npatn, patn);
            CATCH_STATUS(buildPrimitive);

            status = setupAtPmtrs(MODL);
            CHECK_STATUS(setupAtPmtrs);

        } else if (MODL->brch[ibrch].class == OCSM_GROWN) {
            status = buildGrown(MODL, ibrch, &nstack, stack, npatn, patn);
            CATCH_STATUS(buildGrown);

            status = setupAtPmtrs(MODL);
            CHECK_STATUS(setupAtPmtrs);

        } else if (MODL->brch[ibrch].class == OCSM_APPLIED) {
            status = buildApplied(MODL, ibrch, &nstack, stack, npatn, patn);
            CATCH_STATUS(buildApplied);

            status = setupAtPmtrs(MODL);
            CHECK_STATUS(setupAtPmtrs);

        } else if (MODL->brch[ibrch].class == OCSM_BOOLEAN) {
            status = buildBoolean(MODL, ibrch, &nstack, stack);
            CATCH_STATUS(buildBoolean);

            status = setupAtPmtrs(MODL);
            CHECK_STATUS(setupAtPmtrs);

        } else if (MODL->brch[ibrch].class == OCSM_TRANSFORM) {
            status = buildTransform(MODL, ibrch, &nstack, stack);
            CATCH_STATUS(buildTransform);

            status = setupAtPmtrs(MODL);
            CHECK_STATUS(setupAtPmtrs);

        } else if (MODL->brch[ibrch].class == OCSM_SKETCH) {
            status = buildSketch(MODL, ibrch, &nstack, stack, npatn, patn,
                                 &nskpt, skpt);
            CATCH_STATUS(buildSketch);

        } else if (MODL->brch[ibrch].class == OCSM_SOLVER) {
            status = buildSolver(MODL, ibrch, &nsolcon, solcons);
            CATCH_STATUS(buildSolver);

        /* execute: "set pmtrName exprs" */
        } else if (type == OCSM_SET) {
            SPRINT3(1, "    executing [%4d] set:            %s  %s",
                    ibrch, &(MODL->brch[ibrch].arg1[1]),
                           &(MODL->brch[ibrch].arg2[1]));

            /* determine if a single- or multi-set (dependent on whether str2
               contains a semi-colon) */
            if (strstr(&(MODL->brch[ibrch].arg2[1]), ";") == NULL) {

                /* break arg1 into name[irow,icol] */
                pmtrName[0] = '\0';
                row[0]      = '\0';
                col[0]      = '\0';
                icount      = 0;
                jcount      = 0;

                for (i = 1; i < strlen(MODL->brch[ibrch].arg1); i++) {
                    if (icount == 0) {
                        if (MODL->brch[ibrch].arg1[i] != '[') {
                            pmtrName[jcount  ] = MODL->brch[ibrch].arg1[i];
                            pmtrName[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            jcount = 0;
                        }
                    } else if (icount == 1) {
                        if (MODL->brch[ibrch].arg1[i] != ',') {
                            row[jcount  ] = MODL->brch[ibrch].arg1[i];
                            row[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            jcount = 0;
                        }
                    } else {
                        if (MODL->brch[ibrch].arg1[i] != ']') {
                            col[jcount  ] = MODL->brch[ibrch].arg1[i];
                            col[jcount+1] = '\0';
                            jcount++;
                        } else {
                            icount++;
                            break;
                        }
                    }
                }

                if (icount != 0 && icount != 3) {
                    status = OCSM_ILLEGAL_PMTR_NAME;
                    CATCH_STATUS(set);
                }

                if (strlen(row) == 0) sprintf(row, "1");
                if (strlen(col) == 0) sprintf(col, "1");

                /* look for current Parameter */
                jpmtr = 0;
                for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                    if (strcmp(MODL->pmtr[ipmtr].name, pmtrName) == 0) {
                        if (MODL->pmtr[ipmtr].type != OCSM_EXTERNAL) {
                            MODL->pmtr[ipmtr].type =  OCSM_INTERNAL;
                        } else {
                            status = OCSM_PMTR_IS_EXTERNAL;
                            CATCH_STATUS(set);
                        }

                        jpmtr = ipmtr;
                        break;
                    }
                }

                /* create new Parameter (if needed) */
                if (jpmtr == 0) {
                    nrow = 1;
                    ncol = 1;

                    status = ocsmNewPmtr(MODL, pmtrName, OCSM_INTERNAL, nrow, ncol);
                    CATCH_STATUS(ocsmNewPmtr);

                    jpmtr = MODL->npmtr;
                }

                /* store the value */
                status = str2val(row, MODL, &rows);
                CATCH_STATUS(str2val:row);

                status = str2val(col, MODL, &cols);
                CATCH_STATUS(str2val:col);

                status = ocsmSetValu(MODL, jpmtr, NINT(rows), NINT(cols), &(MODL->brch[ibrch].arg2[1]));
                CATCH_STATUS(ocsmSetValu);

                status = ocsmGetValu(MODL, jpmtr, NINT(rows), NINT(cols), &value);
                CATCH_STATUS(ocsmGetValu);

                if (MODL->pmtr[jpmtr].nrow > 1 || MODL->pmtr[jpmtr].ncol > 1) {
                    SPRINT4(1, "                          %s[%d,%d] = %11.5f",
                            pmtrName, NINT(rows), NINT(cols), value);
                } else {
                    SPRINT2(1, "                          %s = %11.5f",
                            pmtrName, value);
                }

            /* multi-set mode */
            } else {

                /* look for current Parameter */
                jpmtr = 0;
                for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                    if (strcmp(MODL->pmtr[ipmtr].name, &(MODL->brch[ibrch].arg1[1])) == 0) {
                        jpmtr = ipmtr;
                        break;
                    }
                }

                /* Parameter must have been defined (in a dimension statement) */
                if (jpmtr <= 0) {
                    status = OCSM_NAME_NOT_FOUND;
                    CATCH_STATUS(set);
                } else if (MODL->pmtr[ipmtr].type != OCSM_INTERNAL) {
                    status = OCSM_PMTR_IS_EXTERNAL;
                    CATCH_STATUS(set);
                }

                /* store the values */
                icount = 1;
                for (irow = 1; irow <= MODL->pmtr[jpmtr].nrow; irow++) {
                    for (icol = 1; icol <= MODL->pmtr[jpmtr].ncol; icol++) {
                        jcount = 0;

                        while (icount < strlen(MODL->brch[ibrch].arg2)) {
                            if (MODL->brch[ibrch].arg2[icount] == ';') {
                                icount++;
                                break;
                            } else {
                                defn[jcount  ] = MODL->brch[ibrch].arg2[icount];
                                defn[jcount+1] = '\0';
                                icount++;
                                jcount++;
                            }
                        }

                        if (strcmp(defn, "") != 0) {
                            status = ocsmSetValu(MODL, jpmtr, irow, icol, defn);
                            CATCH_STATUS(ocsmSetValu);

                            status = ocsmGetValu(MODL, jpmtr, irow, icol, &value);
                            CATCH_STATUS(ocsmGetValu);

                            SPRINT4(1, "                          %s[%d,%d] = %11.5f",
                                    &(MODL->brch[ibrch].arg1[1]), irow, icol, value);
                        }
                    }
                }
            }

        /* execute: "macbeg imacro" */
        } else if (type == OCSM_MACBEG) {
            SPRINT2(1, "    executing [%4d] macbeg:     %11.5f",
                    ibrch, args[1]);

            /* determine the storage location */
            imacro = NINT(args[1]);

            if (imacro < 1 || imacro > MAX_NUM_MACROS) {
                status = OCSM_ILLEGAL_MACRO_INDEX;
                CATCH_STATUS(macbeg);
            }

            /* an error if storage location is not empty */
            if (macros[imacro] > 0) {
                status = OCSM_STORAGE_ALREADY_USED;
                CATCH_STATUS(macbeg);
            }


            macros[imacro] = ibrch;

            SPRINT2(1, "                          Storing Branch %4d in storage %d", ibrch, imacro);

        /* execute: "macend" */
        } else if (type == OCSM_MACEND) {
            SPRINT1(1, "    executing [%4d] macend:",
                    ibrch);

            npatn--;
            ibrch = patn[npatn].ipatend;

        /* execute: "recall imacro" */
        } else if (type == OCSM_RECALL) {
            SPRINT2(1, "    executing [%4d] recall:     %11.5f",
                    ibrch, args[1]);

            /* determine the storage location */
            imacro = NINT(args[1]);

            if (imacro < 1 || imacro > MAX_NUM_MACROS) {
                status = OCSM_ILLEGAL_MACRO_INDEX;
                CATCH_STATUS(recall);
            } else if (macros[imacro] <= 0) {
                status = OCSM_NOTHING_PREVIOUSLY_STORED;
                CATCH_STATUS(recall);
            }

            patn[npatn].ipatbeg = macros[imacro];
            patn[npatn].ipatend = ibrch;
            patn[npatn].ncopy   =  1;
            patn[npatn].icopy   =  1;
            patn[npatn].ipmtr   = -1;
            npatn++;

            /* start executing just after the macbeg */
            ibrch = macros[imacro];

            SPRINT1(1, "                          Entering storage %d", imacro);

        /* execute: "patbeg" */
        } else if (type == OCSM_PATBEG) {
            SPRINT3(1, "    executing [%4d] patbeg:         %s %11.5f",
                    ibrch, &(MODL->brch[ibrch].arg1[1]), args[2]);

            if (npatn >= MAX_NUM_PATTERNS) {
                status = OCSM_PATTERNS_NESTED_TOO_DEEPLY;
                CATCH_STATUS(patbeg);
            }

            /* store info about this pattern */
            patn[npatn].ipatbeg = ibrch;
            patn[npatn].ipatend = -1;
            patn[npatn].icopy   = 1;
            patn[npatn].ncopy   = NINT(args[2]);
            patn[npatn].ipmtr   = -1;

            /* find matching patend */
            i = 1;
            for (ibrchl = ibrch+1; ibrchl <= MODL->nbrch; ibrchl++) {
                if        (MODL->brch[ibrchl].type == OCSM_PATBEG) {
                    i++;
                } else if (MODL->brch[ibrchl].type == OCSM_PATEND) {
                    i--;

                    if (i == 0) {
                        patn[npatn].ipatend = ibrchl;
                        break;
                    }
                }
            }
            if (patn[npatn].ipatbeg < 0) {
                status = OCSM_PATBEG_WITHOUT_PATEND;
                CATCH_STATUS(patbeg);
            }

            /* find the pmtrName */
            for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                if (strcmp(MODL->pmtr[ipmtr].name, &(MODL->brch[ibrch].arg1[1])) == 0) {
                    patn[npatn].ipmtr = ipmtr;
                    break;
                }
            }
            if (patn[npatn].ipmtr < 0) {
                status = OCSM_NAME_NOT_FOUND;
                CATCH_STATUS(patbeg);
            }

            /* if no copies are required, jump to Branch after patend */
            if (patn[npatn].ncopy < 1) {
                ibrch = patn[npatn].ipatend;

            /* otherwise increment the number of patterns and continue */
            } else {
                irow = icol = 1;

                status = ocsmSetValu(MODL, patn[npatn].ipmtr, irow, icol, "1");
                CATCH_STATUS(ocsmSetValu);

                SPRINT2(1, "                          %s = %3d",
                        MODL->pmtr[patn[npatn].ipmtr].name, 1);

                npatn++;
            }

        /* execute: "patend" */
        } else if (type == OCSM_PATEND) {
            SPRINT1(1, "    executing [%4d] patend:",
                    ibrch);

            /* increment the iterator */
            (patn[npatn-1].icopy)++;

            /* go back for another copy */
            if (patn[npatn-1].icopy <= patn[npatn-1].ncopy) {
                sprintf(newValue, "%5d", patn[npatn-1].icopy);

                irow = icol = 1;

                status = ocsmSetValu(MODL, patn[npatn-1].ipmtr, irow, icol, newValue);
                CATCH_STATUS(ocsmSetValu);

                ibrch = patn[npatn-1].ipatbeg;

                SPRINT2(1, "                          %s = %3d",
                        MODL->pmtr[patn[npatn-1].ipmtr].name, patn[npatn-1].icopy);

            /* otherwise, we are finished with the pattern */
            } else {
                npatn--;
            }

        /* execute: "mark" */
        } else if (type == OCSM_MARK) {
            SPRINT1(1, "    executing [%4d] mark:",
                    ibrch);

            /* push a Mark onto the stack */
            stack[nstack++] = 0;

            SPRINT0(1, "                          Mark        created");

        /* execute: "dump filename remove=0" */
        } else if (type == OCSM_DUMP) {
            SPRINT3(1, "    executing [%4d] dump:       %s %11.5f",
                    ibrch, &(MODL->brch[ibrch].arg1[1]), args[2]);

            /* pop a Body from the stack */
            if (nstack < 1) {
                status = OCSM_EXPECTING_ONE_BODY;
                CATCH_STATUS(dump);
            } else {
                ibodyl = stack[--nstack];
            }

            /* make a model and dump the Body */
            #if   defined(GEOM_CAPRI)
                SPRINT0(0, "WARNING:: dump not supported in CAPRI");

            #elif defined(GEOM_EGADS)
                status = EG_copyObject(MODL->body[ibodyl].ebody, NULL, &etemp);
                CATCH_STATUS(EG_copyObject);

                ebodyl = MODL->body[ibodyl].ebody;

                status = EG_makeTopology(MODL->context, NULL, MODEL, 0,
                                         NULL, 1, &etemp, NULL, &emodel);
                CATCH_STATUS(EG_makeTopology);

                /* strip the dollarsign off the filename */
                strcpy(dumpfile, &(MODL->brch[ibrch].arg1[1]));

                /* if file exists, delete it now */
                status = remove(dumpfile);
                if (status == 0) {
                    SPRINT1(0, "WARNING:: file \"%s\" is being overwritten", dumpfile);
                }

                status = EG_saveModel(emodel, dumpfile);
                CATCH_STATUS(EG_saveModel);

                status = EG_deleteObject(emodel);
                CATCH_STATUS(EG_deleteObject);

                status = EG_copyObject(ebodyl, NULL, &ebody);
                CATCH_STATUS(EG_copyObject);
            #endif

            SPRINT1(1, "                          Body   %4d dumped", ibodyl);

            /* push the Body back onto the stack (if it should not be removed) */
            if (NINT(args[2]) == 1) {
                SPRINT1(1, "                          Body   %4d removed", ibodyl);
            } else {
                stack[nstack++] = ibodyl;
            }
        }

        /* record that we successfully executed this Branch */
        *builtTo = ibrch;
    }

finalize:
    SPRINT0(1, "    finalizing:");

    /* special processing if an error was caught */
    if (ncatch > 0) {
        buildStatus = status;

        /* close the sketcher */
        nskpt = 0;

        /* return negative of builtTo */
        (*builtTo) *= -1;
    } else {
        buildStatus = SUCCESS;
    }

    /* free up the sketches */
    #if   defined(GEOM_CAPRI)
        for (i = nsketch; i >= 1; i--) {
            status = gi_sDelete(i);
            CHECK_STATUS(gi_sDelete);
        }
    #elif defined(GEOM_EGADS)
    #endif

    /* make sure the sketcher is not open */
    if (nskpt > 0) {
        status = OCSM_SKETCHER_IS_OPEN;
        CHECK_STATUS(end);
    }

    /* make sure that user-supplied body[] is big enough */
    if (nstack > nbodyMax) {
        status = OCSM_TOO_MANY_BODYS_ON_STACK;
        goto cleanup;
    }

    /* pre-mark all the Bodys as not being on the stack (fixed below) */
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        MODL->body[ibody].onstack = 0;
    }

    /* report a warning if there is a mark or NODE_BODY on the stack */
    for (i = 0; i < nstack; i++) {
        ibody = stack[i];
        if (ibody == 0) {
            SPRINT0(0, "WARNING:: mark being removed from the stack");

            for (j = i+1; j < nstack; j++) {
                stack[j-1] = stack[j];
            }
            nstack--;

            i--;  /* repeat in case first moved stack entry was also a mark */
        } else if (ibody > 0) {
            if (MODL->body[ibody].botype == OCSM_NODE_BODY) {
                SPRINT0(0, "WARNING:: node body being removed from the stack");

                for (j = i+1; j < nstack; j++) {
                    stack[j-1] = stack[j];
                }
                nstack--;

                i--;  /* repeat in case first moved stack entry was also a mark */
            }
        }
    }

    /* mark the stack's Bodys */
    for (i = 0; i < nstack; i++) {
        MODL->body[stack[i]].onstack = 1;
    }

    /* delete the Bodys that are not on the stack */
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        for (ibody = 1; ibody <= MODL->nbody; ibody++) {
            if (MODL->body[ibody].onstack == 0) {
                if (MODL->body[ibody].ebody != NULL) {
                    status = EG_deleteObject(MODL->body[ibody].ebody);
                    if (status == EGADS_EMPTY) status = SUCCESS;
                    CHECK_STATUS(EG_deleteObject);
                }
            }
        }
    #endif

    /* return Bodys on the stack (LIFO) */
    *nbody = nstack;

    for (i = 0; i < *nbody; i++) {
        body[i] = stack[--nstack];

        #if   defined(GEOM_CAPRI)
            toler = 0;
        #elif defined(GEOM_EGADS)
            status = getBodyTolerance(MODL->body[body[i]].ebody, &toler);
            CHECK_STATUS(getBodyTolerance);
        #endif

        SPRINT5(1, "    Body %5d   nnode=%-6d   nedge=%-6d   nface=%-6d   toler=%11.4e", body[i],
                MODL->body[body[i]].nnode,
                MODL->body[body[i]].nedge,
                MODL->body[body[i]].nface, toler);

        if (outLevel >= 3) {
            status = printBodyAttributes(MODL, body[i]);
            CHECK_STATUS(printBodyAttributes);
        }
    }

cleanup:
#undef CATCH_STATUS
    /* if buildStatus is not success, return it */
    if (buildStatus != SUCCESS) {
        status = buildStatus;
    }

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmNewBrch - create a new Branch                                  *
 *                                                                      *
 ************************************************************************
 */

int
ocsmNewBrch(void   *modl,               /* (in)  pointer to MODL */
            int    iafter,              /* (in)  Branch index (0-nbrch) after which to add */
            int    type,                /* (in)  Branch type (see below) */
  /*@null@*/char   arg1[],              /* (in)  Argument 1 (or NULL) */
  /*@null@*/char   arg2[],              /* (in)  Argument 2 (or NULL) */
  /*@null@*/char   arg3[],              /* (in)  Argument 3 (or NULL) */
  /*@null@*/char   arg4[],              /* (in)  Argument 4 (or NULL) */
  /*@null@*/char   arg5[],              /* (in)  Argument 5 (or NULL) */
  /*@null@*/char   arg6[],              /* (in)  Argument 6 (or NULL) */
  /*@null@*/char   arg7[],              /* (in)  Argument 7 (or NULL) */
  /*@null@*/char   arg8[],              /* (in)  Argument 8 (or NULL) */
  /*@null@*/char   arg9[])              /* (in)  Argument 9 (or NULL) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       class, narg, ibrch, jbrch, ipmtr, jpmtr, nrow, ncol, i;

    char      pmtrName[MAX_EXPR_LEN];

    ROUTINE(ocsmNewBrch);
    DPRINT3("%s(iafter=%d, type=%d) {",
            routine, iafter, type);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* make sure valid ibrch was given */
    if (iafter < 0 || iafter > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* check that valid type is given (and determine class) */
    if        (type == OCSM_BOX) {
        class = OCSM_PRIMITIVE;
        narg  = 6;
    } else if (type == OCSM_SPHERE) {
        class = OCSM_PRIMITIVE;
        narg  = 4;
    } else if (type == OCSM_CONE) {
        class = OCSM_PRIMITIVE;
        narg  = 7;
    } else if (type == OCSM_CYLINDER) {
        class = OCSM_PRIMITIVE;
        narg  = 7;
    } else if (type == OCSM_TORUS) {
        class = OCSM_PRIMITIVE;
        narg  = 8;
    } else if (type == OCSM_IMPORT) {
        class = OCSM_PRIMITIVE;
        narg  = 1;
    } else if (type == OCSM_UDPRIM) {
        class = OCSM_PRIMITIVE;
        narg  = 1;
        if (arg2 != NULL && arg3 != NULL) narg = 3;
        if (arg4 != NULL && arg5 != NULL) narg = 5;
        if (arg6 != NULL && arg7 != NULL) narg = 7;
        if (arg8 != NULL && arg9 != NULL) narg = 9;
    } else if (type == OCSM_EXTRUDE) {
        class = OCSM_GROWN;
        narg  = 3;
    } else if (type == OCSM_REVOLVE) {
        class = OCSM_GROWN;
        narg  = 7;
    } else if (type == OCSM_LOFT) {
        class = OCSM_GROWN;
        narg  = 1;
    } else if (type == OCSM_FILLET) {
        class = OCSM_APPLIED;
        narg  = 2;
    } else if (type == OCSM_CHAMFER) {
        class = OCSM_APPLIED;
        narg  = 2;
    } else if (type == OCSM_HOLLOW) {
        class = OCSM_APPLIED;
        narg  = 7;
    } else if (type == OCSM_INTERSECT) {
        class = OCSM_BOOLEAN;
        narg  = 2;
    } else if (type == OCSM_SUBTRACT) {
        class = OCSM_BOOLEAN;
        narg  = 2;
    } else if (type == OCSM_UNION) {
        class = OCSM_BOOLEAN;
        narg  = 0;
    } else if (type == OCSM_TRANSLATE) {
        class = OCSM_TRANSFORM;
        narg  = 3;
    } else if (type == OCSM_ROTATEX) {
        class = OCSM_TRANSFORM;
        narg  = 3;
    } else if (type == OCSM_ROTATEY) {
        class = OCSM_TRANSFORM;
        narg  = 3;
    } else if (type == OCSM_ROTATEZ) {
        class = OCSM_TRANSFORM;
        narg  = 3;
    } else if (type == OCSM_SCALE) {
        class = OCSM_TRANSFORM;
        narg  = 1;
    } else if (type == OCSM_SKBEG) {
        class = OCSM_SKETCH;
        narg  = 3;
    } else if (type == OCSM_LINSEG) {
        class = OCSM_SKETCH;
        narg  = 3;
    } else if (type == OCSM_CIRARC) {
        class = OCSM_SKETCH;
        narg  = 6;
    } else if (type == OCSM_SPLINE) {
        class = OCSM_SKETCH;
        narg  = 3;
    } else if (type == OCSM_SKEND) {
        class = OCSM_SKETCH;
        narg  = 0;
    } else if (type == OCSM_SOLBEG) {
        class = OCSM_SOLVER;
        narg  = 1;
    } else if (type == OCSM_SOLCON) {
        class = OCSM_SOLVER;
        narg  = 1;
    } else if (type == OCSM_SOLEND) {
        class = OCSM_SOLVER;
        narg  = 0;
    } else if (type == OCSM_SET) {
        class = OCSM_UTILITY;
        narg  = 2;
    } else if (type == OCSM_MACBEG) {
        class = OCSM_UTILITY;
        narg  = 1;
    } else if (type == OCSM_MACEND) {
        class = OCSM_UTILITY;
        narg  = 0;
    } else if (type == OCSM_RECALL) {
        class = OCSM_UTILITY;
        narg  = 1;
    } else if (type == OCSM_PATBEG) {
        class = OCSM_UTILITY;
        narg  = 2;
    } else if (type == OCSM_PATEND) {
        class = OCSM_UTILITY;
        narg  = 0;
    } else if (type == OCSM_MARK) {
        class = OCSM_UTILITY;
        narg  = 0;
    } else if (type == OCSM_DUMP) {
        class = OCSM_UTILITY;
        narg  = 2;
    } else {
        status = OCSM_ILLEGAL_TYPE;
        goto cleanup;
    }

    /* mark MODL as not being checked */
    MODL->checked = 0;

    /* increment the number of Branches */
    (MODL->nbrch)++;
    ibrch = iafter + 1;

    /* extend the Branch list (if needed) */
    if (MODL->nbrch > MODL->mbrch) {
        MODL->mbrch += 25;
        if (MODL->brch == NULL) {
            MALLOC(MODL->brch, brch_T, MODL->mbrch+1);
        } else {
            RALLOC(MODL->brch, brch_T, MODL->mbrch+1);
        }
    }

    /* copy the Branches down to make room for the new Branch */
    if (ibrch < MODL->nbrch) {
        for (jbrch = MODL->nbrch; jbrch > ibrch; jbrch--) {
            MODL->brch[jbrch].name  = MODL->brch[jbrch-1].name;
            MODL->brch[jbrch].type  = MODL->brch[jbrch-1].type;
            MODL->brch[jbrch].class = MODL->brch[jbrch-1].class;
            MODL->brch[jbrch].actv  = MODL->brch[jbrch-1].actv;
            MODL->brch[jbrch].nattr = MODL->brch[jbrch-1].nattr;
            MODL->brch[jbrch].attr  = MODL->brch[jbrch-1].attr;
            MODL->brch[jbrch].ileft = MODL->brch[jbrch-1].ileft;
            MODL->brch[jbrch].irite = MODL->brch[jbrch-1].irite;
            MODL->brch[jbrch].ichld = MODL->brch[jbrch-1].ichld;
            MODL->brch[jbrch].narg  = MODL->brch[jbrch-1].narg;
            MODL->brch[jbrch].arg1  = MODL->brch[jbrch-1].arg1;
            MODL->brch[jbrch].arg2  = MODL->brch[jbrch-1].arg2;
            MODL->brch[jbrch].arg3  = MODL->brch[jbrch-1].arg3;
            MODL->brch[jbrch].arg4  = MODL->brch[jbrch-1].arg4;
            MODL->brch[jbrch].arg5  = MODL->brch[jbrch-1].arg5;
            MODL->brch[jbrch].arg6  = MODL->brch[jbrch-1].arg6;
            MODL->brch[jbrch].arg7  = MODL->brch[jbrch-1].arg7;
            MODL->brch[jbrch].arg8  = MODL->brch[jbrch-1].arg8;
            MODL->brch[jbrch].arg9  = MODL->brch[jbrch-1].arg9;
        }
    }

    /* initialize the new Branch */
    MODL->brch[ibrch].name  = NULL;
    MODL->brch[ibrch].type  = type;
    MODL->brch[ibrch].class = class;
    MODL->brch[ibrch].actv  =  OCSM_ACTIVE;
    MODL->brch[ibrch].nattr = 0;
    MODL->brch[ibrch].attr  = NULL;
    MODL->brch[ibrch].ileft = -1;
    MODL->brch[ibrch].irite = -1;
    MODL->brch[ibrch].ichld = -1;
    MODL->brch[ibrch].narg  =  narg;
    MODL->brch[ibrch].arg1  =  NULL;
    MODL->brch[ibrch].arg2  =  NULL;
    MODL->brch[ibrch].arg3  =  NULL;
    MODL->brch[ibrch].arg4  =  NULL;
    MODL->brch[ibrch].arg5  =  NULL;
    MODL->brch[ibrch].arg6  =  NULL;
    MODL->brch[ibrch].arg7  =  NULL;
    MODL->brch[ibrch].arg8  =  NULL;
    MODL->brch[ibrch].arg9  =  NULL;

    /* default name for the Branch */
    MALLOC( MODL->brch[ibrch].name, char, 12);
    sprintf(MODL->brch[ibrch].name, "Brch_%06d", MODL->nextseq);
    (MODL->nextseq)++;

    /* save the Arguments */
    if (narg >= 1) {
        if (arg1 != NULL && strlen(arg1) > 0) {
            MALLOC(MODL->brch[ibrch].arg1, char, (int)(strlen(arg1)+1));
            strcpy(MODL->brch[ibrch].arg1,                    arg1    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 2) {
        if (arg2 != NULL && strlen(arg2) > 0) {
            MALLOC(MODL->brch[ibrch].arg2, char, (int)(strlen(arg2)+1));
            strcpy(MODL->brch[ibrch].arg2,                    arg2    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 3) {
        if (arg3 != NULL && strlen(arg3) > 0) {
            MALLOC(MODL->brch[ibrch].arg3, char, (int)(strlen(arg3)+1));
            strcpy(MODL->brch[ibrch].arg3,                    arg3    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 4) {
        if (arg4 != NULL && strlen(arg4) > 0) {
            MALLOC(MODL->brch[ibrch].arg4, char, (int)(strlen(arg4)+1));
            strcpy(MODL->brch[ibrch].arg4,                    arg4    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 5) {
        if (arg5 != NULL && strlen(arg5) > 0) {
            MALLOC(MODL->brch[ibrch].arg5, char, (int)(strlen(arg5)+1));
            strcpy(MODL->brch[ibrch].arg5,                    arg5    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 6) {
        if (arg6 != NULL && strlen(arg6) > 0) {
            MALLOC(MODL->brch[ibrch].arg6, char, (int)(strlen(arg6)+1));
            strcpy(MODL->brch[ibrch].arg6,                    arg6    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 7) {
        if (arg7 != NULL && strlen(arg7) > 0) {
            MALLOC(MODL->brch[ibrch].arg7, char, (int)(strlen(arg7)+1));
            strcpy(MODL->brch[ibrch].arg7,                    arg7    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 8) {
        if (arg8 != NULL && strlen(arg8) > 0) {
            MALLOC(MODL->brch[ibrch].arg8, char, (int)(strlen(arg8)+1));
            strcpy(MODL->brch[ibrch].arg8,                    arg8    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    if (narg >= 9) {
        if (arg9 != NULL && strlen(arg9) > 0) {
            MALLOC(MODL->brch[ibrch].arg9, char, (int)(strlen(arg9)+1));
            strcpy(MODL->brch[ibrch].arg9,                    arg9    );
        } else {
            status = OCSM_ILLEGAL_NARG;
        }
    }

    /* make Parameter if set or patbeg statement */
    if (type == OCSM_SET || type == OCSM_PATBEG) {

        /* pull Parameter name out of arg1 */
        pmtrName[0] = '\0';
        for (i = 1; i < strlen(arg1); i++) {
            if (arg1[i] != '[') {
                pmtrName[i-1] = arg1[i];
                pmtrName[i  ] = '\0';
            } else {
                break;
            }
        }

        /* check that Parameter exists */
        jpmtr = 0;
        for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
            if (strcmp(MODL->pmtr[ipmtr].name, pmtrName) == 0) {
                jpmtr = ipmtr;
                break;
            }
        }

        /* if it does not exist, create it now */
        if (jpmtr == 0) {
            nrow = ncol = 1;

            status = ocsmNewPmtr(MODL, pmtrName, OCSM_INTERNAL, nrow, ncol);
            CHECK_STATUS(ocsmNewPmtr);

            jpmtr = MODL->npmtr;
        }
    }

cleanup:
    if (status < SUCCESS) {
        MODL->nbrch--;
    }

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetBrch - get info about a Branch                              *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetBrch(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            int    *type,               /* (out) Branch type */
            int    *class,              /* (out) Branch class */
            int    *actv,               /* (out) Branch Activity */
            int    *ichld,              /* (out) ibrch of child (or 0 if root) */
            int    *ileft,              /* (out) ibrch of left parent (or 0) */
            int    *irite,              /* (out) ibrch of rite parent (or 0) */
            int    *narg,               /* (out) number of Arguments */
            int    *nattr)              /* (out) number of Attributes */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmGetBrch);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    /* default return values */
    *type  = 0;
    *class = 0;
    *actv  = 0;
    *ichld = 0;
    *ileft = 0;
    *irite = 0;
    *narg  = 0;
    *nattr = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* return pertinent information */
    *type  = MODL->brch[ibrch].type;
    *class = MODL->brch[ibrch].class;
    *actv  = MODL->brch[ibrch].actv;
    *ichld = MODL->brch[ibrch].ichld;
    *ileft = MODL->brch[ibrch].ileft;
    *irite = MODL->brch[ibrch].irite;
    *narg  = MODL->brch[ibrch].narg;
    *nattr = MODL->brch[ibrch].nattr;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetBrch - set activity for a Branch                            *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSetBrch(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            int    actv)                /* (in)  Branch activity */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmSetBrch);
    DPRINT3("%s(ibrch=%d, actv=%d) {",
            routine, ibrch, actv);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* check that a valid activity was givem */
    if (actv != OCSM_ACTIVE    &&
        actv != OCSM_SUPPRESSED  ) {
        status = OCSM_ILLEGAL_ACTIVITY;
        goto cleanup;
    }

    /* check that branch type can be suppressed */
    if (MODL->brch[ibrch].class != OCSM_PRIMITIVE &&
        MODL->brch[ibrch].class != OCSM_GROWN     &&
        MODL->brch[ibrch].class != OCSM_APPLIED   &&
        MODL->brch[ibrch].class != OCSM_TRANSFORM   ) {
        status = OCSM_CANNOT_BE_SUPPRESSED;
        goto cleanup;
    }

    /* save the activity */
    MODL->brch[ibrch].actv = actv;

    /* mark MODL as not being checked */
    MODL->checked = 0;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmDelBrch - delete the last Branch                               *
 *                                                                      *
 ************************************************************************
 */

int
ocsmDelBrch(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch)               /* (in)  Branch index (1-nbrch) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       jbrch, iattr;

    ROUTINE(ocsmDelBrch);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check for valid Branch index */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* mark MODL as not being checked */
    MODL->checked = 0;

    /* free up the storage associated with ibrch */
    for (iattr = 0; iattr < MODL->brch[ibrch].nattr; iattr++) {
        FREE(MODL->brch[ibrch].attr[iattr].name );
        FREE(MODL->brch[ibrch].attr[iattr].value);
    }

    FREE(MODL->brch[ibrch].name);
    FREE(MODL->brch[ibrch].attr);
    FREE(MODL->brch[ibrch].arg1);
    FREE(MODL->brch[ibrch].arg2);
    FREE(MODL->brch[ibrch].arg3);
    FREE(MODL->brch[ibrch].arg4);
    FREE(MODL->brch[ibrch].arg5);
    FREE(MODL->brch[ibrch].arg6);
    FREE(MODL->brch[ibrch].arg7);
    FREE(MODL->brch[ibrch].arg8);
    FREE(MODL->brch[ibrch].arg9);

    /* move all Branches up to write over deleted Branch */
    for (jbrch = ibrch; jbrch < MODL->nbrch; jbrch++) {
        MODL->brch[jbrch].name  = MODL->brch[jbrch+1].name;
        MODL->brch[jbrch].type  = MODL->brch[jbrch+1].type;
        MODL->brch[jbrch].class = MODL->brch[jbrch+1].class;
        MODL->brch[jbrch].actv  = MODL->brch[jbrch+1].actv;
        MODL->brch[jbrch].nattr = MODL->brch[jbrch+1].nattr;
        MODL->brch[jbrch].attr  = MODL->brch[jbrch+1].attr;
        MODL->brch[jbrch].ileft = MODL->brch[jbrch+1].ileft;
        MODL->brch[jbrch].irite = MODL->brch[jbrch+1].irite;
        MODL->brch[jbrch].ichld = MODL->brch[jbrch+1].ichld;
        MODL->brch[jbrch].narg  = MODL->brch[jbrch+1].narg;
        MODL->brch[jbrch].arg1  = MODL->brch[jbrch+1].arg1;
        MODL->brch[jbrch].arg2  = MODL->brch[jbrch+1].arg2;
        MODL->brch[jbrch].arg3  = MODL->brch[jbrch+1].arg3;
        MODL->brch[jbrch].arg4  = MODL->brch[jbrch+1].arg4;
        MODL->brch[jbrch].arg5  = MODL->brch[jbrch+1].arg5;
        MODL->brch[jbrch].arg6  = MODL->brch[jbrch+1].arg6;
        MODL->brch[jbrch].arg7  = MODL->brch[jbrch+1].arg7;
        MODL->brch[jbrch].arg8  = MODL->brch[jbrch+1].arg8;
        MODL->brch[jbrch].arg9  = MODL->brch[jbrch+1].arg9;
    }

    /* decrement the number of Branches */
    (MODL->nbrch)--;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmPrintBrchs - print Branches to file                            *
 *                                                                      *
 ************************************************************************
 */

int
ocsmPrintBrchs(void   *modl,            /* (in)  pointer to MODL */
               FILE   *fp)              /* (in)  pointer to FILE */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ibrch, i, nindent, iattr;

    ROUTINE(ocsmPrintBrchs);
    DPRINT2("%s(fp=%x) {",
            routine, (int)fp);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    DPRINT0("ibrch  type  actv ileft irite ichld narg  nattr");
    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        DPRINT8("%5d %5d %5d %5d %5d %5d %5d %5d", ibrch,
                MODL->brch[ibrch].type,
                MODL->brch[ibrch].actv,
                MODL->brch[ibrch].ileft,
                MODL->brch[ibrch].irite,
                MODL->brch[ibrch].ichld,
                MODL->brch[ibrch].narg,
                MODL->brch[ibrch].nattr);
    }

    fprintf(fp, "ibrch                    type             ileft irite ichld args...\n");
    nindent = 0;

    for (ibrch = 1; ibrch <= MODL->nbrch; ibrch++) {
        if (MODL->brch[ibrch].type == OCSM_MACEND ||
            MODL->brch[ibrch].type == OCSM_PATEND ||
            MODL->brch[ibrch].type == OCSM_SKEND  ||
            MODL->brch[ibrch].type == OCSM_SOLEND ||
            MODL->brch[ibrch].type == OCSM_LOFT     ) {
            nindent--;
        }

        fprintf(fp, "%5d", ibrch);

        if (MODL->brch[ibrch].actv == OCSM_ACTIVE    ) fprintf(fp, " [a] ");
        if (MODL->brch[ibrch].actv == OCSM_SUPPRESSED) fprintf(fp, " [s] ");
        if (MODL->brch[ibrch].actv == OCSM_INACTIVE  ) fprintf(fp, " [i] ");
        if (MODL->brch[ibrch].actv == OCSM_DEFERRED  ) fprintf(fp, " [d] ");

        fprintf(fp, " %-14s", MODL->brch[ibrch].name);

        for (i = 0; i < nindent; i++) {
            fprintf(fp, ".");
        }

        fprintf(fp, "%-9s", ocsmGetText(MODL->brch[ibrch].type));

        for (i = nindent; i < 8; i++) {
            fprintf(fp, " ");
        }

        fprintf(fp, "%5d %5d %5d", MODL->brch[ibrch].ileft,
                                   MODL->brch[ibrch].irite,
                                   MODL->brch[ibrch].ichld);

        if (MODL->brch[ibrch].narg >= 1) {
            if (MODL->brch[ibrch].arg1[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg1    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg1[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 2) {
            if (MODL->brch[ibrch].arg2[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg2    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg2[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 3) {
            if (MODL->brch[ibrch].arg3[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg3    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg3[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 4) {
            if (MODL->brch[ibrch].arg4[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg4    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg4[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 5) {
            if (MODL->brch[ibrch].arg5[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg5    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg5[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 6) {
            if (MODL->brch[ibrch].arg6[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg6    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg6[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 7) {
            if (MODL->brch[ibrch].arg7[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg7    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg7[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 8) {
            if (MODL->brch[ibrch].arg8[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg8    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg8[1]));
            }
        }
        if (MODL->brch[ibrch].narg >= 9) {
            if (MODL->brch[ibrch].arg9[0] != '$') {
                fprintf(fp, " [%s]",   MODL->brch[ibrch].arg9    );
            } else {
                fprintf(fp, " [%s]", &(MODL->brch[ibrch].arg9[1]));
            }
        }

        fprintf(fp, "\n");

        for (iattr = 0; iattr < MODL->brch[ibrch].nattr; iattr++) {
            fprintf(fp, "                                                            %-20s %-20s\n",
                        MODL->brch[ibrch].attr[iattr].name,
                        MODL->brch[ibrch].attr[iattr].value);
        }

        if (MODL->brch[ibrch].type == OCSM_MACBEG ||
            MODL->brch[ibrch].type == OCSM_PATBEG ||
            MODL->brch[ibrch].type == OCSM_SKBEG  ||
            MODL->brch[ibrch].type == OCSM_SOLBEG ||
            MODL->brch[ibrch].type == OCSM_MARK     ) {
            nindent++;
        }
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetArg - get an Argument for a Branch                          *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetArg(void   *modl,                /* (in)  pointer to MODL */
           int    ibrch,                /* (in)  Branch index (1-nbrch) */
           int    iarg,                 /* (in)  Argument index (1-narg) */
           char   defn[],               /* (out) Argument definition (at least 129 long) */
           double *value)               /* (out) Argument value */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmGetArg);
    DPRINT3("%s(ibrch=%d, iarg=%d) {",
            routine, ibrch, iarg);

    /* --------------------------------------------------------------- */

    /* default return values */
    defn[0] = '\0';
    *value  = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* check that valid iarg is given */
    if (iarg < 1 || iarg > MODL->brch[ibrch].narg) {
        status = OCSM_ILLEGAL_ARG_INDEX;
        goto cleanup;
    }

    /* return the definition and current value */
    if (iarg == 1) strcpy(defn, MODL->brch[ibrch].arg1);
    if (iarg == 2) strcpy(defn, MODL->brch[ibrch].arg2);
    if (iarg == 3) strcpy(defn, MODL->brch[ibrch].arg3);
    if (iarg == 4) strcpy(defn, MODL->brch[ibrch].arg4);
    if (iarg == 5) strcpy(defn, MODL->brch[ibrch].arg5);
    if (iarg == 6) strcpy(defn, MODL->brch[ibrch].arg6);
    if (iarg == 7) strcpy(defn, MODL->brch[ibrch].arg7);
    if (iarg == 8) strcpy(defn, MODL->brch[ibrch].arg8);
    if (iarg == 9) strcpy(defn, MODL->brch[ibrch].arg9);

    status = str2val(defn, MODL, value);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetArg - set an Argument for a Branch                          *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSetArg(void   *modl,                /* (in)  pointer to MODL */
           int    ibrch,                /* (in)  Branch index (1-nbrch) */
           int    iarg,                 /* (in)  Argument index (1-narg) */
           char   defn[])               /* (in)  Argument definition */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmSetArg);
    DPRINT4("%s(ibrch=%d, iarg=%d, defn=%s) {",
            routine, ibrch, iarg, defn);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* check that valid iarg is given */
    if (iarg < 1 || iarg > MODL->brch[ibrch].narg) {
        status = OCSM_ILLEGAL_ARG_INDEX;
        goto cleanup;
    }

    /* save the definition */
    if        (iarg == 1) {
        if (strcmp(defn, MODL->brch[ibrch].arg1) != 0) {
            FREE(  MODL->brch[ibrch].arg1);
            MALLOC(MODL->brch[ibrch].arg1, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg1,                    defn    );
        }
    } else if (iarg == 2) {
        if (strcmp(defn, MODL->brch[ibrch].arg2) != 0) {
            FREE(  MODL->brch[ibrch].arg2);
            MALLOC(MODL->brch[ibrch].arg2, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg2,                    defn    );
        }
    } else if (iarg == 3) {
        if (strcmp(defn, MODL->brch[ibrch].arg3) != 0) {
            FREE(  MODL->brch[ibrch].arg3);
            MALLOC(MODL->brch[ibrch].arg3, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg3,                    defn    );
        }
    } else if (iarg == 4) {
        if (strcmp(defn, MODL->brch[ibrch].arg4) != 0) {
            FREE(  MODL->brch[ibrch].arg4);
            MALLOC(MODL->brch[ibrch].arg4, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg4,                    defn    );
        }
    } else if (iarg == 5) {
        if (strcmp(defn, MODL->brch[ibrch].arg5) != 0) {
            FREE(  MODL->brch[ibrch].arg5);
            MALLOC(MODL->brch[ibrch].arg5, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg5,                    defn    );
        }
    } else if (iarg == 6) {
        if (strcmp(defn, MODL->brch[ibrch].arg6) != 0) {
            FREE(  MODL->brch[ibrch].arg6);
            MALLOC(MODL->brch[ibrch].arg6, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg6,                    defn    );
        }
    } else if (iarg == 7) {
        if (strcmp(defn, MODL->brch[ibrch].arg7) != 0) {
            FREE(  MODL->brch[ibrch].arg7);
            MALLOC(MODL->brch[ibrch].arg7, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg7,                    defn    );
        }
    } else if (iarg == 8) {
        if (strcmp(defn, MODL->brch[ibrch].arg8) != 0) {
            FREE(  MODL->brch[ibrch].arg8);
            MALLOC(MODL->brch[ibrch].arg8, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg8,                    defn    );
        }
    } else if (iarg == 9) {
        if (strcmp(defn, MODL->brch[ibrch].arg9) != 0) {
            FREE(  MODL->brch[ibrch].arg9);
            MALLOC(MODL->brch[ibrch].arg9, char, (int)(strlen(defn)+1));
            strcpy(MODL->brch[ibrch].arg9,                    defn    );
        }
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmRetAttr - return an Attribute for a Branch by index            *
 *                                                                      *
 ************************************************************************
 */

int
ocsmRetAttr(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            int    iattr,               /* (in)  Attribute index (1-nattr) */
            char   aname[],             /* (out) Attribute name  (at least 129 long) */
            char   avalue[])            /* (out) Attribute value (at least 129 long) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    double    temp;

    ROUTINE(ocsmRetAttr);
    DPRINT3("%s(ibrch=%d, iattr=%d) {",
            routine, ibrch, iattr);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* check that valie iattr is given */
    if (iattr < 1 || iattr > MODL->brch[ibrch].nattr) {
        status = OCSM_ILLEGAL_ARG_INDEX;
        goto cleanup;
    }

    /* return the name and value.  if the first character is '!',
       evaluate it before returning */
    if (MODL->brch[ibrch].attr[iattr-1].value[0] == '!') {
        status = str2val(&(MODL->brch[ibrch].attr[iattr-1].value[1]), MODL, &temp);
        CHECK_STATUS(str2val);

        strcpy( aname,  MODL->brch[ibrch].attr[iattr-1].name );
        sprintf(avalue, "%11.6f", temp);
    } else {
        strcpy( aname,  MODL->brch[ibrch].attr[iattr-1].name );
        strcpy( avalue, MODL->brch[ibrch].attr[iattr-1].value);
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetAttr - get an Attribute for a Branch by name                *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetAttr(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            char   aname[],             /* (in)  Attribute name */
            char   avalue[])            /* (out) Attribute value (at least 129 long) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       iattr, jattr;
    double    temp;

    ROUTINE(ocsmGetAttr);
    DPRINT3("%s(ibrch=%d, aname=%s) {",
            routine, ibrch, aname);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* determine the attribute index (if any) */
    iattr = -1;
    for (jattr = 0; jattr < MODL->brch[ibrch].nattr; jattr++) {
        if (strcmp(MODL->brch[ibrch].attr[jattr].name, aname) == 0) {
            iattr = jattr;
            break;
        }
    }

    /* error if Attribute name is not found */
    if (iattr < 0) {
        status = OCSM_NAME_NOT_FOUND;
        goto cleanup;
    }

    /* return the value.  if the first character is '!',
       evaluate it before returning */
    if (MODL->brch[ibrch].attr[iattr-1].value[0] == '!') {
        status = str2val(&(MODL->brch[ibrch].attr[iattr-1].value[1]), MODL, &temp);
        CHECK_STATUS(str2val);

        sprintf(avalue, "%11.6f", temp);
    } else {
        strcpy( avalue, MODL->brch[ibrch].attr[iattr-1].value);
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetAttr - set an Attribute for a Branch                        *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSetAttr(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            char   aname[],             /* (in)  Attribute name */
            char   avalue[])            /* (in)  Attribute value */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       iattr, jattr;

    ROUTINE(ocsmSetAttr);
    DPRINT4("%s(ibrch=%d, aname=%s, avalue=%s) {",
            routine, ibrch, aname, avalue);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* determine the attribute index (if any) */
    iattr = -1;
    for (jattr = 0; jattr < MODL->brch[ibrch].nattr; jattr++) {
        if (strcmp(MODL->brch[ibrch].attr[jattr].name, aname) == 0) {
            iattr = jattr;
            break;
        }
    }

    /* if attribute is not found already, create a new one */
    if (iattr < 0) {
        MODL->brch[ibrch].nattr++;

        if (MODL->brch[ibrch].attr == NULL) {
            MALLOC(MODL->brch[ibrch].attr, attr_T, MODL->brch[ibrch].nattr);
        } else {
            RALLOC(MODL->brch[ibrch].attr, attr_T, MODL->brch[ibrch].nattr);
        }

        iattr = MODL->brch[ibrch].nattr - 1;
    }

    /* set the Attribute's name and value */
    MALLOC(MODL->brch[ibrch].attr[iattr].name,  char, (int)(strlen(aname )+1));
    strcpy(MODL->brch[ibrch].attr[iattr].name,                     aname     );
    MALLOC(MODL->brch[ibrch].attr[iattr].value, char, (int)(strlen(avalue)+1));
    strcpy(MODL->brch[ibrch].attr[iattr].value,                    avalue    );

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetName - get the name of a Branch                             *
 *                                                                      *
 ************************************************************************
 */
int
ocsmGetName(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            char   name[])              /* (out) Branch name (at least 129 long) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmGetName);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    /* default return values */
    name[0] = '\0';

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* return the name */
    strcpy(name, MODL->brch[ibrch].name);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetName - set the name for a Branch                            *
 *                                                                      *
 ************************************************************************
 */
int
ocsmSetName(void   *modl,               /* (in)  pointer to MODL */
            int    ibrch,               /* (in)  Branch index (1-nbrch) */
            char   name[])              /* (in)  Branch name */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       jbrch;

    ROUTINE(ocsmSetName);
    DPRINT3("%s(ibrch=%d, name=%s) {",
            routine, ibrch, name);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibrch is given */
    if (ibrch < 1 || ibrch > MODL->nbrch) {
        status = OCSM_ILLEGAL_BRCH_INDEX;
        goto cleanup;
    }

    /* make sure that the new name is unique */
    for (jbrch = 1; jbrch <= MODL->nbrch; jbrch++) {
        if (strcmp(name, MODL->brch[jbrch].name) == 0) {
            status = OCSM_NAME_NOT_UNIQUE;
            goto cleanup;
        } else if (strncmp(name, "Brch_", 5) == 0) {
            status = OCSM_NAME_NOT_UNIQUE;
            goto cleanup;
        }
    }

    /* set the name */
    FREE(MODL->brch[ibrch].name);

    MALLOC(MODL->brch[ibrch].name, char, (int)(strlen(name)+1));
    strcpy(MODL->brch[ibrch].name,                    name    );

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmNewPmtr - create a new Parameter                               *
 *                                                                      *
 ************************************************************************
 */

int
ocsmNewPmtr(void   *modl,               /* (in)  pointer to MODL */
            char   name[],              /* (in)  Parameter name */
            int    type,                /* (in)  Parameter type */
            int    nrow,                /* (in)  number of rows */
            int    ncol)                /* (in)  number of columns */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ipmtr, jpmtr, i;

    ROUTINE(ocsmNewPmtr);
    DPRINT5("%s(name=%s, type=%d, nrow=%d, ncol=%d) {",
            routine, name, type, nrow, ncol);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that pmtrName is not already defined */
    jpmtr = 0;
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        if (strcmp(name, MODL->pmtr[ipmtr].name) == 0) {
            jpmtr = ipmtr;
            break;
        }
    }
    if (jpmtr != 0) {
        status = OCSM_NAME_ALREADY_DEFINED;
        goto cleanup;
    }

    /* check that pmtrName is a valid name */
    if (strlen(name) == 0 || strlen(name) >= MAX_NAME_LEN) {
        status = OCSM_ILLEGAL_PMTR_NAME;
        goto cleanup;
    } else if (name[0] == '@') {
    } else if (isalpha(name[0]) == 0) {
        status = OCSM_ILLEGAL_PMTR_NAME;
        goto cleanup;
    } else {
        for (i = 1; i < strlen(name); i++) {
            if        (isalpha(name[i]) != 0) {
            } else if (isdigit(name[i]) != 0) {
            } else if (name[i] == '_') {
            } else if (name[i] == '@') {
            } else {
                status = OCSM_ILLEGAL_PMTR_NAME;
                goto cleanup;
            }
        }
    }

    /* check for a valid Parameter type */
    if (type != OCSM_EXTERNAL && type != OCSM_INTERNAL) {
        status = OCSM_ILLEGAL_TYPE;
        goto cleanup;
    }

    /* extend the Parameter list (if needed) */
    if (MODL->npmtr >= MODL->mpmtr) {
        MODL->mpmtr += 25;
        if (MODL->pmtr == NULL) {
            MALLOC(MODL->pmtr, pmtr_T, MODL->mpmtr+1);
        } else {
            RALLOC(MODL->pmtr, pmtr_T, MODL->mpmtr+1);
        }
    }

    /* create the new Parameter and store its name and defn */
    MODL->npmtr += 1;

    ipmtr = MODL->npmtr;

    MALLOC( MODL->pmtr[ipmtr].name, char, (int)(strlen(name)+1));
    strncpy(MODL->pmtr[ipmtr].name, name,       strlen(name)+1 );

    MALLOC( MODL->pmtr[ipmtr].value, double, nrow*ncol);

    MODL->pmtr[ipmtr].type = type;
    MODL->pmtr[ipmtr].nrow = nrow;
    MODL->pmtr[ipmtr].ncol = ncol;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetPmtr - get info about a Parameter                           *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetPmtr(void   *modl,               /* (in)  pointer to MODL */
            int    ipmtr,               /* (in)  Parameter index (1-npmtr) */
            int    *type,               /* (out) Parameter type */
            int    *nrow,               /* (out) number of rows */
            int    *ncol,               /* (out) number of columns */
            char   name[])              /* (out) Parameter name (at least 33 long) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmGetPmtr);
    DPRINT2("%s(ipmtr=%d) {",
            routine, ipmtr);

    /* --------------------------------------------------------------- */

    /* default return values */
    name[0] = '\0';
    *type   = 0;
    *nrow   = 0;
    *ncol   = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ipmtr is given */
    if (ipmtr < 1 || ipmtr > MODL->npmtr) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    }

    /* return the name, type, and size */
    strcpy(name, MODL->pmtr[ipmtr].name);
    *type  =     MODL->pmtr[ipmtr].type;
    *nrow  =     MODL->pmtr[ipmtr].nrow;
    *ncol  =     MODL->pmtr[ipmtr].ncol;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmPrintPmtrs - print external Parameters to file                 *
 *                                                                      *
 ************************************************************************
 */

int
ocsmPrintPmtrs(void   *modl,            /* (in)  pointer to MODL */
               FILE   *fp)              /* (in)  pointer to FILE */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ipmtr, maxlen, irow, icol, index, count;

    ROUTINE(ocsmPrintPmtrs);
    DPRINT2("%s(fp=%d) {",
            routine, (int)fp);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* find maximum name length */
    maxlen = 0;
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        if (strlen(MODL->pmtr[ipmtr].name) > maxlen) {
            maxlen = strlen(MODL->pmtr[ipmtr].name);
        }
    }

    /* loop through all external Parameters and print its name, value, and defn */
    count = 0;
    for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
        if (MODL->pmtr[ipmtr].type == OCSM_EXTERNAL) {
            index = 0;
            for (irow = 1; irow <= MODL->pmtr[ipmtr].nrow; irow++) {
                for (icol = 1; icol <= MODL->pmtr[ipmtr].ncol; icol++) {
                    fprintf(fp, "%5d [e]  %-*s  [%3d,%3d] %11.5f\n",
                            ipmtr, maxlen, MODL->pmtr[ipmtr].name,
                            irow, icol, MODL->pmtr[ipmtr].value[index]);
                    index++;
                }
            }
            count++;
        }
    }

    /* return message if no Paramaters */
    if (count <= 0) {
        fprintf(fp, "--none--\n");
        goto cleanup;
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetValu - get the Value of a Parameter                         *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetValu(void   *modl,               /* (in)  pointer to MODL */
            int    ipmtr,               /* (in)  Parameter index (1-npmtr) */
            int    irow,                /* (in)  row    index (1-nrow) */
            int    icol,                /* (in)  column index (1-ncol) */
            double *value)              /* (out) Parameter value */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       index;

    ROUTINE(ocsmGetValu);
    DPRINT4("%s(ipmtr=%d, irow=%d, icol=%d) {",
            routine, ipmtr, irow, icol);

    /* --------------------------------------------------------------- */

    /* default return values */
    *value = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ipmtr, irow, and icol are given */
    if (ipmtr < 1 || ipmtr > MODL->npmtr) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    } else if (irow < 1 || irow > MODL->pmtr[ipmtr].nrow) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    } else if (icol < 1 || icol > MODL->pmtr[ipmtr].ncol) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    }

    /* return pertinent information */
    index  = (icol-1) + (irow-1) * (MODL->pmtr[ipmtr].ncol);
    *value = MODL->pmtr[ipmtr].value[index];

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}



/*
 ************************************************************************
 *                                                                      *
 *   ocsmSetValu - set a Value for a Parameter                          *
 *                                                                      *
 ************************************************************************
 */

int
ocsmSetValu(void   *modl,               /* (in)  pointer to MODL */
            int    ipmtr,               /* (in)  Parameter index (1-npmtr) */
            int    irow,                /* (in)  row    index (1-nrow) */
            int    icol,                /* (in)  column index (1-nrow) */
            char   defn[])              /* (in) definition of Value */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       index;
    double    value;

    ROUTINE(ocsmSetValu);
    DPRINT5("%s(ipmtr%d, irow=%d, icol=%d, defn=%s) {",
            routine, ipmtr, irow, icol, defn);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ipmtr, irow, and icol are given */
    if (ipmtr < 1 || ipmtr > MODL->npmtr) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    } else if (irow < 1 || irow > MODL->pmtr[ipmtr].nrow) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    } else if (icol < 1 || icol > MODL->pmtr[ipmtr].ncol) {
        status = OCSM_ILLEGAL_PMTR_INDEX;
        goto cleanup;
    }

    /* evaluate the definition */
    if (MODL->pmtr[ipmtr].type == OCSM_EXTERNAL) {
        status = str2val(defn, NULL, &value);
        CHECK_STATUS(str2val);
    } else {
        status = str2val(defn, MODL, &value);
        CHECK_STATUS(str2val);
    }

    /* return pertinent information */
    index  = (icol-1) + (irow-1) * (MODL->pmtr[ipmtr].ncol);
    MODL->pmtr[ipmtr].value[index] = value;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}



/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetBody - get info about a Body                                *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetBody(void   *modl,               /* (in)  pointer to MODL */
            int    ibody,               /* (in)  Body index (1-nbody) */
            int    *type,               /* (out) Branch type */
            int    *ichld,              /* (out) ibody of child (or 0 if root) */
            int    *ileft,              /* (out) ibody of left parent (or 0) */
            int    *irite,              /* (out) ibody of rite parent (or 0) */
            double args[],              /* (out) array  of Arguments (at least 10 long) */
            int    *nnode,              /* (out) number of Nodes */
            int    *nedge,              /* (out) number of Edges */
            int    *nface)              /* (out) number of Faces */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(ocsmGetBody);
    DPRINT2("%s(ibody=%d) {",
            routine, ibody);

    /* --------------------------------------------------------------- */

    /* default return values */
    *type  = 0;
    *ichld = 0;
    *ileft = 0;
    *irite = 0;
    *nnode = 0;
    *nedge = 0;
    *nface = 0;

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibody is given */
    if (ibody < 1 || ibody > MODL->nbody) {
        status = OCSM_ILLEGAL_BODY_INDEX;
        goto cleanup;
    }

    /* return pertinent information */
    *type   = MODL->body[ibody].brtype;
    *ichld  = MODL->body[ibody].ichld;
    *ileft  = MODL->body[ibody].ileft;
    *irite  = MODL->body[ibody].irite;
    args[1] = MODL->body[ibody].arg1;
    args[2] = MODL->body[ibody].arg2;
    args[3] = MODL->body[ibody].arg3;
    args[4] = MODL->body[ibody].arg4;
    args[5] = MODL->body[ibody].arg5;
    args[6] = MODL->body[ibody].arg6;
    args[7] = MODL->body[ibody].arg7;
    args[8] = MODL->body[ibody].arg8;
    args[9] = MODL->body[ibody].arg9;
    *nnode  = MODL->body[ibody].nnode;
    *nedge  = MODL->body[ibody].nedge;
    *nface  = MODL->body[ibody].nface;

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmPrintBodys - print all Bodys to file                           *
 *                                                                      *
 ************************************************************************
 */

int
ocsmPrintBodys(void   *modl,            /* (in)  pointer to MODL */
               FILE   *fp)              /* (in)  pointer to FILE */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       ibody;

    ROUTINE(ocsmPrintBodys);
    DPRINT2("%s(fp=%d) {",
            routine, (int)fp);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* return message if no Bodys */
    if (MODL->nbody <= 0) {
        fprintf(fp, "--none--\n");
        goto cleanup;
    }

    /* print Body table */
    fprintf(fp, "ibody ibrch brchType  ileft irite ichld     arg1     arg2     arg3     arg4     arg5     arg6     arg7     arg8     arg9 bodyType\n");
    for (ibody = 1; ibody <= MODL->nbody; ibody++) {
        fprintf(fp, "%5d %5d %-9s %5d %5d %5d %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %s\n", ibody,
                MODL->body[ibody].ibrch,
                ocsmGetText(MODL->body[ibody].brtype),
                MODL->body[ibody].ileft,
                MODL->body[ibody].irite,
                MODL->body[ibody].ichld,
                MODL->body[ibody].arg1,
                MODL->body[ibody].arg2,
                MODL->body[ibody].arg3,
                MODL->body[ibody].arg4,
                MODL->body[ibody].arg5,
                MODL->body[ibody].arg6,
                MODL->body[ibody].arg7,
                MODL->body[ibody].arg8,
                MODL->body[ibody].arg9,
                ocsmGetText(MODL->body[ibody].botype));
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetText - convert an OCSM code to text                         *
 *                                                                      *
 ************************************************************************
 */

char *
ocsmGetText(int    icode)               /* (in)  code to look up */
{

    static char    unknown[]                          = "UNKNOWN";
    static char    egads[]                            = "EGADS error";
    static char    success[]                          = "success";

    static char    ocsm_box[]                         = "box";
    static char    ocsm_sphere[]                      = "sphere";
    static char    ocsm_cone[]                        = "cone";
    static char    ocsm_cylinder[]                    = "cylinder";
    static char    ocsm_torus[]                       = "torus";
    static char    ocsm_import[]                      = "import";
    static char    ocsm_udprim[]                      = "udprim";

    static char    ocsm_extrude[]                     = "extrude";
    static char    ocsm_loft[]                        = "loft";
    static char    ocsm_revolve[]                     = "revolve";

    static char    ocsm_fillet[]                      = "fillet";
    static char    ocsm_chamfer[]                     = "chamfer";
    static char    ocsm_hollow[]                      = "hollow";

    static char    ocsm_intersect[]                   = "intersect";
    static char    ocsm_subtract[]                    = "subtract";
    static char    ocsm_union[]                       = "union";

    static char    ocsm_translate[]                   = "translate";
    static char    ocsm_rotatex[]                     = "rotatex";
    static char    ocsm_rotatey[]                     = "rotatey";
    static char    ocsm_rotatez[]                     = "rotatez";
    static char    ocsm_scale[]                       = "scale";

    static char    ocsm_skbeg[]                       = "skbeg";
    static char    ocsm_linseg[]                      = "linseg";
    static char    ocsm_cirarc[]                      = "cirarc";
    static char    ocsm_spline[]                      = "spline";
    static char    ocsm_skend[]                       = "skend";

    static char    ocsm_solbeg[]                      = "solbeg";
    static char    ocsm_solcon[]                      = "solcon";
    static char    ocsm_solend[]                      = "solend";

    static char    ocsm_set[]                         = "set";
    static char    ocsm_macbeg[]                      = "macbeg";
    static char    ocsm_macend[]                      = "macend";
    static char    ocsm_recall[]                      = "recall";
    static char    ocsm_patbeg[]                      = "patbeg";
    static char    ocsm_patend[]                      = "patend";
    static char    ocsm_mark[]                        = "mark";
    static char    ocsm_dump[]                        = "dump";

    static char    ocsm_primitive[]                   = "primitive";
    static char    ocsm_grown[]                       = "grown";
    static char    ocsm_applied[]                     = "applied";
    static char    ocsm_boolean[]                     = "boolean";
    static char    ocsm_transform[]                   = "transform";
    static char    ocsm_sketch[]                      = "sketch";
    static char    ocsm_solver[]                      = "solver";
    static char    ocsm_utility[]                     = "utility";

    static char    ocsm_active[]                      = "active";
    static char    ocsm_suppressed[]                  = "suppressed";
    static char    ocsm_inactive[]                    = "inactive";
    static char    ocsm_deferred[]                    = "deferred";

    static char    ocsm_solid_body[]                  = "solid_body";
    static char    ocsm_sheet_body[]                  = "sheet_body";
    static char    ocsm_wire_body[]                   = "wire_body";
    static char    ocsm_node_body[]                   = "node_body";

    static char    ocsm_external[]                    = "external";
    static char    ocsm_internal[]                    = "internal";

    static char    ocsm_file_not_found[]              = "file_not_found";
    static char    ocsm_illegal_statement[]           = "illegal_statement";
    static char    ocsm_not_enough_args[]             = "not_enough_args";
    static char    ocsm_name_already_defined[]        = "name_already_defined";
    static char    ocsm_patterns_nested_too_deeply[]  = "patterns_nested_too_deeply";
    static char    ocsm_patbeg_without_patend[]       = "patbeg_without_patend";
    static char    ocsm_patend_without_patbeg[]       = "patend_without_patbeg";
    static char    ocsm_nothing_to_delete[]           = "nothing_to_delete";
    static char    ocsm_not_modl_structure[]          = "not_modl_structure";

    static char    ocsm_did_not_create_body[]         = "did_not_create_body";
    static char    ocsm_created_too_many_bodys[]      = "created_too_many_bodys";
    static char    ocsm_expecting_one_body[]          = "expecting_one_body";
    static char    ocsm_expecting_two_bodys[]         = "expecting_two_bodys";
    static char    ocsm_expecting_one_sketch[]        = "expecting_one_sketch";
    static char    ocsm_expecting_nloft_sketches[]    = "expecting_nloft_sketches";
    static char    ocsm_loft_without_mark[]           = "loft_without_mark";
    static char    ocsm_too_many_sketches_in_loft[]   = "too_many_sketches_in_loft";
    static char    ocsm_modl_not_checked[]            = "modl_not_checked";

    static char    ocsm_fillet_after_wrong_type[]     = "fillet_after_wrong_type";
    static char    ocsm_chamfer_after_wrong_type[]    = "chamfer_after_wrong_type";
    static char    ocsm_no_bodys_produced[]           = "no_bodys_produced";
    static char    ocsm_not_enough_bodys_produced[]   = "not_enough_bodys_produced";
    static char    ocsm_too_many_bodys_on_stack[]     = "too_many_bodys_on_stack";

    static char    ocsm_sketcher_is_open[]            = "sketcher_is_open";
    static char    ocsm_sketcher_is_not_open[]        = "sketcher_is_not_open";
    static char    ocsm_colinear_sketch_points[]      = "colinear_sketch_points";
    static char    ocsm_non_coplanar_sketch_points[]  = "non_coplanar_sketch_points";
    static char    ocsm_too_many_sketch_points[]      = "too_many_sketch_points";
    static char    ocsm_too_few_spline_points[]       = "too_few_spline_points";
    static char    ocsm_sketch_does_not_close[]       = "sketch_does_not_close";

    static char    ocsm_illegal_char_in_expr[]        = "illegal_char_in_expr";
    static char    ocsm_close_before_open[]           = "close_before_open";
    static char    ocsm_missing_close[]               = "missing_close";
    static char    ocsm_illegal_token_sequence[]      = "illegal_token_sequence";
    static char    ocsm_illegal_number[]              = "illegal_number";
    static char    ocsm_illegal_pmtr_name[]           = "illegal_pmtr_name";
    static char    ocsm_illegal_func_name[]           = "illegal_func_name";
    static char    ocsm_illegal_type[]                = "illegal_type";
    static char    ocsm_illegal_narg[]                = "illegal_narg";

    static char    ocsm_name_not_found[]              = "name_not_found";
    static char    ocsm_name_not_unique[]             = "name_not_unique";
    static char    ocsm_pmtr_is_external[]            = "pmtr_is_external";
    static char    ocsm_pmtr_is_internal[]            = "pmtr_is_internal";
    static char    ocsm_func_arg_out_of_bounds[]      = "func_arg_out_of_bounds";
    static char    ocsm_val_stack_underflow[]         = "val_stack_underflow";
    static char    ocsm_val_stack_overflow[]          = "val_stack_overflow";

    static char    ocsm_illegal_brch_index[]          = "illegal_brch_index";
    static char    ocsm_illegal_pmtr_index[]          = "illegal_pmtr_index";
    static char    ocsm_illegal_body_index[]          = "illegal_body_index";
    static char    ocsm_illegal_arg_index[]           = "illegal_arg_index";
    static char    ocsm_illegal_activity[]            = "illegal_activity";
    static char    ocsm_illegal_macro_index[]         = "illegal_macro_index";
    static char    ocsm_illegal_argument[]            = "illegal_argument";
    static char    ocsm_cannot_be_suppressed[]        = "cannot_be_suppressed";
    static char    ocsm_storage_already_used[]        = "storage_already_used";
    static char    ocsm_nothing_previously_stored[]   = "nothing_previously_stored";

    static char    ocsm_solver_is_open[]              = "solver_is_open";
    static char    ocsm_solver_is_not_open[]          = "solver_is_not_open";
    static char    ocsm_too_many_solver_vars[]        = "too_many_solver_vars";
    static char    ocsm_underconstrained[]            = "underconstrained";
    static char    ocsm_overconstrained[]             = "overconstrained";
    static char    ocsm_singular_matrix[]             = "singular_matrix";
    static char    ocsm_not_converged[]               = "not_converged";

    static char    ocsm_udp_error1[]                  = "udp_error1";
    static char    ocsm_udp_error2[]                  = "udp_error2";
    static char    ocsm_udp_error3[]                  = "udp_error3";
    static char    ocsm_udp_error4[]                  = "udp_error4";
    static char    ocsm_udp_error5[]                  = "udp_error5";
    static char    ocsm_udp_error6[]                  = "udp_error6";
    static char    ocsm_udp_error7[]                  = "udp_error7";
    static char    ocsm_udp_error8[]                  = "udp_error8";
    static char    ocsm_udp_error9[]                  = "udp_error9";

    static char    ocsm_op_stack_underflow[]          = "op_stack_underflow";
    static char    ocsm_op_stack_overflow[]           = "op_stack_overflow";
    static char    ocsm_rpn_stack_underflow[]         = "rpn_stack_underflow";
    static char    ocsm_rpn_stack_overflow[]          = "rpn_stack_overflow";
    static char    ocsm_token_stack_underflow[]       = "token_stack_underflow";
    static char    ocsm_token_stack_overflow[]        = "token_stack_overflow";
    static char    ocsm_unsupported[]                 = "unsupported";
    static char    ocsm_internal_error[]              = "internal_error";

    /* --------------------------------------------------------------- */

    /* OCSM_SUCCESS */
    if (icode == SUCCESS                         ) return success;

    /* OCSM_PRIMITIVE */
    if (icode == OCSM_BOX                        ) return ocsm_box;
    if (icode == OCSM_SPHERE                     ) return ocsm_sphere;
    if (icode == OCSM_CONE                       ) return ocsm_cone;
    if (icode == OCSM_CYLINDER                   ) return ocsm_cylinder;
    if (icode == OCSM_TORUS                      ) return ocsm_torus;
    if (icode == OCSM_IMPORT                     ) return ocsm_import;
    if (icode == OCSM_UDPRIM                     ) return ocsm_udprim;

    /* OCSM_GROWN */
    if (icode == OCSM_EXTRUDE                    ) return ocsm_extrude;
    if (icode == OCSM_LOFT                       ) return ocsm_loft;
    if (icode == OCSM_REVOLVE                    ) return ocsm_revolve;

    /* OCSM_APPLIED */
    if (icode == OCSM_FILLET                     ) return ocsm_fillet;
    if (icode == OCSM_CHAMFER                    ) return ocsm_chamfer;
    if (icode == OCSM_HOLLOW                     ) return ocsm_hollow;

    /* OCSM_BOOLEAN */
    if (icode == OCSM_INTERSECT                  ) return ocsm_intersect;
    if (icode == OCSM_SUBTRACT                   ) return ocsm_subtract;
    if (icode == OCSM_UNION                      ) return ocsm_union;

    /* OCSM_TRANSFORM */
    if (icode == OCSM_TRANSLATE                  ) return ocsm_translate;
    if (icode == OCSM_ROTATEX                    ) return ocsm_rotatex;
    if (icode == OCSM_ROTATEY                    ) return ocsm_rotatey;
    if (icode == OCSM_ROTATEZ                    ) return ocsm_rotatez;
    if (icode == OCSM_SCALE                      ) return ocsm_scale;

    /* OCSM_SKETCH */
    if (icode == OCSM_SKBEG                      ) return ocsm_skbeg;
    if (icode == OCSM_LINSEG                     ) return ocsm_linseg;
    if (icode == OCSM_CIRARC                     ) return ocsm_cirarc;
    if (icode == OCSM_SPLINE                     ) return ocsm_spline;
    if (icode == OCSM_SKEND                      ) return ocsm_skend;

    /* OCSM_SOLVER */
    if (icode == OCSM_SOLBEG                     ) return ocsm_solbeg;
    if (icode == OCSM_SOLCON                     ) return ocsm_solcon;
    if (icode == OCSM_SOLEND                     ) return ocsm_solend;

    /* OCSM_UTILITY */
    if (icode == OCSM_SET                        ) return ocsm_set;
    if (icode == OCSM_MACBEG                     ) return ocsm_macbeg;
    if (icode == OCSM_MACEND                     ) return ocsm_macend;
    if (icode == OCSM_RECALL                     ) return ocsm_recall;
    if (icode == OCSM_PATBEG                     ) return ocsm_patbeg;
    if (icode == OCSM_PATEND                     ) return ocsm_patend;
    if (icode == OCSM_MARK                       ) return ocsm_mark;
    if (icode == OCSM_DUMP                       ) return ocsm_dump;

    /* Branch classes */
    if (icode == OCSM_PRIMITIVE                  ) return ocsm_primitive;
    if (icode == OCSM_GROWN                      ) return ocsm_grown;
    if (icode == OCSM_APPLIED                    ) return ocsm_applied;
    if (icode == OCSM_BOOLEAN                    ) return ocsm_boolean;
    if (icode == OCSM_TRANSFORM                  ) return ocsm_transform;
    if (icode == OCSM_SKETCH                     ) return ocsm_sketch;
    if (icode == OCSM_SOLVER                     ) return ocsm_solver;
    if (icode == OCSM_UTILITY                    ) return ocsm_utility;

    /* Branch activities */
    if (icode == OCSM_ACTIVE                     ) return ocsm_active;
    if (icode == OCSM_SUPPRESSED                 ) return ocsm_suppressed;
    if (icode == OCSM_INACTIVE                   ) return ocsm_inactive;
    if (icode == OCSM_DEFERRED                   ) return ocsm_deferred;

    /* Body types */
    if (icode == OCSM_SOLID_BODY                 ) return ocsm_solid_body;
    if (icode == OCSM_SHEET_BODY                 ) return ocsm_sheet_body;
    if (icode == OCSM_WIRE_BODY                  ) return ocsm_wire_body;
    if (icode == OCSM_NODE_BODY                  ) return ocsm_node_body;

    /* Parameter types */
    if (icode == OCSM_EXTERNAL                   ) return ocsm_external;
    if (icode == OCSM_INTERNAL                   ) return ocsm_internal;

     /* Error codes */
    if (icode == OCSM_FILE_NOT_FOUND             ) return ocsm_file_not_found;
    if (icode == OCSM_ILLEGAL_STATEMENT          ) return ocsm_illegal_statement;
    if (icode == OCSM_NOT_ENOUGH_ARGS            ) return ocsm_not_enough_args;
    if (icode == OCSM_NAME_ALREADY_DEFINED       ) return ocsm_name_already_defined;
    if (icode == OCSM_PATTERNS_NESTED_TOO_DEEPLY ) return ocsm_patterns_nested_too_deeply;
    if (icode == OCSM_PATBEG_WITHOUT_PATEND      ) return ocsm_patbeg_without_patend;
    if (icode == OCSM_PATEND_WITHOUT_PATBEG      ) return ocsm_patend_without_patbeg;
    if (icode == OCSM_NOTHING_TO_DELETE          ) return ocsm_nothing_to_delete;
    if (icode == OCSM_NOT_MODL_STRUCTURE         ) return ocsm_not_modl_structure;

    if (icode == OCSM_DID_NOT_CREATE_BODY        ) return ocsm_did_not_create_body;
    if (icode == OCSM_CREATED_TOO_MANY_BODYS     ) return ocsm_created_too_many_bodys;
    if (icode == OCSM_EXPECTING_ONE_BODY         ) return ocsm_expecting_one_body;
    if (icode == OCSM_EXPECTING_TWO_BODYS        ) return ocsm_expecting_two_bodys;
    if (icode == OCSM_EXPECTING_ONE_SKETCH       ) return ocsm_expecting_one_sketch;
    if (icode == OCSM_EXPECTING_NLOFT_SKETCHES   ) return ocsm_expecting_nloft_sketches;
    if (icode == OCSM_LOFT_WITHOUT_MARK          ) return ocsm_loft_without_mark;
    if (icode == OCSM_TOO_MANY_SKETCHES_IN_LOFT  ) return ocsm_too_many_sketches_in_loft;
    if (icode == OCSM_MODL_NOT_CHECKED           ) return ocsm_modl_not_checked;

    if (icode == OCSM_FILLET_AFTER_WRONG_TYPE    ) return ocsm_fillet_after_wrong_type;
    if (icode == OCSM_CHAMFER_AFTER_WRONG_TYPE   ) return ocsm_chamfer_after_wrong_type;
    if (icode == OCSM_NO_BODYS_PRODUCED          ) return ocsm_no_bodys_produced;
    if (icode == OCSM_NOT_ENOUGH_BODYS_PRODUCED  ) return ocsm_not_enough_bodys_produced;
    if (icode == OCSM_TOO_MANY_BODYS_ON_STACK    ) return ocsm_too_many_bodys_on_stack;

    if (icode == OCSM_SKETCHER_IS_OPEN           ) return ocsm_sketcher_is_open;
    if (icode == OCSM_SKETCHER_IS_NOT_OPEN       ) return ocsm_sketcher_is_not_open;
    if (icode == OCSM_COLINEAR_SKETCH_POINTS     ) return ocsm_colinear_sketch_points;
    if (icode == OCSM_NON_COPLANAR_SKETCH_POINTS ) return ocsm_non_coplanar_sketch_points;
    if (icode == OCSM_TOO_MANY_SKETCH_POINTS     ) return ocsm_too_many_sketch_points;
    if (icode == OCSM_TOO_FEW_SPLINE_POINTS      ) return ocsm_too_few_spline_points;
    if (icode == OCSM_SKETCH_DOES_NOT_CLOSE      ) return ocsm_sketch_does_not_close;

    if (icode == OCSM_ILLEGAL_CHAR_IN_EXPR       ) return ocsm_illegal_char_in_expr;
    if (icode == OCSM_CLOSE_BEFORE_OPEN          ) return ocsm_close_before_open;
    if (icode == OCSM_MISSING_CLOSE              ) return ocsm_missing_close;
    if (icode == OCSM_ILLEGAL_TOKEN_SEQUENCE     ) return ocsm_illegal_token_sequence;
    if (icode == OCSM_ILLEGAL_NUMBER             ) return ocsm_illegal_number;
    if (icode == OCSM_ILLEGAL_PMTR_NAME          ) return ocsm_illegal_pmtr_name;
    if (icode == OCSM_ILLEGAL_FUNC_NAME          ) return ocsm_illegal_func_name;
    if (icode == OCSM_ILLEGAL_TYPE               ) return ocsm_illegal_type;
    if (icode == OCSM_ILLEGAL_NARG               ) return ocsm_illegal_narg;

    if (icode == OCSM_NAME_NOT_FOUND             ) return ocsm_name_not_found;
    if (icode == OCSM_NAME_NOT_UNIQUE            ) return ocsm_name_not_unique;
    if (icode == OCSM_PMTR_IS_EXTERNAL           ) return ocsm_pmtr_is_external;
    if (icode == OCSM_PMTR_IS_INTERNAL           ) return ocsm_pmtr_is_internal;
    if (icode == OCSM_FUNC_ARG_OUT_OF_BOUNDS     ) return ocsm_func_arg_out_of_bounds;
    if (icode == OCSM_VAL_STACK_UNDERFLOW        ) return ocsm_val_stack_underflow;
    if (icode == OCSM_VAL_STACK_OVERFLOW         ) return ocsm_val_stack_overflow;

    if (icode == OCSM_ILLEGAL_BRCH_INDEX         ) return ocsm_illegal_brch_index;
    if (icode == OCSM_ILLEGAL_PMTR_INDEX         ) return ocsm_illegal_pmtr_index;
    if (icode == OCSM_ILLEGAL_BODY_INDEX         ) return ocsm_illegal_body_index;
    if (icode == OCSM_ILLEGAL_ARG_INDEX          ) return ocsm_illegal_arg_index;
    if (icode == OCSM_ILLEGAL_ACTIVITY           ) return ocsm_illegal_activity;
    if (icode == OCSM_ILLEGAL_MACRO_INDEX        ) return ocsm_illegal_macro_index;
    if (icode == OCSM_ILLEGAL_ARGUMENT           ) return ocsm_illegal_argument;
    if (icode == OCSM_CANNOT_BE_SUPPRESSED       ) return ocsm_cannot_be_suppressed;
    if (icode == OCSM_STORAGE_ALREADY_USED       ) return ocsm_storage_already_used;
    if (icode == OCSM_NOTHING_PREVIOUSLY_STORED  ) return ocsm_nothing_previously_stored;

    if (icode == OCSM_SOLVER_IS_OPEN             ) return ocsm_solver_is_open;
    if (icode == OCSM_SOLVER_IS_NOT_OPEN         ) return ocsm_solver_is_not_open;
    if (icode == OCSM_TOO_MANY_SOLVER_VARS       ) return ocsm_too_many_solver_vars;
    if (icode == OCSM_UNDERCONSTRAINED           ) return ocsm_underconstrained;
    if (icode == OCSM_OVERCONSTRAINED            ) return ocsm_overconstrained;
    if (icode == OCSM_SINGULAR_MATRIX            ) return ocsm_singular_matrix;
    if (icode == OCSM_NOT_CONVERGED              ) return ocsm_not_converged;

    if (icode == OCSM_UDP_ERROR1                 ) return ocsm_udp_error1;
    if (icode == OCSM_UDP_ERROR2                 ) return ocsm_udp_error2;
    if (icode == OCSM_UDP_ERROR3                 ) return ocsm_udp_error3;
    if (icode == OCSM_UDP_ERROR4                 ) return ocsm_udp_error4;
    if (icode == OCSM_UDP_ERROR5                 ) return ocsm_udp_error5;
    if (icode == OCSM_UDP_ERROR6                 ) return ocsm_udp_error6;
    if (icode == OCSM_UDP_ERROR7                 ) return ocsm_udp_error7;
    if (icode == OCSM_UDP_ERROR8                 ) return ocsm_udp_error8;
    if (icode == OCSM_UDP_ERROR9                 ) return ocsm_udp_error9;

    if (icode == OCSM_OP_STACK_UNDERFLOW         ) return ocsm_op_stack_underflow;
    if (icode == OCSM_OP_STACK_OVERFLOW          ) return ocsm_op_stack_overflow;
    if (icode == OCSM_RPN_STACK_UNDERFLOW        ) return ocsm_rpn_stack_underflow;
    if (icode == OCSM_RPN_STACK_OVERFLOW         ) return ocsm_rpn_stack_overflow;
    if (icode == OCSM_TOKEN_STACK_UNDERFLOW      ) return ocsm_token_stack_underflow;
    if (icode == OCSM_TOKEN_STACK_OVERFLOW       ) return ocsm_token_stack_overflow;
    if (icode == OCSM_UNSUPPORTED                ) return ocsm_unsupported;
    if (icode == OCSM_INTERNAL_ERROR             ) return ocsm_internal_error;

    /* EGADS errors */
    if (icode < 0 && icode > -30) return egads;

    /* default return */
    return unknown;
}


/*
 ************************************************************************
 *                                                                      *
 *   ocsmGetCode - convert text to an OCSM code                         *
 *                                                                      *
 ************************************************************************
 */

int
ocsmGetCode(char   *text)               /* (in)  text to look up */
{

    /* --------------------------------------------------------------- */

    /* OCSM_SUCCESS */
    if (strcmp(text, "success"     ) == 0) return SUCCESS;

    /* OCSM_PRIMITIVE */
    if (strcmp(text, "box"       ) == 0) return OCSM_BOX;
    if (strcmp(text, "sphere"    ) == 0) return OCSM_SPHERE;
    if (strcmp(text, "cone"      ) == 0) return OCSM_CONE;
    if (strcmp(text, "cylinder"  ) == 0) return OCSM_CYLINDER;
    if (strcmp(text, "torus"     ) == 0) return OCSM_TORUS;
    if (strcmp(text, "import"    ) == 0) return OCSM_IMPORT;
    if (strcmp(text, "udprim"    ) == 0) return OCSM_UDPRIM;

    /* OCSM_GROWN */
    if (strcmp(text, "extrude"   ) == 0) return OCSM_EXTRUDE;
    if (strcmp(text, "loft"      ) == 0) return OCSM_LOFT;
    if (strcmp(text, "revolve"   ) == 0) return OCSM_REVOLVE;

    /* OCSM_APPLIED */
    if (strcmp(text, "fillet"    ) == 0) return OCSM_FILLET;
    if (strcmp(text, "chamfer"   ) == 0) return OCSM_CHAMFER;
    if (strcmp(text, "hollow"    ) == 0) return OCSM_HOLLOW;

    /* OCSM_BOOLEAN */
    if (strcmp(text, "intersect" ) == 0) return OCSM_INTERSECT;
    if (strcmp(text, "subtract"  ) == 0) return OCSM_SUBTRACT;
    if (strcmp(text, "union"     ) == 0) return OCSM_UNION;

    /* OCSM_TRANSFORM */
    if (strcmp(text, "translate" ) == 0) return OCSM_TRANSLATE;
    if (strcmp(text, "rotatex"   ) == 0) return OCSM_ROTATEX;
    if (strcmp(text, "rotatey"   ) == 0) return OCSM_ROTATEY;
    if (strcmp(text, "rotatez"   ) == 0) return OCSM_ROTATEZ;
    if (strcmp(text, "scale"     ) == 0) return OCSM_SCALE;

    /* OCSM_SKETCH */
    if (strcmp(text, "skbeg"     ) == 0) return OCSM_SKBEG;
    if (strcmp(text, "linseg"    ) == 0) return OCSM_LINSEG;
    if (strcmp(text, "cirarc"    ) == 0) return OCSM_CIRARC;
    if (strcmp(text, "spline"    ) == 0) return OCSM_SPLINE;
    if (strcmp(text, "skend"     ) == 0) return OCSM_SKEND;

    /* OCSM_SOLVER */
    if (strcmp(text, "solbeg"    ) == 0) return OCSM_SOLBEG;
    if (strcmp(text, "solcon"    ) == 0) return OCSM_SOLCON;
    if (strcmp(text, "solend"    ) == 0) return OCSM_SOLEND;

    /* OCSM_UTILITY */
    if (strcmp(text, "set"       ) == 0) return OCSM_SET;
    if (strcmp(text, "macbeg"    ) == 0) return OCSM_MACBEG;
    if (strcmp(text, "macend"    ) == 0) return OCSM_MACEND;
    if (strcmp(text, "recall"    ) == 0) return OCSM_RECALL;
    if (strcmp(text, "patbeg"    ) == 0) return OCSM_PATBEG;
    if (strcmp(text, "patend"    ) == 0) return OCSM_PATEND;
    if (strcmp(text, "mark"      ) == 0) return OCSM_MARK;
    if (strcmp(text, "dump"      ) == 0) return OCSM_DUMP;

    /* Branch classes */
    if (strcmp(text, "primitive" ) == 0) return OCSM_PRIMITIVE;
    if (strcmp(text, "grown"     ) == 0) return OCSM_GROWN;
    if (strcmp(text, "applied"   ) == 0) return OCSM_APPLIED;
    if (strcmp(text, "boolean"   ) == 0) return OCSM_BOOLEAN;
    if (strcmp(text, "transform" ) == 0) return OCSM_TRANSFORM;
    if (strcmp(text, "sketch"    ) == 0) return OCSM_SKETCH;
    if (strcmp(text, "solver"    ) == 0) return OCSM_SOLVER;
    if (strcmp(text, "utility"   ) == 0) return OCSM_UTILITY;

    /* Branch activities */
    if (strcmp(text, "active"    ) == 0) return OCSM_ACTIVE;
    if (strcmp(text, "suppressed") == 0) return OCSM_SUPPRESSED;
    if (strcmp(text, "inactive"  ) == 0) return OCSM_INACTIVE;
    if (strcmp(text, "deferred"  ) == 0) return OCSM_DEFERRED;

    /* Body types */
    if (strcmp(text, "solid_body") == 0) return OCSM_SOLID_BODY;
    if (strcmp(text, "sheet_body") == 0) return OCSM_SHEET_BODY;
    if (strcmp(text, "wire_body" ) == 0) return OCSM_WIRE_BODY;
    if (strcmp(text, "node_body" ) == 0) return OCSM_NODE_BODY;

    /* Parameter types */
    if (strcmp(text, "external"  ) == 0) return OCSM_EXTERNAL;
    if (strcmp(text, "internal"  ) == 0) return OCSM_INTERNAL;

    return 0;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildApplied - implement OCSM_APPLIEDs for ocsmBuild               *
 *                                                                      *
 ************************************************************************
 */

static int
buildApplied(modl_T *modl,
             int    ibrch,
             int    *nstack,
             int    stack[],
             int    npatn,
             patn_T patn[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int        nelist, *ielist = NULL;
    int        itype, nlist, type, i, j, iedge, iface, nface, nedge;
    int        iford1, iford2, jford1, jford2, ibody, ibodyl, iprnt, jbrch, ipmtr, jpmtr;
    CINT       *tempIlist;
    double     args[10];
    CDOUBLE    *tempRlist;
    CCHAR      *tempClist;

    #if   defined(GEOM_CAPRI)
        int        ivol, ivoll, nnode, nbound, imodel, ibeg, mfile;
        double     matrix[3][4];
        char       *name, *mdum;
    #elif defined(GEOM_EGADS)
        int        nremove, nf;
        ego        ebody, ebodyl, *eedges, *efaces, eremove[10];
        ego        *eelist = NULL, *eflist = NULL;
    #endif

    ROUTINE(buildApplied);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    if (MODL->brch[ibrch].narg >= 3) {
        status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
        CHECK_STATUS(str2val:val3);
    } else {
        args[3] = 0;
    }
    if (MODL->brch[ibrch].narg >= 4) {
        status = str2val(MODL->brch[ibrch].arg4, MODL, &args[4]);
        CHECK_STATUS(str2val:val4);
    } else {
        args[4] = 0;
    }
    args[5] = 0;
    args[6] = 0;
    args[7] = 0;
    args[8] = 0;
    args[9] = 0;

    /* execute: "fillet radius edgeList" */
    if (type == OCSM_FILLET) {
        SPRINT3(1, "    executing [%4d] fillet:     %11.5f    %s",
                ibrch, args[1], &(MODL->brch[ibrch].arg2[1]));

        /* check for a positive radius */
        if (args[1] <= 0) {
            status = OCSM_ILLEGAL_ARGUMENT;
            CHECK_STATUS(fillet);
        }

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(fillet);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* check that ibodyl is not a Sketch */
        if (MODL->body[ibodyl].botype != OCSM_SOLID_BODY) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(fillet);
        }

        /* cycle back from ibodyl to find the closest left parent Body that
           was either a PRIMITIVE, GROWN, or BOOLEAN */
        iprnt = ibodyl;
        while (iprnt != 0) {
            jbrch = MODL->body[iprnt].ibrch;
            if (MODL->brch[jbrch].class == OCSM_PRIMITIVE ||
                MODL->brch[jbrch].class == OCSM_GROWN     ||
                MODL->brch[jbrch].class == OCSM_BOOLEAN     ) break;
            iprnt = MODL->body[iprnt].ileft;
        }

        /* initialize the list of Edges to which fillets should be applied */
        nelist = 0;
        MALLOC(ielist, int, MODL->body[ibodyl].nedge);

        /* apply fillet to all Edges if edgeList=0 OR iprnt is a Boolean */
        if (strcmp(MODL->brch[ibrch].arg2, "$0") == 0  ||
            MODL->body[iprnt].brtype == OCSM_INTERSECT ||
            MODL->body[iprnt].brtype == OCSM_SUBTRACT  ||
            MODL->body[iprnt].brtype == OCSM_UNION       ) {

            #if   defined(GEOM_CAPRI)
                for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                    ivoll  = MODL->body[ibodyl].ivol;
                    status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                 &itype, &nlist,
                                                 &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(gi_dGetEntAttribute);

                    if (tempIlist[0] == iprnt) {
                        ielist[nelist++] = iedge;
                    }
                }
            #elif defined(GEOM_EGADS)
                ebodyl = MODL->body[ibodyl].ebody;

                status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                CHECK_STATUS(EG_getBodyTopos);

                for (iedge = 1; iedge <= nedge; iedge++) {
                    status = EG_attributeRet(eedges[iedge-1], "body",
                                             &itype, &nlist,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeRet);

                    if (tempIlist[0] == iprnt) {
                        ielist[nelist++] = iedge;
                    }
                }

                EG_free(eedges);
            #endif

        /* otherwise, process the edgeList (in order) */
        } else {
            jpmtr = 0;
            for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                if (strcmp(MODL->pmtr[ipmtr].name, &(MODL->brch[ibrch].arg2[1])) == 0) {
                    jpmtr = ipmtr;
                    break;
                }
            }
            if (jpmtr == 0) {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(fillet);
            }

            for (i = 0; i < MODL->pmtr[jpmtr].nrow; i++) {
                iford1 = MODL->pmtr[jpmtr].value[2*i  ];
                iford2 = MODL->pmtr[jpmtr].value[2*i+1];

                /* add all Edges to ielist */
                if (iford1 == 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            if (tempIlist[0] == iprnt) {
                                ielist[nelist++] = iedge;
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            if (tempIlist[0] == iprnt) {
                                ielist[nelist++] = iedge;
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* add Edges adjacent to +iford1 */
                } else if (iford1 > 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == iford1 || jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == iford1 || jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* remove all Edges adjacent to -iford1 */
                } else if (iford1 < 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == -iford1 || jford2 == -iford1) {
                                    ielist[nelist++] = 0;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == -iford1 || jford2 == -iford1) {
                                    for (j = 0; j < nelist; j++) {
                                        if (ielist[j] == iedge) {
                                            ielist[j] = 0;
                                        }
                                    }
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* add Edges adjacent to +iford1 and +iford2 */
                } else if (iford1 > 0 && iford2 > 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if        (jford1 == iford1 && jford2 == iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == iford2 && jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                   #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if        (jford1 == iford1 && jford2 == iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == iford2 && jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* remove Edges adjacent to -iford1 and -iford2 */
                } else if (iford1 < 0 && iford2 < 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == 0) {
                                if        (jford1 == -iford1 && jford2 == -iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == -iford2 && jford2 == -iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == 0) {
                                if ((jford1 == -iford1 && jford2 == -iford2) ||
                                    (jford1 == -iford2 && jford2 == -iford1)   ) {
                                    for (j = 0; j < nelist; j++) {
                                        if (ielist[j] == iedge) {
                                            ielist[j] = 0;
                                        }
                                    }
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif
                }
            }
        }

        /* remove deleted entries from the ielist */
        for (j = 0; j < nelist; j++) {
            if (ielist[j] == 0) {
                ielist[j] = ielist[nelist-1];
                nelist--;
                j--;
            }
        }

        for (i = 0; i < nelist; i++) {
            DPRINT2("ielist[%3d] = %4d", i, ielist[i]);
        }
        DPRINT1("radius      = %10.5f", args[1]);

        /* create the fillets */
        if (nelist > 0) {

            /* create the new Body */
            status = newBody(MODL, ibrch, OCSM_FILLET, ibodyl, -1,
                             args, OCSM_SOLID_BODY, &ibody);
            CHECK_STATUS(newBody);

            #if   defined(GEOM_CAPRI)
                ivoll  = MODL->body[ibodyl].ivol;

                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                status = gi_fAddFillet(ivoll, nelist, ielist, args[1]);
                if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
                CHECK_STATUS(gi_gAddFillet);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);

                status = gi_iSetDisplace(ivol, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                FREE(ielist);

                MODL->body[ibody].ivol = ivol;
            #elif defined(GEOM_EGADS)
                ebodyl = MODL->body[ibodyl].ebody;

                status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                CHECK_STATUS(EG_getBodyTopos);

                MALLOC(eelist, ego, nelist);

                for (i = 0; i < nelist; i++) {
                    SPRINT1(2, "        fillet with iedge=%d", ielist[i]);
                    eelist[i] = eedges[ielist[i]-1];
                }

                EG_free(eedges);

                status = EG_filletBody(ebodyl, nelist, eelist, args[1], &ebody);
                CHECK_STATUS(EG_filletBody);

                FREE(ielist);
                FREE(eelist);

                MODL->body[ibody].ebody = ebody;
            #endif

            /* mark the new Faces with the current Body */
            #if   defined(GEOM_CAPRI)
                status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
                CHECK_STATUS(gi_dGetVolume);

                for (iface = 1; iface <= nface; iface++) {
                    status = gi_dGetEntAttribute(ivol, CAPRI_FACE, iface, "body", 0,
                                                 &itype, &nlist,
                                                 &tempIlist, &tempRlist, &tempClist);
                    if (status == CAPRI_NOTFOUND) {
                        status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                        CHECK_STATUS(setFaceAttribute);
                    } else {
                        CHECK_STATUS(gi_dGetEntAttribute);
                    }
                }
            #elif defined(GEOM_EGADS)
                status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
                CHECK_STATUS(EG_getBodyTopos);

                for (iface = 1; iface <= nface; iface++) {
                    status = EG_attributeRet(efaces[iface-1], "body",
                                             &itype, &nlist,
                                             &tempIlist, &tempRlist, &tempClist);
                    if (status == EGADS_NOTFOUND) {
                        status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                        CHECK_STATUS(setFaceAttribute);
                    } else {
                        CHECK_STATUS(EG_attributeRet);
                    }
                }

                EG_free(efaces);
            #endif

            /* finish the Body */
            status = finishBody(MODL, ibody);
            CHECK_STATUS(finishBody);
        } else {
            SPRINT0(0, "WARNING:: no edges for fillet");
            ibody = ibodyl;
        }

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "chamfer radius edgeList" */
    } else if (type == OCSM_CHAMFER) {
        SPRINT3(1, "    executing [%4d] chamfer:    %11.5f   %s",
                ibrch, args[1], &(MODL->brch[ibrch].arg2[1]));

        /* check for a positive radius */
        if (args[1] <= 0) {
            status = OCSM_ILLEGAL_ARGUMENT;
            CHECK_STATUS(chamfer);
        }

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(chamfer);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* check that ibodyl is not a Sketch */
        if (MODL->body[ibodyl].botype != OCSM_SOLID_BODY) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(chamfer);
        }

        /* cycle back from ibodyl to find the closest left parent Body that
           was either a PRIMITIVE, GROWN, or BOOLEAN */
        iprnt = ibodyl;
        while (iprnt != 0) {
            jbrch = MODL->body[iprnt].ibrch;
            if (MODL->brch[jbrch].class == OCSM_PRIMITIVE ||
                MODL->brch[jbrch].class == OCSM_GROWN     ||
                MODL->brch[jbrch].class == OCSM_BOOLEAN     ) break;
            iprnt = MODL->body[iprnt].ileft;
        }

        /* initialize the list of Edges to which chamfers should be applied */
        nelist = 0;
        MALLOC(ielist, int, MODL->body[ibodyl].nedge);

        /* apply chamfer to all Edges if edgeList=0 OR iprnt is a Boolean */
        if (strcmp(MODL->brch[ibrch].arg2, "$0") == 0  ||
            MODL->body[iprnt].brtype == OCSM_INTERSECT ||
            MODL->body[iprnt].brtype == OCSM_SUBTRACT  ||
            MODL->body[iprnt].brtype == OCSM_UNION       ) {

            #if   defined(GEOM_CAPRI)
                for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                    ivoll  = MODL->body[ibodyl].ivol;
                    status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                 &itype, &nlist,
                                                 &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(gi_dGetEntAttribute);

                    if (tempIlist[0] == iprnt) {
                        ielist[nelist++] = iedge;
                    }
                }
            #elif defined(GEOM_EGADS)
                ebodyl = MODL->body[ibodyl].ebody;

                status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                CHECK_STATUS(EG_getBodyTopos);

                for (iedge = 1; iedge <= nedge; iedge++) {
                    status = EG_attributeRet(eedges[iedge-1], "body",
                                             &itype, &nlist,
                                             &tempIlist, &tempRlist, &tempClist);
                    CHECK_STATUS(EG_attributeRet);

                    if (tempIlist[0] == iprnt) {
                        ielist[nelist++] = iedge;
                    }
                }

                EG_free(eedges);
            #endif

        /* otherwise, process the edgeList (in order) */
        } else {
            jpmtr = 0;
            for (ipmtr = 1; ipmtr <= MODL->npmtr; ipmtr++) {
                if (strcmp(MODL->pmtr[ipmtr].name, &(MODL->brch[ibrch].arg2[1])) == 0) {
                    jpmtr = ipmtr;
                    break;
                }
            }
            if (jpmtr == 0) {
                status = OCSM_ILLEGAL_PMTR_NAME;
                CHECK_STATUS(chanfer);
            }

            for (i = 0; i < MODL->pmtr[jpmtr].nrow; i++) {
                iford1 = MODL->pmtr[jpmtr].value[2*i  ];
                iford2 = MODL->pmtr[jpmtr].value[2*i+1];

                /* add all Edges to ielist */
                if (iford1 == 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            if (tempIlist[0] == iprnt) {
                                ielist[nelist++] = iedge;
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            if (tempIlist[0] == iprnt) {
                                ielist[nelist++] = iedge;
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* add Edges adjacent to +iford1 */
                } else if (iford1 > 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == iford1 || jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == iford1 || jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* remove all Edges adjacent to -iford1 */
                } else if (iford1 < 0 && iford2 == 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == -iford1 || jford2 == -iford1) {
                                    ielist[nelist++] = 0;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if (jford1 == -iford1 || jford2 == -iford1) {
                                    for (j = 0; j < nelist; j++) {
                                        if (ielist[j] == iedge) {
                                            ielist[j] = 0;
                                        }
                                    }
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* add Edges adjacent to +iford1 and +iford2 */
                } else if (iford1 > 0 && iford2 > 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if        (jford1 == iford1 && jford2 == iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == iford2 && jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                   #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == iprnt) {
                                if        (jford1 == iford1 && jford2 == iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == iford2 && jford2 == iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif

                /* remove Edges adjacent to -iford1 and -iford2 */
                } else if (iford1 < 0 && iford2 < 0) {
                    #if   defined(GEOM_CAPRI)
                        ivoll = MODL->body[ibodyl].ivol;

                        for (iedge = 1; iedge <= MODL->body[ibodyl].nedge; iedge++) {
                            status = gi_dGetEntAttribute(ivoll, CAPRI_EDGE, iedge, "body", 0,
                                                         &itype, &nlist,
                                                         &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(gi_dGetEntAttribute);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == 0) {
                                if        (jford1 == -iford1 && jford2 == -iford2) {
                                    ielist[nelist++] = iedge;
                                } else if (jford1 == -iford2 && jford2 == -iford1) {
                                    ielist[nelist++] = iedge;
                                }
                            }
                        }
                    #elif defined(GEOM_EGADS)
                        ebodyl = MODL->body[ibodyl].ebody;

                        status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                        CHECK_STATUS(EG_getBodyTopos);

                        for (iedge = 1; iedge <= nedge; iedge++) {
                            status = EG_attributeRet(eedges[iedge-1], "body",
                                                     &itype, &nlist,
                                                     &tempIlist, &tempRlist, &tempClist);
                            CHECK_STATUS(EG_attributeRet);

                            jford1 = tempIlist[1] % 100;
                            jford2 = tempIlist[1] / 100;

                            if (tempIlist[0] == 0) {
                                if ((jford1 == -iford1 && jford2 == -iford2) ||
                                    (jford1 == -iford2 && jford2 == -iford1)   ) {
                                    for (j = 0; j < nelist; j++) {
                                        if (ielist[j] == iedge) {
                                            ielist[j] = 0;
                                        }
                                    }
                                }
                            }
                        }

                        EG_free(eedges);
                    #endif
                }
            }
        }

        /* remove deleted entries from the ielist */
        for (j = 0; j < nelist; j++) {
            if (ielist[j] == 0) {
                ielist[j] = ielist[nelist-1];
                nelist--;
                j--;
            }
        }

        for (i = 0; i < nelist; i++) {
            DPRINT2("ielist[%3d] = %4d", i, ielist[i]);
        }
        DPRINT1("radius      = %10.5f", args[1]);

        /* create the chamfers */
        if (nelist > 0) {

            /* create the new Body */
            status = newBody(MODL, ibrch, OCSM_CHAMFER, ibodyl, -1,
                             args, OCSM_SOLID_BODY, &ibody);
            CHECK_STATUS(newBody);

            #if   defined(GEOM_CAPRI)
                ivoll  = MODL->body[ibodyl].ivol;

                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                status = gi_fAddChamfer(ivoll, nelist, ielist, args[1], 45.0);
                if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
                CHECK_STATUS(gi_gAddChamfer);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);

                status = gi_iSetDisplace(ivol, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                FREE(ielist);

                MODL->body[ibody].ivol = ivol;
            #elif defined(GEOM_EGADS)
                ebodyl = MODL->body[ibodyl].ebody;

                status = EG_getBodyTopos(ebodyl, NULL, EDGE, &nedge, &eedges);
                CHECK_STATUS(EG_getBodyTopos);

                MALLOC(eelist, ego, nelist);
                MALLOC(eflist, ego, nelist);

                for (i = 0; i < nelist; i++) {
                    SPRINT1(2, "        chamfer with iedge=%d", ielist[i]);
                    eelist[i] = eedges[ielist[i]-1];

                    status = EG_getBodyTopos(ebodyl, eelist[i], FACE, &nf, &efaces);
                    CHECK_STATUS(EG_getBodyTopos);

                    eflist[i] = efaces[0];
                    EG_free(efaces);
                }

                EG_free(eedges);

                status = EG_chamferBody(ebodyl, nelist, eelist, eflist, args[1], args[1], &ebody);
                CHECK_STATUS(EG_chamferBody);

                FREE(ielist);
                FREE(eelist);
                FREE(eflist);

                MODL->body[ibody].ebody = ebody;
            #endif

            /* mark the new Faces with the current Body */
            #if   defined(GEOM_CAPRI)
                status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
                CHECK_STATUS(gi_dGetVolume);

                for (iface = 1; iface <= nface; iface++) {
                    status = gi_dGetEntAttribute(ivol, CAPRI_FACE, iface, "body", 0,
                                                 &itype, &nlist,
                                                 &tempIlist, &tempRlist, &tempClist);
                    if (status == CAPRI_NOTFOUND) {
                        status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                        CHECK_STATUS(setFaceAttribute);
                    } else {
                        CHECK_STATUS(gi_dGetEntAttribute);
                    }
                }
            #elif defined(GEOM_EGADS)
                status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
                CHECK_STATUS(EG_getBodyTopos);

                for (iface = 1; iface <= nface; iface++) {
                    status = EG_attributeRet(efaces[iface-1], "body",
                                             &itype, &nlist,
                                             &tempIlist, &tempRlist, &tempClist);
                    if (status == EGADS_NOTFOUND) {
                        status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                        CHECK_STATUS(setFaceAttribute);
                    } else {
                        CHECK_STATUS(EG_attributeRet);
                    }
                }

                EG_free(efaces);
            #endif

            /* finish the Body */
            status = finishBody(MODL, ibody);
            CHECK_STATUS(finishBody);
        } else {
            SPRINT0(0, "WARNING:: no edges for chamfer");
            ibody = ibodyl;
        }

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "hollow thick iface1 iface2 iface3 iface4 iface5 iface6" */
    } else if (type == OCSM_HOLLOW) {
        SPRINT8(1, "    executing [%4d] hollow:     %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4], args[5], args[6], args[7]);

        /* pop an Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(hollow);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_HOLLOW, ibodyl, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        /* generate the hollow */
        #if   defined(GEOM_CAPRI)
            status = OCSM_UNSUPPORTED;
            CHECK_STATUS(hollow);
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            status = EG_getBodyTopos(ebodyl, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            nremove = 0;
            if (NINT(args[2]) > 0) {
                eremove[nremove++] = efaces[NINT(args[2])-1];
            }
            if (NINT(args[3]) > 0) {
                eremove[nremove++] = efaces[NINT(args[3])-1];
            }
            if (NINT(args[4]) > 0) {
                eremove[nremove++] = efaces[NINT(args[4])-1];
            }
            if (NINT(args[5]) > 0) {
                eremove[nremove++] = efaces[NINT(args[5])-1];
            }
            if (NINT(args[6]) > 0) {
                eremove[nremove++] = efaces[NINT(args[6])-1];
            }
            if (NINT(args[7]) > 0) {
                eremove[nremove++] = efaces[NINT(args[7])-1];
            }

            EG_free(efaces);

            status = EG_hollowBody(ebodyl, nremove, eremove, args[1], 1, &ebody);
            CHECK_STATUS(EG_hollowBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            iford1 = 0;
            for (iface = 1; iface <= nface; iface++) {
                status = EG_attributeRet(efaces[iface-1], "body",
                                         &itype, &nlist,
                                         &tempIlist, &tempRlist, &tempClist);

                if (status == EGADS_NOTFOUND) {
                    iford1++;
                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                } else {
                    CHECK_STATUS(EG_attributeRet);
                }
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);
    }

cleanup:
    FREE(ielist);

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildBoolean - implement OCSM_BOOLEANs for ocsmBuild               *
 *                                                                      *
 ************************************************************************
 */

static int
buildBoolean(modl_T *modl,
             int    ibrch,
             int    *nstack,
             int    stack[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int        type;
    int        ibody, ibodyl, ibodyr, index;
    double     args[10];
    char       order[MAX_EXPR_LEN];

    #if   defined(GEOM_CAPRI)
    int        ivol, ivoll, ivolr, imodel, ibeg, iend, mfile;
        char       *mdum;
    #elif defined(GEOM_EGADS)
        int         oclass, mtype, nchild, *senses, i, nface, nshell;
        double      data[20];
        ego         ebody, ebodyl, ebodyr, emodel, eref, *ebodys, *echilds=NULL, etemp;
        ego         *eshells=NULL, *efaces=NULL;
    #endif

    ROUTINE(buildBoolean);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    args[3] = 0;
    args[4] = 0;
    args[5] = 0;
    args[6] = 0;
    args[7] = 0;
    args[8] = 0;
    args[9] = 0;

    /* execute: "intersect order index" */
    if (type == OCSM_INTERSECT) {
        SPRINT3(1, "    executing [%4d] intersect:  %s %11.5f",
                ibrch, &(MODL->brch[ibrch].arg1[1]), args[2]);

        /* pop 2 Bodys from the stack */
        if ((*nstack) < 2) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(intersect);
        } else {
            ibodyr = stack[--(*nstack)];
            ibodyl = stack[--(*nstack)];
        }

        /* check that ibodyl and ibodyr are not Sketches */
        if        (MODL->body[ibodyl].botype != OCSM_SOLID_BODY &&
                   MODL->body[ibodyl].botype != OCSM_SHEET_BODY   ) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(intersect);
        } else if (MODL->body[ibodyr].botype != OCSM_SOLID_BODY) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(intersect);
        }

        /* extract the arguments */
        strcpy(order, &(MODL->brch[ibrch].arg1[1]));
        index = NINT(args[2]);

        /* create the new Body */
        if (MODL->body[ibodyl].botype == OCSM_SOLID_BODY) {
            status = newBody(MODL, ibrch, OCSM_INTERSECT, ibodyl, ibodyr,
                             args, OCSM_SOLID_BODY, &ibody);
            CHECK_STATUS(newBody);
        } else {
            status = newBody(MODL, ibrch, OCSM_INTERSECT, ibodyl, ibodyr,
                             args, OCSM_SHEET_BODY, &ibody);
            CHECK_STATUS(newBody);
        }

        #if   defined(GEOM_CAPRI)
            ivoll  = MODL->body[ibodyl].ivol;
            ivolr  = MODL->body[ibodyr].ivol;
            status = gi_gInterVolume(ivoll, ivolr);
            CHECK_STATUS(gi_gInterVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &iend, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            if (strcmp(order, "none") != 0) {
                SPRINT0(0, "WARNING:: 'none' being assumed as order in CAPRI");
            }

            ivol = ibeg + index - 1;
            if (ivol > iend) {
                status = OCSM_NOT_ENOUGH_BODYS_PRODUCED;
                CHECK_STATUS(gi_gInterVolume);
            }

            if (iend-ibeg > 0) {
                SPRINT1(0, "WARNING:: %d Bodys are being lost", iend-ibeg);
            }

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            if (MODL->body[ibodyl].botype == OCSM_SOLID_BODY) {
                ebodyl = MODL->body[ibodyl].ebody;
                ebodyr = MODL->body[ibodyr].ebody;
            } else {
                status = EG_copyObject(MODL->body[ibodyl].ebody, NULL, &ebody);
                CHECK_STATUS(EG_copyObject);

                status = EG_makeTopology(MODL->context, NULL, MODEL, 0,
                                         NULL, 1, &ebody, NULL, &ebodyl);
                CHECK_STATUS(EG_makeTopology);

                ebodyr = MODL->body[ibodyr].ebody;
            }

            status = EG_solidBoolean(ebodyl, ebodyr, INTERSECTION, &emodel);
            CHECK_STATUS(EG_solidBoolean);

            status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                                    data, &nchild, &ebodys, &senses);
            CHECK_STATUS(EG_getTopology);

            if (nchild < 1) {
                status = OCSM_NO_BODYS_PRODUCED;
                CHECK_STATUS(intersect);
            } else if (index > nchild) {
                status = OCSM_NOT_ENOUGH_BODYS_PRODUCED;
                CHECK_STATUS(intersect);
            } else {
                i = selectBody(emodel, order, index);

                status = EG_copyObject(ebodys[i], NULL, &ebody);
                CHECK_STATUS(EG_copyObject);

                MODL->body[ibody].ebody = ebody;

                if (nchild > 1) {
                    SPRINT1(0, "WARNING:: %d Bodys are being lost", nchild-1);
                }
            }

            status = EG_deleteObject(emodel);
            CHECK_STATUS(EG_deleteObject);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "subtract order index" */
    } else if (type == OCSM_SUBTRACT) {
        SPRINT3(1, "    executing [%4d] subtract:   %s %11.5f",
                ibrch, &(MODL->brch[ibrch].arg1[1]), args[2]);

        /* pop 2 Bodys from the stack */
        if ((*nstack) < 2) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(subtract);
        } else {
            ibodyr = stack[--(*nstack)];
            ibodyl = stack[--(*nstack)];
        }

        /* check that ibodyl and ibodyr are not Sketches */
        if        (MODL->body[ibodyl].botype != OCSM_SOLID_BODY) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(subtract);
        } else if (MODL->body[ibodyr].botype != OCSM_SOLID_BODY) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(subtract);
        }

        /* extract the arguments */
        strcpy(order, &(MODL->brch[ibrch].arg1[1]));
        index = NINT(args[2]);

        /* create the new Body */
        status = newBody(MODL, ibrch, OCSM_SUBTRACT, ibodyl, ibodyr,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            ivoll  = MODL->body[ibodyl].ivol;
            ivolr  = MODL->body[ibodyr].ivol;
            status = gi_gSubVolume(ivoll, ivolr);
            CHECK_STATUS(gi_gSubVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &iend, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            if (strcmp(order, "none") != 0) {
                SPRINT0(0, "WARNING:: 'none' being assumed as order in CAPRI");
            }

            ivol = ibeg + index - 1;
            if (ivol > iend) {
                status = OCSM_NOT_ENOUGH_BODYS_PRODUCED;
                CHECK_STATUS(gi_gSubVolume);
            }

            if (iend-ibeg > 0) {
                SPRINT1(0, "WARNING:: %d Bodys are being lost", iend-ibeg);
            }

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;
            ebodyr = MODL->body[ibodyr].ebody;

            status = EG_solidBoolean(ebodyl, ebodyr, SUBTRACTION, &emodel);
            CHECK_STATUS(EG_solidBoolean);

            status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                                    data, &nchild, &ebodys, &senses);
            CHECK_STATUS(EG_getTopology);

            if (nchild < 1) {
                status = OCSM_NO_BODYS_PRODUCED;
                CHECK_STATUS(subtract);
            } else if (index > nchild) {
                status = OCSM_NOT_ENOUGH_BODYS_PRODUCED;
                CHECK_STATUS(subtract);
            } else {
                i = selectBody(emodel, order, index);

                status = EG_copyObject(ebodys[i], NULL, &ebody);
                CHECK_STATUS(EG_copyObject);

                MODL->body[ibody].ebody = ebody;

                if (nchild > 1) {
                    SPRINT1(0, "WARNING:: %d Bodys are being lost", nchild-1);
                }
            }

            status = EG_deleteObject(emodel);
            CHECK_STATUS(EG_deleteObject);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "union" */
    } else if (type == OCSM_UNION) {
        SPRINT1(1, "    executing [%4d] union:",
                ibrch);

        /* pop 2 Bodys from the stack */
        if ((*nstack) < 2) {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(union);
        } else {
            ibodyr = stack[--(*nstack)];
            ibodyl = stack[--(*nstack)];
        }

        if (MODL->body[ibodyl].botype == OCSM_SOLID_BODY &&
            MODL->body[ibodyr].botype == OCSM_SOLID_BODY   ) {

            /* create the new Body */
            status = newBody(MODL, ibrch, OCSM_UNION, ibodyl, ibodyr,
                             args, OCSM_SOLID_BODY, &ibody);
            CHECK_STATUS(newBody);

            #if   defined(GEOM_CAPRI)
                ivoll  = MODL->body[ibodyl].ivol;
                ivolr  = MODL->body[ibodyr].ivol;
                status = gi_gUnionVolume(ivoll, ivolr);
                CHECK_STATUS(gi_gUnionVolume);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);

                MODL->body[ibody].ivol = ivol;
            #elif defined(GEOM_EGADS)
                ebodyl = MODL->body[ibodyl].ebody;
                ebodyr = MODL->body[ibodyr].ebody;

                status = EG_solidBoolean(ebodyl, ebodyr, FUSION, &emodel);
                CHECK_STATUS(EG_solidBoolean);

                status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                                        data, &nchild, &ebodys, &senses);
                CHECK_STATUS(EG_getTopology);

                if (nchild == 1) {
                    status = EG_copyObject(ebodys[0], NULL, &ebody);
                    CHECK_STATUS(EG_copyObject);

                    MODL->body[ibody].ebody = ebody;
                } else {
                    status = OCSM_DID_NOT_CREATE_BODY;
                    CHECK_STATUS(union);
                }

                status = EG_deleteObject(emodel);
                CHECK_STATUS(EG_deleteObject);
            #endif

            /* finish the Body */
            status = finishBody(MODL, ibody);
            CHECK_STATUS(finishBody);

        } else if (MODL->body[ibodyl].botype == OCSM_SHEET_BODY &&
                   MODL->body[ibodyr].botype == OCSM_SHEET_BODY    ) {

            #if   defined(GEOM_CAPRI)
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(union);
            #elif defined(GEOM_EGADS)
                /* create the new Body */
                status = newBody(MODL, ibrch, OCSM_UNION, ibodyl, ibodyr,
                                 args, OCSM_SHEET_BODY, &ibody);
                CHECK_STATUS(newBody);

                status = EG_copyObject(MODL->body[ibodyl].ebody, NULL, &etemp);
                CHECK_STATUS(EG_copyObject);

                status = EG_makeTopology(MODL->context, NULL, MODEL, 0,
                                         NULL, 1, &etemp, NULL, &ebodyl);
                CHECK_STATUS(EG_makeTopology);

                ebodyr = MODL->body[ibodyr].ebody;

                /* if ebodyr is a SHEETBODY that contains only 1 Face, set
                   ebodyr to point to that Face instead */
                status = EG_getTopology(ebodyr, &eref, &oclass, &mtype, data, &nshell, &eshells, &senses);
                CHECK_STATUS(EG_getTopology);

                if (oclass == BODY && mtype == SHEETBODY) {
                    if (nshell == 1) {
                        status = EG_getTopology(eshells[0], &eref, &oclass, &mtype, data, &nface, &efaces, &senses);
                        CHECK_STATUS(EG_getTopology);

                        if (oclass == SHELL && nface == 1) {
                            ebodyr = efaces[0];
                        } else {
                            status = OCSM_INTERNAL_ERROR;
                            CHECK_STATUS(right_body_not_single_face);
                        }
                    } else {
                        status = OCSM_INTERNAL_ERROR;
                        CHECK_STATUS(right_body_not_single_shell);
                    }
                }

                status = EG_solidBoolean(ebodyl, ebodyr, FUSION, &emodel);
                CHECK_STATUS(EG_solidBoolean);

                status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                                        data, &nchild, &ebodys, &senses);
                CHECK_STATUS(EG_getTopology);

                if (nchild == 1) {
                    status = EG_copyObject(ebodys[0], NULL, &ebody);
                    CHECK_STATUS(EG_copyObject);

                    MODL->body[ibody].ebody = ebody;
                } else {
                    status = OCSM_DID_NOT_CREATE_BODY;
                    CHECK_STATUS(union);
                }

                status = EG_deleteObject(emodel);
                CHECK_STATUS(EG_deleteObject);

                /* finish the Body */
                status = finishBody(MODL, ibody);
                CHECK_STATUS(finishBody);
            #endif

        } else {
            status = OCSM_EXPECTING_TWO_BODYS;
            CHECK_STATUS(union);
        }

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);
    }

cleanup:
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        FREE(echilds);
    #endif

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildGrown - implement OCSM_GROWNs for ocsmBuild                   *
 *                                                                      *
 ************************************************************************
 */

static int
buildGrown(modl_T *modl,
           int    ibrch,
           int    *nstack,
           int    stack[],
           int    npatn,
           patn_T patn[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       type, iface, nface, loftOpts;
    int       numSketches, ibody, ibodyl;
    double    args[10], dirn[3], alen;

    #if   defined(GEOM_CAPRI)
        int       ivol, ivoll, ivolr, sketches[MAX_NUM_SKETCHES], *errs;
        int       itype, nnode, nbound, imodel, ibeg, mfile, nedge;
        double    dot, orig[3], xaxis[3], yaxis[3];
        char      *name, *mdum;

        char       *server      = NULL;
        char       modeller[10] = "Parasolid";
    #elif defined(GEOM_EGADS)
        int        nloops, iford1;
        ego        ebody, ebodyl, *efaces, *echildren, esketches[MAX_NUM_SKETCHES];
    #endif

    ROUTINE(buildGrown);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    if (MODL->brch[ibrch].narg >= 3) {
        status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
        CHECK_STATUS(str2val:val3);
    } else {
        args[3] = 0;
    }
    if (MODL->brch[ibrch].narg >= 4) {
        status = str2val(MODL->brch[ibrch].arg4, MODL, &args[4]);
        CHECK_STATUS(str2val:val4);
    } else {
        args[4] = 0;
    }
    if (MODL->brch[ibrch].narg >= 5) {
        status = str2val(MODL->brch[ibrch].arg5, MODL, &args[5]);
        CHECK_STATUS(str2val:val5);
    } else {
        args[5] = 0;
    }
    if (MODL->brch[ibrch].narg >= 6) {
        status = str2val(MODL->brch[ibrch].arg6, MODL, &args[6]);
        CHECK_STATUS(str2val:val6);
    } else {
        args[6] = 0;
    }
    if (MODL->brch[ibrch].narg >= 7) {
        status = str2val(MODL->brch[ibrch].arg7, MODL, &args[7]);
        CHECK_STATUS(str2val:val7);
    } else {
        args[7] = 0;
    }
    args[8] = 0;
    args[9] = 0;

    /* execute: "extrude dx dy dz" */
    if (type == OCSM_EXTRUDE) {
        SPRINT4(1, "    executing [%4d] extrude:    %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* pop a Sketch from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(extrude);
        } else {
                ibodyl = stack[--(*nstack)];
        }

        alen = sqrt(SQR(args[1]) + SQR(args[2]) + SQR(args[3]));
        dirn[0] = args[1] / alen;
        dirn[1] = args[2] / alen;
        dirn[2] = args[3] / alen;

        /* check that ibodyl is a Sketch */
        if (MODL->body[ibodyl].botype != OCSM_SHEET_BODY &&
            MODL->body[ibodyl].botype != OCSM_WIRE_BODY    ) {
            status = OCSM_EXPECTING_ONE_SKETCH;
            CHECK_STATUS(extrude);
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_EXTRUDE, ibodyl, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            sketches[0] = -(MODL->body[ibodyl].ivol);

            status = gi_gExtrude(server, modeller, 0, 0, sketches[0], alen, dirn);
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gExtrude);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            status = EG_extrude(ebodyl, alen, dirn, &ebody);
            CHECK_STATUS(EG_extrude);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface-2; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, iface+2, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = setFaceAttribute(MODL, ibody, nface-1, 2, npatn, patn);
            CHECK_STATUS(setFaceAttribute);

            status = setFaceAttribute(MODL, ibody, nface, 1, npatn, patn);
            CHECK_STATUS(setFaceAttribute);

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                if        (iface == nface-1) {
                    iford1 = 1;
                } else if (iface == nface  ) {
                    iford1 = 2;
                } else {
                    iford1 = iface + 2;
                    }
                status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "loft smooth" */
    } else if (type == OCSM_LOFT) {
        SPRINT2(1, "    executing [%4d] loft:       %11.5f",
                ibrch, args[1]);

        /* pop Sketches from the stack (until the Mark) */
        numSketches = 0;
        ibodyl      = 0;
        loftOpts    = -1;
        while ((*nstack) > 0) {
            ibodyl = stack[--(*nstack)];

            if (ibodyl == 0) {
                break;
            } else {
                if (loftOpts >= 0) {
                } else if (MODL->body[ibodyl].botype == OCSM_NODE_BODY) {
                    status = newBody(MODL, ibrch, OCSM_LOFT, ibodyl, ibodyl,
                                     args, OCSM_SOLID_BODY, &ibody);
                    CHECK_STATUS(newBody);

                    loftOpts = 1;
                } else if (MODL->body[ibodyl].botype == OCSM_SHEET_BODY) {
                    status = newBody(MODL, ibrch, OCSM_LOFT, ibodyl, ibodyl,
                                     args, OCSM_SOLID_BODY, &ibody);
                    CHECK_STATUS(newBody);

                    loftOpts = 1;
                } else if (MODL->body[ibodyl].botype == OCSM_WIRE_BODY) {
                    status = newBody(MODL, ibrch, OCSM_LOFT, ibodyl, ibodyl,
                                     args, OCSM_SHEET_BODY, &ibody);
                    CHECK_STATUS(newBody);

                    loftOpts = 0;
                }

                #if   defined(GEOM_CAPRI)
                    sketches[numSketches++] = -(MODL->body[ibodyl].ivol);
                #elif defined(GEOM_EGADS)
                    ebodyl = MODL->body[ibodyl].ebody;

                    if (MODL->body[ibodyl].botype == OCSM_NODE_BODY) {
                        esketches[numSketches++] = ebodyl;
                    } else {
                        status = EG_getBodyTopos(ebodyl, NULL, LOOP, &nloops, &echildren);
                        CHECK_STATUS(EG_getBodyTopos);

                        esketches[numSketches++] = echildren[0];

                        EG_free(echildren);
                    }
                #endif

                MODL->body[ibody ].ileft = ibodyl;
                MODL->body[ibodyl].ichld = ibody;
            }
        }

        if (numSketches < 2) {
            status = OCSM_EXPECTING_NLOFT_SKETCHES;
            CHECK_STATUS(loft);
        } else {
            SPRINT1(1, "                          lofting %d Sketches...", numSketches);
        }

        if (NINT(args[1]) != 1) {
            loftOpts += 2;
        }

        /* create the Body */
        #if   defined(GEOM_CAPRI)
            /* itype=0 for solid-body, itype=1 for sheet-body */
            /* do not forget "setenv CAPRIshell ON" if you want to create shells */
            itype = 0;
            status = gi_gLoft(server, modeller, 0, itype, numSketches, sketches);
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gLoft);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_loft(numSketches, esketches, loftOpts, &ebody);
            CHECK_STATUS(EG_loft);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                if        (iface == nface-1) {
                    iford1 = 1;
                } else if (iface == nface  ) {
                    iford1 = 2;
                } else {
                    iford1 = iface + 2;
                }
                status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "revolve xorig yorig zorig dxaxis dyaxis dzaxis angDeg" */
    } else if (type == OCSM_REVOLVE) {
        SPRINT8(1, "    executing [%4d] revolve:    %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6], args[7]);

        /* pop a Sketch from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(revolve);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* check that ibodyl is a Sketch */
        if (MODL->body[ibodyl].botype != OCSM_SHEET_BODY) {
            status = OCSM_EXPECTING_ONE_SKETCH;
            CHECK_STATUS(revolve);
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_REVOLVE, ibodyl, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            sketches[0] = -(MODL->body[ibodyl].ivol);
            if (args[7] < 359.9) {
                status = gi_gRevolve(server, modeller, 0, 0, sketches[0], &(args[1]), &(args[4]), args[7]);
                if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
                CHECK_STATUS(gi_gRevolve);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);
            } else {
                status = gi_gRevolve(server, modeller, 0, 0, sketches[0], &(args[1]), &(args[4]), +180.0);
                if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
                CHECK_STATUS(gi_gRevolve);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivoll, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);

                status = gi_gRevolve(server, modeller, 0, 0, sketches[0], &(args[1]), &(args[4]), -180.0);
                if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
                CHECK_STATUS(gi_gRevolve);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivolr, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);

                status = gi_gUnionVolume(ivoll, ivolr);
                CHECK_STATUS(gi_gUnionVolume);

                imodel = status;
                status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
                CHECK_STATUS(gi_dGetModel);
            }

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            status = EG_rotate(ebodyl, args[7], &(args[1]), &ebody);
            CHECK_STATUS(EG_rotate);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            /* find the dot product of the sketches' x-axis and the rotation axis
               (to be used to set iford=1 and iford=2 below) */
            status = gi_sOpen(0, sketches[0], orig, xaxis, yaxis);
            CHECK_STATUS(gi_sOpen);

            status = gi_sClose();
            CHECK_STATUS(gi_sClose);

            dot = xaxis[0] * args[4] + xaxis[1] * args[5] + xaxis[2] * args[6];

            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface-2; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, iface+2, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            if (dot > 0.5) {
                status = setFaceAttribute(MODL, ibody, nface-1, 2, npatn, patn);
                CHECK_STATUS(setFaceAttribute);

                status = setFaceAttribute(MODL, ibody, nface, 1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            } else {
                status = setFaceAttribute(MODL, ibody, nface-1, 1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);

                status = setFaceAttribute(MODL, ibody, nface, 2, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                if        (iface == nface-1) {
                    iford1 = 1;
                } else if (iface == nface  ) {
                    iford1 = 2;
                } else {
                    iford1 = iface + 2;
                }
                status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildPrimitive - implement OCSM_PRIMITIVEs for ocsmBuild           *
 *                                                                      *
 ************************************************************************
 */

static int
buildPrimitive(modl_T *modl,
               int    ibrch,
               int    *nstack,
               int    stack[],
               int    npatn,
               patn_T patn[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int        type, iface, nface, ibody;
    double     args[10];

    #if   defined(GEOM_CAPRI)
        int        ivol, *errs, nnode, nbound, imodel, ibeg, mfile, nedge;
        char       *name, *mdum;

        /* order that faces come back from Parasolid */
        /*                        zmax ymin xmin ymax zmin xmax */
        int        ford_box[6] = {   6,   3,   1,   4,   5,   2};
        /*                         min  end  beg  max */
        int        ford_cyl[4] = {   3,   2,   1,   4};
        /*                         min base  max */
        int        ford_con[3] = {   2,   1,   3};
        /*                         max  min */
        int        ford_sph[2] = {   1,   2};
        /*                         pt1  pt2  pt3  pt4 */
        int        ford_tor[4] = {   1,   2,   3,   4};

        char       *server      = NULL;
        char       modeller[10] = "Parasolid";
    #elif defined(GEOM_EGADS)
        int         oclass, mtype, iford1, udp_num, *udp_types, *udp_idef, udp_nmesh;
        int         ipmtr, jpmtr, ij, old_outLevel;
        double      *udp_ddef, toler;
        char        **udp_names, *udp_errStr, primtype[MAX_EXPR_LEN], foo[MAX_EXPR_LEN];
        char        dumpfile[MAX_EXPR_LEN], argname[MAX_EXPR_LEN], argvalue[MAX_STR_LEN];
        ego         ebody, *efaces, topRef, prev, next;
    #endif

    ROUTINE(buildPrimitive);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    if (MODL->brch[ibrch].narg >= 3) {
        status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
        CHECK_STATUS(str2val:val3);
    } else {
        args[3] = 0;
    }
    if (MODL->brch[ibrch].narg >= 4) {
        status = str2val(MODL->brch[ibrch].arg4, MODL, &args[4]);
        CHECK_STATUS(str2val:val4);
    } else {
        args[4] = 0;
    }
    if (MODL->brch[ibrch].narg >= 5) {
        status = str2val(MODL->brch[ibrch].arg5, MODL, &args[5]);
        CHECK_STATUS(str2val:val5);
    } else {
        args[5] = 0;
    }
    if (MODL->brch[ibrch].narg >= 6) {
        status = str2val(MODL->brch[ibrch].arg6, MODL, &args[6]);
        CHECK_STATUS(str2val:val6);
    } else {
        args[6] = 0;
    }
    if (MODL->brch[ibrch].narg >= 7) {
        status = str2val(MODL->brch[ibrch].arg7, MODL, &args[7]);
        CHECK_STATUS(str2val:val7);
    } else {
        args[7] = 0;
    }
    if (MODL->brch[ibrch].narg >= 8) {
        status = str2val(MODL->brch[ibrch].arg8, MODL, &args[8]);
        CHECK_STATUS(str2val:val8);
    } else {
        args[8] = 0;
    }
    if (MODL->brch[ibrch].narg >= 9) {
        status = str2val(MODL->brch[ibrch].arg9, MODL, &args[9]);
        CHECK_STATUS(str2val:val8);
    } else {
        args[9] = 0;
    }

    /* execute: "box xbase ybase zbase dx dy dz" */
    if (type == OCSM_BOX) {
        SPRINT7(1, "    executing [%4d] box:        %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6]);

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_BOX, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            status = gi_gCreateVolume(server, modeller, BOX, &(args[1]));
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gCreateVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_makeSolidBody(MODL->context, BOX, &(args[1]), &ebody);
            CHECK_STATUS(EG_makeSolidBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, ford_box[iface-1], npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            iford1 = 0;
            for (iface = 1; iface <= nface; iface++) {
                if        (faceContains(efaces[iface-1], args[1],
                                                         args[2]+args[5]/2,
                                                         args[3]+args[6]/2)) {
                    iford1 = 1;
                } else if (faceContains(efaces[iface-1], args[1]+args[4],
                                                         args[2]+args[5]/2,
                                                         args[3]+args[6]/2)) {
                    iford1 = 2;
                } else if (faceContains(efaces[iface-1], args[1]+args[4]/2,
                                                         args[2],
                                                         args[3]+args[6]/2)) {
                    iford1 = 3;
                } else if (faceContains(efaces[iface-1], args[1]+args[4]/2,
                                                         args[2]+args[5],
                                                         args[3]+args[6]/2)) {
                    iford1 = 4;
                } else if (faceContains(efaces[iface-1], args[1]+args[4]/2,
                                                         args[2]+args[5]/2,
                                                         args[3])          ) {
                    iford1 = 5;
                } else if (faceContains(efaces[iface-1], args[1]+args[4]/2,
                                                         args[2]+args[5]/2,
                                                         args[3]+args[6])  ) {
                    iford1 = 6;
                }

                status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "sphere xcent ycent zcent radius" */
    } else if (type == OCSM_SPHERE) {
        SPRINT5(1, "    executing [%4d] sphere:     %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4]);

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_SPHERE, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            status = gi_gCreateVolume(server, modeller, SPHERE, &(args[1]));
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gCreateVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_makeSolidBody(MODL->context, SPHERE, &(args[1]), &ebody);
            CHECK_STATUS(EG_makeSolidBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, ford_sph[iface-1], npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            iford1 = 0;
            for (iface = 1; iface <= nface; iface++) {
                if        (faceContains(efaces[iface-1], args[1],
                                                         args[2]-args[4]/2,
                                                         args[3]          )) {
                    iford1 = 1;
                } else if (faceContains(efaces[iface-1], args[1],
                                                         args[2]+args[4]/2,
                                                         args[3]          )) {
                    iford1 = 2;
                }

                status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "cone xvrtx yvrtx zvrtx xbase ybase zbase radius" */
    } else if (type == OCSM_CONE) {
        SPRINT8(1, "    executing [%4d] cone:       %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6], args[7]);

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_CONE, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            status = gi_gCreateVolume(server, modeller, CONE, &(args[1]));
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gCreateVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_makeSolidBody(MODL->context, CONE, &(args[1]), &ebody);
            CHECK_STATUS(EG_makeSolidBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, ford_con[iface-1], npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            iford1 = 0;
            if        (args[1] != args[4]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]-args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 2;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]+args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 1;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            } else if (args[2] != args[5]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]-args[7])/2,
                                                             (args[2]+args[5])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 2;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]+args[7])/2,
                                                             (args[2]+args[5]        )/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 1;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            } else if (args[3] != args[6]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]-args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 2;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]+args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 1;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "cylinder xbeg ybeg zbeg xend yend zend radius" */
    } else if (type == OCSM_CYLINDER) {
        SPRINT8(1, "    executing [%4d] cylinder:   %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6], args[7]);

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_CYLINDER, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            status = gi_gCreateVolume(server, modeller, CYLINDER, &(args[1]));
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gCreateVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_makeSolidBody(MODL->context, CYLINDER, &(args[1]), &ebody);
            CHECK_STATUS(EG_makeSolidBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, ford_cyl[iface-1], npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            iford1 = 0;
            if        (args[1] != args[4]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]-args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]+args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 4;
                    } else if (faceContains(efaces[iface-1],  args[1],
                                                              args[2],
                                                              args[3]                   )) {
                        iford1 = 1;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 2;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            } else if (args[2] != args[5]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]-args[7])/2,
                                                             (args[2]+args[5])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]+args[7])/2,
                                                             (args[2]+args[5]        )/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 4;
                    } else if (faceContains(efaces[iface-1],  args[1],
                                                              args[2],
                                                              args[3]                   )) {
                        iford1 = 1;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 2;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            } else if (args[3] != args[6]) {
                for (iface = 1; iface <= nface; iface++) {
                    if        (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]-args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 3;
                    } else if (faceContains(efaces[iface-1], (args[1]+args[4]        )/2,
                                                             (args[2]+args[5]+args[7])/2,
                                                             (args[3]+args[6]        )/2)) {
                        iford1 = 4;
                    } else if (faceContains(efaces[iface-1],   args[1],
                                                               args[2],
                                                               args[3]                  )) {
                        iford1 = 1;
                    } else if (faceContains(efaces[iface-1],          args[4],
                                                                      args[5],
                                                                      args[6]           )) {
                        iford1 = 2;
                    }

                    status = setFaceAttribute(MODL, ibody, iface, iford1, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "torus xcent ycent zcent dxaxis dyaxis dzaxis majorRad minorRad" */
    } else if (type == OCSM_TORUS) {
        SPRINT9(1, "    executing [%4d] torus:      %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6], args[7], args[8]);

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_TORUS, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        #if   defined(GEOM_CAPRI)
            status = gi_gCreateVolume(server, modeller, TORUS, &(args[1]));
            if (status == 0) status = OCSM_DID_NOT_CREATE_BODY;
            CHECK_STATUS(gi_gCreateVolume);

            imodel = status;
            status = gi_dGetModel(imodel, &ibeg, &ivol, &mfile, &mdum);
            CHECK_STATUS(gi_dGetModel);

            MODL->body[ibody].ivol = ivol;
        #elif defined(GEOM_EGADS)
            status = EG_makeSolidBody(MODL->context, TORUS, &(args[1]), &ebody);
            CHECK_STATUS(EG_makeSolidBody);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);

            status = gi_qBegin();
            CHECK_STATUS(gi_qBegin);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, ford_tor[iface-1], npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            status = gi_qEnd(&errs);
            CHECK_STATUS(gi_qEnd);

            gi_free(errs);
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "import filename" */
    } else if (type == OCSM_IMPORT) {
        SPRINT2(1, "    executing [%4d] import:     %s",
                ibrch, &(MODL->brch[ibrch].arg1[1]));

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_IMPORT, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        /* load and execute the user-defined primitive */
        #if   defined(GEOM_CAPRI)
            ivol = 0;
            status = OCSM_UNSUPPORTED;
            CHECK_STATUS(import);
        #elif defined(GEOM_EGADS)
            strcpy(primtype, "import");

            status = udp_initialize(primtype, &udp_num, &udp_names, &udp_types, &udp_idef, &udp_ddef);
            if (status == EGADS_NOLOAD) {
                status = 0;
            } else {
                CHECK_STATUS(udp_initialize);

                EG_free(udp_names);
                EG_free(udp_types);
                EG_free(udp_idef);
                EG_free(udp_ddef);
            }

            status = udp_clrArguments(primtype);
            CHECK_STATUS(udp_clrArguments);

            /* strip the dollarsign off the filename */
            strcpy(dumpfile, &(MODL->brch[ibrch].arg1[1]));

            status = udp_setArgument(primtype, "FileName", dumpfile);
            CHECK_STATUS(udp_setArgument);

            status = udp_executePrim(primtype, MODL->context, &ebody, &udp_nmesh, &udp_errStr);
            CHECK_STATUS(udp_executePrim);

            EG_free(udp_errStr);

            /* check the type of ebody */
            status = EG_getInfo(ebody, &oclass, &mtype, &topRef, &prev, &next);
            CHECK_STATUS(EG_getInfo);

            if        (oclass == BODY && mtype == SOLIDBODY) {
//                                         OCSM_SOLID_BODY
            } else if (oclass == BODY && mtype == FACEBODY) {
                MODL->body[ibody].botype = OCSM_SHEET_BODY;
            } else if (oclass == BODY && mtype == SHEETBODY) {
                MODL->body[ibody].botype = OCSM_SHEET_BODY;
            } else if (oclass == BODY && mtype == WIREBODY) {
                MODL->body[ibody].botype = OCSM_WIRE_BODY;
            } else {
                status = OCSM_EXPECTING_ONE_BODY;
                CHECK_STATUS(udp_executePrim);
            }

            /* check the tolerance and issue warning if a problem */
            status = getBodyTolerance(ebody, &toler);
            CHECK_STATUS(getBodyTolerance);

            if (toler > 2.0e-7) {
                SPRINT1(0, "WARNING:: toler = %12.4e for import", toler);
            }

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                old_outLevel = EG_setOutLevel(MODL->context, 0);
                status = EG_attributeDel(efaces[iface-1], "body");
                (void) EG_setOutLevel(MODL->context, old_outLevel);
                if (status != EGADS_NOTFOUND) {
                    CHECK_STATUS(EG_attributeDel);
                }

                old_outLevel = EG_setOutLevel(MODL->context, 0);
                status = EG_attributeDel(efaces[iface-1], "brch");
                (void) EG_setOutLevel(MODL->context, old_outLevel);
                if (status != EGADS_NOTFOUND) {
                    CHECK_STATUS(EG_attributeDel);
                }

                status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body back onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "udprim primtype argName1 argValue1 argName2 argValue2 argName3 argValue3 argName4 argValue4" */
    } else if (type == OCSM_UDPRIM) {
        if        (MODL->brch[ibrch].narg == 1) {
            SPRINT2(1, "    executing [%4d] udprim:     %s",
                    ibrch, &(MODL->brch[ibrch].arg1[1]));
        } else if (MODL->brch[ibrch].narg == 3) {
            SPRINT4(1, "    executing [%4d] udprim:     %s  %s=%s",
                    ibrch, &(MODL->brch[ibrch].arg1[1]),
                           &(MODL->brch[ibrch].arg2[1]),
                           &(MODL->brch[ibrch].arg3[1]));
        } else if (MODL->brch[ibrch].narg == 5) {
            SPRINT6(1, "    executing [%4d] udprim:     %s  %s=%s  %s=%s",
                    ibrch, &(MODL->brch[ibrch].arg1[1]),
                           &(MODL->brch[ibrch].arg2[1]),
                           &(MODL->brch[ibrch].arg3[1]),
                           &(MODL->brch[ibrch].arg4[1]),
                           &(MODL->brch[ibrch].arg5[1]));
        } else if (MODL->brch[ibrch].narg == 7) {
            SPRINT8(1, "    executing [%4d] udprim:     %s  %s=%s  %s=%s  %s=%s",
                    ibrch, &(MODL->brch[ibrch].arg1[1]),
                           &(MODL->brch[ibrch].arg2[1]),
                           &(MODL->brch[ibrch].arg3[1]),
                           &(MODL->brch[ibrch].arg4[1]),
                           &(MODL->brch[ibrch].arg5[1]),
                           &(MODL->brch[ibrch].arg6[1]),
                           &(MODL->brch[ibrch].arg7[1]));
        } else if (MODL->brch[ibrch].narg == 9) {
            SPRINT10(1, "    executing [%4d] udprim:     %s  %s=%s  %s=%s  %s=%s  %s=%s",
                     ibrch, &(MODL->brch[ibrch].arg1[1]),
                            &(MODL->brch[ibrch].arg2[1]),
                            &(MODL->brch[ibrch].arg3[1]),
                            &(MODL->brch[ibrch].arg4[1]),
                            &(MODL->brch[ibrch].arg5[1]),
                            &(MODL->brch[ibrch].arg6[1]),
                            &(MODL->brch[ibrch].arg7[1]),
                            &(MODL->brch[ibrch].arg8[1]),
                            &(MODL->brch[ibrch].arg9[1]));
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_UDPRIM, -1, -1,
                         args, OCSM_SOLID_BODY, &ibody);
        CHECK_STATUS(newBody);

        /* load and execute the user-defined primitive */
        #if   defined(GEOM_CAPRI)
            ivol = 0;
            status = OCSM_UNSUPPORTED;
            CHECK_STATUS(udprim);
        #elif defined(GEOM_EGADS)
            strcpy(primtype, &(MODL->brch[ibrch].arg1[1]));

            status = udp_initialize(primtype, &udp_num, &udp_names, &udp_types, &udp_idef, &udp_ddef);

            /* if successfully loaded, nothing to do */
            if (status == SUCCESS) {

            /* not loaded since it is already loaded */
            } else if (status == EGADS_NOLOAD) {
                status = 0;

            /* a REAL error occurred */
            } else {
                CHECK_STATUS(udp_initialize);

                EG_free(udp_names);
                EG_free(udp_types);
                EG_free(udp_idef);
                EG_free(udp_ddef);
            }

            status = udp_clrArguments(primtype);
            CHECK_STATUS(udp_clrArguments);

            /* set up each argument for the udp */
            if (MODL->brch[ibrch].narg >= 3) {
                strcpy(argname, &(MODL->brch[ibrch].arg2[1]));

                if (MODL->brch[ibrch].arg3[1] == '!') {
                    ipmtr = 0;
                    for (jpmtr = 1; jpmtr <= MODL->npmtr; jpmtr++) {
                        if (strcmp(&(MODL->brch[ibrch].arg3[2]), MODL->pmtr[jpmtr].name) == 0) {
                            ipmtr = jpmtr;
                            break;
                        }
                    }

                    if (ipmtr == 0) {
                        status = str2val(&(MODL->brch[ibrch].arg3[2]), MODL, &args[3]);
                        CHECK_STATUS(str2val:val1);

                        sprintf(argvalue, "%11.6f", args[3]);
                    } else {
                        argvalue[0] = '\0';
                        for (ij = 0; ij < (MODL->pmtr[ipmtr].ncol)*(MODL->pmtr[ipmtr].nrow); ij++) {
                            sprintf(foo, "%11.6f;", MODL->pmtr[ipmtr].value[ij]);
                            strcat(argvalue, foo);
                        }
                    }
                } else {
                    strcpy(argvalue, &(MODL->brch[ibrch].arg3[1]));
                }

                status = udp_setArgument(primtype, argname, argvalue);
                CHECK_STATUS(udp_setArgument);
            }
            if (MODL->brch[ibrch].narg >= 5) {
                strcpy(argname, &(MODL->brch[ibrch].arg4[1]));

                if (MODL->brch[ibrch].arg5[1] == '!') {
                    ipmtr = 0;
                    for (jpmtr = 1; jpmtr <= MODL->npmtr; jpmtr++) {
                        if (strcmp(&(MODL->brch[ibrch].arg5[2]), MODL->pmtr[jpmtr].name) == 0) {
                            ipmtr = jpmtr;
                            break;
                        }
                    }

                    if (ipmtr == 0) {
                        status = str2val(&(MODL->brch[ibrch].arg5[2]), MODL, &args[5]);
                        CHECK_STATUS(str2val:val2);

                        sprintf(argvalue, "%11.6f", args[5]);
                    } else {
                        argvalue[0] = '\0';
                        for (ij = 0; ij < (MODL->pmtr[ipmtr].ncol)*(MODL->pmtr[ipmtr].nrow); ij++) {
                            sprintf(foo, "%11.6f;", MODL->pmtr[ipmtr].value[ij]);
                            strcat(argvalue, foo);
                        }
                    }
                } else {
                    strcpy(argvalue, &(MODL->brch[ibrch].arg5[1]));
                }

                status = udp_setArgument(primtype, argname, argvalue);
                CHECK_STATUS(udp_setArgument);
            }
            if (MODL->brch[ibrch].narg >= 7) {
                strcpy(argname, &(MODL->brch[ibrch].arg6[1]));

                if (MODL->brch[ibrch].arg7[1] == '!') {
                    ipmtr = 0;
                    for (jpmtr = 1; jpmtr <= MODL->npmtr; jpmtr++) {
                        if (strcmp(&(MODL->brch[ibrch].arg7[2]), MODL->pmtr[jpmtr].name) == 0) {
                            ipmtr = jpmtr;
                            break;
                        }
                    }

                    if (ipmtr == 0) {
                        status = str2val(&(MODL->brch[ibrch].arg7[2]), MODL, &args[7]);
                        CHECK_STATUS(str2val:val2);

                        sprintf(argvalue, "%11.6f", args[7]);
                    } else {
                        argvalue[0] = '\0';
                        for (ij = 0; ij < (MODL->pmtr[ipmtr].ncol)*(MODL->pmtr[ipmtr].nrow); ij++) {
                            sprintf(foo, "%11.6f;", MODL->pmtr[ipmtr].value[ij]);
                            strcat(argvalue, foo);
                        }
                    }
                } else {
                    strcpy(argvalue, &(MODL->brch[ibrch].arg7[1]));
                }

                status = udp_setArgument(primtype, argname, argvalue);
                CHECK_STATUS(udp_setArgument);
            }
            if (MODL->brch[ibrch].narg >= 9) {
                strcpy(argname, &(MODL->brch[ibrch].arg8[1]));

                if (MODL->brch[ibrch].arg9[1] == '!') {
                    ipmtr = 0;
                    for (jpmtr = 1; jpmtr <= MODL->npmtr; jpmtr++) {
                        if (strcmp(&(MODL->brch[ibrch].arg9[2]), MODL->pmtr[jpmtr].name) == 0) {
                            ipmtr = jpmtr;
                            break;
                        }
                    }

                    if (ipmtr == 0) {
                        status = str2val(&(MODL->brch[ibrch].arg9[2]), MODL, &args[9]);
                        CHECK_STATUS(str2val:val2);

                        sprintf(argvalue, "%11.6f", args[9]);
                    } else {
                        argvalue[0] = '\0';
                        for (ij = 0; ij < (MODL->pmtr[ipmtr].ncol)*(MODL->pmtr[ipmtr].nrow); ij++) {
                            sprintf(foo, "%11.6f;", MODL->pmtr[ipmtr].value[ij]);
                            strcat(argvalue, foo);
                        }
                    }
                } else {
                    strcpy(argvalue, &(MODL->brch[ibrch].arg9[1]));
                }

                status = udp_setArgument(primtype, argname, argvalue);
                CHECK_STATUS(udp_setArgument);
            }

            status = udp_executePrim(primtype, MODL->context, &ebody, &udp_nmesh, &udp_errStr);
            CHECK_STATUS(udp_executePrim);

            EG_free(udp_errStr);

            /* check the type of ebody */
            status = EG_getInfo(ebody, &oclass, &mtype, &topRef, &prev, &next);
            CHECK_STATUS(EG_getInfo);

            if        (oclass == BODY && mtype == SOLIDBODY) {
//                                         OCSM_SOLID_BODY
            } else if (oclass == BODY && mtype == FACEBODY) {
                MODL->body[ibody].botype = OCSM_SHEET_BODY;
            } else if (oclass == BODY && mtype == SHEETBODY) {
                MODL->body[ibody].botype = OCSM_SHEET_BODY;
            } else if (oclass == BODY && mtype == WIREBODY) {
                MODL->body[ibody].botype = OCSM_WIRE_BODY;
            } else {
                status = OCSM_EXPECTING_ONE_BODY;
                CHECK_STATUS(udp_executePrim);
            }

            MODL->body[ibody].ebody = ebody;
        #endif

        /* mark the new Faces with the current Branch */
        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                old_outLevel = EG_setOutLevel(MODL->context, 0);
                status = EG_attributeDel(efaces[iface-1], "body");
                (void) EG_setOutLevel(MODL->context, old_outLevel);
                if (status != EGADS_NOTFOUND) {
                    CHECK_STATUS(EG_attributeDel);
                }

                old_outLevel = EG_setOutLevel(MODL->context, 0);
                status = EG_attributeDel(efaces[iface-1], "brch");
                (void) EG_setOutLevel(MODL->context, old_outLevel);
                if (status != EGADS_NOTFOUND) {
                    CHECK_STATUS(EG_attributeDel);
                }

                status = setFaceAttribute(MODL, ibody, iface, iface, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            }

            EG_free(efaces);
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body back onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildSketch - implement OCSM_SKETCHs for ocsmBuild                 *
 *                                                                      *
 ************************************************************************
 */

static int
buildSketch(modl_T *modl,
            int    ibrch,
            int    *nstack,
            int    stack[],
            int    npatn,
            patn_T patn[],
            int    *nskpt,
            skpt_T skpt[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       type, i, n, nspln=0, ibody, iopen;
    double    args[10], xlast, ylast, zlast;
    double    xmin, xmax, ymin, ymax, zmin, zmax, pts[3*MAX_SKETCH_SIZE];
    double    dx1, dy1, dz1, ds1, dx2, dy2, dz2, ds2, dot;

    #if   defined(GEOM_CAPRI)
        int        nsketch=0;
        double     orig[3], xaxis[3], yaxis[3];
    #elif defined(GEOM_EGADS)
        int        iseg, nseg, sense[MAX_SKETCH_SIZE], header[4], ii, jj;
        int        iface, nface, iter, niter;
        double     data[20], tdata[2], *cp=NULL, result[3], du, dx, dy, dz;
        double     scent, xcent, ycent, zcent, dxyzmax;
        ego        ebody, *efaces, ecurve, eflip, eloop;
        ego        Enodes[MAX_SKETCH_SIZE+1], Eedges[MAX_SKETCH_SIZE], Efaces[1];
    #endif

    ROUTINE(buildSketch);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    if (MODL->brch[ibrch].narg >= 3) {
        status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
        CHECK_STATUS(str2val:val3);
    } else {
        args[3] = 0;
    }
    if (MODL->brch[ibrch].narg >= 4) {
        status = str2val(MODL->brch[ibrch].arg4, MODL, &args[4]);
        CHECK_STATUS(str2val:val4);
    } else {
        args[4] = 0;
    }
    if (MODL->brch[ibrch].narg >= 5) {
        status = str2val(MODL->brch[ibrch].arg5, MODL, &args[5]);
        CHECK_STATUS(str2val:val5);
    } else {
        args[5] = 0;
    }
    if (MODL->brch[ibrch].narg >= 6) {
        status = str2val(MODL->brch[ibrch].arg6, MODL, &args[6]);
        CHECK_STATUS(str2val:val6);
    } else {
        args[6] = 0;
    }
    args[7] = 0;
    args[8] = 0;
    args[9] = 0;

    /* execute: "skbeg x y z" */
    if (type == OCSM_SKBEG) {
        SPRINT4(1, "    executing [%4d] skbeg:      %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* add the point to the sketch */
        skpt[(*nskpt)].itype = OCSM_SKBEG;
        skpt[(*nskpt)].ibrch = ibrch;
        skpt[(*nskpt)].x     = args[1];
        skpt[(*nskpt)].y     = args[2];
        skpt[(*nskpt)].z     = args[3];
        (*nskpt)++;

        status = newBody(MODL, ibrch, OCSM_SKBEG, -1, -1,
                         args, OCSM_SKETCH, &ibody);
        CHECK_STATUS(newBody);

    /* execute: "linseg x y z" */
    } else if (type == OCSM_LINSEG) {
        SPRINT4(1, "    executing [%4d] linseg:     %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* add the linseg to the sketch */
        skpt[(*nskpt)].itype = OCSM_LINSEG;
        skpt[(*nskpt)].ibrch = ibrch;
        skpt[(*nskpt)].x     = args[1];
        skpt[(*nskpt)].y     = args[2];
        skpt[(*nskpt)].z     = args[3];
        (*nskpt)++;

        status = newBody(MODL, ibrch, OCSM_LINSEG, MODL->nbody, -1,
                         args, OCSM_SKETCH, &ibody);
        CHECK_STATUS(newBody);

    /* execute: "cirarc xon yon zon xend yend zend" */
    } else if (type == OCSM_CIRARC) {
        SPRINT7(1, "    executing [%4d] cirarc:     %11.5f %11.5f %11.5f %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3], args[4],
                       args[5], args[6]);

        /* check for the colinearity of the 3 points */
        dx1 = args[1] - skpt[(*nskpt)-1].x;
        dy1 = args[2] - skpt[(*nskpt)-1].y;
        dz1 = args[3] - skpt[(*nskpt)-1].z;
        ds1 = sqrt(dx1*dx1 + dy1*dy1 + dz1*dz1);

        dx2 = args[4] - args[1];
        dy2 = args[5] - args[2];
        dz2 = args[6] - args[3];
        ds2 = sqrt(dx2*dx2 + dy2*dy2 + dz2*dz2);

        dot = (dx1*dx2 + dy1*dy2 + dz1*dz2) / ds1 / ds2;

        /* if points are co-linear, convert to a linseg */
        if (fabs(dot) > 0.9999) {
            SPRINT0(0, "WARNING:: converting to linseg since points are colinear");

            skpt[(*nskpt)].itype = OCSM_LINSEG;
            skpt[(*nskpt)].ibrch = ibrch;
            skpt[(*nskpt)].x     = args[4];
            skpt[(*nskpt)].y     = args[5];
            skpt[(*nskpt)].z     = args[6];
            (*nskpt)++;

            status = newBody(MODL, ibrch, OCSM_LINSEG, MODL->nbody, -1,
                             &(args[3]), OCSM_SKETCH, &ibody);
            CHECK_STATUS(newBody);

        /* add the cirarc to the sketch (as two entries) */
        } else {
            skpt[(*nskpt)].itype = OCSM_CIRARC;
            skpt[(*nskpt)].ibrch = ibrch;
            skpt[(*nskpt)].x     = args[1];
            skpt[(*nskpt)].y     = args[2];
            skpt[(*nskpt)].z     = args[3];
            (*nskpt)++;

            skpt[(*nskpt)].itype = OCSM_CIRARC;
            skpt[(*nskpt)].ibrch = ibrch;
            skpt[(*nskpt)].x     = args[4];
            skpt[(*nskpt)].y     = args[5];
            skpt[(*nskpt)].z     = args[6];
            (*nskpt)++;

            status = newBody(MODL, ibrch, OCSM_CIRARC, MODL->nbody, -1,
                             args, OCSM_SKETCH, &ibody);
            CHECK_STATUS(newBody);
        }

    /* execute: "spline x y z" */
    } else if (type == OCSM_SPLINE) {
        SPRINT4(1, "    executing [%4d] spline:     %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* add the spline to the sketch */
        skpt[(*nskpt)].itype = OCSM_SPLINE;
        skpt[(*nskpt)].ibrch = ibrch;
        skpt[(*nskpt)].x     = args[1];
        skpt[(*nskpt)].y     = args[2];
        skpt[(*nskpt)].z     = args[3];
        (*nskpt)++;

        status = newBody(MODL, ibrch, OCSM_SPLINE, MODL->nbody, -1,
                         args, OCSM_SKETCH, &ibody);
        CHECK_STATUS(newBody);

    /* execute: "skend" */
    } else if (type == OCSM_SKEND) {
        SPRINT1(1, "    executing [%4d] skend:",
                ibrch);

        /* add the skend to the sketch */
        skpt[(*nskpt)].itype = OCSM_SKEND;
        skpt[(*nskpt)].ibrch = ibrch;
        skpt[(*nskpt)].x     = skpt[(*nskpt)-1].x;
        skpt[(*nskpt)].y     = skpt[(*nskpt)-1].y;
        skpt[(*nskpt)].z     = skpt[(*nskpt)-1].z;
        (*nskpt)++;

        /* if we have only one Sketch point, the we are creating a OCSM_NODE_BODY */
        if ((*nskpt) == 2) {
            /* close the sketcher */
            (*nskpt) = 0;

            /* create the Body */
            status = newBody(MODL, ibrch, OCSM_SKEND, -1, -1,
                             args, OCSM_NODE_BODY, &ibody);
            CHECK_STATUS(newBody);

            #if   defined(GEOM_CAPRI)
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(skend);
            #elif defined(GEOM_EGADS)
                pts[0] = skpt[0].x;
                pts[1] = skpt[0].y;
                pts[2] = skpt[0].z;

                status = EG_makeTopology(MODL->context, NULL, NODE, 0, pts, 0, NULL, NULL, &ebody);
                CHECK_STATUS(EG_makeTopology);

                MODL->body[ibody].ebody = ebody;
            #endif

            /* push the Body onto the stack */
            stack[(*nstack)++] = ibody;

            SPRINT1(1, "                          Node   %4d created", ibody);

        } else {

            /* find the extrema of the sketch points */
            xmin = skpt[0].x;
            xmax = skpt[0].x;
            ymin = skpt[0].y;
            ymax = skpt[0].y;
            zmin = skpt[0].z;
            zmax = skpt[0].z;
            DPRINT5("skpnt[%3d] %3d %10.5f %10.5f %10.5f",
                    0, skpt[0].itype, skpt[0].x, skpt[0].y, skpt[0].z);

            for (i = 1; i < (*nskpt); i++) {
                if (skpt[i].x < xmin) xmin = skpt[i].x;
                if (skpt[i].x > xmax) xmax = skpt[i].x;
                if (skpt[i].y < ymin) ymin = skpt[i].y;
                if (skpt[i].y > ymax) ymax = skpt[i].y;
                if (skpt[i].z < zmin) zmin = skpt[i].z;
                if (skpt[i].z > zmax) zmax = skpt[i].z;

                DPRINT5("skpnt[%3d] %3d %10.5f %10.5f %10.5f",
                        i, skpt[i].itype, skpt[i].x, skpt[i].y, skpt[i].z);
            }

//$$$                if        (xmin == xmax && ymin == ymax) {
//$$$                    SPRINT4(0, "xmin=%11.5f, xmax=%11.5f, ymin=%11.5f, ymax=%11.5f", xmin, xmax, ymin, ymax);
//$$$                    status = OCSM_COLINEAR_SKETCH_POINTS;
//$$$                    CHECK_STATUS(skend);
//$$$                } else if (ymin == ymax && zmin == zmax) {
//$$$                    SPRINT4(0, "ymin=%11.5f, ymax=%11.5f, zmin=%11.5f, zmax=%11.5f", ymin, ymax, zmin, zmax);
//$$$                    status = OCSM_COLINEAR_SKETCH_POINTS;
//$$$                    CHECK_STATUS(skend);
//$$$                } else if (zmin == zmax && xmin == xmax) {
//$$$                    SPRINT4(0, "zmin=%11.5f, zmax=%11.5f, xmin=%11.5f, xmax=%11.5f", zmin, zmax, xmin, xmax);
//$$$                    status = OCSM_COLINEAR_SKETCH_POINTS;
//$$$                    CHECK_STATUS(skend);
//$$$                }

            /* create the sketch plane */
            #if   defined(GEOM_CAPRI)
                if        (xmin == xmax) {
                    orig[ 0] = xmin; orig[ 1] = 0.0;  orig[ 2] = 0.0;
                    xaxis[0] = 0.0;  xaxis[1] = 1.0;  xaxis[2] = 0.0;
                    yaxis[0] = 0.0;  yaxis[1] = 0.0;  yaxis[2] = 1.0;
                } else if (ymin == ymax) {
                    orig[ 0] = 0.0;  orig[ 1] = ymin; orig[ 2] = 0.0;
                    xaxis[0] = 0.0;  xaxis[1] = 0.0;  xaxis[2] = 1.0;
                    yaxis[0] = 1.0;  yaxis[1] = 0.0;  yaxis[2] = 0.0;
                } else if (zmin == zmax) {
                    orig[ 0] = 0.0;  orig[ 1] = 0.0;  orig[ 2] = zmin;
                    xaxis[0] = 1.0;  xaxis[1] = 0.0;  xaxis[2] = 0.0;
                    yaxis[0] = 0.0,  yaxis[1] = 1.0,  yaxis[2] = 0.0;
                } else {
                    status = OCSM_NON_COPLANAR_SKETCH_POINTS;
                    CHECK_STATUS(skend);
                }

                DPRINT3("orig  = %10.5f %10.5f %10.5f",  orig[0],  orig[1],  orig[2]);
                DPRINT3("xaxis = %10.5f %10.5f %10.5f", xaxis[0], xaxis[1], xaxis[2]);
                DPRINT3("yaxis = %10.5f %10.5f %10.5f", yaxis[0], yaxis[1], yaxis[2]);

                status = gi_sCreate(orig, xaxis, yaxis, NULL);
                CHECK_STATUS(gi_sCreate);
            #elif defined(GEOM_EGADS)
            #endif

            /* determine if the sketch is open or closed */
            if (fabs(skpt[(*nskpt)-2].x-skpt[0].x) < EPS06 &&
                fabs(skpt[(*nskpt)-2].y-skpt[0].y) < EPS06 &&
                fabs(skpt[(*nskpt)-2].z-skpt[0].z) < EPS06   ) {
                iopen = 0;
            } else {
                iopen = 1;

                #if   defined(GEOM_CAPRI)
                    status = OCSM_SKETCH_DOES_NOT_CLOSE;
                    CHECK_STATUS(skend);
                #elif defined(GEOM_EGADS)
                #endif
            }
            DPRINT1("iopen=%d", iopen);

            /* create the beginning node */
            #if   defined(GEOM_CAPRI)
            #elif defined(GEOM_EGADS)
                nseg = 0;
                pts[0] = skpt[0].x;
                pts[1] = skpt[0].y;
                pts[2] = skpt[0].z;
                status = EG_makeTopology(MODL->context, NULL, NODE, 0,
                                         pts, 0, NULL, NULL, &(Enodes[0]));
                CHECK_STATUS(EG_makeTopology);
            #endif

            /* save "last" point */
            xlast = skpt[0].x;
            ylast = skpt[0].y;
            zlast = skpt[0].z;

            /* no points in spline so far */
            nspln = 0;

            /* add the lines, circular-arcs, and splines to the sketch plane */
            for (i = 1; i < (*nskpt); i++) {

                /* if we were defining a spline but the new segment is not
                   a spline, generate the spline now */
                if (nspln > 0 && skpt[i].itype != OCSM_SPLINE) {
                    if (nspln < 3) {
                        status = OCSM_TOO_FEW_SPLINE_POINTS;
                        CHECK_STATUS(skend);
                    }

                    DPRINT1("spline (w/%d points):", nspln);
                    #if   defined(GEOM_CAPRI)
                        for (n = 0; n < nspln; n++) {
                            DPRINT3("%5d %11.5f %11.5f", n, pts[2*n], pts[2*n+1]);
                        }

                        status = gi_sAddEntity(4, nspln, pts);
                        CHECK_STATUS(gi_sAddEntity);
                    #elif defined(GEOM_EGADS)
                        for (n = 0; n < nspln; n++) {
                            DPRINT4("%5d %11.5f %11.5f %11.5f",   n, pts[3*n], pts[3*n+1], pts[3*n+2]);
                        }

                        if (i < (*nskpt)-1 || iopen == 1) {
                            data[0] = pts[3*nspln-3];
                            data[1] = pts[3*nspln-2];
                            data[2] = pts[3*nspln-1];
                            status = EG_makeTopology(MODL->context, NULL, NODE, 0,
                                                     data, 0, NULL, NULL, &(Enodes[nseg+1]));
                            CHECK_STATUS(EG_makeTopology);
                        } else {
                            Enodes[nseg+1] = Enodes[0];
                        }

                        header[0] = 0;
                        header[1] = 3;
                        header[2] = nspln + 2;
                        header[3] = nspln + 6;

                        MALLOC(cp, double, header[3]+3*header[2]);

                        /* knots (spaced according to pseudo-arc-length) */
                        jj = 0;
                        cp[jj] = 0; jj++;
                        cp[jj] = 0; jj++;
                        cp[jj] = 0; jj++;
                        cp[jj] = 0; jj++;

                        for (ii = 1; ii < nspln; ii++) {
                            cp[jj] = cp[jj-1] + sqrt(pow(pts[3*ii  ]-pts[3*ii-3],2)
                                                    +pow(pts[3*ii+1]-pts[3*ii-2],2)
                                                    +pow(pts[3*ii+2]-pts[3*ii-1],2));
                            jj++;
                        }

                        cp[jj] = cp[jj-1]; jj++;
                        cp[jj] = cp[jj-1]; jj++;
                        cp[jj] = cp[jj-1]; jj++;

                        for (ii = 0; ii < header[3]; ii++) {
                            cp[ii] /= cp[header[3]-1];
                        }

                        /* initial point */
                        cp[jj] = pts[0]; jj++;
                        cp[jj] = pts[1]; jj++;
                        cp[jj] = pts[2]; jj++;

                        /* initial interior point (for slope) */
                        cp[jj] = (3 * pts[0] + pts[3]) / 4; jj++;
                        cp[jj] = (3 * pts[1] + pts[4]) / 4; jj++;
                        cp[jj] = (3 * pts[2] + pts[5]) / 4; jj++;

                        /* interior points */
                        for (ii = 1; ii < nspln-1; ii++) {
                            cp[jj] = pts[3*ii  ]; jj++;
                            cp[jj] = pts[3*ii+1]; jj++;
                            cp[jj] = pts[3*ii+2]; jj++;
                        }

                        /* penultimate interior point (for slope) */
                        cp[jj] = (3 * pts[3*nspln-3] + pts[3*nspln-6]) / 4; jj++;
                        cp[jj] = (3 * pts[3*nspln-2] + pts[3*nspln-5]) / 4; jj++;
                        cp[jj] = (3 * pts[3*nspln-1] + pts[3*nspln-4]) / 4; jj++;

                        /* final point */
                        cp[jj] = pts[3*nspln-3]; jj++;
                        cp[jj] = pts[3*nspln-2]; jj++;
                        cp[jj] = pts[3*nspln-1]; jj++;

                        /* make the original BSPLINE (based upon the assumed control points) */
                        status = EG_makeGeometry(MODL->context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
                        CHECK_STATUS(EG_makeGeometry);

                        /* iterate to have knot evaluations match data points */
                        niter = 100;
                        for (iter = 0; iter < niter; iter++) {
                            dxyzmax = 0;

                            /* natural end condition at beginning */
                            status = EG_evaluate(ecurve, &(cp[0]), data);
                            CHECK_STATUS(EG_evaluate);

                            /* note: the 0.01 is needed to make this work, buts its
                               "theoreical justification" is unknown (perhap under-relaxation?) */
                            du = cp[4] - cp[3];
                            dx = 0.01 * du * du * data[6];
                            dy = 0.01 * du * du * data[7];
                            dz = 0.01 * du * du * data[8];

                            if (fabs(dx/du) > dxyzmax) dxyzmax = fabs(dx/du);
                            if (fabs(dy/du) > dxyzmax) dxyzmax = fabs(dy/du);
                            if (fabs(dz/du) > dxyzmax) dxyzmax = fabs(dz/du);

                            cp[header[3]+3] += dx;
                            cp[header[3]+4] += dy;
                            cp[header[3]+5] += dz;

                            /* match interior spline points */
                            for (ii = 1; ii < nspln-1; ii++) {
                                status = EG_evaluate(ecurve, &(cp[ii+3]), data);
                                CHECK_STATUS(EG_evaluate);

                                dx = pts[3*ii  ] - data[0];
                                dy = pts[3*ii+1] - data[1];
                                dz = pts[3*ii+2] - data[2];

                                if (fabs(dx) > dxyzmax) dxyzmax = fabs(dx);
                                if (fabs(dy) > dxyzmax) dxyzmax = fabs(dy);
                                if (fabs(dz) > dxyzmax) dxyzmax = fabs(dz);

                                cp[header[3]+3*ii+3] += dx;
                                cp[header[3]+3*ii+4] += dy;
                                cp[header[3]+3*ii+5] += dz;
                            }

                            /* natural end condition at end */
                            status = EG_evaluate(ecurve, &(cp[nspln+3]), data);
                            CHECK_STATUS(EG_evaluate);

                            du = cp[nspln+2] - cp[nspln+1];
                            dx = 0.01 * du * du * data[6];
                            dy = 0.01 * du * du * data[7];
                            dz = 0.01 * du * du * data[8];

                            if (fabs(dx/du) > dxyzmax) dxyzmax = fabs(dx/du);
                            if (fabs(dy/du) > dxyzmax) dxyzmax = fabs(dy/du);
                            if (fabs(dz/du) > dxyzmax) dxyzmax = fabs(dz/du);

                            cp[header[3]+3*nspln  ] += dx;
                            cp[header[3]+3*nspln+1] += dy;
                            cp[header[3]+3*nspln+2] += dz;

                            if (dxyzmax < EPS06) {
                                niter = iter;
                                break;
                            }

                            /* make the new curve */
                            status = EG_deleteObject(ecurve);
                            CHECK_STATUS(EG_deleteObject);

                            status = EG_makeGeometry(MODL->context, CURVE, BSPLINE, NULL, header, cp, &ecurve);
                            CHECK_STATUS(EG_makeGeometry);
                        }

                        status = EG_invEvaluate(ecurve, &(pts[0]),         &(tdata[0]), result);
                        CHECK_STATUS(EG_invEvaluate);

                        status = EG_invEvaluate(ecurve, &(pts[3*nspln-3]), &(tdata[1]), result);
                        CHECK_STATUS(EG_invEvaluate);

                        #ifdef SHOW_SPLINES
                        {
                            int    io_kbd=5, io_scr=6, indgr=1+4+16+64;
                            int    i, nline=9, ilin[9], isym[9], nper[9];
                            float  xplot[9000], yplot[9000];
                            double uu, xyz3[9];

                            for (i = 0; i < nline; i++) {
                                ilin[i] = +i;
                                isym[i] = -i;
                                nper[i] = 1000;
                            }

                            for (i = 0; i < 1000; i++) {
                                uu = tdata[0] + (double)i/999 * (tdata[1] - tdata[0]);

                                EG_evaluate(ecurve, &uu, xyz3);

                                xplot[      i] = uu;   yplot[      i] = xyz3[0];   //   x
                                xplot[ 1000+i] = uu;   yplot[ 1000+i] = xyz3[1];   //   y
                                xplot[ 2000+i] = uu;   yplot[ 2000+i] = xyz3[2];   //   z
                                xplot[ 3000+i] = uu;   yplot[ 3000+i] = xyz3[3];   //  dx/du
                                xplot[ 4000+i] = uu;   yplot[ 4000+i] = xyz3[4];   //  dy/du
                                xplot[ 5000+i] = uu;   yplot[ 5000+i] = xyz3[5];   //  dz/du
                                xplot[ 6000+i] = uu;   yplot[ 6000+i] = xyz3[6];   // d2x/du2
                                xplot[ 7000+i] = uu;   yplot[ 7000+i] = xyz3[7];   // d2y/du2
                                xplot[ 8000+i] = uu;   yplot[ 8000+i] = xyz3[8];   // d2z/du2
                            }

                            grinit_(&io_kbd, &io_scr, "generation of spline in ocsmBuild",
                                               strlen("generation of spline in ocsmBuild"));
                            grline_(ilin, isym, &nline,                "~x~y~spline data",
                                    &indgr, xplot, yplot, nper, strlen("~x~y~spline data"));
                        }
                        #endif

                        FREE(cp);

                        status = EG_makeTopology(MODL->context, ecurve, EDGE, TWONODE,
                                                 tdata, 2, &(Enodes[nseg]), NULL, &(Eedges[nseg]));
                        CHECK_STATUS(EG_makeTopology);

                        nseg++;
                    #endif

                    nspln = 0;

                    xlast = skpt[i-1].x;
                    ylast = skpt[i-1].y;
                    zlast = skpt[i-1].z;
                }

                /* if the new segment is a skend, stop now */
                if (skpt[i].itype == OCSM_SKEND) {
                    break;

                /* add a linseg */
                } else if (skpt[i].itype == OCSM_LINSEG) {
                    if (fabs(xlast-skpt[i].x) < EPS06 &&
                        fabs(ylast-skpt[i].y) < EPS06 &&
                        fabs(zlast-skpt[i].z) < EPS06   ) {
                        DPRINT0("skipped (zero length linseg)");
                    } else {
                        #if   defined(GEOM_CAPRI)
                            if        (xmin == xmax) {
                                pts[0] = ylast;     pts[1] = zlast;
                                pts[2] = skpt[i].y; pts[3] = skpt[i].z;
                            } else if (ymin == ymax) {
                                pts[0] = zlast;     pts[1] = xlast;
                                pts[2] = skpt[i].z; pts[3] = skpt[i].x;
                            } else {
                                pts[0] = xlast;     pts[1] = ylast;
                                pts[2] = skpt[i].x; pts[3] = skpt[i].y;
                            }

                            DPRINT0("linseg:");
                            DPRINT3("%5d %11.5f %11.5f", 0, pts[0], pts[1]);
                            DPRINT3("%5d %11.5f %11.5f", 1, pts[2], pts[3]);

                            status = gi_sAddEntity(2, 2, pts);
                            CHECK_STATUS(gi_sAddEntity);
                        #elif defined(GEOM_EGADS)
                            DPRINT0("linseg:");
                            DPRINT4("%5d %11.5f %11.5f %11.5f", 0, xlast,     ylast,     zlast    );
                            DPRINT4("%5d %11.5f %11.5f %11.5f", i, skpt[i].x, skpt[i].y, skpt[i].z);

                            if (i < (*nskpt)-2 || iopen == 1) {
                                pts[0] = skpt[i].x;
                                pts[1] = skpt[i].y;
                                pts[2] = skpt[i].z;
                                status = EG_makeTopology(MODL->context, NULL, NODE, 0,
                                                         pts, 0, NULL, NULL, &(Enodes[nseg+1]));
                                CHECK_STATUS(EG_makeTopology);
                            } else {
                                Enodes[nseg+1] = Enodes[0];
                            }

                            pts[0] = xlast;
                            pts[1] = ylast;
                            pts[2] = zlast;
                            pts[3] = skpt[i].x - xlast;
                            pts[4] = skpt[i].y - ylast;
                            pts[5] = skpt[i].z - zlast;
                            status = EG_makeGeometry(MODL->context, CURVE, LINE, NULL, NULL, pts, &ecurve);
                            CHECK_STATUS(EG_makeGeometry);

                            status = EG_invEvaluate(ecurve, pts, &(tdata[0]), result);
                            CHECK_STATUS(EG_invEvaluate);

                            pts[0] = skpt[i].x;
                            pts[1] = skpt[i].y;
                            pts[2] = skpt[i].z;
                            status = EG_invEvaluate(ecurve, pts, &(tdata[1]), result);
                            CHECK_STATUS(EG_invEvaluate);

                            status = EG_makeTopology(MODL->context, ecurve, EDGE, TWONODE,
                                                     tdata, 2, &(Enodes[nseg]), NULL, &(Eedges[nseg]));
                            CHECK_STATUS(EG_makeTopology);

                            nseg++;
                        #endif
                    }

                    xlast = skpt[i].x;
                    ylast = skpt[i].y;
                    zlast = skpt[i].z;

                /* add a cirarc */
                } else if (skpt[i].itype == OCSM_CIRARC) {
                    #if   defined(GEOM_CAPRI)
                        if        (xmin == xmax) {
                            pts[0] = ylast;       pts[1] = zlast;
                            pts[2] = skpt[i  ].y; pts[3] = skpt[i  ].z;
                            pts[4] = skpt[i+1].y; pts[5] = skpt[i+1].z;
                        } else if (ymin == ymax) {
                            pts[0] = zlast;       pts[1] = xlast;
                            pts[2] = skpt[i  ].z; pts[3] = skpt[i  ].x;
                            pts[4] = skpt[i+1].z; pts[5] = skpt[i+1].x;
                        } else {
                            pts[0] = xlast;       pts[1] = ylast;
                            pts[2] = skpt[i  ].x; pts[3] = skpt[i  ].y;
                            pts[4] = skpt[i+1].x; pts[5] = skpt[i+1].y;
                        }

                        DPRINT0("cirarc:");
                        DPRINT3("%5d %11.5f %11.5f", 0, pts[0], pts[1]);
                        DPRINT3("%5d %11.5f %11.5f", 1, pts[2], pts[3]);
                        DPRINT3("%5d %11.5f %11.5f", 2, pts[4], pts[5]);

                        status = gi_sAddEntity(3, 3, pts);
                        CHECK_STATUS(gi_sAddEntity);
                    #elif defined(GEOM_EGADS)
                        DPRINT0("cirarc:");
                        DPRINT4("%5d %11.5f %11.5f %11.5f", 0,   xlast,       ylast,       zlast      );
                        DPRINT4("%5d %11.5f %11.5f %11.5f", i,   skpt[i  ].x, skpt[i  ].y, skpt[i  ].z);
                        DPRINT4("%5d %11.5f %11.5f %11.5f", i+1, skpt[i+1].x, skpt[i+1].y, skpt[i+1].z);

                        if (i < (*nskpt)-3 || iopen == 1) {
                            pts[0] = skpt[i+1].x;
                            pts[1] = skpt[i+1].y;
                            pts[2] = skpt[i+1].z;
                            status = EG_makeTopology(MODL->context, NULL, NODE, 0,
                                                     pts, 0, NULL, NULL, &(Enodes[nseg+1]));
                            CHECK_STATUS(EG_makeTopology);
                        } else {
                            Enodes[nseg+1] = Enodes[0];
                        }

                        if        (xmin == xmax) {
                            scent = ((skpt[i+1].y - ylast    ) * (skpt[i  ].y - skpt[i+1].y)
                                    -(skpt[i+1].z - zlast    ) * (skpt[i+1].z - skpt[i  ].z))
                                   /((zlast       - skpt[i].z) * (skpt[i  ].y - skpt[i+1].y)
                                   - (skpt[i].y   - ylast    ) * (skpt[i+1].z - skpt[i  ].z));
                            ycent = (ylast + skpt[i].y + scent * (zlast     - skpt[i].z)) / 2;
                            zcent = (zlast + skpt[i].z + scent * (skpt[i].y - ylast    )) / 2;

                            data[0] = xmin;
                            data[1] = ycent;
                            data[2] = zcent;
                            data[3] = 0;
                            data[4] = ylast - ycent;
                            data[5] = zlast - zcent;
                            data[6] = 0;
                            data[7] = -data[5];
                            data[8] = +data[4];
                            data[9] = sqrt(pow(ylast-ycent,2) + pow(zlast-zcent,2));

                        } else if (ymin == ymax) {
                            scent = ((skpt[i+1].z - zlast    ) * (skpt[i  ].z - skpt[i+1].z)
                                    -(skpt[i+1].x - xlast    ) * (skpt[i+1].x - skpt[i  ].x))
                                   /((xlast       - skpt[i].x) * (skpt[i  ].z - skpt[i+1].z)
                                   - (skpt[i].z   - zlast    ) * (skpt[i+1].x - skpt[i  ].x));
                            zcent = (zlast + skpt[i].z + scent * (xlast     - skpt[i].x)) / 2;
                            xcent = (xlast + skpt[i].x + scent * (skpt[i].z - zlast    )) / 2;

                            data[0] = xcent;
                            data[1] = ymin;
                            data[2] = zcent;
                            data[3] = xlast - xcent;
                            data[4] = 0;
                            data[5] = zlast - zcent;
                            data[6] = +data[5];
                            data[7] = 0;
                            data[8] = -data[3];
                            data[9] = sqrt(pow(zlast-zcent,2) + pow(xlast-xcent,2));
                        } else {
                            scent = ((skpt[i+1].x - xlast    ) * (skpt[i  ].x - skpt[i+1].x)
                                    -(skpt[i+1].y - ylast    ) * (skpt[i+1].y - skpt[i  ].y))
                                   /((ylast       - skpt[i].y) * (skpt[i  ].x - skpt[i+1].x)
                                   - (skpt[i].x   - xlast    ) * (skpt[i+1].y - skpt[i  ].y));
                            xcent = (xlast + skpt[i].x + scent * (ylast     - skpt[i].y)) / 2;
                            ycent = (ylast + skpt[i].y + scent * (skpt[i].x - xlast    )) / 2;

                            data[0] = xcent;
                            data[1] = ycent;
                            data[2] = zmin;
                            data[3] = xlast - xcent;
                            data[4] = ylast - ycent;
                            data[5] = 0;
                            data[6] = -data[4];
                            data[7] = +data[3];
                            data[8] = 0;
                            data[9] = sqrt(pow(xlast-xcent,2) + pow(ylast-ycent,2));
                        }

                        if (scent > 0) {
                            status = EG_makeGeometry(MODL->context, CURVE, CIRCLE, NULL, NULL, data, &ecurve);
                            CHECK_STATUS(EG_makeGeometry);
                        } else {
                            status = EG_makeGeometry(MODL->context, CURVE, CIRCLE, NULL, NULL, data, &eflip);
                            CHECK_STATUS(EG_makeGeometry);

                            status = EG_flipObject(eflip, &ecurve);
                            CHECK_STATUS(EG_flipObject);
                        }

                        data[0] = xlast;
                        data[1] = ylast;
                        data[2] = zlast;
                        status = EG_invEvaluate(ecurve, data, &(tdata[0]), result);
                        CHECK_STATUS(EG_invEvaluate);

                        data[0] = skpt[i+1].x;
                        data[1] = skpt[i+1].y;
                        data[2] = skpt[i+1].z;
                        status = EG_invEvaluate(ecurve, data, &(tdata[1]), result);
                        CHECK_STATUS(EG_invEvaluate);

                        status = EG_makeTopology(MODL->context, ecurve, EDGE, TWONODE,
                                                 tdata, 2, &(Enodes[nseg]), NULL, &(Eedges[nseg]));
                        CHECK_STATUS(EG_makeTopology);

                        nseg++;
                    #endif

                    i++;   /* this is because cirarc show up in pairs in iskpnt */

                    xlast = skpt[i].x;
                    ylast = skpt[i].y;
                    zlast = skpt[i].z;

                /* intiaize or add points to a spline (to be processed above in future
                   trip through this for loop) */
                } else if (skpt[i].itype == OCSM_SPLINE) {
                    if (nspln == 0) {
                        #if   defined(GEOM_CAPRI)
                            if (xmin == xmax) {
                                pts[0] = ylast;
                                pts[1] = zlast;
                            } else if (ymin == ymax) {
                                pts[0] = zlast;
                                pts[1] = xlast;
                            } else {
                                pts[0] = xlast;
                                pts[1] = ylast;
                            }
                        #elif defined(GEOM_EGADS)
                            pts[0] = xlast;
                            pts[1] = ylast;
                            pts[2] = zlast;
                        #endif

                        nspln = 1;
                    }

                    #if  defined(GEOM_CAPRI)
                        if        (xmin == xmax) {
                            pts[2*nspln  ] = skpt[i].y;
                            pts[2*nspln+1] = skpt[i].z;
                        } else if (ymin == ymax) {
                            pts[2*nspln  ] = skpt[i].z;
                            pts[2*nspln+1] = skpt[i].x;
                        } else {
                            pts[2*nspln  ] = skpt[i].x;
                            pts[2*nspln+1] = skpt[i].y;
                        }
                    #elif defined(GEOM_EGADS)
                        pts[3*nspln  ] = skpt[i].x;
                        pts[3*nspln+1] = skpt[i].y;
                        pts[3*nspln+2] = skpt[i].z;
                    #endif

                    nspln++;
                }
            }

            /* close the sketch */
            #if   defined(GEOM_CAPRI)
                status = -gi_sIsComplete();
                CHECK_STATUS(gi_sIsComplete);

                status = gi_sClose();
                CHECK_STATUS(gi_sClose);
            #elif defined(GEOM_EGADS)
                for (iseg = 0; iseg < nseg; iseg++) {
                    sense[iseg] = SFORWARD;
                }

                if (iopen == 0) {
                    status = EG_makeTopology(MODL->context, NULL, LOOP, CLOSED,
                                             NULL, nseg, Eedges, sense, &eloop);
                    CHECK_STATUS(EG_makeTopology);

                    status = EG_makeFace(eloop, SFORWARD, NULL, Efaces);
                    CHECK_STATUS(EG_makeFace);

                    status = EG_makeTopology(MODL->context, NULL, BODY, FACEBODY,
                                             NULL, 1, Efaces, sense, &ebody);
                    CHECK_STATUS(EG_makeTopology);
                } else {
                    status = EG_makeTopology(MODL->context, NULL, LOOP, OPEN,
                                             NULL, nseg, Eedges, sense, &eloop);
                    CHECK_STATUS(EG_makeTopology);

                    status = EG_makeTopology(MODL->context, NULL, BODY, WIREBODY,
                                             NULL, 1, &eloop, NULL, &ebody);
                    CHECK_STATUS(EG_makeTopology);
                }
            #endif

            /* remember the sketch number just created */
            #if   defined(GEOM_CAPRI)
                nsketch = status;
            #elif defined(GEOM_EGADS)
            #endif

            #if   defined(SHOW_SPLINES)
            {
                #define NPER          251
                #define MAX_PLOT_PTS 9999

                int    nplot, nline, ilin[MAX_SKETCH_SIZE+1], isym[MAX_SKETCH_SIZE+1], nper[MAX_SKETCH_SIZE+1];
                int    io_kbd=5, io_scr=6, indgr=1+2+4+16+64;
                float  xplot[MAX_PLOT_PTS], yplot[MAX_PLOT_PTS];
                char   pltitl[30], title[] = "Spline  (skbeg=^, linseg=+, cirarc=o, spline=x)";

                #if   defined(GEOM_CAPRI)
                #elif defined(GEOM_EGADS)
                    int    j, periodic;
                    double trange[4], tt;
                #endif

                if        (xmin == xmax) {
                    sprintf(title, "~y~z~Sketch at x=%10.5f", xmin);
                } else if (ymin == ymax) {
                    sprintf(title, "~z~x~Sketch at y=%10.5f", ymin);
                } else {
                    sprintf(title, "~x~y~Sketch at z=%10.5f", zmin);
                }

                /* put sketch points onto plot */
                nline = 0;
                nplot = 0;
                for (i = 0; i < (*nskpt); i++) {
                    ilin[nline] = 0;
                    nper[nline] = 1;

                    if        (skpt[i].itype == OCSM_SKBEG) {
                        isym[nline] = GR_TRIANGLE;
                    } else if (skpt[i].itype == OCSM_LINSEG) {
                        isym[nline] = GR_PLUS;
                    } else if (skpt[i].itype == OCSM_CIRARC) {
                        isym[nline] = GR_CIRCLE;
                    } else if (skpt[i].itype == OCSM_SPLINE) {
                        isym[nline] = GR_X;
                    }
                    nline++;

                    if        (xmin == xmax) {
                        xplot[nplot] = skpt[i].y;
                        yplot[nplot] = skpt[i].z;
                        nplot++;
                    } else if (ymin == ymax) {
                        xplot[nplot] = skpt[i].z;
                        yplot[nplot] = skpt[i].x;
                        nplot++;
                    } else {
                        xplot[nplot] = skpt[i].x;
                        yplot[nplot] = skpt[i].y;
                        nplot++;
                    }
                }

                /* put sketch evaluations onto plot */
                #if   defined(GEOM_CAPRI)
                    SPRINT0(0, "WARNING:: dotted line is not actual spline evaluation");

                    ilin[nline] = +3;
                    isym[nline] =  0;
                    nper[nline] = (*nskpt);
                    nline++;

                    for (i = 0; i < (*nskpt); i++) {
                        if        (xmin == xmax) {
                            xplot[nplot] = skpt[i].y;
                            yplot[nplot] = skpt[i].z;
                            nplot++;
                        } else if (ymin == ymax) {
                            xplot[nplot] = skpt[i].z;
                            yplot[nplot] = skpt[i].x;
                            nplot++;
                        } else {
                            xplot[nplot] = skpt[i].x;
                            yplot[nplot] = skpt[i].y;
                            nplot++;
                        }
                    }
                #elif defined(GEOM_EGADS)
                    status = EG_getTopology(eloop, &eref, &oclass, &mtype,
                                            data, &nchild, &ebodys, &senses);
                    CHECK_STATUS(EG_getTopology);

                    if (nplot+nchild*NPER > MAX_PLOT_PTS) {
                        SPRINT4(0, "nplot=%d, nchild=%d, NPER=%d, MAX_PLOT_PTS=%d",
                                nplot, nchild, NPER, MAX_PLOT_PTS);
                        status = OCSM_INTERNAL_ERROR;
                        CHECK_STATUS(ShowSketch);
                    }

                    for (j = 0; j < nchild; j++) {
                        status = EG_getRange(ebodys[j], trange, &periodic);
                        CHECK_STATUS(EG_getRange);

                        for (i = 0; i < NPER; i++) {
                            tt = trange[0] + (double)(i) / (double)(NPER-1) * (trange[1] - trange[0]);

                            status = EG_evaluate(ebodys[j], &tt, data);
                            CHECK_STATUS(EG_evaluate);

                            if        (xmin == xmax) {
                                xplot[nplot] = data[1];
                                yplot[nplot] = data[2];
                                nplot++;
                            } else if (ymin == ymax) {
                                xplot[nplot] = data[2];
                                yplot[nplot] = data[0];
                                nplot++;
                            } else {
                                xplot[nplot] = data[0];
                                yplot[nplot] = data[1];
                                nplot++;
                            }
                        }

                        ilin[nline] =  +1;
                        isym[nline] =  -1;
                        nper[nline] = NPER;
                        nline++;
                    }
                #endif

                /* plot */
                grinit_(&io_kbd, &io_scr, title, strlen(title));
                grline_(ilin, isym, &nline, pltitl, &indgr, xplot, yplot, nper, strlen(pltitl));
            }
            #endif

            /* close the sketcher */
            (*nskpt) = 0;

            /* make a new wire Body */
            if (iopen == 0) {
                status = newBody(MODL, ibrch, OCSM_SKEND, MODL->nbody, -1,
                                 args, OCSM_SHEET_BODY, &ibody);
                CHECK_STATUS(newBody);
            } else {
                status = newBody(MODL, ibrch, OCSM_SKEND, MODL->nbody, -1,
                                 args, OCSM_WIRE_BODY, &ibody);
                CHECK_STATUS(newBody);
            }

            #if   defined(GEOM_CAPRI)
                MODL->body[ibody].ivol = -nsketch;
            #elif defined(GEOM_EGADS)
                MODL->body[ibody].ebody = ebody;
            #endif

            /* mark the new Faces (if any) with the current Body */
            #if   defined(GEOM_CAPRI)
                /* this does not actually do anything but is used to avoid compiler warnings */
                status = setFaceAttribute(MODL, 0, 0, 0, npatn, patn);
                CHECK_STATUS(setFaceAttribute);
            #elif defined(GEOM_EGADS)
                status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
                CHECK_STATUS(EG_getBodyTopos);

                for (iface = 1; iface <= nface; iface++) {
                    status = setFaceAttribute(MODL, ibody, iface, 0, npatn, patn);
                    CHECK_STATUS(setFaceAttribute);
                }

                EG_free(efaces);
            #endif

            /* finish the Body */
            status = finishBody(MODL, ibody);
            CHECK_STATUS(finishBody);

            /* push the Sketch onto the stack */
            stack[(*nstack)++] = ibody;

            SPRINT1(1, "                          Sketch %4d created", ibody);
        }
    }

cleanup:
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        FREE(cp);
    #endif

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildSolver - implement OCSM_SOLVERs for ocsmBuild                 *
 *                                                                      *
 ************************************************************************
 */

static int
buildSolver(modl_T *modl,
            int    ibrch,
            int    *ncon,
            int    solcons[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       jbrch, type, iter, niter, ipmtr, jpmtr, i, j, ivar, icon;
    double    neg_f0[MAX_SOLVER_SIZE], dfdx[MAX_SOLVER_SIZE*MAX_SOLVER_SIZE];
    double    delx[MAX_SOLVER_SIZE], f0max, f0last, save_value, omega;
    double    args[10], value;
    char      name[MAX_EXPR_LEN];

    static int nvar;
    static int solvars[MAX_SOLVER_SIZE];

    ROUTINE(buildSolver);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    args[2] = 0;
    args[3] = 0;
    args[4] = 0;
    args[5] = 0;
    args[6] = 0;
    args[7] = 0;
    args[8] = 0;
    args[9] = 0;

    /* execute: "solbeg" */
    if (type == OCSM_SOLBEG) {
        SPRINT2(1, "    executing [%4d] solbeg:         %s",
                ibrch, &(MODL->brch[ibrch].arg1[1]));

        /* initialize the number of solver constraints and variables */
        *ncon = 0;
        nvar  = 0;

        /* make a list of the solver variables by parsing arg1 */
        name[j=0] = '\0';
        for (i = 1; i < strlen(MODL->brch[ibrch].arg1); i++) {
            if (MODL->brch[ibrch].arg1[i] != ';') {
                name[j  ] = MODL->brch[ibrch].arg1[i];
                name[j+1] = '\0';
                j++;
            } else {
                ipmtr = 0;
                for (jpmtr = 1; jpmtr <= MODL->npmtr; jpmtr++) {
                    if (strcmp(MODL->pmtr[jpmtr].name, name) == 0            &&
                               MODL->pmtr[jpmtr].type        == OCSM_INTERNAL   ) {
                        ipmtr = jpmtr;
                        break;
                    }
                }

                if (ipmtr == 0) {
                    SPRINT1(0, "WARNING:: name \"%s\" not an INTERNAL parameter", name);
                    status = OCSM_NAME_NOT_FOUND;
                    goto cleanup;
                }

                solvars[nvar++] = ipmtr;

                if (nvar > MAX_SOLVER_SIZE) {
                    status = OCSM_TOO_MANY_SOLVER_VARS;
                    CHECK_STATUS(solbeg);
                }

                name[j=0] = '\0';
            }
        }

    /* execute: "solcon expr" */
    } else if (type == OCSM_SOLCON) {
        SPRINT2(1, "    executing [%4d] solcon:         %s",
                ibrch, &(MODL->brch[ibrch].arg1[1]));

        solcons[(*ncon)++] = ibrch;

        if (*ncon > MAX_SOLVER_SIZE) {
            status = OCSM_TOO_MANY_SOLVER_VARS;
            CHECK_STATUS(solcon);
        }

    /* execute: "solend" */
    } else if (type == OCSM_SOLEND) {
        SPRINT1(1, "    executing [%4d] solend:",
                ibrch);

        for (ivar = 0; ivar < nvar; ivar++) {
            jpmtr = solvars[ivar];
            SPRINT3(2, "        var[%2d] = %3d [%s]",
                    ivar, jpmtr, MODL->pmtr[jpmtr].name);
        }
        for (icon = 0; icon < *ncon; icon++) {
            jbrch = solcons[icon];
            SPRINT3(2, "        con[%2d] = %3d [%s]",
                    icon, jbrch, &(MODL->brch[jbrch].arg1[1]));
        }

        /* if the number of constraints does not match the number of
           sketch Parameters, return an error */
        if        (*ncon < nvar) {
            status = OCSM_UNDERCONSTRAINED;
            CHECK_STATUS(solend);
        } else if (*ncon > nvar) {
            status = OCSM_OVERCONSTRAINED;
            CHECK_STATUS(solend);
        }

        /* if there were no constraints, simply return */
        if (*ncon == 0) {
            goto cleanup;
        }

        /* Newton iteration to change the solver variables until
           the constraints are satisfied */
        niter  = 100;
        omega  = 0.50;
        f0last = 0;
        for (iter = 0; iter < niter; iter++) {

            /* evaluate the constraints */
            f0max = 0;
            for (icon = 0; icon < *ncon; icon++) {
                jbrch  = solcons[icon];
                status = str2val(&(MODL->brch[jbrch].arg1[1]), MODL, &value);
                CHECK_STATUS(str2val);

                neg_f0[icon] = -value;
                SPRINT2(2,"        f0[%4d] = %11.5f", jbrch, value);

                if (fabs(value) > f0max) {
                    f0max = fabs(value);
                }
            }
            SPRINT2(1, "    -> solving sketch: iter = %3d,   f0max = %12.4e", iter, f0max);

            /* if we have converged, stop the Newton iterations */
            if (f0max < EPS06) {
                DPRINT0("converged");
                break;
            }

            /* f0max < f0last, we are converging, so increase omega */
            if (f0max < f0last) {
                omega = MIN(1.2*omega, 1);
            }
            f0last = f0max;

            /* build up the Jacobian matrix by perturbing the solver variables
               one at a time */
            for (ivar = 0; ivar < nvar; ivar++) {
                jpmtr = solvars[ivar];

                save_value = MODL->pmtr[jpmtr].value[0];
                MODL->pmtr[jpmtr].value[0] += EPS06;

                for (icon = 0; icon < *ncon; icon++) {
                    jbrch = solcons[icon];
                    status = str2val(&(MODL->brch[jbrch].arg1[1]), MODL, &value);
                    CHECK_STATUS(str2val);

                    dfdx[icon*(*ncon)+ivar] = (value + neg_f0[icon]) / EPS06;
                }

                MODL->pmtr[jpmtr].value[0] = save_value;
            }

            /* print out the Jacobian matrix */
            DPRINT0("Jacobian matrix");
            for (icon = 0; icon < *ncon; icon++) {
                DPRINT1x("%3d: ", icon);

                for (ivar = 0; ivar < nvar; ivar++) {
                    DPRINT1x("%8.4f ", dfdx[icon*(*ncon)+ivar]);
                }
                DPRINT0(" ");
            }

            /* take the Newton step */
            status = matsol(dfdx, neg_f0, *ncon, delx);
            CHECK_STATUS(matsol);

            for (ivar = 0; ivar < nvar; ivar++) {
                jpmtr = solvars[ivar];
                MODL->pmtr[jpmtr].value[0] += omega * delx[ivar];
            }
        }

        /* now that we have run out of iterations, check for convergence */
        if (f0max > EPS06) {
            status = OCSM_NOT_CONVERGED;
            goto cleanup;
        }

        /* initialize variables for next solver */
        nvar  = 0;
        *ncon = 0;
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   buildTransform - implemet OCSM_TRANSFORMs for ocsmBuild            *
 *                                                                      *
 ************************************************************************
 */

static int
buildTransform(modl_T *modl,
               int    ibrch,
               int    *nstack,
               int    stack[])
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int        type, ibody, ibodyl;
    double     args[10], matrix[3][4];
    double     cosx, cosy, cosz, sinx, siny, sinz, fact, dx, dy, dz;

    #if   defined(GEOM_CAPRI)
        int        ivoll, j;
        double     temp0, temp1, temp2;
    #elif defined(GEOM_EGADS)
        ego        ebody, ebodyl, exform;
    #endif

    ROUTINE(buildTransform);
    DPRINT2("%s(ibrch=%d) {",
            routine, ibrch);

    /* --------------------------------------------------------------- */

    type = MODL->brch[ibrch].type;

    /* get the values for the arguments */
    if (MODL->brch[ibrch].narg >= 1) {
        status = str2val(MODL->brch[ibrch].arg1, MODL, &args[1]);
        CHECK_STATUS(str2val:val1);
    } else {
        args[1] = 0;
    }
    if (MODL->brch[ibrch].narg >= 2) {
        status = str2val(MODL->brch[ibrch].arg2, MODL, &args[2]);
        CHECK_STATUS(str2val:val2);
    } else {
        args[2] = 0;
    }
    if (MODL->brch[ibrch].narg >= 3) {
        status = str2val(MODL->brch[ibrch].arg3, MODL, &args[3]);
        CHECK_STATUS(str2val:val3);
    } else {
        args[3] = 0;
    }
    args[4] = 0;
    args[5] = 0;
    args[6] = 0;
    args[7] = 0;
    args[8] = 0;
    args[9] = 0;

    /* execute: "translate dx dy dz" */
    if (type == OCSM_TRANSLATE) {
        SPRINT4(1, "    executing [%4d] translate:  %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* pop an Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(translate);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_TRANSLATE, ibodyl, -1,
                         args, MODL->body[ibodyl].botype, &ibody);
        CHECK_STATUS(newBody);

        /* apply the new transformation matrix */
        dx = args[1];
        dy = args[2];
        dz = args[3];

        #if   defined(GEOM_CAPRI)
            ivoll = MODL->body[ibodyl].ivol;
            if (ivoll < 0) {
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(translate:sketch);
            } else {
                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                matrix[0][3] += dx;
                matrix[1][3] += dy;
                matrix[2][3] += dz;

                status = gi_iSetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                MODL->body[ibody].ivol = ivoll;
            }
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            matrix[0][0] = 1; matrix[0][1] = 0; matrix[0][2] = 0; matrix[0][3] = dx;
            matrix[1][0] = 0; matrix[1][1] = 1; matrix[1][2] = 0; matrix[1][3] = dy;
            matrix[2][0] = 0; matrix[2][1] = 0; matrix[2][2] = 1; matrix[2][3] = dz;

            status = EG_makeTransform(MODL->context, (double*)matrix, &exform);
            CHECK_STATUS(EG_makeTransform);

            status = EG_copyObject(ebodyl, exform, &ebody);
            CHECK_STATUS(EG_copyObject);

            status = EG_deleteObject(exform);
            CHECK_STATUS(EG_deleteObject);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "rotatex angDeg yaxis zaxis" */
    } else if (type == OCSM_ROTATEX) {
        SPRINT4(1, "    executing [%4d] rotatex:    %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(rotatex);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_ROTATEX, ibodyl, -1,
                         args, MODL->body[ibodyl].botype, &ibody);
        CHECK_STATUS(newBody);

        /* apply the new transformation matrix */
        cosx = cos(args[1] * PIo180);
        sinx = sin(args[1] * PIo180);
        dy   =     args[2];
        dz   =     args[3];

        #if   defined(GEOM_CAPRI)
            ivoll = MODL->body[ibodyl].ivol;
            if (ivoll < 0) {
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(rotatex:sketch);
            } else {
                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                for (j = 0; j < 4; j++) {
//                  temp0 = matrix[0][j];
                    temp1 = matrix[1][j];
                    temp2 = matrix[2][j];

                    matrix[1][j] = temp1 * cosx - temp2 * sinx;
                    matrix[2][j] = temp1 * sinx + temp2 * cosx;
                }

                matrix[1][3] += dy - dy * cosx + dz * sinx;
                matrix[2][3] += dz - dy * sinx - dz * cosx;

                status = gi_iSetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                MODL->body[ibody].ivol = ivoll;
            }
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            matrix[0][0] = 1; matrix[0][1] = 0;    matrix[0][2] = 0;    matrix[0][3] = 0;
            matrix[1][0] = 0; matrix[1][1] = cosx; matrix[1][2] =-sinx; matrix[1][3] = dy - dy * cosx + dz * sinx;
            matrix[2][0] = 0; matrix[2][1] = sinx; matrix[2][2] = cosx; matrix[2][3] = dz - dy * sinx - dz * cosx;

            status = EG_makeTransform(MODL->context, (double*)matrix, &exform);
            CHECK_STATUS(EG_makeTransform);

            status = EG_copyObject(ebodyl, exform, &ebody);
            CHECK_STATUS(EG_copyObject);

            status = EG_deleteObject(exform);
            CHECK_STATUS(EG_deleteObject);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "rotatey angDeg zaxis xaxis" */
    } else if (type == OCSM_ROTATEY) {
        SPRINT4(1, "    executing [%4d] rotatey:    %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(rotatey);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_ROTATEY, ibodyl, -1,
                         args, MODL->body[ibodyl].botype, &ibody);
        CHECK_STATUS(newBody);

        /* apply the new transformation matrix */
        cosy = cos(args[1] * PIo180);
        siny = sin(args[1] * PIo180);
        dz   =     args[2];
        dx   =     args[3];

        #if   defined(GEOM_CAPRI)
            ivoll = MODL->body[ibodyl].ivol;
            if (ivoll < 0) {
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(rotatey:sketch);
            } else {
                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                for (j = 0; j < 4; j++) {
                    temp0 = matrix[0][j];
//                  temp1 = matrix[1][j];
                    temp2 = matrix[2][j];

                    matrix[2][j] = temp2 * cosy - temp0 * siny;
                    matrix[0][j] = temp2 * siny + temp0 * cosy;
                }

                matrix[2][3] += dz - dz * cosy + dx * siny;
                matrix[0][3] += dx - dz * siny - dx * cosy;

                status = gi_iSetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                MODL->body[ibody].ivol = ivoll;
            }
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            matrix[0][0] = cosy; matrix[0][1] = 0; matrix[0][2] = siny; matrix[0][3] = dx - dz * siny - dx * cosy;
            matrix[1][0] = 0;    matrix[1][1] = 1; matrix[1][2] = 0;    matrix[1][3] = 0;
            matrix[2][0] =-siny; matrix[2][1] = 0; matrix[2][2] = cosy; matrix[2][3] = dz - dz * cosy + dx * siny;

            status = EG_makeTransform(MODL->context, (double*)matrix, &exform);
            CHECK_STATUS(EG_makeTransform);

            status = EG_copyObject(ebodyl, exform, &ebody);
            CHECK_STATUS(EG_copyObject);

            status = EG_deleteObject(exform);
            CHECK_STATUS(EG_deleteObject);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "rotatez angdeg xaxis yaxis" */
    } else if (type == OCSM_ROTATEZ) {
        SPRINT4(1, "    executing [%4d] rotatez:    %11.5f %11.5f %11.5f",
                ibrch, args[1], args[2], args[3]);

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(rotatez);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_ROTATEZ, ibodyl, -1,
                         args, MODL->body[ibodyl].botype, &ibody);
        CHECK_STATUS(newBody);

        /* apply the new transformation matrix */
        cosz = cos(args[1] * PIo180);
        sinz = sin(args[1] * PIo180);
        dx   =     args[2];
        dy   =     args[3];

        #if   defined(GEOM_CAPRI)
            ivoll = MODL->body[ibodyl].ivol;
            if (ivoll < 0) {
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(rotatez:sketch);
            } else {
                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                for (j = 0; j < 4; j++) {
                    temp0 = matrix[0][j];
                    temp1 = matrix[1][j];
//                  temp2 = matrix[2][j];

                    matrix[0][j] = temp0 * cosz - temp1 * sinz;
                    matrix[1][j] = temp0 * sinz + temp1 * cosz;
                }

                matrix[0][3] += dx - dx * cosz + dy * sinz;
                matrix[1][3] += dy - dx * sinz - dy * cosz;

                status = gi_iSetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                MODL->body[ibody].ivol = ivoll;
            }
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            matrix[0][0] = cosz; matrix[0][1] =-sinz; matrix[0][2] = 0; matrix[0][3] = dx - dx * cosz + dy * sinz;
            matrix[1][0] = sinz; matrix[1][1] = cosz; matrix[1][2] = 0; matrix[1][3] = dy - dx * sinz - dy * cosz;
            matrix[2][0] = 0;    matrix[2][1] = 0;    matrix[2][2] = 1; matrix[2][3] = 0;

            status = EG_makeTransform(MODL->context, (double*)matrix, &exform);
            CHECK_STATUS(EG_makeTransform);

            status = EG_copyObject(ebodyl, exform, &ebody);
            CHECK_STATUS(EG_copyObject);

            status = EG_deleteObject(exform);
            CHECK_STATUS(EG_deleteObject);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);

    /* execute: "scale fact" */
    } else if (type == OCSM_SCALE) {
        SPRINT2(1, "    executing [%4d] scale:      %11.5f",
                ibrch, args[1]);

        /* pop a Body from the stack */
        if ((*nstack) < 1) {
            status = OCSM_EXPECTING_ONE_BODY;
            CHECK_STATUS(scale);
        } else {
            ibodyl = stack[--(*nstack)];
        }

        /* create the Body */
        status = newBody(MODL, ibrch, OCSM_SCALE, ibodyl, -1,
                         args, MODL->body[ibodyl].botype, &ibody);
        CHECK_STATUS(newBody);

        /* apply the new transformation matrix */
        fact = args[1];

        #if   defined(GEOM_CAPRI)
            ivoll = MODL->body[ibodyl].ivol;
            if (ivoll < 0) {
                status = OCSM_UNSUPPORTED;
                CHECK_STATUS(scale:sketch);
            } else {
                status = gi_iGetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iGetDisplace);

                for (j = 0; j < 4; j++) {
                    matrix[0][j] *= fact;
                    matrix[1][j] *= fact;
                    matrix[2][j] *= fact;
                }

                status = gi_iSetDisplace(ivoll, &(matrix[0][0]));
                CHECK_STATUS(gi_iSetDisplace);

                MODL->body[ibody].ivol = ivoll;
            }
        #elif defined(GEOM_EGADS)
            ebodyl = MODL->body[ibodyl].ebody;

            matrix[0][0] = fact; matrix[0][1] = 0;    matrix[0][2] = 0;    matrix[0][3] = 0;
            matrix[1][0] = 0;    matrix[1][1] = fact; matrix[1][2] = 0;    matrix[1][3] = 0;
            matrix[2][0] = 0;    matrix[2][1] = 0;    matrix[2][2] = fact; matrix[2][3] = 0;

            status = EG_makeTransform(MODL->context, (double*)matrix, &exform);
            CHECK_STATUS(EG_makeTransform);

            status = EG_copyObject(ebodyl, exform, &ebody);
            CHECK_STATUS(EG_copyObject);

            status = EG_deleteObject(exform);
            CHECK_STATUS(EG_deleteObject);

            MODL->body[ibody].ebody = ebody;
        #endif

        /* finish the Body */
        status = finishBody(MODL, ibody);
        CHECK_STATUS(finishBody);

        /* push the Body onto the stack */
        stack[(*nstack)++] = ibody;

        SPRINT1(1, "                          Body   %4d created", ibody);
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   newBody - create and initialize a new Body                         *
 *                                                                      *
 ************************************************************************
 */

static int
newBody(modl_T *modl,                   /* (in)  pointer to MODL */
        int    ibrch,                   /* (in)  Branch index (1-mbrch) */
        int    brtype,                  /* (in)  Branch type */
        int    ileft,                   /* (in)  left parent Body (or 0) */
        int    irite,                   /* (in)  rite parent Body (or 0) */
        double args[],                  /* (in)  arguments */
        int    botype,                  /* (in)  Body type */
        int    *ibody)                  /* (out) new Body index */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    ROUTINE(newBody);
    DPRINT6("%s(ibrch=%d, brtype=%d, ileft=%d, irite=%d, botype=%d) {",
            routine, ibrch, brtype, ileft, irite, botype);

    /* --------------------------------------------------------------- */

    /* extend the Body list (if needed) */
    if (MODL->nbody >= MODL->mbody) {
        MODL->mbody += 25;
        if (MODL->body == NULL) {
            MALLOC(MODL->body, body_T, MODL->mbody+1);
        } else {
            RALLOC(MODL->body, body_T, MODL->mbody+1);
        }
    }

    /* create the new Body and initialize it */
    MODL->nbody += 1;

    *ibody = MODL->nbody;
    MODL->body[*ibody].ibrch  = ibrch;
    MODL->body[*ibody].brtype = brtype;
    MODL->body[*ibody].ileft  = ileft;
    MODL->body[*ibody].irite  = irite;
    MODL->body[*ibody].ichld  = 0;
    MODL->body[*ibody].arg1   = args[1];
    MODL->body[*ibody].arg2   = args[2];
    MODL->body[*ibody].arg3   = args[3];
    MODL->body[*ibody].arg4   = args[4];
    MODL->body[*ibody].arg5   = args[5];
    MODL->body[*ibody].arg6   = args[6];
    MODL->body[*ibody].arg7   = args[7];
    MODL->body[*ibody].arg8   = args[8];
    MODL->body[*ibody].arg9   = args[9];

    #if   defined(GEOM_CAPRI)
        MODL->body[*ibody].ivol  = 0;
    #elif defined(GEOM_EGADS)
        MODL->body[*ibody].ebody = NULL;
        MODL->body[*ibody].etess = NULL;
    #endif

    MODL->body[*ibody].onstack= 0;
    MODL->body[*ibody].botype = botype;
    MODL->body[*ibody].nnode  = 0;
    MODL->body[*ibody].node   = NULL;
    MODL->body[*ibody].nedge  = 0;
    MODL->body[*ibody].edge   = NULL;
    MODL->body[*ibody].nface  = 0;
    MODL->body[*ibody].face   = NULL;

    /* link children */
    if (ileft > 0) {
        MODL->body[ileft].ichld = *ibody;
    }
    if (irite > 0) {
        MODL->body[irite].ichld = *ibody;
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}



/*
 ************************************************************************
 *                                                                      *
 *   setFaceAttribute - set attribute(s) to Face                        *
 *                                                                      *
 ************************************************************************
 */

static int
setFaceAttribute(modl_T *modl,          /* (in)  pointer to MODL */
                 int    ibody,          /* (in)  Body index (1-nbody) */
                 int    iface,          /* (in)  Face index (1-nface) */
                 int    iford,          /* (in)  Face order */
                 int    npatn,          /* (in)  number of active patterns */
                 patn_T *patn)          /* (in)  array  of active patterns */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       *iattrib = NULL, nattrib, i, ibrch;
    double    value;
    body_T    *body = NULL;

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        int       nface;
        ego       *efaces;
    #endif

    ROUTINE(setFaceAttribute);
    DPRINT5("%s(ibody=%d, iface=%d, iford=%d, npatn=%d) {",
            routine, ibody, iface, iford, npatn);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibody is given */
    if (ibody == 0) {
        goto cleanup;
    } else if (ibody < 1 || ibody > MODL->nbody) {
        status = OCSM_ILLEGAL_BODY_INDEX;
        goto cleanup;
    }

    /* get Body info */
    body = &(MODL->body[ibody]);
    ibrch =  MODL->body[ibody].ibrch;

    /* allocate storage for new attributes */
    MALLOC(iattrib, int, 2+2*npatn);

    nattrib = 0;
    iattrib[nattrib++] = ibody;
    iattrib[nattrib++] = iford;

    for (i = npatn-1; i >= 0; i--) {
        iattrib[nattrib++] = patn[i].ipatbeg;
        iattrib[nattrib++] = patn[i].icopy;
    }

    /* save the Body attributes */
    #if   defined(GEOM_CAPRI)
       status = gi_qSetEntAttribute(body->ivol, CAPRI_FACE, iface, "body", 0,
                                    CAPRI_INTEGER, nattrib, iattrib, NULL, NULL);
       CHECK_STATUS(gi_qSetEntAttribute);

       iattrib[0] = ibrch;

       status = gi_qSetEntAttribute(body->ivol, CAPRI_FACE, iface, "brch", 0,
                                    CAPRI_INTEGER, nattrib, iattrib, NULL, NULL);
       CHECK_STATUS(gi_qSetEntAttribute);

        for (i = 0; i < MODL->brch[ibrch].nattr; i++) {
            status = str2val(MODL->brch[ibrch].attr[i].value, MODL, &value);
            CHECK_STATUS(str2val);

            status = gi_qSetEntAttribute(body->ivol, CAPRI_FACE, iface,
                                         MODL->brch[ibrch].attr[i].name, 0,
                                         CAPRI_REAL, 1, NULL, &value, NULL);
            CHECK_STATUS(gi_qSetEntAttribute);
        }
    #elif defined(GEOM_EGADS)
        status = EG_getBodyTopos(body->ebody, NULL, FACE, &nface, &efaces);
        CHECK_STATUS(EG_getBodyTopos);

        if (nface > 0) {
            status = EG_attributeAdd(efaces[iface-1], "body", ATTRINT,
                                     2, iattrib, NULL, NULL);
            CHECK_STATUS(EG_attributeAdd);

            iattrib[0] = ibrch;

            status = EG_attributeAdd(efaces[iface-1], "brch", ATTRINT,
                                     nattrib, iattrib, NULL, NULL);
            CHECK_STATUS(EG_attributeAdd);

            for (i = 0; i < MODL->brch[ibrch].nattr; i++) {
                status = str2val(MODL->brch[ibrch].attr[i].value, MODL, &value);
                CHECK_STATUS(str2val);

                status = EG_attributeAdd(efaces[iface-1], MODL->brch[ibrch].attr[i].name,
                                         ATTRREAL, 1, NULL, &value, NULL);
                CHECK_STATUS(EG_attributeAdd);
            }

            EG_free(efaces);
        }
    #endif

cleanup:
    FREE(iattrib);

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   finishBody - finish the definition of the body                     *
 *                                                                      *
 ************************************************************************
 */

static int
finishBody(modl_T *modl,                /* (in)  pointer to MODL */
           int    ibody)                /* (in)  Body index (1-nbody) */
{
    int       status = SUCCESS;         /* (out) return status */

    modl_T    *MODL = (modl_T*)modl;

    int       nnode, nedge, nface, inode, iedge, iface, ibodyl, ibodyr;
    int       jnode, jedge, jface;
    int       ileft, irite, nlist, itype, iattrib[2], iokayl, iokayr, i;
    CINT      *tempIlist;
    CDOUBLE   *tempRlist;
    CCHAR     *tempClist;
    body_T    *body = NULL;

    #if   defined(GEOM_CAPRI)
        int       n, nloop, iloop, *loops, *edges;
        int       ivol, nbound, nodes[2];
        double    trange[2], urange[4];
        char      *name;
    #elif defined(GEOM_EGADS)
        int          nchild, ichild, oclass, mtype, ibrch, nn;
        ego          ebody, *enodes, eedge, *eedges, eface, *efaces, *echildren;
        ego          topRef, prev, next;
    #endif

    ROUTINE(finishBody);
    DPRINT2("%s(ibody=%d) {",
            routine, ibody);

    /* --------------------------------------------------------------- */

    /* check magic number */
    if (MODL == NULL) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    } else if (MODL->magic != OCSM_MAGIC) {
        status = OCSM_NOT_MODL_STRUCTURE;
        goto cleanup;
    }

    /* check that valid ibody is given */
    if (ibody < 1 || ibody > MODL->nbody) {
        status = OCSM_ILLEGAL_BODY_INDEX;
        goto cleanup;
    }

    /* get Body info */
    body = &(MODL->body[ibody]);

    #if   defined(GEOM_CAPRI)
        if (body->botype != OCSM_SOLID_BODY) {
            goto cleanup;
        } else {
            ivol   = body->ivol;
            status = gi_dGetVolume(ivol, &nnode, &nedge, &nface, &nbound, &name);
            CHECK_STATUS(gi_dGetVolume);
        }
    #elif defined(GEOM_EGADS)
        nnode = 0;
        nedge = 0;
        nface = 0;

        ebody  = body->ebody;

        status = EG_getInfo(ebody, &oclass, &mtype, &topRef, &prev, &next);
        CHECK_STATUS(EG_getInfo);

        if (oclass != BODY) {
            goto cleanup;
        }

        status = EG_getBodyTopos(ebody, NULL, NODE, &nn, &enodes);
        CHECK_STATUS(EG_getBodyTopos:NODE);
        nnode += nn;
        EG_free(enodes);

        status = EG_getBodyTopos(ebody, NULL, EDGE, &nn, &eedges);
        CHECK_STATUS(EG_getBodyTopos:EDGE);
        nedge += nn;
        EG_free(eedges);

        status = EG_getBodyTopos(ebody, NULL, FACE, &nn, &efaces);
        CHECK_STATUS(EG_getBodyTopos:FACE);
        nface += nn;
        EG_free(efaces);
    #endif

    body->nnode = nnode;
    body->nedge = nedge;
    body->nface = nface;

    /* add the current Branch as an attribute for this Body */
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        ebody = body->ebody;
        ibrch = MODL->body[ibody].ibrch;

        status = EG_attributeAdd(ebody, "body", ATTRINT, 1,
                                 &ibody, NULL, NULL);
        CHECK_STATUS(EG_attributeAdd);

        status = EG_attributeAdd(ebody, "brch", ATTRINT, 1,
                                 &ibrch, NULL, NULL);
        CHECK_STATUS(EG_attributeAdd);
    #endif

    /* initialize the Nodes for this Body */
    MALLOC(body->node, node_T, nnode+1);

    #if   defined(GEOM_CAPRI)
        jnode = 1;
        for (inode = 1; inode <= nnode; inode++) {
            body->node[jnode].nedge = 0;

            jnode++;
        }
    #elif defined(GEOM_EGADS)
        jnode = 1;

        status = EG_getBodyTopos(body->ebody, NULL, NODE, &nnode, &enodes);
        CHECK_STATUS(EG_getBodyTopos);

        for (inode = 1; inode <= nnode; inode++) {
            body->node[jnode].enode = enodes[inode-1];

            jnode++;
        }

        EG_free(enodes);
    #endif

    body->gratt.object = NULL;
    body->gratt.active = 1;                              /* active */
    body->gratt.color  = 0x00000000;                     /* black */
    body->gratt.ptsize = 5;
    body->gratt.render = 64;                             /* FORWARD */
    body->gratt.dirty  = 1;

    /* initialize the Edges for this Body */
    MALLOC(body->edge, edge_T, nedge+1);

    jedge = 1;

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        status = EG_getBodyTopos(body->ebody, NULL, EDGE, &nedge, &eedges);
        CHECK_STATUS(EG_getBodyTopos);
    #endif

    for (iedge = 1; iedge <= nedge; iedge++) {
        body->edge[jedge].ileft        = -1;
        body->edge[jedge].irite        = -1;
        body->edge[jedge].ibody        = -1;
        body->edge[jedge].iford        = -1;
        body->edge[jedge].gratt.object = NULL;
        body->edge[jedge].gratt.active = 1;              /* active */
        body->edge[jedge].gratt.color  = 0x00ff0000;     /* red */
        body->edge[jedge].gratt.bcolor = 0x00ffffff;     /* white */
        body->edge[jedge].gratt.mcolor = 0x00000000;     /* black */
        body->edge[jedge].gratt.lwidth = 2;
        body->edge[jedge].gratt.ptsize = 3;
        body->edge[jedge].gratt.render = 2 + 64;         /* FOREGROUND | FORWARD */
        body->edge[jedge].gratt.dirty  = 1;

        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            body->edge[jedge].eedge = eedges[iedge-1];
        #endif

        jedge++;
    }

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        EG_free(eedges);
    #endif

    /* initialize the Faces for this Body */
    MALLOC(body->face, face_T, nface+1);

    jface = 1;

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        status = EG_getBodyTopos(body->ebody, NULL, FACE, &nface, &efaces);
        CHECK_STATUS(EG_getBodyTopos);
    #endif

    for (iface = 1; iface <= nface; iface++) {
        body->face[jface].nbody        = 0;
        body->face[jface].ibody        = NULL;
        body->face[jface].iford        = NULL;
        body->face[jface].gratt.object = NULL;
        body->face[jface].gratt.active = 1;              /* active */
        body->face[jface].gratt.color  = 0x00ffff00;     /* yellow */
        body->face[jface].gratt.bcolor = 0x003f3f00;     /* dark yellow */
        body->face[jface].gratt.mcolor = 0x00bfbfbf;     /* lt grey */
        body->face[jface].gratt.lwidth = 1;
        body->face[jface].gratt.ptsize = 1;
        body->face[jface].gratt.render = 2 + 4 + 64;     /* FOREGROUND | ORIENTATION | FORWARD */
        body->face[jface].gratt.dirty  = 1;

        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            body->face[jface].eface = efaces[iface-1];
        #endif

        jface++;
    }

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        EG_free(efaces);
    #endif

    nnode = body->nnode;
    nedge = body->nedge;
    nface = body->nface;

    /* determine the number of Edges adjacent to each Node */
    #if   defined(GEOM_CAPRI)
        ivol   = body->ivol;
        for (iedge = 1; iedge <= nedge; iedge++) {
            status = gi_dGetEdge(ivol, iedge, trange, nodes);
            CHECK_STATUS(gi_dGetEdge);

            body->node[nodes[0]].nedge ++;
            body->node[nodes[1]].nedge ++;
        }
    #elif defined(GEOM_EGADS)
        for (inode = 1; inode <= nnode; inode++) {
            status = EG_getBodyTopos(MODL->body[ibody].ebody,
                                     body->node[inode].enode, EDGE,
                                     &(body->node[inode].nedge), &eedges);
            CHECK_STATUS(EG_getBodyTopos);

            EG_free(eedges);
        }
    #endif

    /* set up ileft and irite on the adjacent Edges */
    #if   defined(GEOM_CAPRI)
        for (iface = 1; iface <= nface; iface++) {
            status = gi_dGetFace(ivol, iface, urange, &nloop, &loops, &edges);
            CHECK_STATUS(gi_dGetFace);

            n = 0;
            for (iloop = 0; iloop < nloop; iloop++) {
                n += loops[iloop];
            }

            for (i = 0; i < n; i++) {
                iedge = edges[2*i];
                if (edges[2*i+1] > 0) {
                    body->edge[iedge].ileft = iface;
                } else {
                    body->edge[iedge].irite = iface;
                }
            }
        }
    #elif defined(GEOM_EGADS)
        for (iedge = 1; iedge <= nedge; iedge++) {
            ebody = MODL->body[ibody].ebody;
            eedge = body->edge[iedge].eedge;
            status = EG_getBodyTopos(ebody, eedge, FACE, &nchild, &echildren);
            CHECK_STATUS(EG_getBodyTopos);

            for (iface = 1; iface <= nface; iface++) {
                eface = body->face[iface].eface;
                for (ichild = 0; ichild < nchild; ichild++) {
                    if (echildren[ichild] == eface) {
                        body->edge[iedge].ileft = body->edge[iedge].irite;
                        body->edge[iedge].irite = iface;
                    }
                }
            }

            EG_free(echildren);
        }
    #endif

    /* retrieve the Branch and Face order from the Faces attributes */
    #if   defined(GEOM_CAPRI)
        for (iface = 1; iface <= nface; iface++) {
            status = gi_dGetEntAttribute(ivol, CAPRI_FACE, iface, "body", 0,
                                         &itype, &nlist,
                                         &tempIlist, &tempRlist, &tempClist);
            CHECK_STATUS(gi_dGetEntAttribute);

            body->face[iface].nbody = nlist / 2;

            MALLOC(body->face[iface].ibody, int, body->face[iface].nbody);
            MALLOC(body->face[iface].iford, int, body->face[iface].nbody);

            for (i = 0; i < body->face[iface].nbody; i++) {
                body->face[iface].ibody[i] = tempIlist[2*i  ];
                body->face[iface].iford[i] = tempIlist[2*i+1];
            }
        }
    #elif defined(GEOM_EGADS)
        for (iface = 1; iface <= nface; iface++) {
            eface = body->face[iface].eface;
            status = EG_attributeRet(eface, "body", &itype, &nlist,
                                     &tempIlist, &tempRlist, &tempClist);

            if (status == SUCCESS && nlist >= 2) {
                body->face[iface].nbody = nlist / 2;

                MALLOC(body->face[iface].ibody, int, body->face[iface].nbody);
                MALLOC(body->face[iface].iford, int, body->face[iface].nbody);

                for (i = 0; i < body->face[iface].nbody; i++) {
                    body->face[iface].ibody[i] = tempIlist[2*i  ];
                    body->face[iface].iford[i] = tempIlist[2*i+1];
                }
            } else if (status == SUCCESS && nlist == 1) {
                body->face[iface].ibody[0] = tempIlist[0];
            } else {
                SPRINT2(0, "ERROR:: \"body\" attribute error for iface=%d (nlist=%d)", iface, nlist);
                status = OCSM_INTERNAL_ERROR;
                CHECK_STATUS(no_attribute);
            }
        }
    #endif

    /* set up the Branch and iford for each Edge */
    for (iedge = 1; iedge <= nedge; iedge++) {
        ileft = body->edge[iedge].ileft;
        irite = body->edge[iedge].irite;

        /* mark as error if non-manifold */
        if (ileft < 1 || irite < 1) {
            body->edge[iedge].ibody = -3;
            body->edge[iedge].iford = -3;

        /* if ileft and irite have same Body, then Edge has that Body too */
        } else if (body->face[ileft].ibody[0] == body->face[irite].ibody[0]) {
            body->edge[iedge].ibody = body->face[ileft].ibody[0];
            body->edge[iedge].iford = 100 * MIN(body->face[ileft].iford[0],
                                                body->face[irite].iford[0])
                                          + MAX(body->face[ileft].iford[0],
                                                body->face[irite].iford[0]);

        /* search up the tree to find the first common Branch */
        } else {
            body->edge[iedge].ibody = -4;
            body->edge[iedge].iford = -4;

            /* try each Body as a candidate */
            for (ibody = 1; ibody <= MODL->nbody; ibody++) {

                /* see if you can get to ibody from ileft */
                iokayl = 0;
                ibodyl = body->face[ileft].ibody[0];
                while (ibodyl > 0) {
                    if (ibodyl == ibody) {
                        iokayl++;
                        break;
                    }

                    ibodyl = MODL->body[ibodyl].ichld;
                }

                /* see if you can get to ibody from irite */
                iokayr = 0;
                ibodyr = body->face[irite].ibody[0];
                while (ibodyr > 0) {
                    if (ibodyr == ibody) {
                        iokayr++;
                        break;
                    }

                    ibodyr = MODL->body[ibodyr].ichld;
                }

                /* success, so mark Edge with ibody and jump out of loop */
                if (iokayl == 1 && iokayr == 1) {
                    body->edge[iedge].ibody = ibody;
                    body->edge[iedge].iford = 0;
                    break;
                }
            }
        }
    }

    /* set up the Body for each Edge */
    for (iedge = 1; iedge <= nedge; iedge++) {
        ileft = body->edge[iedge].ileft;
        irite = body->edge[iedge].irite;

        /* mark as error if non-manifold */
        if (ileft < 1 || irite < 1) {
            body->edge[iedge].ibody = -3;

        /* if ileft and irite have same Body, then Edge has that Body too */
        } else if (body->face[ileft].ibody == body->face[irite].ibody) {
            body->edge[iedge].ibody = body->face[ileft].ibody[0];

        /* search up the tree to find the first common Body */
        } else {
            body->edge[iedge].ibody = -4;

            /* try each Body as a candidate */
            for (ibody = 1; ibody <= MODL->nbody; ibody++) {

                /* see if you can get to ibody from ileft */
                iokayl = 0;
                ibodyl = body->face[ileft].ibody[0];
                while (ibodyl > 0) {
                    if (ibodyl == ibody) {
                        iokayl++;
                        break;
                    }

                    ibodyl = MODL->body[ibodyl].ichld;
                }

                /* see if you can get to ibody from irite */
                iokayr = 0;
                ibodyr = body->face[irite].ibody[0];
                while (ibodyr > 0) {
                    if (ibodyr == ibody) {
                        iokayr++;
                        break;
                    }

                    ibodyr = MODL->body[ibodyr].ichld;
                }

                /* success, so mark Edge with ibody and jump out of loop */
                if (iokayl == 1 && iokayr == 1) {
                    body->edge[iedge].ibody = ibody;
                    break;
                }
            }
        }
    }

    /* classify Edges as interior (green) if iford!=0.  otherwise, the edge is exterior (blue) */
    for (iedge = 1; iedge <= nedge; iedge++) {
        if (body->edge[iedge].iford != 0) {
            body->edge[iedge].gratt.color = 0x0000ff00;          /* green */
        } else {
            body->edge[iedge].gratt.color = 0x000000ff;          /* blue  */
        }
    }

    /* store the Branch, Face order, and Body as attributes for each Edge */
    #if   defined(GEOM_CAPRI)
        for (iedge = 1; iedge <= nedge; iedge++) {
            iattrib[0] = body->edge[iedge].ibody;
            iattrib[1] = body->edge[iedge].iford;

            status = gi_qSetEntAttribute(ivol, CAPRI_EDGE, iedge, "body", 0,
                                         CAPRI_INTEGER, 2, iattrib, NULL, NULL);
            CHECK_STATUS(gi_qSetEntAttribute);

//$$$            iattrib[0] = ibrch;
//$$$
//$$$            status = gi_qSetEntAttribute(ivol, CAPRI_EDGE, iedge, "brch", 0,
//$$$                                         CAPRI_INTEGER, 2, iattrib, NULL, NULL);
//$$$            CHECK_STATUS(gi_qSetEntAttribute);
        }
    #elif defined(GEOM_EGADS)
        for (iedge = 1; iedge <= nedge; iedge++) {
            eedge = body->edge[iedge].eedge;
            iattrib[0] = body->edge[iedge].ibody;
            iattrib[1] = body->edge[iedge].iford;

            status = EG_attributeAdd(eedge, "body", ATTRINT, 2,
                                     iattrib, NULL, NULL);
            CHECK_STATUS(EG_attributeAdd);

//$$$            iattrib[0] = ibrch;
//$$$
//$$$            status = EG_attributeAdd(eedge, "brch", ATTRINT, 2,
//$$$                                     iattrib, NULL, NULL);
//$$$            CHECK_STATUS(EG_attributeAdd);
        }
    #endif

    /* report un-attributed Edges and Faces */
    for (iedge = 1; iedge <= nedge; iedge++) {
        if        (body->edge[iedge].ibody == -3 &&
                   body->edge[iedge].iford == -3   ) {

        } else if (body->edge[iedge].ibody <=  0 ||
                   body->edge[iedge].iford <   0   ) {
            SPRINT3(0, "WARNING:: Edge %3d has .ibody=%d  .iford=%d",
                    iedge, body->edge[iedge].ibody,
                           body->edge[iedge].iford);
        }
    }

    for (iface = 1; iface <= nface; iface++) {
        if (body->face[iface].ibody[0] <= 0 ||
            body->face[iface].iford[0] <  0   ) {
            SPRINT3(0, "WARNING:: Face %3d has .ibody=%d  .iford=%d",
                    iface, body->face[iface].ibody[0],
                           body->face[iface].iford[0]);
        }
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   printBodyAttributes - prints attributes associated with an ebody   *
 *                                                                      *
 ************************************************************************
 */

static int
printBodyAttributes(modl_T *modl,       /* (in)  pointer to MODL */
                    int    ibody)       /* (in)  Body index (1-nbody) */
{
    int       status = 0;               /* (out) body that satisfies criterion (bias-0) */

    modl_T    *MODL = (modl_T*)modl;

    int       nlist, nattr, iattr, nface, nedge, iface, iedge;

    #if   defined(GEOM_CAPRI)
        int       ivol;
    #elif defined(GEOM_EGADS)
        int       itype, i;
        CINT      *tempIlist;
        CDOUBLE   *tempRlist;
        CCHAR     *attrName, *tempClist;
        ego       ebody, *efaces, *eedges;
    #endif

    ROUTINE(printBodyAttributes);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    /* get attributes associated with the Body */
    #if   defined(GEOM_CAPRI)
        ivol = MODL->body[ibody].ivol;
        nattr = 0;

        CHECK_STATUS(should never happen);
    #elif defined(GEOM_EGADS)
        ebody = MODL->body[ibody].ebody;

        status = EG_attributeNum(ebody, &nattr);
        CHECK_STATUS(EG_attributeNum);
    #endif

    for (iattr = 1; iattr <= nattr; iattr++) {
        #if   defined(GEOM_CAPRI)
            nlist = 0;
        #elif defined(GEOM_EGADS)
            status = EG_attributeGet(ebody, iattr, &attrName, &itype, &nlist,
                                     &tempIlist, &tempRlist, &tempClist);
            CHECK_STATUS(EG_attributeGet);

            SPRINT1x(3, "        %-20s =", attrName);

            if        (itype == ATTRINT) {
                for (i = 0; i < nlist; i++) {
                    SPRINT1x(3, "%5d ", tempIlist[i]);
                }
                SPRINT0(3, " ");
            } else if (itype == ATTRREAL) {
                for (i = 0; i < nlist; i++) {
                    SPRINT1x(3, "%11.5f ", tempRlist[i]);
                }
                SPRINT0(3, " ");
            } else if (itype == ATTRSTRING) {
                SPRINT1(3, "%s", tempClist);
            }
        #endif
    }

    /* for each Face associated with ebody, get its attributes and tolerances */
    #if   defined(GEOM_CAPRI)
        nface = 0;
    #elif defined(GEOM_EGADS)
        status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
        CHECK_STATUS(EG_getBodyTopos);
    #endif

    for (iface = 0; iface < nface; iface++) {
        SPRINT1(3, "    iface  = %d", iface);

        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            status = EG_attributeNum(efaces[iface], &nattr);
            CHECK_STATUS(EG_attributeNum);
        #endif

        for (iattr = 1; iattr <= nattr; iattr++) {
            #if   defined(GEOM_CAPRI)
            #elif defined(GEOM_EGADS)
                status = EG_attributeGet(efaces[iface], iattr, &attrName, &itype, &nlist,
                                         &tempIlist, &tempRlist, &tempClist);
                CHECK_STATUS(EG_attributeGet);

                SPRINT1x(3, "        %-20s =", attrName);

                if        (itype == ATTRINT) {
                    for (i = 0; i < nlist; i++) {
                        SPRINT1x(3, "%5d ", tempIlist[i]);
                    }
                    SPRINT0(3, " ");
                } else if (itype == ATTRREAL) {
                    for (i = 0; i < nlist; i++) {
                        SPRINT1x(3, "%11.5f ", tempRlist[i]);
                    }
                    SPRINT0(3, " ");
                } else if (itype == ATTRSTRING) {
                    SPRINT1(3, "%s", tempClist);
                }
            #endif
        }
    }

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        EG_free(efaces);
    #endif

    /* for each Edge associated with ebody, get its attributes and tolerances */
    #if   defined(GEOM_CAPRI)
        nedge = 0;
    #elif defined(GEOM_EGADS)
        status = EG_getBodyTopos(ebody, NULL, EDGE, &nedge, &eedges);
        CHECK_STATUS(EG_getBodyTopos);
    #endif

    for (iedge = 0; iedge < nedge; iedge++) {
        SPRINT1(3, "    iedge  = %d", iedge);

        #if   defined(GEOM_CAPRI)
        #elif defined(GEOM_EGADS)
            status = EG_attributeNum(eedges[iedge], &nattr);
            CHECK_STATUS(EG_attributeNum);
        #endif

        for (iattr = 1; iattr <= nattr; iattr++) {
            #if   defined(GEOM_CAPRI)
            #elif defined(GEOM_EGADS)
                status = EG_attributeGet(eedges[iedge], iattr, &attrName, &itype, &nlist,
                                         &tempIlist, &tempRlist, &tempClist);
                CHECK_STATUS(EG_attributeGet);

                SPRINT1x(3, "        %-20s =", attrName);

                if        (itype == ATTRINT) {
                    for (i = 0; i < nlist; i++) {
                        SPRINT1x(3, "%5d ", tempIlist[i]);
                    }
                    SPRINT0(3, " ");
                } else if (itype == ATTRREAL) {
                    for (i = 0; i < nlist; i++) {
                        SPRINT1x(3, "%11.5f ", tempRlist[i]);
                    }
                    SPRINT0(3, " ");
                } else if (itype == ATTRSTRING) {
                    SPRINT1(3, "%s", tempClist);
                }
            #endif
        }
    }

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        EG_free(eedges);
    #endif

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   setupAtPmtrs - sets up Parameters starting with @                  *
 *                                                                      *
 ************************************************************************
 */

static int
setupAtPmtrs(modl_T *modl)              /* (in)  pointer to MODL */
{
    int       status = 0;               /* (out) body that satisfies criterion (bias-0) */

    modl_T    *MODL = (modl_T*)modl;

    double    box[6];
    char      string[MAX_EXPR_LEN];

    #if   defined(GEOM_CAPRI)
        int       ivol;
    #elif defined(GEOM_EGADS)
        double    data[14];
        ego       ebody;
    #endif

    ROUTINE(setupAtPmtrs);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    /* get the Parameter indices for each of the variables (if not already set) */
    if (MODL->atPmtrs[ 0] <= 0) {
        status = ocsmNewPmtr(MODL, "@ibody", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 0] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 1] <= 0) {
        status = ocsmNewPmtr(MODL, "@nnode", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 1] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 2] <= 0) {
        status = ocsmNewPmtr(MODL, "@nedge", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 2] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 3] <= 0) {
        status = ocsmNewPmtr(MODL, "@nface", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 3] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 4] <= 0) {
        status = ocsmNewPmtr(MODL, "@xmin", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 4] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 5] <= 0) {
        status = ocsmNewPmtr(MODL, "@ymin", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 5] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 6] <= 0) {
        status = ocsmNewPmtr(MODL, "@zmin", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 6] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 7] <= 0) {
        status = ocsmNewPmtr(MODL, "@xmax", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 7] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 8] <= 0) {
        status = ocsmNewPmtr(MODL, "@ymax", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 8] = MODL->npmtr;
    }

    if (MODL->atPmtrs[ 9] <= 0) {
        status = ocsmNewPmtr(MODL, "@zmax", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[ 9] = MODL->npmtr;
    }

    if (MODL->atPmtrs[10] <= 0) {
        status = ocsmNewPmtr(MODL, "@volume", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[10] = MODL->npmtr;
    }

    if (MODL->atPmtrs[11] <= 0) {
        status = ocsmNewPmtr(MODL, "@area", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[11] = MODL->npmtr;
    }

    if (MODL->atPmtrs[12] <= 0) {
        status = ocsmNewPmtr(MODL, "@xcg", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[12] = MODL->npmtr;
    }

    if (MODL->atPmtrs[13] <= 0) {
        status = ocsmNewPmtr(MODL, "@ycg", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[13] = MODL->npmtr;
    }

    if (MODL->atPmtrs[14] <= 0) {
        status = ocsmNewPmtr(MODL, "@zcg", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[14] = MODL->npmtr;
    }

    if (MODL->atPmtrs[15] <= 0) {
        status = ocsmNewPmtr(MODL, "@Ixx", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[15] = MODL->npmtr;
    }

    if (MODL->atPmtrs[16] <= 0) {
        status = ocsmNewPmtr(MODL, "@Ixy", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[16] = MODL->npmtr;
    }

    if (MODL->atPmtrs[17] <= 0) {
        status = ocsmNewPmtr(MODL, "@Ixz", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[17] = MODL->npmtr;
    }

    if (MODL->atPmtrs[18] <= 0) {
        status = ocsmNewPmtr(MODL, "@Iyx", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[18] = MODL->npmtr;
    }

    if (MODL->atPmtrs[19] <= 0) {
        status = ocsmNewPmtr(MODL, "@Iyy", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[19] = MODL->npmtr;
    }

    if (MODL->atPmtrs[20] <= 0) {
        status = ocsmNewPmtr(MODL, "@Iyz", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[20] = MODL->npmtr;
    }

    if (MODL->atPmtrs[21] <= 0) {
        status = ocsmNewPmtr(MODL, "@Izx", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[21] = MODL->npmtr;
    }

    if (MODL->atPmtrs[22] <= 0) {
        status = ocsmNewPmtr(MODL, "@Izy", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[22] = MODL->npmtr;
    }

    if (MODL->atPmtrs[23] <= 0) {
        status = ocsmNewPmtr(MODL, "@Izz", OCSM_INTERNAL, 1, 1);
        CHECK_STATUS(ocsmNewPmtr);
        MODL->atPmtrs[23] = MODL->npmtr;
    }

    /* set @ibody */
    sprintf(string, "%d", MODL->nbody);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 0], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    /* set @nnode, @nedge, and @nface */
    sprintf(string, "%d", MODL->body[MODL->nbody].nnode);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 1], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%d", MODL->body[MODL->nbody].nedge);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 2], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%d", MODL->body[MODL->nbody].nface);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 3], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    /* set @xmin, @xmax, @ymin, @ymax, @zmin, and @zmax */
    #if   defined(GEOM_CAPRI)
        ivol = MODL->body[MODL->nbody].ivol;

        status = gi_dBox(ivol, box);
        CHECK_STATUS(gi_dBox);
    #elif defined(GEOM_EGADS)
        ebody = MODL->body[MODL->nbody].ebody;

        status = EG_getBoundingBox(ebody, box);
        CHECK_STATUS(EG_attributeNum);
    #endif

    sprintf(string, "%20.13e", box[ 0]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 4], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%20.13e", box[ 1]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 5], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%20.13e", box[ 2]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 6], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%20.13e", box[ 3]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 7], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%20.13e", box[ 4]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 8], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    sprintf(string, "%20.13e", box[ 5]);
    status = ocsmSetValu(MODL, MODL->atPmtrs[ 9], 1, 1, string);
    CHECK_STATUS(ocsmSetValu);

    /* set @volume, @area, @xcg, @ycg, @zcg, @Ixx, ... */
    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        status = EG_getMassProperties(ebody, data);
        CHECK_STATUS(EG_attributeNum);

        sprintf(string, "%20.13e", data[ 0]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[10], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 1]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[11], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 2]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[12], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 3]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[13], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 4]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[14], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 5]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[15], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 6]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[16], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 7]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[17], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 8]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[18], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[ 9]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[19], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[10]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[20], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[11]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[21], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[12]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[22], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);

        sprintf(string, "%20.13e", data[13]);
        status = ocsmSetValu(MODL,MODL->atPmtrs[23], 1, 1, string);
        CHECK_STATUS(ocsmSetValu);
    #endif

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   faceContains - determine if point is in Face's bounding box        *
 *                                                                      *
 ************************************************************************
 */

#if   defined(GEOM_CAPRI)
#elif defined(GEOM_EGADS)
static int
faceContains(ego    eface,              /* (in)  pointer to Face */
             double xx,                 /* (in)  x-coordinate */
             double yy,                 /* (in)  y-coordinate */
             double zz)                 /* (in)  z-coordinate */
{
    int       status = 0;               /* (out) =0  not in bounding box */
                                        /*       =1      in bounding box */

    double    box[6];

    ROUTINE(faceContains);
    DPRINT4("%s(xx=%f, yy=%f, zz=%f) {",
            routine, xx, yy, zz);

    /* --------------------------------------------------------------- */

    status = EG_getBoundingBox(eface, box);
    CHECK_STATUS(EG_getBoundingBox);

    if (xx < box[0] || yy < box[1] || zz < box[2] ||
        xx > box[3] || yy > box[4] || zz > box[5]   ) {
        status = 0;
    } else {
        status = 1;
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}
#endif


/*
 ************************************************************************
 *                                                                      *
 *   selectBody - select a Body from a Model                            *
 *                                                                      *
 ************************************************************************
 */

#if   defined(GEOM_CAPRI)
#elif defined(GEOM_EGADS)
static int
selectBody(ego    emodel,               /* (in)  pointer to Model */
           char*  order,                /* (in)  order type */
           int    index)                /* (in)  index into list (1->nchild) */
{
    int       status = 0;               /* (out) body that satisfies criterion (bias-0) */

    int       i, j, imin, oclass, mtype, nchild, *senses;
    double    box[14], datamin, datamax, *data=NULL;
    ego       eref, *ebodys;

    ROUTINE(selectBody);
    DPRINT3("%s(order=%s, index=%d) {",
            routine, order, index);

    /* --------------------------------------------------------------- */

    status = EG_getTopology(emodel, &eref, &oclass, &mtype,
                            data, &nchild, &ebodys, &senses);
    CHECK_STATUS(EG_getTopology);

    /* if nchild is just one, return the answer immediately */
    if (nchild == 1) {
        status = 0;
        goto cleanup;
    }

    /* make an array that has data based upon order.  note that we
       will always want to return the (index-1)'th smallest entry so that
       quantities that are to maximized are stored as negative value */

    MALLOC(data, double, nchild);

    if        (strcmp(order, "none") == 0) {
        for (i = 0; i < nchild; i++) {
            data[i] = i;
        }
    } else if (strcmp(order, "xmin") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = +box[0];
        }
    } else if (strcmp(order, "xmax") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = -box[3];
        }
    } else if (strcmp(order, "ymin") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = +box[1];
        }
    } else if (strcmp(order, "ymax") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = -box[4];
        }
    } else if (strcmp(order, "zmin") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = +box[2];
        }
    } else if (strcmp(order, "zmax") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getBoundingBox(ebodys[i], box);
            CHECK_STATUS(EG_getBoundingBox);

            data[i] = -box[5];
        }
    } else if (strcmp(order, "amin") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getMassProperties(ebodys[i], box);
            CHECK_STATUS(EG_getMassProperties);

            data[i] = +box[1];
        }
    } else if (strcmp(order, "amax") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getMassProperties(ebodys[i], box);
            CHECK_STATUS(EG_getMassProperties);

            data[i] = -box[1];
        }
    } else if (strcmp(order, "vmin") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getMassProperties(ebodys[i], box);
            CHECK_STATUS(EG_getMassProperties);

            data[i] = +box[0];
        }
    } else if (strcmp(order, "vmax") == 0) {
        for (i = 0; i < nchild; i++) {
            status = EG_getMassProperties(ebodys[i], box);
            CHECK_STATUS(EG_getMassProperties);

            data[i] = -box[0];
        }
    } else {
        status = OCSM_ILLEGAL_TYPE;
        goto cleanup;
    }

    /* find the largest entry in data */
    datamax = data[0];
    for (i = 1; i < nchild; i++) {
        if (data[i] > datamax) datamax = data[i];
    }

    /* move the smallest index-1 entries to the end of the list */
    for (j = 0; j < index-1; j++) {
        datamin = data[0];
        imin    = 0;
        for (i = 1; i < nchild; i++) {
            if (data[i] < datamin) {
                datamin = data[i];
                imin    = i;
            }
        }

        data[imin] = (++datamax);
    }

    /* return the smallest entry in data */
    datamin = data[0];
    imin    = 0;
    for (i = 1; i < nchild; i++) {
        if (data[i] < datamin) {
            datamin = data[i];
            imin    = i;
        }
    }

    status = imin;

cleanup:
    FREE(data);

    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}
#endif


/*
 ************************************************************************
 *                                                                      *
 *   getBodyTolerance - get the largest tolerance associated with       *
 *                      ebody and its Edges/Faces                       *
 *                                                                      *
 ************************************************************************
 */

#if   defined(GEOM_CAPRI)
#elif defined(GEOM_EGADS)
static int
getBodyTolerance(ego    ebody,          /* (in)  pointer to Body */
                 double *toler)         /* (out) largest tolerance */
{
    int       status = 0;               /* (out) body that satisfies criterion (bias-0) */

    int       nface, nedge, iface, iedge, oclass, mtype;
    double    tol;

    ego       *efaces, *eedges, eref, eprev, enext;

    ROUTINE(getBodyTolerance);
    DPRINT1("%s() {",
            routine);

    /* --------------------------------------------------------------- */

    *toler = 0;

    /* if ebody is really a MODEL, return a tolerance of -1 */
    status = EG_getInfo(ebody, &oclass, &mtype, &eref, &eprev, &enext);
    CHECK_STATUS(EG_getInfo);

    if (oclass == MODEL) {
        *toler = -1;
        goto cleanup;
    }

    /* get tolerance associated with the Body */
    status = EG_getTolerance(ebody, &tol);
    CHECK_STATUS(EG_getTolerance);

    SPRINT1(3, "    body         toler=%11.4e", tol);

    *toler = MAX(*toler, tol);

    /* for each Face associated with ebody, get its tolerance */
    status = EG_getBodyTopos(ebody, NULL, FACE, &nface, &efaces);
    CHECK_STATUS(EG_getBodyTopos);

    for (iface = 0; iface < nface; iface++) {
        status = EG_getTolerance(efaces[iface], &tol);
        CHECK_STATUS(EG_getTolerance);

        SPRINT2(3, "    iface=%-5d  toler=%11.4e", iface+1, tol);

        *toler = MAX(*toler, tol);
    }

    EG_free(efaces);

    /* for each Edge associated with ebody, get its tolerance */
    status = EG_getBodyTopos(ebody, NULL, EDGE, &nedge, &eedges);
    CHECK_STATUS(EG_getBodyTopos);

    for (iedge = 0; iedge < nedge; iedge++) {
        status = EG_getTolerance(eedges[iedge], &tol);
        CHECK_STATUS(EG_getTolerance);

        SPRINT2(3, "    iedge=%-5d  toler=%11.4e", iedge+1, tol);

        *toler = MAX(*toler, tol);
    }

    EG_free(eedges);

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}
#endif






/*
 ************************************************************************
 *                                                                      *
 *   matsol - Gaussian elimination with partial pivoting                *
 *                                                                      *
 ************************************************************************
 */

static int
matsol(double    A[],                   /* (in)  matrix to be solved (stored rowwise) */
                                        /* (out) upper-triangular form of matrix */
       double    b[],                   /* (in)  right hand side */
                                        /* (out) right-hand side after swapping */
       int       n,                     /* (in)  size of matrix */
       double    x[])                   /* (out) solution of A*x=b */
{
    int       status = SUCCESS;         /* (out) return status */

    int       ir, jc, kc, imax;
    double    amax, swap, fact;

    ROUTINE(matsol);
    DPRINT2("%s(n=%d) {",
            routine, n);

    /* --------------------------------------------------------------- */

    /* reduce each column of A */
    for (kc = 0; kc < n; kc++) {

        /* find pivot element */
        imax = kc;
        amax = fabs(A[kc*n+kc]);

        for (ir = kc+1; ir < n; ir++) {
            if (fabs(A[ir*n+kc]) > amax) {
                imax = ir;
                amax = fabs(A[ir*n+kc]);
            }
        }

        /* check for possibly-singular matrix (ie, near-zero pivot) */
        if (amax < EPS12) {
            status = OCSM_SINGULAR_MATRIX;
            goto cleanup;
        }

        /* if diagonal is not pivot, swap rows in A and b */
        if (imax != kc) {
            for (jc = 0; jc < n; jc++) {
                swap         = A[kc  *n+jc];
                A[kc  *n+jc] = A[imax*n+jc];
                A[imax*n+jc] = swap;
            }

            swap    = b[kc  ];
            b[kc  ] = b[imax];
            b[imax] = swap;
        }

        /* row-reduce part of matrix to the bottom of and right of [kc,kc] */
        for (ir = kc+1; ir < n; ir++) {
            fact = A[ir*n+kc] / A[kc*n+kc];

            for (jc = kc+1; jc < n; jc++) {
                A[ir*n+jc] -= fact * A[kc*n+jc];
            }

            b[ir] -= fact * b[kc];

            A[ir*n+kc] = 0;
        }
    }

    /* back-substitution pass */
    x[n-1] = b[n-1] / A[(n-1)*n+(n-1)];

    for (jc = n-2; jc >= 0; jc--) {
        x[jc] = b[jc];
        for (kc = jc+1; kc < n; kc++) {
            x[jc] -= A[jc*n+kc] * x[kc];
        }
        x[jc] /= A[jc*n+jc];
    }

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   str2rpn - convert expression to Rpn-code                           *
 *                                                                      *
 ************************************************************************
 */

static int
str2rpn(char      str[],                /* (in)  string containing expression */
        rpn_T     *rpn)                 /* (in)  pointer to Rpn-code */
{
    int       status = SUCCESS;         /* (out) return status */

    typedef struct {
        int       type;                 /* type of token */
        char      text[MAX_NAME_LEN];   /* text associated with token */
    } tok_T;

    tok_T     token[  MAX_EXPR_LEN],
              opstack[MAX_EXPR_LEN];
    int       istr, jstr, ntoken, nrpn, nopstack, nparen, nbrakt;
    int       i, j, count, type;
    char      text[MAX_NAME_LEN];

    ROUTINE(str2rpn);
    DPRINT2("%s(str=%s) {",
            routine, str);

#define ADD_TOKEN(TYPE, TEXT)                   \
    if (ntoken < MAX_EXPR_LEN-1) {              \
        token[ntoken].type = TYPE;              \
        strcpy(token[ntoken].text, TEXT);       \
        ntoken++;                               \
    } else {                                    \
        status = OCSM_TOKEN_STACK_OVERFLOW;     \
        goto cleanup;                           \
    }

#define ADD_TOKEN_CHAR(TYPE, CHAR)              \
    if (ntoken < MAX_EXPR_LEN-1) {              \
        token[ntoken].type = TYPE;              \
        token[ntoken].text[0] = CHAR;           \
        token[ntoken].text[1] = '\0';           \
        ntoken++;                               \
    } else {                                    \
        status = OCSM_TOKEN_STACK_OVERFLOW;     \
        goto cleanup;                           \
    }

#define PUSH_OP(TYPE, TEXT)                     \
    if (nopstack < MAX_EXPR_LEN-1) {            \
        opstack[nopstack].type = TYPE;          \
        strcpy(opstack[nopstack].text, TEXT);   \
        nopstack++;                             \
    } else {                                    \
        status = OCSM_OP_STACK_OVERFLOW;        \
        goto cleanup;                           \
    }

#define POP_OP(TYPE, TEXT)                      \
    if (nopstack > 0) {                         \
        nopstack--;                             \
        TYPE = opstack[nopstack].type;          \
        strcpy(TEXT, opstack[nopstack].text);   \
    } else {                                    \
        status = OCSM_OP_STACK_UNDERFLOW;       \
        goto cleanup;                           \
    }

#define PUSH_RPN(TYPE, TEXT)                    \
    if (nrpn < MAX_EXPR_LEN-1) {                \
        rpn[nrpn].type = TYPE;                  \
        strcpy(rpn[nrpn].text, TEXT);           \
        nrpn++;                                 \
    } else {                                    \
        status = OCSM_RPN_STACK_OVERFLOW;       \
        goto cleanup;                           \
    }

#define POP_RPN(TYPE, TEXT)                     \
    if (nrpn > 0) {                             \
        nrpn--;                                 \
        TYPE = rpn[nrpn].type;                  \
        strcpy(TEXT, rpn[nrpk].text);           \
    } else {                                    \
        status = OCSM_RPN_STACK_UNDERFLOW;      \
        goto cleanup;                           \
    }

    /* --------------------------------------------------------------- */

    /* convert str to tokens
       - surrounded by parentheses
       - remove white space
       - check for valid character
       - convert unary ops to binary ops:
            (+  to  (0+
            (-  to  (0-
            [+  to  [0+
            [-  to  [0-
            ,+  to  ,0+
            ,-  to  ,0-
       - check count and nesting of parentheses
    */
    ntoken = 0;               /* number of tokens */
    nparen = 1;               /* number of unmatched open parens */
    nbrakt = 0;               /* number of unmatched open brackets */

    /* if first character is dollarsign, then str is a string, so return 0  */
    if (str[0] == '$') {
        nrpn = 0;
        PUSH_RPN(PARSE_STRING, " ");
        goto cleanup;
    }

    /* first token is an open paren */
    ADD_TOKEN(PARSE_OPENP, "(");

    /* loop through all characters of the string containing the expression */
    for (istr = 0; istr < strlen(str); istr++) {

        /* open parenthesis */
        if        (str[istr] == '(') {

            /* add the new PARSE_OPENP token */
            ADD_TOKEN(PARSE_OPENP, "(");

            nparen++;

        /* close parenthesis */
        } else if (str[istr] == ')') {

            /* add the new PARSE_CLOSEP token */
            ADD_TOKEN(PARSE_CLOSEP, ")");

            nparen--;

            /* make sure there is a matching open for this close */
            if (nparen < 1) {
                DPRINT0("OCSM_CLOSE_BEFORE_OPEN:");
                status = OCSM_CLOSE_BEFORE_OPEN;
                goto cleanup;
            }

        /* open bracket */
        } else if (str[istr] == '[') {

            /* add the new PARSE_OPENB token */
            ADD_TOKEN(PARSE_OPENB, "[");

            nbrakt++;

        /* close bracket */
        } else if (str[istr] == ']') {

            /* add the new PARSE_CLOSEB token */
            ADD_TOKEN(PARSE_CLOSEB, "]");

            nbrakt--;

            /* make sure there is a matching open for this close */
            if (nbrakt < 0) {
                DPRINT0("OCSM_CLOSE_BEFORE_OPEN:");
                status = OCSM_CLOSE_BEFORE_OPEN;
                goto cleanup;
            }

        /* comma (which separates the arguments of a function) */
        } else if (str[istr] == ',') {

            /* add the new PARSE_COMMA token */
            ADD_TOKEN(PARSE_COMMA, ",");

        /* digit (0-9) or period (which starts a number) */
        } else if (str[istr] == '.' || isdigit(str[istr]) != 0) {

            /* start a new PARSE_NUMBER token */
            ADD_TOKEN_CHAR(PARSE_NUMBER, str[istr]);

            /* add new characters from str as long as we are within a number */
            i = 1;
            for (jstr = istr+1; jstr < strlen(str); jstr++) {
                if (isdigit(str[jstr]) != 0 || str[jstr] == '.') {
                    token[ntoken-1].text[i  ] = str[jstr];
                    token[ntoken-1].text[i+1] = '\0';
                    i++;
                    istr++;

                /* if we find an 'E' or 'e' while making a number, put it
                   and the character after it (which might be a + or - or
                   digit) into the number */
                } else if (str[jstr] == 'E' || str[jstr] == 'e') {
                    token[ntoken-1].text[i  ] = str[jstr  ];
                    token[ntoken-1].text[i+1] = str[jstr+1];
                    token[ntoken-1].text[i+2] = '\0';
                    i++; i++;
                    istr++; istr++;
                    jstr++;   /* advance jstr since we consumed 2 characters */
                } else {
                    break;
                }
            }

        /* letter (a-z, A-Z, and @) (which starts a name or func) */
        } else if (isalpha(str[istr]) != 0 || str[istr] == '@') {

            /* start a new PARSE_NAME token */
            ADD_TOKEN_CHAR(PARSE_NAME, str[istr]);

            /* add new characters from str as long as we are within a name */
            i = 1;
            for (jstr = istr+1; jstr < strlen(str); jstr++) {
                if (isalpha(str[jstr]) !=  0  || isdigit(str[jstr]) !=  0 ||
                            str[jstr]  == '_' ||         str[jstr]  == '@'  ) {
                    token[ntoken-1].text[i  ] = str[jstr];
                    token[ntoken-1].text[i+1] = '\0';
                    i++;
                    istr++;

                /* convert to PARSE_FUNC if followed by open parenthesis */
                } else if (str[jstr] == '(') {
                    token[ntoken-1].type = PARSE_FUNC;
                    break;

                /* convert to PARSE_ARRAY if followed by open bracket */
                } else if (str[jstr] == '[') {
                    token[ntoken-1].type = PARSE_ARRAY;
                    break;

                } else {
                    break;
                }
            }

        /* plus or minus */
        } else if (str[istr] == '+' || str[istr] == '-') {

            /* if previous character is comma or open, convert this
               unary +/- into a binary +/- by adding a "0" before it */
            if (token[ntoken-1].type == PARSE_OPENP ||
                token[ntoken-1].type == PARSE_OPENB ||
                token[ntoken-1].type == PARSE_COMMA   ) {
                ADD_TOKEN(PARSE_NUMBER, "0");
            }

            /* add the new PARSE_OP1 token */
            ADD_TOKEN_CHAR(PARSE_OP1, str[istr]);

        /* asterisk or slash */
        } else if (str[istr] == '*' || str[istr] == '/') {

            /* add the new PARSE_OP2 token */
            ADD_TOKEN_CHAR(PARSE_OP2, str[istr]);

        /* caret */
        } else if (str[istr] == '^') {

            /* add the new PARSE_OP3 token */
            ADD_TOKEN_CHAR(PARSE_OP3, str[istr]);

        /* white space (' ' or \t or \n) */
        } else if (str[istr] == ' ' || str[istr] == '\t' || str[istr] == '\n') {

        /* illegal character */
        } else {
            DPRINT1("ILLEGAL_CHAR: %c", str[istr]);
            status = OCSM_ILLEGAL_CHAR_IN_EXPR;
            goto cleanup;
        }
    }

    /* add a close parenthesis at the end */
    ADD_TOKEN(PARSE_CLOSEP, ")");

    nparen--;

    /* verify that the open and close parentheses are balanced */
    if        (nparen < 0) {
        DPRINT0("OCSM_CLOSE_BEFORE_OPEN:");
        status = OCSM_CLOSE_BEFORE_OPEN;
        goto cleanup;
    } else if (nparen > 0) {
        DPRINT0("OCSM_MISSING_CLOSE");
        status = OCSM_MISSING_CLOSE;
        goto cleanup;
    }

    if        (nbrakt < 0) {
        DPRINT0("OCSM_CLOSE_BEFORE_OPEN:");
        status = OCSM_CLOSE_BEFORE_OPEN;
        goto cleanup;
    } else if (nbrakt > 0) {
        DPRINT0("OCSM_MISSING_CLOSE");
        status = OCSM_MISSING_CLOSE;
        goto cleanup;
    }

    /* check for a proper sequencing of tokens */
    for (i = 0; i < ntoken-1; i++) {
        if         (token[i].type == PARSE_OP1  ||
                    token[i].type == PARSE_OP2  ||
                    token[i].type == PARSE_OP3    ) {
            if (token[i+1].type == PARSE_OP1    ||
                token[i+1].type == PARSE_OP2    ||
                token[i+1].type == PARSE_OP3    ||
                token[i+1].type == PARSE_CLOSEP ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_CLOSEB ||
                token[i+1].type == PARSE_COMMA    ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_OPENP) {
            if (token[i+1].type == PARSE_OP1    ||
                token[i+1].type == PARSE_OP2    ||
                token[i+1].type == PARSE_OP3    ||
                token[i+1].type == PARSE_CLOSEP ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_CLOSEB ||
                token[i+1].type == PARSE_COMMA    ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_CLOSEP) {
            if (token[i+1].type == PARSE_OPENP  ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_NAME   ||
                token[i+1].type == PARSE_ARRAY  ||
                token[i+1].type == PARSE_FUNC   ||
                token[i+1].type == PARSE_NUMBER   ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_OPENB) {
            if (token[i+1].type == PARSE_OP1    ||
                token[i+1].type == PARSE_OP2    ||
                token[i+1].type == PARSE_OP3    ||
                token[i+1].type == PARSE_CLOSEP ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_CLOSEB ||
                token[i+1].type == PARSE_COMMA    ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_CLOSEB) {
            if (token[i+1].type == PARSE_OPENP  ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_NAME   ||
                token[i+1].type == PARSE_ARRAY  ||
                token[i+1].type == PARSE_FUNC   ||
                token[i+1].type == PARSE_NUMBER   ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_COMMA) {
            if (token[i+1].type == PARSE_OP1    ||
                token[i+1].type == PARSE_OP2    ||
                token[i+1].type == PARSE_OP3    ||
                token[i+1].type == PARSE_CLOSEP ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_CLOSEB ||
                token[i+1].type == PARSE_COMMA    ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_NAME) {
            if (token[i+1].type == PARSE_OPENP  ||
                token[i+1].type == PARSE_NAME   ||
                token[i+1].type == PARSE_ARRAY  ||
                token[i+1].type == PARSE_FUNC   ||
                token[i+1].type == PARSE_NUMBER   ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_ARRAY) {
            if (token[i+1].type == PARSE_OPENP  ||
                token[i+1].type == PARSE_NAME   ||
                token[i+1].type == PARSE_ARRAY  ||
                token[i+1].type == PARSE_FUNC   ||
                token[i+1].type == PARSE_NUMBER   ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_NUMBER) {
            if (token[i+1].type == PARSE_OPENP  ||
                token[i+1].type == PARSE_OPENB  ||
                token[i+1].type == PARSE_NAME   ||
                token[i+1].type == PARSE_ARRAY  ||
                token[i+1].type == PARSE_FUNC   ||
                token[i+1].type == PARSE_NUMBER   ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        } else if (token[i].type == PARSE_FUNC    ) {
            if (token[i+1].type != PARSE_OPENP    ) {
                DPRINT2("OCSM_ILLEGAL_TOKEN_SEQUENCE: %s %s", token[i].text, token[i+1].text);
                status = OCSM_ILLEGAL_TOKEN_SEQUENCE;
                goto cleanup;
            }
        }
    }

    /* make sure all function names are known */
    for (i = 0; i < ntoken; i++) {
        if (token[i].type == PARSE_FUNC) {
            if (strcmp(token[i].text, "pi"     ) == 0 ||
                strcmp(token[i].text, "min"    ) == 0 ||
                strcmp(token[i].text, "max"    ) == 0 ||
                strcmp(token[i].text, "sqrt"   ) == 0 ||
                strcmp(token[i].text, "abs"    ) == 0 ||
                strcmp(token[i].text, "int"    ) == 0 ||
                strcmp(token[i].text, "nint"   ) == 0 ||
                strcmp(token[i].text, "exp"    ) == 0 ||
                strcmp(token[i].text, "log"    ) == 0 ||
                strcmp(token[i].text, "log10"  ) == 0 ||
                strcmp(token[i].text, "sin"    ) == 0 ||
                strcmp(token[i].text, "sind"   ) == 0 ||
                strcmp(token[i].text, "asin"   ) == 0 ||
                strcmp(token[i].text, "asind"  ) == 0 ||
                strcmp(token[i].text, "cos"    ) == 0 ||
                strcmp(token[i].text, "cosd"   ) == 0 ||
                strcmp(token[i].text, "acos"   ) == 0 ||
                strcmp(token[i].text, "acosd"  ) == 0 ||
                strcmp(token[i].text, "tan"    ) == 0 ||
                strcmp(token[i].text, "tand"   ) == 0 ||
                strcmp(token[i].text, "atan"   ) == 0 ||
                strcmp(token[i].text, "atand"  ) == 0 ||
                strcmp(token[i].text, "atan2"  ) == 0 ||
                strcmp(token[i].text, "atan2d" ) == 0 ||
                strcmp(token[i].text, "hypot"  ) == 0 ||
                strcmp(token[i].text, "Xcent"  ) == 0 ||
                strcmp(token[i].text, "Ycent"  ) == 0 ||
                strcmp(token[i].text, "Xmidl"  ) == 0 ||
                strcmp(token[i].text, "Ymidl"  ) == 0 ||
                strcmp(token[i].text, "turnang") == 0 ||
                strcmp(token[i].text, "tangent") == 0 ||
                strcmp(token[i].text, "ifzero" ) == 0 ||
                strcmp(token[i].text, "ifpos"  ) == 0 ||
                strcmp(token[i].text, "ifneg"  ) == 0   ) {
            } else {
                DPRINT1("OCSM_ILLEGAL_FUNC_NAME: %s", token[i].text);
                status = OCSM_ILLEGAL_FUNC_NAME;
                goto cleanup;
            }
        }
    }

    /* make sure all numbers are properly formed */
    for (i = 0; i < ntoken; i++) {
        if (token[i].type == PARSE_NUMBER) {
            count = 0;
            for (j = 0; j < strlen(token[i].text); j++) {
                if (token[i].text[j] == '.') count++;
            }

            if (count > 1) {
                DPRINT1("OCSM_ILLEGAL_NUMBER: %s", token[i].text);
                status = OCSM_ILLEGAL_NUMBER;
                goto cleanup;
            }
        }
    }

    /* print the tokens */
    DPRINT0("token list");
    for (i = 0; i < ntoken; i++) {
        DPRINT3("    %3d: type=%2d, text=%s",
                i, token[i].type, token[i].text);
    }

    /* start with empty Rpn-code and op-stacks */
    nrpn     = 0;
    nopstack = 0;

    /* create the Rpn-code stack by cycling through the tokens */
    for (i = 0; i < ntoken; i++) {

        /* PARSE_NAME and PARSE_NUMBER */
        if (token[i].type == PARSE_NAME  ||
            token[i].type == PARSE_NUMBER  ) {
            PUSH_RPN(token[i].type, token[i].text);

        /* PARSE_OP1 */
        } else if (token[i].type == PARSE_OP1) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if (type == PARSE_OPENP || type == PARSE_FUNC ||
                                           type == PARSE_ARRAY  ) {
                    PUSH_OP(type, text);
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_OP2 */
        } else if (token[i].type == PARSE_OP2) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if (type == PARSE_OPENP || type == PARSE_FUNC ||
                    type == PARSE_ARRAY || type == PARSE_OP1    ) {
                    PUSH_OP(type, text);
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_OP3 */
        } else if (token[i].type == PARSE_OP3) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if (type == PARSE_OPENP || type == PARSE_FUNC ||
                    type == PARSE_ARRAY || type == PARSE_OP1  ||
                                           type == PARSE_OP2    ) {
                    PUSH_OP(type, text);
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_OPENP */
        } else if (token[i].type == PARSE_OPENP) {
            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_CLOSEP */
        } else if (token[i].type == PARSE_CLOSEP) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if  (type == PARSE_OPENP) {
                    if (nopstack <= 0) {

                    } else if (opstack[nopstack-1].type == PARSE_FUNC) {
                        POP_OP(type, text);
                        PUSH_RPN(type, text);
                    }
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

        /* PARSE_OPENB */
        } else if (token[i].type == PARSE_OPENB) {
            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_CLOSEB */
        } else if (token[i].type == PARSE_CLOSEB) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if (type == PARSE_OPENB) {
                    if (opstack[nopstack-1].type == PARSE_ARRAY) {
                        POP_OP(type, text);
                        PUSH_RPN(type, text);
                    }
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

        /* PARSE_COMMA */
        } else if (token[i].type == PARSE_COMMA) {
            while (nopstack > 0) {
                POP_OP(type, text);
                if (type == PARSE_OPENP || type == PARSE_FUNC ||
                                           type == PARSE_ARRAY  ) {
                    PUSH_OP(type, text);
                    break;
                } else {
                    PUSH_RPN(type, text);
                }
            }

            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_FUNC */
        } else if (token[i].type == PARSE_FUNC) {
            PUSH_OP(token[i].type, token[i].text);

        /* PARSE_ARRAY */
        } else if (token[i].type == PARSE_ARRAY) {
            PUSH_OP(token[i].type, token[i].text);

        }
    }

    /* add everything left on the op-stack to the Rpn-code */
    while (nopstack > 0) {
        POP_OP(type, text);

        PUSH_RPN(type, text);
    }

    /* add a PARSE_END to the end of the Rpn-code */
    PUSH_RPN(PARSE_END, "");

    /* print the Rpn-code */
    DPRINT0("rpn-code list");
    for (i = 0; i < nrpn; i++) {
        DPRINT3("    %3d: type=%2d, text=%s",
                i, rpn[i].type, rpn[i].text);
    }

#undef PUSH_OP
#undef POP_OP

cleanup:
    DPRINT2("%s --> status=%d}", routine, status);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   evalRpn - evaluate Rpn-code                                        *
 *                                                                      *
 ************************************************************************
 */

static int
evalRpn(rpn_T     *rpn,                 /* (in)  pointer to Rpn-code */
        modl_T    *modl,                /* (in)  pointer to MODL */
        double    *val)                 /* (out) value of expression */
{
    int       status = SUCCESS;         /* (out) return status */

    int       irpn, nvalstack, ival, ipmtr, jpmtr, type, nrow, ncol, irow, icol;
    double    valstack[MAX_EXPR_LEN], val1, val2, val3;
    double    xa, ya, xb, yb, xc, yc, Cab, Cbc, R, d, L, angab, angbc, diffang;
    double    rad2deg = 180 / PI;
    char      pmtrName[MAX_NAME_LEN];

    ROUTINE(evalRpn);
    DPRINT3("%s(rpn=%x. modl=%x) {",
            routine, (int)rpn, (int)modl);

#define PUSH_VAL(VAL)                           \
    if (nvalstack < MAX_EXPR_LEN-1) {           \
        valstack[nvalstack] = VAL;              \
        nvalstack++;                            \
    } else {                                    \
        status = OCSM_VAL_STACK_OVERFLOW;       \
        goto cleanup;                           \
    }

#define POP_VAL(VAL)                            \
    if (nvalstack > 0) {                        \
        nvalstack--;                            \
        VAL = valstack[nvalstack];              \
    } else {                                    \
        status = OCSM_VAL_STACK_UNDERFLOW;      \
        goto cleanup;                           \
    }

    /* --------------------------------------------------------------- */

    /* default answer (in case there is an error) */
    *val = 0;

    /* if we begin with PARSE_STRING, simply return default value */
    if (rpn[0].type == PARSE_STRING) {
        goto cleanup;
    }

    /* start with an empty value stack */
    nvalstack = 0;

    /* loop through whole Rpn-code */
    irpn = 0;
    while (rpn[irpn].type != PARSE_END) {

        /* PARSE_NUM */
        if (rpn[irpn].type == PARSE_NUMBER) {
            PUSH_VAL(strtod(rpn[irpn].text, (char**)NULL));

        /* PARSE_NAME */
        } else if (rpn[irpn].type == PARSE_NAME) {
            jpmtr = 0;
            if (modl != NULL) {
                for (ipmtr = 1; ipmtr <= modl->npmtr; ipmtr++) {
                    status = ocsmGetPmtr(modl, ipmtr, &type, &nrow, &ncol, pmtrName);
                    CHECK_STATUS(ocsmGetPmtr);

                    if (strcmp(rpn[irpn].text, pmtrName) == 0) {
                        jpmtr = ipmtr;

                        irow = icol = 1;

                        status = ocsmGetValu(modl, jpmtr, irow, icol, &val1);
                        CHECK_STATUS(ocsmGetValu);

                        PUSH_VAL(val1);
                        break;
                    }
                }
            }
            if (jpmtr == 0) {
                status = OCSM_NAME_NOT_FOUND;
                goto cleanup;
            }

        /* PARSE_ARRAY */
        } else if (rpn[irpn].type == PARSE_ARRAY) {
            jpmtr = 0;
            if (modl != NULL) {
                for (ipmtr = 1; ipmtr <= modl->npmtr; ipmtr++) {
                    status = ocsmGetPmtr(modl, ipmtr, &type, &nrow, &ncol, pmtrName);
                    CHECK_STATUS(ocsmGetPmtr);

                    if (strcmp(rpn[irpn].text, pmtrName) == 0) {
                        jpmtr = ipmtr;

                        POP_VAL(val1);
                        POP_VAL(val2);

                        irow = NINT(val2);
                        icol = NINT(val1);

                        status = ocsmGetValu(modl, jpmtr, irow, icol, &val1);
                        CHECK_STATUS(ocsmGetValu);

                        PUSH_VAL(val1);
                        break;
                    }
                }
            }
            if (jpmtr == 0) {
                status = OCSM_NAME_NOT_FOUND;
                goto cleanup;
            }

        /* PARSE_OP1 or PARSE_OP2 or PARSE_OP3 */
        } else if (rpn[irpn].type == PARSE_OP1 ||
                   rpn[irpn].type == PARSE_OP2 ||
                   rpn[irpn].type == PARSE_OP3   ) {
            if        (strcmp(rpn[irpn].text, "+") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                PUSH_VAL(val2 + val1);
            } else if (strcmp(rpn[irpn].text, "-") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                PUSH_VAL(val2 - val1);
            } else if (strcmp(rpn[irpn].text, "*") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                PUSH_VAL(val2 * val1);
            } else if (strcmp(rpn[irpn].text, "/") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                if (val1 == 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(val2 / val1);
            } else if (strcmp(rpn[irpn].text, "^") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);
                ival = (int)val1;

                if (val1 == (double)ival){
                    PUSH_VAL(pow(val2, ival));
                } else if (val2 < 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                } else {
                    PUSH_VAL(pow(val2, val1));
                }
            }

        /* PARSE_FUNC */
        } else if (rpn[irpn].type == PARSE_FUNC) {
            if        (strcmp(rpn[irpn].text, "pi") == 0) {
                POP_VAL(val1);

                PUSH_VAL(val1*PI);
            } else if (strcmp(rpn[irpn].text, "min") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                if (val1 < val2) {
                    PUSH_VAL(val1);
                } else {
                    PUSH_VAL(val2);
                }
            } else if (strcmp(rpn[irpn].text, "max") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                if (val1 > val2) {
                    PUSH_VAL(val1);
                } else {
                    PUSH_VAL(val2);
                }
            } else if (strcmp(rpn[irpn].text, "sqrt") == 0) {
                POP_VAL(val1);

                if (val1 < 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(sqrt(val1));
            } else if (strcmp(rpn[irpn].text, "abs") == 0) {
                POP_VAL(val1);

                PUSH_VAL(fabs(val1));
            } else if (strcmp(rpn[irpn].text, "int") == 0) {
                POP_VAL(val1);

                ival = (int) val1;
                PUSH_VAL((double) ival);
            } else if (strcmp(rpn[irpn].text, "nint") == 0) {
                POP_VAL(val1);

                ival = (int) (val1 + 0.50);
                PUSH_VAL((double) ival);
            } else if (strcmp(rpn[irpn].text, "exp") == 0) {
                POP_VAL(val1);

                PUSH_VAL(exp(val1));
            } else if (strcmp(rpn[irpn].text, "log") == 0) {
                POP_VAL(val1);

                if (val1 < 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(log(val1));
            } else if (strcmp(rpn[irpn].text, "log10") == 0) {
                POP_VAL(val1);

                if (val1 < 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(log10(val1));
            } else if (strcmp(rpn[irpn].text, "sin") == 0) {
                POP_VAL(val1);

                PUSH_VAL(sin(val1));
            } else if (strcmp(rpn[irpn].text, "sind") == 0) {
                POP_VAL(val1);

                PUSH_VAL(sin(val1 / rad2deg));
            } else if (strcmp(rpn[irpn].text, "asin") == 0) {
                POP_VAL(val1);

                if (val1 < -1.0 || val1 > 1.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(asin(val1));
            } else if (strcmp(rpn[irpn].text, "asind") == 0) {
                POP_VAL(val1);

                if (val1 < -1.0 || val1 > 1.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(asin(val1) * rad2deg);
            } else if (strcmp(rpn[irpn].text, "cos") == 0) {
                POP_VAL(val1);

                PUSH_VAL(cos(val1));
            } else if (strcmp(rpn[irpn].text, "cosd") == 0) {
                POP_VAL(val1);

                PUSH_VAL(cos(val1 / rad2deg));
            } else if (strcmp(rpn[irpn].text, "acos") == 0) {
                POP_VAL(val1);

                if (val1 < -1.0 || val1 > 1.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(acos(val1));
            } else if (strcmp(rpn[irpn].text, "acosd") == 0) {
                POP_VAL(val1);

                if (val1 < -1.0 || val1 > 1.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(acos(val1) * rad2deg);
            } else if (strcmp(rpn[irpn].text, "tan") == 0) {
                POP_VAL(val1);

                PUSH_VAL(tan(val1));
            } else if (strcmp(rpn[irpn].text, "tand") == 0) {
                POP_VAL(val1);

                PUSH_VAL(tan(val1 / rad2deg));
            } else if (strcmp(rpn[irpn].text, "atan") == 0) {
                POP_VAL(val1);

                PUSH_VAL(atan(val1));
            } else if (strcmp(rpn[irpn].text, "atand") == 0) {
                POP_VAL(val1);

                PUSH_VAL(atan(val1) * rad2deg);
            } else if (strcmp(rpn[irpn].text, "atan2") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                if (val1 == 0.0 && val2 == 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(atan2(val2, val1));
            } else if (strcmp(rpn[irpn].text, "atan2d") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                if (val1 == 0.0 && val2 == 0.0) {
                    status = OCSM_FUNC_ARG_OUT_OF_BOUNDS;
                    goto cleanup;
                }

                PUSH_VAL(atan2(val2, val1) * rad2deg);
            } else if (strcmp(rpn[irpn].text, "hypot") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);

                PUSH_VAL(sqrt(val1*val1 + val2*val2));
            } else if (strcmp(rpn[irpn].text, "Xcent") == 0) {
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                if (fabs(Cab) < EPS06) {
                    PUSH_VAL((xa+xb)/2);
                } else {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        PUSH_VAL((xa+xb)/2 - (yb-ya)*L/d);
                    } else {
                        PUSH_VAL((xa+xb)/2 + (yb-ya)*L/d);
                    }
                }
            } else if (strcmp(rpn[irpn].text, "Ycent") == 0) {
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                if (fabs(Cab) < EPS06) {
                    PUSH_VAL((ya+yb)/2);
                } else {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        PUSH_VAL((ya+yb)/2 + (xb-xa)*L/d);
                    } else {
                        PUSH_VAL((ya+yb)/2 - (xb-xa)*L/d);
                    }
                }
            } else if (strcmp(rpn[irpn].text, "Xmidl") == 0) {
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                if (fabs(Cab) < EPS06) {
                    PUSH_VAL((xa+xb)/2);
                } else {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        PUSH_VAL((xa+xb)/2 + (yb-ya)*(R-L)/d);
                    } else {
                        PUSH_VAL((xa+xb)/2 - (yb-ya)*(R-L)/d);
                    }
                }
            } else if (strcmp(rpn[irpn].text, "Ymidl") == 0) {
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                if (fabs(Cab) < EPS06) {
                    PUSH_VAL((ya+yb)/2);
                } else {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        PUSH_VAL((ya+yb)/2 - (xb-xa)*(R-L)/d);
                    } else {
                        PUSH_VAL((ya+yb)/2 + (xb-xa)*(R-L)/d);
                    }
                }
            } else if (strcmp(rpn[irpn].text, "turnang") == 0) {
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                if (fabs(Cab) < EPS06) {
                    PUSH_VAL(0);
                } else {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        PUSH_VAL(+2 * acos(L/R) * rad2deg);
                    } else {
                        PUSH_VAL(-2 * acos(L/R) * rad2deg);
                    }
                }
            } else if (strcmp(rpn[irpn].text, "tangent") == 0) {
                POP_VAL(yc);
                POP_VAL(xc);
                POP_VAL(Cbc);
                POP_VAL(yb);
                POP_VAL(xb);
                POP_VAL(Cab);
                POP_VAL(ya);
                POP_VAL(xa);

                angab = atan2(yb-ya, xb-xa);
                if (fabs(Cab) > EPS06) {
                    d = sqrt((xb-xa)*(xb-xa) + (yb-ya)*(yb-ya));
                    R = MAX(1/fabs(Cab), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cab > 0) {
                        angab += acos(L/R);
                    } else {
                        angab -= acos(L/R);
                    }
                }

                angbc = atan2(yc-yb, xc-xb);
                if (fabs(Cbc) > EPS06) {
                    d = sqrt((xc-xb)*(xc-xb) + (yc-yb)*(yc-yb));
                    R = MAX(1/fabs(Cbc), d/2);
                    L = sqrt(R*R - d*d/4);

                    if (Cbc > 0) {
                        angbc -= acos(L/R);
                    } else {
                        angbc += acos(L/R);
                    }
                }

                diffang = angbc - angab;
                while (diffang > +PI) {
                    diffang -= TWOPI;
                }
                while (diffang < -PI) {
                    diffang += TWOPI;
                }

                PUSH_VAL(diffang * rad2deg);
            } else if (strcmp(rpn[irpn].text, "ifzero") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);
                POP_VAL(val3);

                if (val3 == 0) {
                    PUSH_VAL(val2);
                } else {
                    PUSH_VAL(val1);
                }
            } else if (strcmp(rpn[irpn].text, "ifpos") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);
                POP_VAL(val3);

                if (val3 > 0) {
                    PUSH_VAL(val2);
                } else {
                    PUSH_VAL(val1);
                }
            } else if (strcmp(rpn[irpn].text, "ifneg") == 0) {
                POP_VAL(val1);
                POP_VAL(val2);
                POP_VAL(val3);

                if (val3 < 0) {
                    PUSH_VAL(val2);
                } else {
                    PUSH_VAL(val1);
                }
            }
        }

        irpn++;
    }

    /* if there is only one entry on the valstack, return it */
    POP_VAL(*val);

    if (nvalstack > 0) {
        status = OCSM_VAL_STACK_OVERFLOW;
    }

#undef PUSH_VAL
#undef POP_VAL

cleanup:
    DPRINT3("%s --> status=%d, val=%f}", routine, status, *val);
    return status;
}


/*
 ************************************************************************
 *                                                                      *
 *   str2val - convert and evaluate expression (in string)              *
 *                                                                      *
 ************************************************************************
 */

static int
str2val(char      str[],                /* (in)  string containing expression */
        modl_T    *modl,                /* (in)  pointer to MODL */
        double    *val)                 /* (out) value of expression */
{
    int       status = SUCCESS;         /* (out) return status */

    rpn_T     rpn[1024];                /* Rpn-code */

    ROUTINE(str2val);
    DPRINT3("%s(str=%s, modl=%x) {",
            routine, str, (int)modl);

    /* --------------------------------------------------------------- */

    /* convert the expression to Rpn-code */
    status = str2rpn(str, rpn);
    CHECK_STATUS(str2rpn);

    /* evaluate the Rpn-code */
    status = evalRpn(rpn, modl, val);
    CHECK_STATUS(evalRpn);

cleanup:
    DPRINT3("%s --> status=%d, val=%f}", routine, status, *val);
    return status;
}
