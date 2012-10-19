/*
 ************************************************************************
 *                                                                      *
 * OpenCSM.h -- header for OpenCSM.c                                    *
 *                                                                      *
 *              Written by John Dannenhoffer @ Syracuse University      *
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

#ifndef _OPENCSM_H_
#define _OPENCSM_H_

#define OCSM_MAJOR_VERSION  1
#define OCSM_MINOR_VERSION 00

/*
 ************************************************************************
 *                                                                      *
 * Overview                                                             *
 *                                                                      *
 ************************************************************************
 */

//  OpenCSM is an open-source Constructive Solid Modeller for building
//     nearly any part and/or assembly based upon the CSG paradigm. It
//     is currently built upon both the EGADS/OpenCascade system and
//     the CAPRI vendor-neutral API (and has been tested with the
//     Parasolids geometry kernel).
//
//  There are several components of OpenCSM: an API and a description
//     language, both of which are described below.
//
//  To use OpenCSM from within a program, the programmer would typically
//     make the following (C) calls:
//        ocsmLoad    to read a part/assembly description from an
//                       ASCII file (described below)
//
//        ocsmNewBrch to create a new feature tree Branch
//        ocsmGetBrch to get information about a feature tree Branch
//        ocsmSetBrch to set the activity of a feature tree Branch
//        ocsmDelBrch to delete the last feature tree Branch
//
//        ocsmGetArg  to get the definition of a Branch argument
//        ocsmSetArg  to redefine a Branch argument
//
//        ocsmGetAttr to get the definition of a Branch's Attribute
//        ocsmSetAttr to set the definition of a Branch's Attribute
//
//        ocsmGetName to get the name of a Branch
//        ocsmSetName to set the name of a Branch
//
//        ocsmNewPmtr to create a new Parameter
//        ocsmGetPmtr to get info about a Parameter
//
//        ocsmGetValu to get the Value of a Parameter
//        ocsmSetValu to set a Value for a Paremeter
//
//        ocsmCheck   to check that the Branches are properly ordered
//
//        ocsmBuild   to execute the feature tree and generate a series
//                       of Bodies
//
//        <various EGADS and CAPRI calls to interact with the configuration>
//
//        ocsmFree    to free-up all memory used by OpenCSM

/*
 ************************************************************************
 *                                                                      *
 * CSM file format                                                      *
 *                                                                      *
 ************************************************************************
 */

//  The .csm file contains a series of statements.
//
//  If a line contains a hash (#), all characters starting at the hash
//     are ignored.
//
//  If a line contains a backslash, all characters starting at the
//     backslash are ignored and the next line is appended; spaces at
//     the beginning of the next line are treated normally.
//
//  All statements begin with a keyword (described below) and must
//     contain at least the indicated number of arguments.
//
//  Extra arguments in a statement are discarded and can thus be
//     used as a comment.
//
//  The last statement must be "end". (Everything else is ignored.)
//
//  All arguments must not contain any spaces or must be enclosed
//     in a pair of double quotes (for example, "a + b").
//
//  Parameters are evaluated in the order that they appear in the
//     file, using MATLAB-like syntax (see "Expression rules" below).
//
//  During the build process, OpenCSM maintains a LIFO "Stack" that
//     can contain Bodies and Sketches.
//
//  The csm statements are executed in a stack-like way, taking their
//     inputs from the Stack and depositing their results onto the Stack.
//
//  The default name for each Branch is "Brch_xxxxxx", where xxxxxx
//     is a unique sequence number.
//
//  Special characters:
//     #          introduces comment
//     "          ignore spaces until following "
//     \          ignore this and following characters and concatenate next line
//     <space>    separates arguments in .csm file
//
//     0-9        digits used in numbers and in names
//     A-Z a-z _  letters used in names
//     .          decimal separator (used in numbers)
//     ,          separates function arguments and row/column in subscripts
//     ;          multi-value item separator
//     ( )        groups expressions and function arguments
//     [ ]        specifies subscripts in form [row,column]
//     + - * / ^  arithmetic operators
//     !          as first character, forces argument to be evaluated
//     $          as first character, forces argument not to be evaluated (used internally)
//     @          as first character, introduces named constants
//
//     ~          not used
//     %          not used
//     &          not used
//     =          not used
//     { }        not used
//     '          not used
//     :          not used
//     < >        not used
//     ?          not used

/*
 ************************************************************************
 *                                                                      *
 * Valid CSM statements                                                 *
 *                                                                      *
 ************************************************************************
 */

