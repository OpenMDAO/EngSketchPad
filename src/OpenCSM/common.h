/*
 ************************************************************************
 *                                                                      *
 * common.h -- common header file                                       *
 *                                                                      *
 *              Written by John Dannenhoffer @ Syracuse University      *
 *                                                                      *
 ************************************************************************
 */

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

#ifndef _COMMON_H_
#define _COMMON_H_

/* macros for error checking */
#define CHECK_STATUS(X)                                                 \
    if (status < SUCCESS) {                                             \
        printf( "ERROR:: BAD STATUS = %d from %s (called from %s:%d)\n", status, #X, routine, __LINE__); \
        goto cleanup;                                                   \
    }
#define MALLOC(PTR,TYPE,SIZE)                                           \
    DPRINT3("mallocing %s in routine %s (size=%d)", #PTR, routine, SIZE); \
    PTR = (TYPE *) malloc((SIZE) * sizeof(TYPE));                       \
    if (PTR == NULL) {                                                  \
        printf("ERROR:: MALLOC PROBLEM for %s (called from %s:%d)\n", #PTR, routine, __LINE__); \
        status = BAD_MALLOC;                                            \
        goto cleanup;                                                   \
    }
#define RALLOC(PTR,TYPE,SIZE)                                           \
    DPRINT3("rallocing %s in routine %s (size=%d)", #PTR, routine, SIZE); \
    realloc_temp = realloc(PTR, (SIZE) * sizeof(TYPE));                 \
    if (PTR == NULL) {                                                  \
        printf("ERROR:: RALLOC PROBLEM for %s (called from %s:%d)\n", #PTR, routine, __LINE__); \
        status = BAD_MALLOC;                                            \
        goto cleanup;                                                   \
    } else {                                                            \
        PTR = (TYPE *)realloc_temp;                                     \
    }
#define FREE(PTR) \
    if (PTR != NULL) {                                          \
        DPRINT2("freeing %s in routine %s", #PTR, routine);     \
    }                                                           \
    free(PTR);                                                  \
    PTR = NULL;

/* macros for debug printing */
#ifdef   DEBUG
   #define DFLUSH \
           {fprintf(dbg_fp, "\n"); fflush(dbg_fp);}
   #define DPRINT0(FORMAT) \
           {DOPEN; fprintf(dbg_fp, FORMAT); DFLUSH;}
   #define DPRINT0x(FORMAT) \
           {DOPEN; fprintf(dbg_fp, FORMAT);}
   #define DPRINT1(FORMAT,A) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A); DFLUSH;}
   #define DPRINT1x(FORMAT,A) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A);}
   #define DPRINT2(FORMAT,A,B) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B); DFLUSH;}
   #define DPRINT2x(FORMAT,A,B) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B);}
   #define DPRINT3(FORMAT,A,B,C) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C); DFLUSH;}
   #define DPRINT4(FORMAT,A,B,C,D) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D); DFLUSH;}
   #define DPRINT5(FORMAT,A,B,C,D,E) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E); DFLUSH;}
   #define DPRINT6(FORMAT,A,B,C,D,E,F) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F); DFLUSH;}
   #define DPRINT7(FORMAT,A,B,C,D,E,F,G) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G); DFLUSH;}
   #define DPRINT7x(FORMAT,A,B,C,D,E,F,G) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G);}
   #define DPRINT8(FORMAT,A,B,C,D,E,F,G,H) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H); DFLUSH;}
   #define DPRINT9(FORMAT,A,B,C,D,E,F,G,H,I) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I); DFLUSH;}
   #define DPRINT10(FORMAT,A,B,C,D,E,F,G,H,I,J) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J); DFLUSH;}
   #define DPRINT11(FORMAT,A,B,C,D,E,F,G,H,I,J,K) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J, K); DFLUSH;}
   #define DPRINT12(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J, K, L); DFLUSH;}
   #define DPRINT13(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J, K, L, M); DFLUSH;}
   #define DPRINT14(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M,N) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J, K, L, M, N); DFLUSH;}
   #define DPRINT15(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O) \
           {DOPEN; fprintf(dbg_fp, FORMAT, A, B, C, D, E, F, G, H, I, J, K, L, M, N, O); DFLUSH;}
   #define DCLOSE \
           {if (dbg_fp != NULL) fclose(dbg_fp); dbg_fp = NULL;}
#else
   #define DPRINT0(FORMAT)
   #define DPRINT0x(FORMAT)
   #define DPRINT1(FORMAT,A)
   #define DPRINT1x(FORMAT,A)
   #define DPRINT2(FORMAT,A,B)
   #define DPRINT2x(FORMAT,A,B)
   #define DPRINT3(FORMAT,A,B,C)
   #define DPRINT4(FORMAT,A,B,C,D)
   #define DPRINT5(FORMAT,A,B,C,D,E)
   #define DPRINT6(FORMAT,A,B,C,D,E,F)
   #define DPRINT7(FORMAT,A,B,C,D,E,F,G)
   #define DPRINT7x(FORMAT,A,B,C,D,E,F,G)
   #define DPRINT8(FORMAT,A,B,C,D,E,F,G,H)
   #define DPRINT9(FORMAT,A,B,C,D,E,F,G,H,I)
   #define DPRINT10(FORMAT,A,B,C,D,E,F,G,H,I,J)
   #define DPRINT11(FORMAT,A,B,C,D,E,F,G,H,I,J,K)
   #define DPRINT12(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L)
   #define DPRINT13(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M)
   #define DPRINT14(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M,N)
   #define DPRINT15(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L,M,N,O)
   #define DCLOSE
#endif

#define ROUTINE(NAME) char routine[] = #NAME ;\
    if (routine[0] == '\0') printf("bad routine(%s)\n", routine);

/* macros for status printing */
#define SPRINT0(OL,FORMAT)                                   \
    DPRINT0(FORMAT);                                         \
    if (outLevel >= OL) {                                    \
        printf(FORMAT); printf("\n");                        \
    }
#define SPRINT0x(OL,FORMAT)                                  \
    DPRINT0x(FORMAT);                                        \
    if (outLevel >= OL) {                                    \
        printf(FORMAT);                                      \
    }
#define SPRINT1(OL,FORMAT,A)                                 \
    DPRINT1(FORMAT,A);                                       \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A); printf("\n");                      \
    }
#define SPRINT1x(OL,FORMAT,A)                                \
    DPRINT1x(FORMAT,A);                                      \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A);                                    \
    }
#define SPRINT2(OL,FORMAT,A,B)                               \
    DPRINT2(FORMAT,A,B);                                     \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B); printf("\n");                    \
    }
