/*
 * GEM Diamond -- Extended User Defined Primitive Interface
 *
 */

#ifdef GEOM_EGADS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "egads.h"

#ifdef WIN32
#define DLL HINSTANCE
#else
#define DLL void *
#endif

#define MAXPRIM 32


typedef int (*DLLFunc) (void);
typedef int (*udpI)    (int *, char ***, int **, int **, double **);
typedef int (*udpR)    (int);
typedef int (*udpS)    (char *, char *);
typedef int (*udpE)    (ego, ego *, int *, char **);
typedef int (*udpM)    (ego, int, int *, int *, int *, double **);
typedef int (*udpP)    (ego, char *, int, int *, double *, double *);
typedef int (*udpZ)    (ego, char *, double *);

static char *udpName[MAXPRIM];
static DLL   udpDLL[MAXPRIM];
static udpI  udpInit[MAXPRIM];
static udpR  udpReset[MAXPRIM];
static udpS  udpSet[MAXPRIM];
static udpE  udpExec[MAXPRIM];
static udpM  udpMesh[MAXPRIM];
static udpP  udpSens[MAXPRIM];
static udpZ  udpStSz[MAXPRIM];

static int udp_nPrim = 0;


/* ************************* Utility Functions ***************************** */

static /*@null@*/ DLL udpDLopen(const char *name)
{
    int  i, len;
    DLL  dll;
    char *full;

    if (name == NULL) {
        printf(" Information: Dynamic Loader invoked with NULL name!\n");
        return NULL;
    }
    len  = strlen(name);
    full = (char *) malloc((len+5)*sizeof(char));
    if (full == NULL) {
        printf(" Information: Dynamic Loader MALLOC Error!\n");
        return NULL;
    }
    for (i = 0; i < len; i++) full[i] = name[i];
    full[len  ] = '.';
#ifdef WIN32
    full[len+1] = 'D';
    full[len+2] = 'L';
    full[len+3] = 'L';
    full[len+4] =  0;
    dll = LoadLibrary(full);
#else
    full[len+1] = 's';
    full[len+2] = 'o';
    full[len+3] =  0;
    dll = dlopen(full, RTLD_NOW /* RTLD_LAZY */);
#endif

    if (!dll) {
        printf(" Information: Dynamic Loader Error for %s\n", full);
#ifndef WIN32
        printf("              %s\n", dlerror());
#endif
        free(full);
        return NULL;
    }

    free(full);
    return dll;
}


static void udpDLclose(/*@only@*/ DLL dll)
{
#ifdef WIN32
    FreeLibrary(dll);
#else
    dlclose(dll);
#endif
}


static DLLFunc udpDLget(DLL dll, const char *symname, const char *name)
{
    DLLFunc data;

#ifdef WIN32
    data = (DLLFunc) GetProcAddress(dll, symname);
#else
    data = (DLLFunc) dlsym(dll, symname);
#endif
    if (!data)
        printf("               Couldn't get symbol %s in %s\n", symname, name);
    return data;
}


static int udpDLoaded(const char *name)
{
    int i;

    for (i = 0; i < udp_nPrim; i++)
        if (strcmp(name, udpName[i]) == 0) return i;

    return -1;
}


static int udpDYNload(const char *name)
{
    int i, len, ret;
    DLL dll;

    if (udp_nPrim >= MAXPRIM) {
        printf(" Information: Number of Primitives > %d!\n", MAXPRIM);
        return EGADS_INDEXERR;
    }
    dll = udpDLopen(name);
    if (dll == NULL) return EGADS_NULLOBJ;

    ret = udp_nPrim;
    udpInit[ret]  = (udpI) udpDLget(dll, "udpInitialize",  name);
    udpReset[ret] = (udpR) udpDLget(dll, "udpReset",       name);
    udpSet[ret]   = (udpS) udpDLget(dll, "udpSet",         name);
    udpExec[ret]  = (udpE) udpDLget(dll, "udpExecute",     name);
    udpMesh[ret]  = (udpM) udpDLget(dll, "udpMesh",        name);
    udpSens[ret]  = (udpP) udpDLget(dll, "udpSensitivity", name);
    udpStSz[ret]  = (udpZ) udpDLget(dll, "udpStepSize",    name);
    if ((udpInit[ret] == NULL) || (udpReset[ret] == NULL) ||
        (udpSet[ret]  == NULL) || (udpExec[ret]  == NULL) ||
        (udpMesh[ret] == NULL) || (udpSens[ret]  == NULL) ||
        (udpStSz[ret] == NULL)) {
        udpDLclose(dll);
        return EGADS_EMPTY;
    }

    len  = strlen(name) + 1;
    udpName[ret] = (char *) malloc(len*sizeof(char));
    if (udpName[ret] == NULL) {
        udpDLclose(dll);
        return EGADS_MALLOC;
    }
    for (i = 0; i < len; i++) udpName[ret][i] = name[i];
    udpDLL[ret] = dll;
    udp_nPrim++;

    return ret;
}


/* ************************* Exposed Functions ***************************** */


int udp_initialize(const char *primName,
                   int        *nArgs,
                   char       ***name,
                   int        **type,
                   int        **idefault,
                   double     **ddefault)
{
    int i;

    if (udpDLoaded(primName) != -1) return EGADS_NOLOAD;

    i = udpDYNload(primName);
    if (i < 0) return i;

    return udpInit[i](nArgs, name, type, idefault, ddefault);
}


int udp_clrArguments(const char *primName)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpReset[i](0);
}


int udp_setArgument(const char *primName,
                    char       *name,
                    char       *value)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpSet[i](name, value);
}


int udp_executePrim(const char *primName,
                    ego        context,
                    ego        *body,
                    int        *nMesh,
                    char       **string)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpExec[i](context, body, nMesh, string);
}


int udp_getMesh(const char *primName,
                ego        body,
                int        imesh,
                int        *imax,
                int        *jmax,
                int        *kmax,
                double     **mesh)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpMesh[i](body, imesh, imax, jmax, kmax, mesh);
}


int udp_sensitivity(const char *primName,
                    ego        body,
                    char       *vname,
                    int        npts,
                    int        *fIndices,
                    double     *uvs,
                    double     *dxdname)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpSens[i](body, vname, npts, fIndices, uvs, dxdname);
}


int udp_stepSize(const char *primName,
                 ego        body,
                 char       *vname,
                 double     *delta)
{
    int i;

    i = udpDLoaded(primName);
    if (i == -1) return EGADS_NOTFOUND;

    return udpStSz[i](body, vname, delta);
}


void udp_cleanupAll()
{
    int i;

    if (udp_nPrim == 0) return;

    for (i = 0; i < udp_nPrim; i++) {
        udpReset[i](1);
        free(udpName[i]);
        udpDLclose(udpDLL[i]);
    }

    udp_nPrim = 0;
}

#endif