// dimension pmtrName nrow ncol despmtr=0
//           use:    set up a multi-value Parameter
//           pops:   -
//           pushes: -
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   nrow >= 1
//                   ncol >= 1
//                   pmtrName must not start with '@'
//                   if despmtr=0, then marked as INTERNAL
//                   if despmtr=1, then marked as EXTERNAL
//                   does not create a Branch
//
// despmtr   pmtrName values
//           use:    define a (constant) driving design Parameter
//           pops:   -
//           pushes: -
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   pmtrName can be in form "name" or "name[irow,icol]"
//                   pmtrName must not start with '@'
//                   name must not refer to an INTERNAL Parameter
//                   name will be marked as EXTERNAL
//                   name is used directly (without evaluation)
//                   irow and icol cannot contain a comma or open bracket
//                   values cannot refer to any other Parameter
//                   if values has multiple values (separated by ;), then
//                      any subscripts in pmtrName are ignored
//                   values are defined across rows
//                   if values is longer than Parameter size, extra values are lost
//                   if values is shorter than Parameter size, last value is repeated
//                   does not create a Branch
//
// set       pmtrName exprs
//           use:    define a (redefinable) driven Parameter
//           pops:   -
//           pushes: -
//           notes:  solver   may not be open
//                   pmtrName can be in form "name" or "name[irow,icol]"
//                   pmtrName must not start with '@'
//                   name must not refer to an EXTERNAL Parameter
//                   name will be marked as INTERNAL
//                   name is used directly (without evaluation)
//                   irow and icol cannot contain a comma or open bracket
//                   if exprs has multiple values (separated by ;), then
//                      any subscripts in pmtrName are ignored
//                   exprs are defined across rows
//                   if exprs is longer than Parameter size, extra exprs are lost
//                   if exprs is shorter than Parameter size, last expr is repeated
//
// box       xbase ybase zbase dx dy dz
//           use:    create a box Body
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   face order is: xmin, xmax, ymin, ymax, zmin, zmax
//
// sphere    xcent ycent zcent radius
//           use:    create a sphere Body
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   face order is: ymin, ymax
//
// cone      xvrtx yvrtx zvrtx xbase ybase zbase radius
//           use:    create a cone Body
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   face order is: base, (empty), (xyz)min, (xyz)max
//
// cylinder  xbeg ybeg zbeg xend yend zend radius
//           use:    create a cylinder Body
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   face order is: beg, end, (xyz)min, (xyz)max
//
// torus     xcent ycent zcent dxaxis dyaxis dzaxis majorRad minorRad
//           use:    create a torus Body
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   face order is: xmin/ymin, xmin/ymax, xmax/ymax, xmax,ymax
//
// import    filename
//           use:    import from filename
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   filename is used directly (without evaluation)
//
// udprim    primtype argName1 argValue1 argName2 argValue2 argName3 argValue3 argName4 argValue4
//           use:    create a Body by executing a user-defined primitive
//           pops:   -
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   primtype  determines the type of primitive and the number of argName/argValue pairs
//                   primtype  is used directly (without evaluation)
//                   argName#  is used directly (without evaluation)
//                   argValue# is evaluated if it starts with "!", otherwise it is used directly
//                   see udp documentation for full information
//
// extrude   dx dy dz
//           use:    create a Body by extruding a Sketch
//           pops:   Sketch
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   if Sketch is a SHEET Body, then a SOLID Body is created
//                   if Sketch is a WIRE  Body, then a SHEET Body is created
//                   face order is: base, end, feat1, ...
//
// loft      smooth
//           use:    create a Body by lofting through Sketches since mark
//           pops:   Sketch1 ...
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   all Sketches must have the same number of Segments
//                   if Sketch is a SHEET Body, then a SOLID Body is created
//                   if Sketch is a WIRE  Body, then a SHEET Body is created
//                   face order is: base, end, feat1, ...
//                   if NINT(smooth)==1, then sections are smoothed
//                   the first and/or last Sketch can be a point
//
// revolve   xorig yorig zorig dxaxis dyaxis dzaxis angDeg
//           use:    create a Body by revolving a Sketch around an axis
//           pops:   Sketch
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   if Sketch is a SHEET Body, then a SOLID Body is created
//                   if Sketch is a WIRE  Body, then a SHEET Body is created
//                   face order is: (base), (end), feat1, ...
//
// fillet    radius edgeList=0
//           use:    apply a fillet to a Body
//           pops:   Body
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   if previous operation is boolean, apply to all new Edges
//                   edgeList=0 is the same as edgeList=[0;0]
//                   edgeList is a [n*2] multi-valued Parameter
//                   rows of edgeList are processed in order
//                   rows of edgeList are interpreted as follows:
//                     col1  col2   meaning
//                      =0    =0    add all Edges
//                      >0    >0    add    Edges between iford=+icol1 and iford=+icol2
//                      <0    <0    remove Edges between iford=-icol1 and iford=-icol2
//                      >0    =0    add    Edges adjacent to iford=+icol1
//                      <0    =0    remove Edges adjacent to iford=-icol1
//
// chamfer   radius edgeList=0
//           use:    apply a chamfer to a Body
//           pops:   Body
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   if previous operation is boolean, apply to all new Edges
//                   edgeList=0 is the same as edgeList=[0;0]
//                   edgeList is a [n*2] multi-valued Parameter
//                   rows of edgeList are processed in order
//                   rows of edgeList are interpreted as follows:
//                     col1  col2   meaning
//                      =0    =0    add all Edges
//                      >0    >0    add    Edges between iford=+icol1 and iford=+icol2
//                      <0    <0    remove Edges between iford=-icol1 and iford=-icol2
//                      >0    =0    add    Edges adjacent to iford=+icol1
//                      <0    =0    remove Edges adjacent to iford=-icol1
//
// hollow    thick   iface1=0  iface2=0  iface3=0  iface4=0  iface5=0  iface6=0
//           use:    hollow out a solid Body
//           pops:   Body
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   if iface*=0, then Face not added to list
//                   if all iface*=0 then create an offset body instead
//
// intersect order=none index=1
//           use:    perform Boolean intersection (Body2 & Body1)
//           pops:   Body1 Body2
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   Body1 and Body2 must be SOLID Bodys
//                   if intersection does not produce at least index Bodies, an error is returned
//                   order may be one of:
//                       none    same order as returned from geometry engine
//                       xmin    minimum xmin   is first
//                       xmax    maximum xmax   is first
//                       ymin    minimum ymin   is first
//                       ymax    maximum ymax   is first
//                       zmin    minimum zmin   is first
//                       zmax    maximum zmax   is first
//                       amin    minimum area   is first
//                       amax    maximum area   is first
//                       vmin    minimum volume is first
//                       vmax    maximum volume is first
//                   order is used directly (without evaluation)
//
// subtract  order=none index=1
//           use:    perform Boolean subtraction (Body2 - Body1)
//           pops:   Body1 Body2
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   Body1 and Body2 must be SOLID Bodys
//                   if subtraction does not produce at least index Bodies, an error is returned
//                   order may be one of:
//                       none    same order as returned from geometry engine
//                       xmin    minimum xmin   is first
//                       xmax    maximum xmax   is first
//                       ymin    minimum ymin   is first
//                       ymax    maximum ymax   is first
//                       zmin    minimum zmin   is first
//                       zmax    maximum zmax   is first
//                       amin    minimum area   is first
//                       amax    maximum area   is first
//                       vmin    minimum volume is first
//                       vmax    maximum volume is first
//                   order is used directly (without evaluation)
//
// union
//           use:    perform Boolean union (Body2 | Body1)
//           pops:   Body1 Body2
//           pushes: Body
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   Body1 and Body2 must be SOLID Bodys
//
// translate dx dy dz
//           use:    translates the entry on top of Stack
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// rotatex   angDeg yaxis zaxis
//           use:    rotates entry on top of Stack around x-like axis
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// rotatey   angDeg zaxis xaxis
//           use:    rotates entry on top of Stack around y-like axis
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// rotatez   angDeg xaxis yaxis
//           use:    rotates entry on top of Stack around z-like axis
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// scale     fact
//           use:    scales entry on top of Stack
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// skbeg     x y z
//           use:    start a new sketch with the given point
//           pops:   -
//           pushes: -
//           notes:  opens sketcher
//                   solver   may not be open
//
// linseg    x y z
//           use:    create a new line segment, connecting the previous
//                      and specified points
//           pops:   -
//           pushes: -
//           notes:  sketcher must be open
//                   solver   may not be open
//
// cirarc    xon yon zon xend yend zend
//           use:    create a new circular arc, using t he previous point
//                      as well as the two points specified
//           pops:   -
//           pushes: -
//           notes:  sketcher must be open
//                   solver   may not be open
//
// spline    x y z
//           use:    add a point to a spline
//           pops:   -
//           pushes: -
//           notes:  sketcher must be open
//                   solver   may not be open
//
// skend
//           use:    completes a Sketch
//           pops:   -
//           pushes: Sketch
//           notes:  sketcher must be open
//                   solver   may not be open
//                   all linsegs and cirarcs must be x-, y-, or z-co-planar
//                   if sketch is     closed, then a SHEET Body is created
//                   if sketch is not closed, then a WIRE  Body is created
//                   if skend immediately follows skbeg, then a Point
//                      is created (which can be used at either end of a loft)
//                   closes sketcher
// solbeg    varlist
//           use:    starts a solver block
//           pops:   -
//           pushes: -
//           notes:  solver must not be open
//                   opens the solver
//                   varlist is a list of semi-colon-separated INTERNAL parameters
//                   varlist must end with a semi-colon
//
// solcon    expr
//           use:    constraint used to set solver variables
//           pops:   -
//           pushes: -
//           notes:  sketcher must not be open
//                   solver must be open
//                   solend will drive expr to zero
//
// solend
//           use:    close a solver black
//           pops:   -
//           pushes: -
//           notes:  sketcher must not be open
//                   solver must be open
//                   adjust variables to drive constrains to zero
//                   closes the solver
//
// macbeg    imacro
//           use:    marks the start of a macro
//           pops:   -
//           pushes: -
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   imacro must be between 1 and 100
//                   cannot overwrite a previous macro
//
// macend
//           use:    ends a macro
//           pops:   -
//           pushes: -
//           notes:
//
// recall    imacro
//           use:    recalls copy of macro from a storage location imacro
//           pops:   -
//           pushes: any
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   storage location imacro must have been previously filled by a macro command
//
// patbeg    pmtrName ncopy
//           use:    execute the pattern multiple times
//           pops:   -
//           pushes: -
//           notes:  solver   may not be open
//                   pattern contains all statement up to the matching patend
//                   pmtrName must not start with '@'
//                   pmtrName takes values from 1 to ncopy (see below)
//                   pmtrName is used directly (without evaluation)
//
// patend
//           use:    mark the end of a pattern
//           pops:   -
//           pushes: -
//           notes:  solver   may not be open
//                   there must be a matching patbeg for each patend
//
// mark
//           use:    used to identify groups such as in loft
//           pops:   -
//           pushes: -
//           notes:  sketcher may not be open
//                   solver   may not be open
//
// dump      filename remove=0
//           pops:   -
//           pushes: -
//           notes:  solver   may not be open
//                   if file exists, it is overwritten
//                   filename is used directly (without evaluation)
//                   if remove == 1, then Body is removed after dumping
//
// name      branchName
//           use:    names the entry on top of Stack
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   does not create a Branch
//
// attribute attrName attrValue
//           use:    sets an attribute for the entry on top of Stack
//           pops:   any
//           pushes: any
//           notes:  sketcher may not be open
//                   attrValue is treated as a string
//                   if first char of attrValue is !, then evaluate
//                   does not create a Branch
//
// end
//           pops:   -
//           pushes: -
//           notes:  sketcher may not be open
//                   solver   may not be open
//                   Bodys on Stack are returned in LIFO

