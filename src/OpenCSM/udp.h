/*
 * GEM/OpenCSM User Defined Primitive include
 *
 */


/* initialize & get info about the list of arguments */
extern int
udp_initialize(char   *primName,
               int    *nArgs,
               char   **name[],
               int    *type[],
               int    *idefault[],
               double *ddefault[]);

/* set the argument list back to default */
extern int
udp_clrArguments(char *primName);

/* set an argument -- characters are converted based on type */
extern int
udp_setArgument(char *primName,
                char *name,
                char *value);

/* execute */
extern int
udp_executePrim(char *primName,
                ego  context,
                ego  *body,             // destroy with EG_deleteObject
                int  *nMesh,            // =0 for no 3D mesh
                char **str);            // freeable

/* return the OverSet Mesh */
extern int
udp_getMesh(char   *primName,
            ego    body,
            int    iMesh,               // bias-1
            int    *imax,
            int    *jmax,
            int    *kmax,
            double **mesh);             // freeable

/* return sensitivity derivatives for the "real" argument */
extern int
udp_sensitivity(char   *primName,
                ego    body,
                char   *vname,
                int    npts,
                int    *fIndices,
                double *uvs,
                double *dxdname);

/* return sensitivity step-size of finite differencing */
extern int
udp_stepSize(char   *primName,
             ego    body,
             char   *name,
             double *delta);

/* unload and cleanup for all */
extern void
udp_cleanupAll();