#define SPRINT3(OL,FORMAT,A,B,C)                             \
    DPRINT3(FORMAT,A,B,C);                                   \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C); printf("\n");                  \
    }
#define SPRINT4(OL,FORMAT,A,B,C,D)                           \
    DPRINT4(FORMAT,A,B,C,D);                                 \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D); printf("\n");                \
    }
#define SPRINT5(OL,FORMAT,A,B,C,D,E)                         \
    DPRINT5(FORMAT,A,B,C,D,E);                               \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E); printf("\n");              \
    }
#define SPRINT6(OL,FORMAT,A,B,C,D,E,F)                       \
    DPRINT6(FORMAT,A,B,C,D,E,F);                             \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F); printf("\n");            \
    }
#define SPRINT7(OL,FORMAT,A,B,C,D,E,F,G)                     \
    DPRINT7(FORMAT,A,B,C,D,E,F,G);                           \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G); printf("\n");          \
    }
#define SPRINT8(OL,FORMAT,A,B,C,D,E,F,G,H)                   \
    DPRINT8(FORMAT,A,B,C,D,E,F,G,H);                         \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G,H); printf("\n");        \
    }
#define SPRINT9(OL,FORMAT,A,B,C,D,E,F,G,H,I)                 \
    DPRINT9(FORMAT,A,B,C,D,E,F,G,H,I);                       \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G,H,I); printf("\n");      \
    }
#define SPRINT10(OL,FORMAT,A,B,C,D,E,F,G,H,I,J)              \
    DPRINT10(FORMAT,A,B,C,D,E,F,G,H,I,J);                    \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G,H,I,J); printf("\n");    \
    }
#define SPRINT11(OL,FORMAT,A,B,C,D,E,F,G,H,I,J,K)            \
    DPRINT11(FORMAT,A,B,C,D,E,F,G,H,I,J,K);                  \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G,H,I,J,K); printf("\n");  \
    }
#define SPRINT12(OL,FORMAT,A,B,C,D,E,F,G,H,I,J,K,L)          \
    DPRINT12(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L);                \
    if (outLevel >= OL) {                                    \
        printf(FORMAT,A,B,C,D,E,F,G,H,I,J,K,L); printf("\n"); \
    }


/* error codes */
#define           BAD_MALLOC      -900

/*  miscellaneous macros */
#define           PI              3.1415926535897931159979635
#define           TWOPI           6.2831853071795862319959269
#define           PIo2            1.5707963267948965579989817
#define           PIo4            0.7853981633974482789994909
#define           PIo180          0.0174532925199432954743717

#define           HUGEQ           99999999.0
#define           HUGEI           9999999
#define           EPS03           1.0e-03
#define           EPS06           1.0e-06
#define           EPS12           1.0e-12
#define           EPS20           1.0e-20
#define           SQR(A)          ((A) * (A))
#define           NINT(A)         ((int)(A+0.5))
#define           MIN(A,B)        (((A) < (B)) ? (A) : (B))
#define           MAX(A,B)        (((A) < (B)) ? (B) : (A))
#define           MINMAX(A,B,C)   MIN(MAX((A), (B)), (C))

#endif   /* _COMMON_H_ */