/*
 ************************************************************************
 *                                                                      *
 * Expression rules                                                     *
 *                                                                      *
 ************************************************************************
 */

//  Valid names:
//      start with a letter
//      contain letters, digits, and underscores
//      contain fewer than 32 characters
//
//  Array names:
//      basic format is: name[irow,icol]
//      name must follow rules above
//      irow and icol must be valid expressions
//
//  Valid operators (in order of precedence):
//      ( )            parentheses, inner-most evaluated first
//      func(a,b)      function arguments, then function itself
//      ^              exponentiation      (evaluated left to right)
//      * /            multiply and divide (evaluated left to right)
//      + -            add and subtract    (evaluated left to right)
//
//  Valid function calls:
//      pi(x)                        3.14159...*x
//      min(x,y)                     minimum of x and y
//      max(x,y)                     maximum of x and y
//      sqrt(x)                      square root of x
//      abs(x)                       absolute value of x
//      int(x)                       integer part of x  (3.5 -> 3, -3.5 -> -3)
//      nint(x)                      nearest integer to x
//      exp(x)                       exponential of x
//      log(x)                       natural logarithm of x
//      log10(x)                     common logarithm of x
//      sin(x)                       sine of x          (in radians)
//      sind(x)                      sine of x          (in degrees)
//      asin(x)                      arc-sine of x      (in radians)
//      asind(x)                     arc-sine of x      (in degrees)
//      cos(x)                       cosine of x        (in radians)
//      cosd(x)                      cosine of x        (in dagrees)
//      acos(x)                      arc-cosine of x    (in radians)
//      acosd(x)                     arc-cosine of x    (in degrees)
//      tan(x)                       tangent of x       (in radians)
//      tand(x)                      tangent of x       (in degrees)
//      atan(x)                      arc-tangent of x   (in radians)
//      atand(x)                     arc-tangent of x   (in degrees)
//      atan2(y,x)                   arc-tangent of y/x (in radians)
//      atan2d(y,x)                  arc-tangent of y/x (in degrees)
//      hypot(x,y)                   hypoteneuse: sqrt(x^2+y^2)
//      Xcent(xa,ya,Cab,xb,yb)       X-center of circular arc
//      Ycent(xa,ya,Cab,xb,yb)       Y-center of circular arc
//      Xmidl(xa,ya,Cab,xb,yb)       X-point at midpoint of circular arc
//      Ymidl(xa,ya,Cab,xb,yb)       Y-point at midpoint of circular arc
//      turnang(xa,ya,Cab,xb,yb)     turning angle of circular arc (in degrees)
//      tangent(xa,ya,Cab,xb,yb,...
//                       Cbc,xc,yc)  tangent angle at b (in degrees)
//      ifzero(test,ifTrue,ifFalse)  if test=0, return ifTrue, else ifFalse
//      ifpos(test,ifTrue,ifFalse)   if test>0, return ifTrue, else ifFalse
//      ifneg(test,ifTrue,ifFalse)   if test<0, return ifTrue, else ifFalse

