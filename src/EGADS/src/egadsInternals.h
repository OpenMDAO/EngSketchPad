#ifndef EGADSINTERNALS_H
#define EGADSINTERNALS_H
/*
 *      EGADS: Electronic Geometry Aircraft Design System
 *
 *             Internal function Header
 *
 *      Copyright 2011-2012, Massachusetts Institute of Technology
 *      Licensed under The GNU Lesser General Public License, version 2.1
 *      See http://www.opensource.org/licenses/lgpl-2.1.php
 *
 */

#define PI     3.14159265358979324


#ifdef __ProtoExt__
#undef __ProtoExt__
#endif
#ifdef __cplusplus
extern "C" {
#define __ProtoExt__
#else
#define __ProtoExt__ extern
#endif

__ProtoExt__ /*@null@*/ /*@out@*/ /*@only@*/ 
             void *EG_alloc( int nbytes );
__ProtoExt__ /*@null@*/ /*@only@*/ 
             void *EG_calloc( int nele, int size );
__ProtoExt__ /*@null@*/ /*@only@*/ 
             void *EG_reall( /*@null@*/ /*@only@*/ /*@returned@*/ void *ptr,
                             int nbytes );
__ProtoExt__ void EG_free( /*@null@*/ /*@only@*/ void *pointer );
__ProtoExt__ /*@null@*/ /*@only@*/ 
             char *EG_strdup( /*@null@*/ const char *str );

__ProtoExt__ /*@kept@*/ /*@null@*/ egObject *
                  EG_context( const egObject *object );
__ProtoExt__ int  EG_outLevel( const egObject *object );
__ProtoExt__ int  EG_makeObject( /*@null@*/ egObject *context, egObject **obj );
__ProtoExt__ int  EG_deleteObject( egObject *object );
__ProtoExt__ int  EG_dereferenceObject( egObject *object,
                                        /*@null@*/ const egObject *ref );
__ProtoExt__ int  EG_dereferenceTopObj( egObject *object,
                                        /*@null@*/ const egObject *ref );
__ProtoExt__ int  EG_referenceObject( egObject *object, 
                                      /*@null@*/ const egObject *ref );
__ProtoExt__ int  EG_referenceTopObj( egObject *object, 
                                      /*@null@*/ const egObject *ref );
__ProtoExt__ int  EG_removeCntxtRef( egObject *object );

__ProtoExt__ int  EG_attributeDel( egObject *obj, /*@null@*/ const char *name );
__ProtoExt__ int  EG_attributeDup( const egObject *src, egObject *dst );
__ProtoExt__ int  EG_attributePrint( const egObject *src );

#ifdef __cplusplus
}
#endif

#endif