/*
 ************************************************************************
 *                                                                      *
 * Structures                                                           *
 *                                                                      *
 ************************************************************************
 */

/* "Attr" is a Branch attribute */
typedef struct {
    char          *name;                /* Attribute name */
    char          *value;               /* Attribute value */
} attr_T;

/* "GRatt" is a graphic attribute */
typedef struct {
    void          *object;              /* pointer to GvGraphic (or NULL) */
    int           active;               /* =1 if entity should be rendered */
    int           color;                /* entity color in form 0x00rrggbb */
    int           bcolor;               /* back   color in form 0x00rrggbb */
    int           mcolor;               /* mesh   color in form 0x00rrggbb */
    int           lwidth;               /* line width in pixels */
    int           ptsize;               /* point size in pixels */
    int           render;               /* render flags: */
                                        /*    2 GV_FOREGROUND */
                                        /*    4 GV_ORIENTATION */
                                        /*    8 GV_TRANSPARENT */
                                        /*   16 GV_FACETLIGHT */
                                        /*   32 GV_MESH */
                                        /*   64 GV_FORWARD */
    int           dirty;               /* =1 if attributes have been changed */
} grat_T;

/* "Node" is a 0-D topological entity in a Body */
typedef struct {
    int           nedge;                /* number of indicent Edges */

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        ego       enode;                /* EGADS node object */
    #endif
} node_T;

/* "Edge" is a 1-D topological entity in a Body */
typedef struct {
    int           ileft;                /* Face on the left */
    int           irite;                /* Face on the rite */
    int           ibody;                /* Body index (1-nbody) */
    int           iford;                /* Face order */
    grat_T        gratt;                /* GRatt of the Edge */

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        ego       eedge;                /* EGADS edge object */
    #endif
} edge_T;

/* "Face" is a 2-D topological entity ina Body */
typedef struct {
    int           nbody;                /* number of Body indices */
    int           *ibody;               /* array  of Body indices (1-nbody) */
    int           *iford;               /* array  of Face orders */
    grat_T        gratt;                /* GRatt of the Face */

    #if   defined(GEOM_CAPRI)
    #elif defined(GEOM_EGADS)
        ego       eface;                /* EGADS face object */
    #endif
} face_T;

/* "Body" is a boundary rperesentation */
typedef struct {
    int           ibrch;                /* Branch associated with Body */
    int           brtype;               /* Branch type (see below) */
    int           ileft;                /* left parent Body (or 0) */
    int           irite;                /* rite parent Body (or 0) */
    int           ichld;                /* child Body (or 0 for root) */
    double        arg1;                 /* first   argument */
    double        arg2;                 /* second  argument */
    double        arg3;                 /* third   argument */
    double        arg4;                 /* fourth  argument */
    double        arg5;                 /* fifth   argument */
    double        arg6;                 /* sixth   argument */
    double        arg7;                 /* seventh argument */
    double        arg8;                 /* eighth  argument */
    double        arg9;                 /* nineth  argument */

    #if   defined(GEOM_CAPRI)
        int       ivol;                 /* CAPRI volume index */
    #elif defined(GEOM_EGADS)
        ego       ebody;                /* EGADS Body         object(s) */
        ego       etess;                /* EGADS Tessellation object(s) */
    #endif

    int           onstack;              /* =1 if on stack (and returned); =0 otherwise */
    int           botype;               /* Body type (see below) */
    int           nnode;                /* number of Nodes */
    node_T        *node;                /* array  of Nodes */
    int           nedge;                /* number of Edges */
    edge_T        *edge;                /* array  of Edges */
    int           nface;                /* number of Faces */
    face_T        *face;                /* array  of Faces */
    grat_T        gratt;                /* GRatt of the Nodes */
} body_T;

/* "Brch" is a Branch in a feature tree */
typedef struct {
    char          *name;                /* name  of Branch */
    int           type;                 /* type  of Branch (see OpenCSM.h) */
    int           class;                /* class of Branch (see OpenCSM.h) */
    int           actv;                 /* activity of Branch (see OpenCSM.h) */
    int           nattr;                /* number of Attributes */
    attr_T        *attr;                /* array  of Attributes */
    int           ileft;                /* left parent Branch (or 0)*/
    int           irite;                /* rite parent Branch (or 0)*/
    int           ichld;                /* child Branch (or 0 for root) */
    int           narg;                 /* number of arguments */
    char          *arg1;                /* definition for args[1] */
    char          *arg2;                /* definition for args[2] */
    char          *arg3;                /* definition for args[3] */
    char          *arg4;                /* definition for args[4] */
    char          *arg5;                /* definition for args[5] */
    char          *arg6;                /* definition for args[6] */
    char          *arg7;                /* definition for args[7] */
    char          *arg8;                /* definition for args[8] */
    char          *arg9;                /* definition for args[9] */
} brch_T;

/* "Pmtr" is a driving or driven Parameter */
typedef struct {
    char          *name;                /* name of Parameter */
    int           type;                 /* Parameter type (see below) */
    int           nrow;                 /* number of rows */
    int           ncol;                 /* number of columns */
    double        *value;               /* current value(s) */
} pmtr_T;

/* "Modl" is a constructive solid model consisting of a tree of Branches
         and (possibly) a set of Parameters as well as the associated Bodys */
typedef struct {
    int           magic;                /* magic number to check for valid *modl */
    int           checked;              /* =1 if successfully passed checks */
    int           nextseq;              /* number of next automatcally-numbered item */
    int           atPmtrs[24];          /* array of "at" Paremeters */

    int           nbrch;                /* number of Branches */
    int           mbrch;                /* maximum   Branches */
    brch_T        *brch;                /* array  of Branches */

    int           npmtr;                /* number of Parameters */
    int           mpmtr;                /* maximum   Parameters */
    pmtr_T        *pmtr;                /* array  of Parameters */

    int           nbody;                /* number of Bodys */
    int           mbody;                /* maximum   Bodys */
    body_T        *body;                /* array  of Bodys */

    #if   defined(GEOM_CAPRI)
        int       *context;             /* CAPRI context (not used) */
    #elif defined(GEOM_EGADS)
        ego       context;              /* EGADS context */
    #endif
} modl_T;

/*
 ************************************************************************
 *                                                                      *
 * Callable routines                                                    *
 *                                                                      *
 ************************************************************************
 */

/* return current version */
int ocsmVersion(int   *imajor,          /* (out) major version number */
                int   *iminor);         /* (out) minor version number */

/* set output level */
int ocsmSetOutLevel(int    ilevel);     /* (in)  output level: */
                                        /*       =0 warnings and errors only */
                                        /*       =1 nominal (default) */
                                        /*       =2 debug */

/* create a MODL by reading a .csm file */
int ocsmLoad(char   filename[],         /* (in)  file to be read (with .csm) */
             void   **modl);            /* (out) pointer to MODL */

/* save a MODL to a file */
int ocsmSave(void   *modl,              /* (in)  pointer to MODL */
             char   filename[]);        /* (in)  file to be written (with .csm) */

/* copy a MODL */
int ocsmCopy(void   *srcModl,           /* (in)  pointer to source MODL */
             void   **newModl);         /* (out) pointer to new    MODL */

/* free up all storage associated with a MODL */
int ocsmFree(void   *modl);             /* (in)  pointer to MODL */

/* get info about a MODL */
int ocsmInfo(void   *modl,              /* (in)  pointer to MODL */
             int    *nbrch,             /* (out) number of Branches */
             int    *npmtr,             /* (out) number of Parameters */
             int    *nbody);            /* (out) number of Bodys */


/* check that Branches are properly ordered */
int ocsmCheck(void   *modl);            /* (in)  pointer to MODL */

/* build Bodys by executing the MODL up to a given Branch */
int ocsmBuild(void   *modl,             /* (in)  pointer to MODL */
              int    buildTo,           /* (in)  last Branch to execute (or 0 for all) */
              int    *builtTo,          /* (out) last Branch executed successfully */
              int    *nbody,            /* (in)  number of entries allocated in body[] */
                                        /* (out) number of Bodys on the stack */
              int    body[]);           /* (out) array  of Bodys on the stack (LIFO)
                                                 (at least nbody long) */

/* create a new Branch */
int ocsmNewBrch(void   *modl,           /* (in)  pointer to MODL */
                int    iafter,          /* (in)  Branch index (0-nbrch) after which to add */
                int    type,            /* (in)  Branch type (see below) */
      /*@null@*/char   arg1[],          /* (in)  Argument 1 (or NULL) */
      /*@null@*/char   arg2[],          /* (in)  Argument 2 (or NULL) */
      /*@null@*/char   arg3[],          /* (in)  Argument 3 (or NULL) */
      /*@null@*/char   arg4[],          /* (in)  Argument 4 (or NULL) */
      /*@null@*/char   arg5[],          /* (in)  Argument 5 (or NULL) */
      /*@null@*/char   arg6[],          /* (in)  Argument 6 (or NULL) */
      /*@null@*/char   arg7[],          /* (in)  Argument 7 (or NULL) */
      /*@null@*/char   arg8[],          /* (in)  Argument 8 (or NULL) */
      /*@null@*/char   arg9[]);         /* (in)  Argument 9 (or NULL) */

/* get info about a Branch */
int ocsmGetBrch(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                int    *type,           /* (out) Branch type (see below) */
                int    *class,          /* (out) Branch class (see below) */
                int    *actv,           /* (out) Branch Activity (see below) */
                int    *ichld,          /* (out) ibrch of child (or 0 if root) */
                int    *ileft,          /* (out) ibrch of left parent (or 0) */
                int    *irite,          /* (out) ibrch of rite parent (or 0) */
                int    *narg,           /* (out) number of Arguments */
                int    *nattr);         /* (out) number of Attributes */

/* set activity for a Branch */
int ocsmSetBrch(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                int    actv);           /* (in)  Branch activity (see below) */

/* delete the last Branch */
int ocsmDelBrch(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch);          /* (in)  Branch index (1-nbrch) */

/* print Branches to file */
int ocsmPrintBrchs(void   *modl,        /* (in)  pointer to MODL */
                   FILE   *fp);         /* (in)  pointer to FILE */

/* get an Argument for a Branch */
int ocsmGetArg(void   *modl,            /* (in)  pointer to MODL */
               int    ibrch,            /* (in)  Branch index (1-nbrch) */
               int    iarg,             /* (in)  Argument index (1-narg) */
               char   defn[],           /* (out) Argument definition (at least 129 long) */
               double *value);          /* (out) Argument value */

/* set an Argument for a Branch */
int ocsmSetArg(void   *modl,            /* (in)  pointer to MODL */
               int    ibrch,            /* (in)  Branch index (1-nbrch) */
               int    iarg,             /* (in)  Argument index (1-narg) */
               char   defn[]);          /* (in)  Argument definition */

/* return an Attribute for a Branch by index */
int ocsmRetAttr(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                int    iattr,           /* (in)  Attribute index (1-nattr) */
                char   aname[],         /* (out) Attribute name  (at least 129 long) */
                char   avalue[]);       /* (out) Attribute value (at least 129 long) */

/* get an Attribute for a Branch by name*/
int ocsmGetAttr(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                char   aname[],         /* (in)  Attribute name */
                char   avalue[]);       /* (out) Attribute value (at least 129 long) */

/* set an Attribute for a Branch */
int ocsmSetAttr(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                char   aname[],         /* (in)  Attribute name */
                char   avalue[]);       /* (in)  Attribute value */

/* get the name of a Branch */
int ocsmGetName(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                char   name[]);         /* (out) Branch name (at least 129 long) */

/* set the name for a Branch */
int ocsmSetName(void   *modl,           /* (in)  pointer to MODL */
                int    ibrch,           /* (in)  Branch index (1-nbrch) */
                char   name[]);         /* (in)  Branch name */

/* create a new Parameter */
int ocsmNewPmtr(void   *modl,           /* (in)  pointer to MODL */
                char   name[],          /* (in)  Parameter name */
                int    type,            /* (in)  Parameter type */
                int    nrow,            /* (in)  number of rows */
                int    ncol);           /* (in)  number of columns */

/* get info about a Parameter */
int ocsmGetPmtr(void   *modl,           /* (in)  pointer to MODL */
                int    ipmtr,           /* (in)  Parameter index (1-npmtr) */
                int    *type,           /* (out) Parameter type */
                int    *nrow,           /* (out) number of rows */
                int    *ncol,           /* (out) number of columns */
                char   name[]);         /* (out) Parameter name (at least 33 long) */

/* print external Parameters to file */
int ocsmPrintPmtrs(void   *modl,        /* (in)  pointer to MODL */
                   FILE   *fp);         /* (in)  pointer to FILE */

/* get the Value of a Parameter */
int ocsmGetValu(void   *modl,           /* (in)  pointer to MODL */
                int    ipmtr,           /* (in)  Parameter index (1-npmtr) */
                int    irow,            /* (in)  row    index (1-nrow) */
                int    icol,            /* (in)  column index (1-ncol) */
                double *value);         /* (out) Parameter value */

/* set a Value for a Parameter */
int ocsmSetValu(void   *modl,           /* (in)  pointer to MODL */
                int    ipmtr,           /* (in)  Parameter index (1-npmtr) */
                int    irow,            /* (in)  row    index (1-nrow) */
                int    icol,            /* (in)  column index (1-ncol) */
                char   defn[]);         /* (in)  definition of Value */

/* get info about a Body */
int ocsmGetBody(void   *modl,           /* (in)  pointer to MODL */
                int    ibody,           /* (in)  Body index (1-nbody) */
                int    *type,           /* (out) Branch type (see below) */
                int    *ichld,          /* (out) ibody of child (or 0 if root) */
                int    *ileft,          /* (out) ibody of left parent (or 0) */
                int    *irite,          /* (out) ibody of rite parent (or 0) */
                double args[],          /* (out) array  of Arguments (at least 10 long) */
                int    *nnode,          /* (out) number of Nodes */
                int    *nedge,          /* (out) number of Edges */
                int    *nface);         /* (out) number of Faces */

/* print all Bodys to file */
int ocsmPrintBodys(void   *modl,        /* (in)  pointer to MODL */
                   FILE   *fp);         /* (in)  pointer to FILE */

/* convert an OCSM code to text */
/*@observer@*/
char *ocsmGetText(int    icode);        /* (in)  code to look up */

/* convert text to an OCSM code */
int ocsmGetCode(char   *text);          /* (in)  text to look up */

/*
 ************************************************************************
 *                                                                      *
 * Defined constants                                                    *
 *                                                                      *
 ************************************************************************
 */

#define           OCSM_BOX        111   /* OCSM_PRIMITIVE */
#define           OCSM_SPHERE     112
#define           OCSM_CONE       113
#define           OCSM_CYLINDER   114
#define           OCSM_TORUS      115
#define           OCSM_IMPORT     116
#define           OCSM_UDPRIM     117
#define           OCSM_EXTRUDE    121   /* OCSM_GROWN */
#define           OCSM_LOFT       122
#define           OCSM_REVOLVE    123
#define           OCSM_FILLET     131   /* OCSM_APPLIED */
#define           OCSM_CHAMFER    132
#define           OCSM_HOLLOW     133
#define           OCSM_INTERSECT  141   /* OCSM_BOOLEAN */
#define           OCSM_SUBTRACT   142
#define           OCSM_UNION      143
#define           OCSM_TRANSLATE  151   /* OCSM_TRANSFORM */
#define           OCSM_ROTATEX    152
#define           OCSM_ROTATEY    153
#define           OCSM_ROTATEZ    154
#define           OCSM_SCALE      155
#define           OCSM_SKBEG      161   /* OCSM_SKETCH */
#define           OCSM_LINSEG     162
#define           OCSM_CIRARC     163
#define           OCSM_SPLINE     164
#define           OCSM_SKEND      165
#define           OCSM_SOLBEG     171   /* OCSM_SOLVER */
#define           OCSM_SOLCON     172
#define           OCSM_SOLEND     173
#define           OCSM_SET        181   /* OCSM_UTILITY */
#define           OCSM_MACBEG     182
#define           OCSM_MACEND     183
#define           OCSM_RECALL     184
#define           OCSM_PATBEG     185
#define           OCSM_PATEND     186
#define           OCSM_MARK       187
#define           OCSM_DUMP       188

#define           OCSM_PRIMITIVE  201   /* Branch classes */
#define           OCSM_GROWN      202
#define           OCSM_APPLIED    203
#define           OCSM_BOOLEAN    204
#define           OCSM_TRANSFORM  205
#define           OCSM_SKETCH     206
#define           OCSM_SOLVER     207
#define           OCSM_UTILITY    208

#define           OCSM_ACTIVE     300   /* Branch activities */
#define           OCSM_SUPPRESSED 301
#define           OCSM_INACTIVE   302
#define           OCSM_DEFERRED   303

#define           OCSM_SOLID_BODY 400   /* Body types */
#define           OCSM_SHEET_BODY 401
#define           OCSM_WIRE_BODY  402
#define           OCSM_NODE_BODY  403

#define           OCSM_EXTERNAL   500   /* Parameter types */
#define           OCSM_INTERNAL   501

/*
 ************************************************************************
 *                                                                      *
 * Return codes (errors are -201 to -299)                               *
 *                                                                      *
 ************************************************************************
 */

#define SUCCESS                                 0

#define OCSM_FILE_NOT_FOUND                  -201
#define OCSM_ILLEGAL_STATEMENT               -202
#define OCSM_NOT_ENOUGH_ARGS                 -203
#define OCSM_NAME_ALREADY_DEFINED            -204
#define OCSM_PATTERNS_NESTED_TOO_DEEPLY      -205
#define OCSM_PATBEG_WITHOUT_PATEND           -206
#define OCSM_PATEND_WITHOUT_PATBEG           -207
#define OCSM_NOTHING_TO_DELETE               -208
#define OCSM_NOT_MODL_STRUCTURE              -209

#define OCSM_DID_NOT_CREATE_BODY             -211
#define OCSM_CREATED_TOO_MANY_BODYS          -212
#define OCSM_EXPECTING_ONE_BODY              -213
#define OCSM_EXPECTING_TWO_BODYS             -214
#define OCSM_EXPECTING_ONE_SKETCH            -215
#define OCSM_EXPECTING_NLOFT_SKETCHES        -216
#define OCSM_LOFT_WITHOUT_MARK               -217
#define OCSM_TOO_MANY_SKETCHES_IN_LOFT       -218
#define OCSM_MODL_NOT_CHECKED                -219

#define OCSM_FILLET_AFTER_WRONG_TYPE         -221
#define OCSM_CHAMFER_AFTER_WRONG_TYPE        -222
#define OCSM_NO_BODYS_PRODUCED               -223
#define OCSM_NOT_ENOUGH_BODYS_PRODUCED       -224
#define OCSM_TOO_MANY_BODYS_ON_STACK         -225

#define OCSM_SKETCHER_IS_OPEN                -231
#define OCSM_SKETCHER_IS_NOT_OPEN            -232
#define OCSM_COLINEAR_SKETCH_POINTS          -233
#define OCSM_NON_COPLANAR_SKETCH_POINTS      -234
#define OCSM_TOO_MANY_SKETCH_POINTS          -235
#define OCSM_TOO_FEW_SPLINE_POINTS           -236
#define OCSM_SKETCH_DOES_NOT_CLOSE           -237

#define OCSM_ILLEGAL_CHAR_IN_EXPR            -241
#define OCSM_CLOSE_BEFORE_OPEN               -242
#define OCSM_MISSING_CLOSE                   -243
#define OCSM_ILLEGAL_TOKEN_SEQUENCE          -244
#define OCSM_ILLEGAL_NUMBER                  -245
#define OCSM_ILLEGAL_PMTR_NAME               -246
#define OCSM_ILLEGAL_FUNC_NAME               -247
#define OCSM_ILLEGAL_TYPE                    -248
#define OCSM_ILLEGAL_NARG                    -249

#define OCSM_NAME_NOT_FOUND                  -251
#define OCSM_NAME_NOT_UNIQUE                 -252
#define OCSM_PMTR_IS_EXTERNAL                -253
#define OCSM_PMTR_IS_INTERNAL                -254
#define OCSM_FUNC_ARG_OUT_OF_BOUNDS          -255
#define OCSM_VAL_STACK_UNDERFLOW             -256  /* probably not enough args to func */
#define OCSM_VAL_STACK_OVERFLOW              -257  /* probably too many   args to func */

#define OCSM_ILLEGAL_BRCH_INDEX              -261  /* should be from 1 to nbrch */
#define OCSM_ILLEGAL_PMTR_INDEX              -262  /* should be from 1 to npmtr */
#define OCSM_ILLEGAL_BODY_INDEX              -263  /* should be from 1 to nbody */
#define OCSM_ILLEGAL_ARG_INDEX               -264  /* should be from 1 to narg  */
#define OCSM_ILLEGAL_ACTIVITY                -265  /* should OCSM_ACTIVE or OCSM_SUPPRESSED */
#define OCSM_ILLEGAL_MACRO_INDEX             -266  /* should be between 1 and 100 */
#define OCSM_ILLEGAL_ARGUMENT                -267
#define OCSM_CANNOT_BE_SUPPRESSED            -268
#define OCSM_STORAGE_ALREADY_USED            -269
#define OCSM_NOTHING_PREVIOUSLY_STORED       -270

#define OCSM_SOLVER_IS_OPEN                  -271
#define OCSM_SOLVER_IS_NOT_OPEN              -272
#define OCSM_TOO_MANY_SOLVER_VARS            -273
#define OCSM_UNDERCONSTRAINED                -274
#define OCSM_OVERCONSTRAINED                 -275
#define OCSM_SINGULAR_MATRIX                 -276
#define OCSM_NOT_CONVERGED                   -277

#define OCSM_UDP_ERROR1                      -281
#define OCSM_UDP_ERROR2                      -282
#define OCSM_UDP_ERROR3                      -283
#define OCSM_UDP_ERROR4                      -284
#define OCSM_UDP_ERROR5                      -285
#define OCSM_UDP_ERROR6                      -286
#define OCSM_UDP_ERROR7                      -287
#define OCSM_UDP_ERROR8                      -288
#define OCSM_UDP_ERROR9                      -289

#define OCSM_OP_STACK_UNDERFLOW              -291
#define OCSM_OP_STACK_OVERFLOW               -292
#define OCSM_RPN_STACK_UNDERFLOW             -293
#define OCSM_RPN_STACK_OVERFLOW              -294
#define OCSM_TOKEN_STACK_UNDERFLOW           -295
#define OCSM_TOKEN_STACK_OVERFLOW            -296
#define OCSM_UNSUPPORTED                     -298
#define OCSM_INTERNAL_ERROR                  -299

/*
 ************************************************************************
 *                                                                      *
 * Other Notes                                                          *
 *                                                                      *
 ************************************************************************
 */

// 1. the Edges and Faces in the created Bodys are annotated with a
//    "body" attribute that contains the Body number (ibody) and
//     order number (iford) that created it

#endif  /* _OPENCSM_H_ */
