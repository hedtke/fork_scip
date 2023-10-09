/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*  Copyright (c) 2002-2023 Zuse Institute Berlin (ZIB)                      */
/*                                                                           */
/*  Licensed under the Apache License, Version 2.0 (the "License");          */
/*  you may not use this file except in compliance with the License.         */
/*  You may obtain a copy of the License at                                  */
/*                                                                           */
/*      http://www.apache.org/licenses/LICENSE-2.0                           */
/*                                                                           */
/*  Unless required by applicable law or agreed to in writing, software      */
/*  distributed under the License is distributed on an "AS IS" BASIS,        */
/*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. */
/*  See the License for the specific language governing permissions and      */
/*  limitations under the License.                                           */
/*                                                                           */
/*  You should have received a copy of the Apache-2.0 license                */
/*  along with SCIP; see the file LICENSE. If not visit scipopt.org.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   compute.c
 * @brief  unit tests for computing symmetry
 * @author Marc Pfetsch
 * @author Fabian Wegscheider
 */

#include <scip/scip.h>
#include <include/scip_test.h>
#include <scip/prop_symmetry.c>
#include <scip/symmetry.h>
#include <symmetry/compute_symmetry.h>
#include <scip/scipdefplugins.h>

/* global SCIP instance */
static SCIP* scip;


/** check whether two int arrays are equal */
static
void checkIntArraysEqual(
   int*                  expected,           /**< array of expected values */
   int*                  candidate,          /**< array of values to be checked */
   int                   length,             /**< length of arrays */
   const char*           name                /**< name to be printed */
   )
{
   int i;

   for( i = 0; i < length; ++i )
      cr_expect(expected[i] == candidate[i], "%s[%d]: expected %d, but got %d\n", name, i, expected[i], candidate[i]);
}

/** setup: create SCIP */
static
void setup(void)
{
   SCIP_CALL( SCIPcreate(&scip) );
   SCIP_CALL( SCIPincludeDefaultPlugins(scip) );

   /* turn on symmetry computation */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 1) );

#ifdef SCIP_DEBUG
   /* output external codes in order to see which external symmetry computation code is used */
   SCIPprintExternalCodes(scip, NULL);
   SCIPinfoMessage(scip, NULL, "\n");
#endif
}

/** teardown: free SCIP */
static
void teardown(void)
{
   SCIP_CALL( SCIPfree(&scip) );
   cr_assert_eq(BMSgetMemoryUsed(), 0, "Memory leak!");
}

/** simple example with 4 variables and 2 linear constraints */
static
void simpleExample1(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_CONS* cons;
   SCIP_VAR* vars[2];
   SCIP_Real vals[2];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 + x2 + x3 + x4
    *     x1 + x2           = 1
    *               x3 + x4 = 1
    *     x1, ..., x4 binary
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "basic1"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var4) );

   vars[0] = var1;
   vars[1] = var2;
   vals[0] = 1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e1", 2, vars, vals, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   vals[0] = 1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e2", 2, vars, vals, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* determine symmetry type */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 8;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 4;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );
   cr_assert( nperms == 3 );
   cr_assert( ncomponents == 1 );
   cr_assert( componentbegins[0] == 0 );
   cr_assert( componentbegins[1] == 3 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == 0 );
   cr_assert( vartocomponent[2] == 0 );
   cr_assert( vartocomponent[3] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      printf("norbits: %d\n\n", norbits);
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
      cr_assert( orbitbegins[2] == 8 );
      cr_assert( orbits[0] == 0 );
      cr_assert( orbits[1] == 1 );
      cr_assert( orbits[2] == 2 );
      cr_assert( orbits[3] == 3 );
      cr_assert( orbits[4] == 4 );
      cr_assert( orbits[5] == 5 );
      cr_assert( orbits[6] == 6 );
      cr_assert( orbits[7] == 7 );
   }
   else
   {
      cr_assert( norbits == 1 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
      cr_assert( orbits[0] == 0 );
      cr_assert( orbits[1] == 1 );
      cr_assert( orbits[2] == 2 );
      cr_assert( orbits[3] == 3 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
}


/** simple example with 4 variables and 4 linear constraints */
static
void simpleExample2(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_CONS* cons;
   SCIP_VAR* vars[2];
   SCIP_Real vals[2];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 + x2 + x3 + x4
    *     x1 + x2           =  1
    *               x3 + x4 =  1
    *    2x1 +           x4 <= 2
    *         2x2 + x3      <= 2
    *     x1, ..., x4 binary
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "basic2"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var4) );

   vars[0] = var1;
   vars[1] = var2;
   vals[0] = 1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e1", 2, vars, vals, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   vals[0] = 1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e2", 2, vars, vals, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var1;
   vars[1] = var4;
   vals[0] = 2.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "i1", 2, vars, vals, -SCIPinfinity(scip), 2.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var2;
   vars[1] = var3;
   vals[0] = 2.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "i2", 2, vars, vals, -SCIPinfinity(scip), 2.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 8;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 4;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );
   cr_assert( nperms == 1 );
   cr_assert( ncomponents == 1 );
   cr_assert( componentbegins[0] == 0 );
   cr_assert( componentbegins[1] == 1 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == 0 );
   cr_assert( vartocomponent[2] == 0 );
   cr_assert( vartocomponent[3] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      cr_assert( norbits == 4 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
      cr_assert( orbitbegins[3] == 6 );
      cr_assert( orbits[0] == 0 );
      cr_assert( orbits[1] == 1 );
      cr_assert( orbits[2] == 2 );
      cr_assert( orbits[3] == 3 );
      cr_assert( orbits[4] == 4 );
      cr_assert( orbits[5] == 5 );
      cr_assert( orbits[6] == 6 );
      cr_assert( orbits[7] == 7 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbits[0] == 0 );
      cr_assert( orbits[1] == 1 );
      cr_assert( orbits[2] == 2 );
      cr_assert( orbits[3] == 3 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
}

/** simple example with 4 variables and 4 linear constraints */
static
void simpleExample3(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* var5;
   SCIP_CONS* cons;
   SCIP_VAR* vars[3];
   SCIP_Real vals[3];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 + x2 + x3 + x4
    *     x1 + x2           + x5 = 1
    *               x3 + x4 + x5 = 2
    *     x1, ..., x4, x5  binary
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "basic4"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var5, "x5", 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, var5) );

   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var5;
   vals[0] = 1.0;
   vals[1] = 1.0;
   vals[2] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e1", 3, vars, vals, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   vars[2] = var5;
   vals[0] = 1.0;
   vals[1] = 1.0;
   vals[2] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "e2", 3, vars, vals, 2.0, 2.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 10;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 5;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );
   cr_assert( nperms == 2 );
   cr_assert( ncomponents == 2 );
   cr_assert( vartocomponent[0] == vartocomponent[1] );
   cr_assert( vartocomponent[2] == vartocomponent[3] );
   cr_assert( vartocomponent[0] != vartocomponent[2] );
   cr_assert( vartocomponent[1] != vartocomponent[3] );
   cr_assert( vartocomponent[4] == -1 );
   cr_assert( componentbegins[0] == 0 );
   cr_assert( componentbegins[1] == 1 );
   cr_assert( componentbegins[2] == 2 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      cr_assert( norbits == 4 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
      cr_assert( orbitbegins[3] == 6 );
      cr_assert( orbitbegins[4] == 8 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &var5) );
}

/** simple example with 6 variables and 3 bounddisjunction constraints */
static
void exampleBounddisjunction(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* var5;
   SCIP_VAR* var6;
   SCIP_CONS* cons;
   SCIP_VAR* vars[2];
   SCIP_Real vals[2];
   SCIP_BOUNDTYPE btypes[2];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 - x2 + x3 - x4 + x5 - x6
    *     BD(x1 <= -1, x2 >= 1)
    *     BD(x3 <= 7, x4 >= 9)
    *     BD(x5 <= -1, x6 >= 1)
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "BD"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -10, 10, 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -10, 10, -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -10, 10, 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -10, 10, -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var5, "x5", -10, 10, 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var5) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var6, "x6", -10, 10, -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var6) );

   vars[0] = var1;
   vars[1] = var2;
   vals[0] = -1.0;
   vals[1] = 1.0;
   btypes[0] = SCIP_BOUNDTYPE_UPPER;
   btypes[1] = SCIP_BOUNDTYPE_LOWER;
   SCIP_CALL( SCIPcreateConsBasicBounddisjunction(scip, &cons, "c1", 2, vars, btypes, vals) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   vals[0] = 7.0;
   vals[1] = 9.0;
   SCIP_CALL( SCIPcreateConsBasicBounddisjunction(scip, &cons, "c2", 2, vars, btypes, vals) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var5;
   vars[1] = var6;
   vals[0] = -1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicBounddisjunction(scip, &cons, "c3", 2, vars, btypes, vals) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 12;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 6;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if ( detectsignedperms )
   {
      cr_assert( nperms == 2 );
      cr_assert( ncomponents == 1 );
      cr_assert( vartocomponent[0] == vartocomponent[1] );
      cr_assert( vartocomponent[1] == vartocomponent[4] );
      cr_assert( vartocomponent[4] == vartocomponent[5] );
      cr_assert( vartocomponent[2] == -1 );
      cr_assert( vartocomponent[3] == -1 );
      cr_assert( componentbegins[0] == 0 );
      cr_assert( componentbegins[1] == 2 );
   }
   else
   {
      cr_assert( nperms == 1 );
      cr_assert( ncomponents == 1 );
      cr_assert( vartocomponent[0] == vartocomponent[1] );
      cr_assert( vartocomponent[1] == vartocomponent[4] );
      cr_assert( vartocomponent[4] == vartocomponent[5] );
      cr_assert( vartocomponent[2] == -1 );
      cr_assert( vartocomponent[3] == -1 );
      cr_assert( componentbegins[0] == 0 );
      cr_assert( componentbegins[1] == 1 );
   }

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
      cr_assert( orbitbegins[2] == 8 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &var5) );
   SCIP_CALL( SCIPreleaseVar(scip, &var6) );
}

/** simple example with 4 variables and a cardinality constraint */
static
void exampleCardinality(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* ind1;
   SCIP_VAR* ind2;
   SCIP_VAR* ind3;
   SCIP_VAR* ind4;
   SCIP_CONS* cons;
   SCIP_VAR* vars[4];
   SCIP_VAR* inds[4];
   SCIP_Real vals[4];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 - x2 + x3 + x4
    *     x1 - x2 + x3 + x4 >= 2
    *     CARD(x1, x2, x3, x4) <= 3
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "Card"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &ind1, "ind1", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, ind1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &ind2, "ind2", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, ind2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &ind3, "ind3", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, ind3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &ind4, "ind4", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, ind4) );

   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vars[3] = var4;
   vals[0] = 1.0;
   vals[1] = -1.0;
   vals[2] = 1.0;
   vals[3] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 4, vars, vals, 2.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   inds[0] = ind1;
   inds[1] = ind2;
   inds[2] = ind3;
   inds[3] = ind4;
   SCIP_CALL( SCIPcreateConsBasicCardinality(scip, &cons, "c2", 4, vars, 3, inds, NULL) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 16;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 8;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if ( detectsignedperms )
   {
      cr_assert( nperms == 3 );
      cr_assert( ncomponents == 1 );
      cr_assert( vartocomponent[0] == 0 );
      cr_assert( vartocomponent[1] == 0 );
      cr_assert( vartocomponent[2] == 0 );
      cr_assert( vartocomponent[3] == 0 );
      cr_assert( vartocomponent[4] == 0 );
      cr_assert( vartocomponent[5] == 0 );
      cr_assert( vartocomponent[6] == 0 );
      cr_assert( vartocomponent[7] == 0 );
   }
   else
   {
      cr_assert( nperms == 2 );
      cr_assert( ncomponents == 1 );
      cr_assert( vartocomponent[0] == vartocomponent[2] );
      cr_assert( vartocomponent[2] == vartocomponent[3] );
      cr_assert( vartocomponent[1] == -1 );
      cr_assert( componentbegins[0] == 0 );
      cr_assert( componentbegins[1] == 2 );
   }

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      cr_assert( norbits == 4 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
      cr_assert( orbitbegins[2] == 8 );
      cr_assert( orbitbegins[3] == 12 );
      cr_assert( orbitbegins[4] == 16 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 3 );
      cr_assert( orbitbegins[2] == 6 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &ind1) );
   SCIP_CALL( SCIPreleaseVar(scip, &ind2) );
   SCIP_CALL( SCIPreleaseVar(scip, &ind3) );
   SCIP_CALL( SCIPreleaseVar(scip, &ind4) );
}

/** simple example with 6 variables and indicator constraints */
static
void exampleIndicator(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* bin1;
   SCIP_VAR* bin2;
   SCIP_CONS* cons;
   SCIP_VAR* vars[4];
   SCIP_Real vals[4];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 - x2 + x3 - x4
    *     b1 = 1 --> x1 - x2 <= 2
    *     b2 = 1 --> x3 - x4 <= 2
    *     x1 - x2 + x3 - x4 >= 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "Indicator"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &bin1, "bin1", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, bin1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &bin2, "bin2", 0, 1, 0.0, SCIP_VARTYPE_BINARY) );
   SCIP_CALL( SCIPaddVar(scip, bin2) );

   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vars[3] = var4;
   vals[0] = 1.0;
   vals[1] = -1.0;
   vals[2] = 1.0;
   vals[3] = -1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 4, vars, vals, 0.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   SCIP_CALL( SCIPcreateConsBasicIndicator(scip, &cons, "c2", bin1, 2, vars, vals, 2.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   SCIP_CALL( SCIPcreateConsBasicIndicator(scip, &cons, "c3", bin2, 2, vars, vals, 2.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 16;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 8;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if( detectsignedperms )
   {
      cr_assert( nperms == 3 );
   }
   else
   {
      cr_assert( nperms == 1 );
   }
   cr_assert( ncomponents == 1 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == 0 );
   cr_assert( vartocomponent[2] == 0 );
   cr_assert( vartocomponent[3] == 0 );
   cr_assert( vartocomponent[4] == 0 );
   cr_assert( vartocomponent[5] == 0 );
   cr_assert( vartocomponent[6] == 0 );
   cr_assert( vartocomponent[7] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      int orbitlens[6];
      int i;

      cr_assert( norbits == 6 );

      for (i = 0; i < 6; ++i)
         orbitlens[i] = orbitbegins[i+1] - orbitbegins[i];
      SCIPsortInt(orbitlens, 6);

      cr_assert( orbitlens[0] == 2 );
      cr_assert( orbitlens[1] == 2 );
      cr_assert( orbitlens[2] == 2 );
      cr_assert( orbitlens[3] == 2 );
      cr_assert( orbitlens[4] == 4 );
      cr_assert( orbitlens[5] == 4 );
   }
   else
   {
      cr_assert( norbits == 4 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
      cr_assert( orbitbegins[3] == 6 );
      cr_assert( orbitbegins[4] == 8 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &bin1) );
   SCIP_CALL( SCIPreleaseVar(scip, &bin2) );
}

/** simple example with 4 variables and SOS1 constraints */
static
void exampleSOS1(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_CONS* cons;
   SCIP_VAR* vars[4];
   SCIP_Real vals[4];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 - x2 + x3 - x4
    *     SOS1(x1,x2)
    *     SOS1(X3,x4)
    *     x1 - x2 + x3 - x4 >= 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "SOS1"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );

   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vars[3] = var4;
   vals[0] = 1.0;
   vals[1] = -1.0;
   vals[2] = 1.0;
   vals[3] = -1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 4, vars, vals, 0.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   SCIP_CALL( SCIPcreateConsBasicSOS1(scip, &cons, "c2", 2, vars, NULL) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var3;
   vars[1] = var4;
   SCIP_CALL( SCIPcreateConsBasicSOS1(scip, &cons, "c3", 2, vars, NULL) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 8;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 4;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if( detectsignedperms )
   {
      cr_assert( nperms == 2 );
   }
   else
   {
      cr_assert( nperms == 1 );
   }
   cr_assert( ncomponents == 1 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == 0 );
   cr_assert( vartocomponent[2] == 0 );
   cr_assert( vartocomponent[3] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
      cr_assert( orbitbegins[2] == 8 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
}

/** simple example with 6 variables and SOS2 constraints */
static
void exampleSOS2(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* var5;
   SCIP_VAR* var6;
   SCIP_CONS* cons;
   SCIP_VAR* vars[6];
   SCIP_Real vals[6];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* setup problem:
    * min x1 + x2 + x3 - x4 - x5 - x6
    *     SOS2(x1,x2,x3)
    *     SOS2(X4,x5,x6)
    *     x1 + x2 + x3 - x4 -x5 -x6 >= 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "SOS2"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var5, "x5", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var5) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var6, "x6", -SCIPinfinity(scip), SCIPinfinity(scip), -1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var6) );

   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vars[3] = var4;
   vars[4] = var5;
   vars[5] = var6;
   vals[0] = 1.0;
   vals[1] = 1.0;
   vals[2] = 1.0;
   vals[3] = -1.0;
   vals[4] = -1.0;
   vals[5] = -1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 6, vars, vals, 0.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   SCIP_CALL( SCIPcreateConsBasicSOS2(scip, &cons, "c2", 3, vars, NULL) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   vars[0] = var4;
   vars[1] = var5;
   vars[2] = var6;
   SCIP_CALL( SCIPcreateConsBasicSOS2(scip, &cons, "c3", 3, vars, NULL) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 12;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 6;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if( detectsignedperms )
   {
      cr_assert( nperms == 3 );
      cr_assert( ncomponents == 1 );
      cr_assert( vartocomponent[0] == 0 );
      cr_assert( vartocomponent[1] == 0 );
      cr_assert( vartocomponent[2] == 0 );
      cr_assert( vartocomponent[3] == 0 );
      cr_assert( vartocomponent[4] == 0 );
      cr_assert( vartocomponent[5] == 0 );
   }
   else
   {
      cr_assert( nperms == 2 );
      cr_assert( ncomponents == 2 );
      cr_assert( vartocomponent[0] == 0 );
      cr_assert( vartocomponent[1] == -1 );
      cr_assert( vartocomponent[2] == 0 );
      cr_assert( vartocomponent[3] == 1 );
      cr_assert( vartocomponent[4] == -1 );
      cr_assert( vartocomponent[5] == 1 );
   }

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );
   if ( detectsignedperms )
   {
      int orbitlens[4];
      int i;

      cr_assert( norbits == 4 );

      for (i = 0; i < 4; ++i)
         orbitlens[i] = orbitbegins[i+1] - orbitbegins[i];
      SCIPsortInt(orbitlens, 4);

      cr_assert( orbitlens[0] == 2 );
      cr_assert( orbitlens[1] == 2 );
      cr_assert( orbitlens[2] == 4 );
      cr_assert( orbitlens[3] == 4 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }
   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &var5) );
   SCIP_CALL( SCIPreleaseVar(scip, &var6) );
}

/** simple example with 3 variables and nonlinear constraints */
static
void exampleExpr1(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_CONS* cons;
   SCIP_CONSHDLR* conshdlr;
   SCIP_EXPR* varexpr1;
   SCIP_EXPR* varexpr2;
   SCIP_EXPR* varexpr3;
   SCIP_EXPR* powexpr;
   SCIP_EXPR* prodexpr;
   SCIP_EXPR* exprs[3];
   SCIP_VAR* vars[3];
   SCIP_Real vals[3];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* get nonlinear conshdlr */
   conshdlr = SCIPfindConshdlr(scip, "nonlinear");
   cr_assert(conshdlr != NULL);

   /* setup problem:
    * min x1 + x2 + x3
    *     x1 + x2 + x3   >= 2
    *     x1^3 * x2 * x3 == 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "expr1"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );

   /* create linear constraint */
   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vals[0] = 1.0;
   vals[1] = 1.0;
   vals[2] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 3, vars, vals, 2.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* create nonlinear constraint */
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr1, var1, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr2, var2, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr3, var3, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprPow(scip, &powexpr, varexpr1, 3, NULL, NULL) );
   exprs[0] = powexpr;
   exprs[1] = varexpr2;
   exprs[2] = varexpr3;
   SCIP_CALL( SCIPcreateExprProduct(scip, &prodexpr, 3, exprs, 1.0, NULL, NULL) );

   SCIP_CALL( SCIPcreateConsBasicNonlinear(scip, &cons, "c2", prodexpr, 0.0, 0.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 6;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 3;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   cr_assert( nperms == 1 );
   cr_assert( ncomponents == 1 );
   cr_assert( vartocomponent[0] == -1 );
   cr_assert( vartocomponent[1] == 0 );
   cr_assert( vartocomponent[2] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );

   if ( detectsignedperms )
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }
   else
   {
      cr_assert( norbits == 1 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
   }

   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseExpr(scip, &prodexpr) );
   SCIP_CALL( SCIPreleaseExpr(scip, &powexpr) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr3) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
}

/** simple example with 5 variables and nonlinear constraints */
static
void exampleExpr2(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_VAR* var5;
   SCIP_CONS* cons;
   SCIP_CONSHDLR* conshdlr;
   SCIP_EXPR* varexpr1;
   SCIP_EXPR* varexpr2;
   SCIP_EXPR* varexpr3;
   SCIP_EXPR* varexpr4;
   SCIP_EXPR* varexpr5;
   SCIP_EXPR* powexpr1;
   SCIP_EXPR* powexpr2;
   SCIP_EXPR* prodexpr1;
   SCIP_EXPR* prodexpr2;
   SCIP_EXPR* exprs[3];
   SCIP_VAR* vars[5];
   SCIP_Real vals[5];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* get nonlinear conshdlr */
   conshdlr = SCIPfindConshdlr(scip, "nonlinear");
   cr_assert(conshdlr != NULL);

   /* setup problem:
    * min x1 + x2 + x3 + x4 + x5
    *     x1 + x2 + x3 + x4 + x5  >= 2
    *     x1^3 * x2 * x3          == 0
    *     x4^3 * x2 * x5          == 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "expr2"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var5, "x5", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var5) );

   /* create linear constraint */
   vars[0] = var1;
   vars[1] = var2;
   vars[2] = var3;
   vars[3] = var4;
   vars[4] = var5;
   vals[0] = 1.0;
   vals[1] = 1.0;
   vals[2] = 1.0;
   vals[3] = 1.0;
   vals[4] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 5, vars, vals, 2.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* create nonlinear constraints */
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr1, var1, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr2, var2, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr3, var3, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr4, var4, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr5, var5, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprPow(scip, &powexpr1, varexpr1, 3, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprPow(scip, &powexpr2, varexpr4, 3, NULL, NULL) );
   exprs[0] = powexpr1;
   exprs[1] = varexpr2;
   exprs[2] = varexpr3;
   SCIP_CALL( SCIPcreateExprProduct(scip, &prodexpr1, 3, exprs, 1.0, NULL, NULL) );
   exprs[0] = powexpr2;
   exprs[1] = varexpr2;
   exprs[2] = varexpr5;
   SCIP_CALL( SCIPcreateExprProduct(scip, &prodexpr2, 3, exprs, 1.0, NULL, NULL) );

   SCIP_CALL( SCIPcreateConsBasicNonlinear(scip, &cons, "c2", prodexpr1, 0.0, 0.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   SCIP_CALL( SCIPcreateConsBasicNonlinear(scip, &cons, "c3", prodexpr2, 0.0, 0.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 10;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 5;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   cr_assert( nperms == 1 );
   cr_assert( ncomponents == 1 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == -1 );
   cr_assert( vartocomponent[2] == 0 );
   cr_assert( vartocomponent[3] == 0 );
   cr_assert( vartocomponent[4] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );

   if ( detectsignedperms )
   {
      cr_assert( norbits == 4 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
      cr_assert( orbitbegins[3] == 6 );
      cr_assert( orbitbegins[4] == 8 );
   }
   else
   {
      cr_assert( norbits == 2 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
      cr_assert( orbitbegins[2] == 4 );
   }

   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseExpr(scip, &prodexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &prodexpr1) );
   SCIP_CALL( SCIPreleaseExpr(scip, &powexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &powexpr1) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr5) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr4) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr3) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
   SCIP_CALL( SCIPreleaseVar(scip, &var5) );
}

/** simple example with 4 variables and nonlinear constraints */
static
void exampleExpr3(
   SCIP_Bool             detectsignedperms   /**< whether signed permutations shall be detected */
   )
{
   SCIP_VAR* var1;
   SCIP_VAR* var2;
   SCIP_VAR* var3;
   SCIP_VAR* var4;
   SCIP_CONS* cons;
   SCIP_CONSHDLR* conshdlr;
   SCIP_EXPR* varexpr1;
   SCIP_EXPR* varexpr2;
   SCIP_EXPR* powexpr1;
   SCIP_EXPR* powexpr2;
   SCIP_EXPR* sumexpr;
   SCIP_EXPR* exprs[2];
   SCIP_VAR* vars[2];
   SCIP_Real vals[2];
   SCIP_VAR** permvars;
   int** perms;
   int* orbits;
   int* orbitbegins;
   int* components;
   int* componentbegins;
   int* vartocomponent;
   int ncomponents;
   int norbits;
   int npermvars;
   int nperms;
   int permlen;

   /* skip test if no symmetry can be computed */
   if ( ! SYMcanComputeSymmetry() )
      return;

   /* get nonlinear conshdlr */
   conshdlr = SCIPfindConshdlr(scip, "nonlinear");
   cr_assert(conshdlr != NULL);

   /* setup problem:
    * min x3 + 2*x4
    *     x3 + x4     >= 2
    *     x1^2 + x2^2 == 0
    */
   SCIP_CALL( SCIPcreateProbBasic(scip, "expr3"));

   SCIP_CALL( SCIPcreateVarBasic(scip, &var1, "x1", -SCIPinfinity(scip), SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var1) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var2, "x2", -SCIPinfinity(scip), SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var2) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var3, "x3", -SCIPinfinity(scip), SCIPinfinity(scip), 1.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var3) );
   SCIP_CALL( SCIPcreateVarBasic(scip, &var4, "x4", -SCIPinfinity(scip), SCIPinfinity(scip), 2.0, SCIP_VARTYPE_CONTINUOUS) );
   SCIP_CALL( SCIPaddVar(scip, var4) );

   /* create linear constraint */
   vars[0] = var3;
   vars[1] = var4;
   vals[0] = 1.0;
   vals[1] = 1.0;
   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &cons, "c1", 2, vars, vals, 2.0, SCIPinfinity(scip)) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* create nonlinear constraints */
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr1, var1, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprVar(scip, &varexpr2, var2, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprPow(scip, &powexpr1, varexpr1, 2, NULL, NULL) );
   SCIP_CALL( SCIPcreateExprPow(scip, &powexpr2, varexpr2, 2, NULL, NULL) );
   exprs[0] = powexpr1;
   exprs[1] = powexpr2;
   SCIP_CALL( SCIPcreateExprSum(scip, &sumexpr, 2, exprs, vals, 0.0, NULL, NULL) );

   SCIP_CALL( SCIPcreateConsBasicNonlinear(scip, &cons, "c2", sumexpr, 1.0, 1.0) );
   SCIP_CALL( SCIPaddCons(scip, cons) );
   SCIP_CALL( SCIPreleaseCons(scip, &cons) );

   /* turn off presolving in order to avoid having trivial problem afterwards */
   SCIP_CALL( SCIPsetIntParam(scip, "presolving/maxrounds", 0) );

   /* turn on checking of symmetries */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/checksymmetries", TRUE) );

   /* turn off subgroup detection */
   SCIP_CALL( SCIPsetBoolParam(scip, "propagating/symmetry/detectsubgroups", FALSE) );

   /* general symmetry detection */
   SCIP_CALL( SCIPsetIntParam(scip, "misc/usesymmetry", 7) );

   /* note that indicator constraints introduce a slack variable, i.e., we have 8 variables */
   if ( detectsignedperms )
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 1) );
      permlen = 8;
   }
   else
   {
      SCIP_CALL( SCIPsetIntParam(scip, "propagating/symmetry/symtype", 0) );
      permlen = 4;
   }

   /* presolve problem (symmetry will be available afterwards) */
   SCIP_CALL( SCIPpresolve(scip) );

   /* get symmetry */
   SCIP_CALL( SCIPgetSymmetry(scip,
         &npermvars, &permvars, NULL, &nperms, &perms, NULL, NULL, NULL,
         &components, &componentbegins, &vartocomponent, &ncomponents) );

   if ( detectsignedperms )
   {
      cr_assert( nperms == 2 );
   }
   else
   {
      cr_assert( nperms == 1 );
   }
   cr_assert( ncomponents == 1 );
   cr_assert( vartocomponent[0] == 0 );
   cr_assert( vartocomponent[1] == 0 );

   /* compute orbits */
   SCIP_CALL( SCIPallocBufferArray(scip, &orbits, permlen) );
   SCIP_CALL( SCIPallocBufferArray(scip, &orbitbegins, permlen) );
   SCIP_CALL( SCIPcomputeOrbitsSym(scip, detectsignedperms, permvars, npermvars,
         perms, nperms, orbits, orbitbegins, &norbits) );

   if ( detectsignedperms )
   {
      cr_assert( norbits == 1 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 4 );
   }
   else
   {
      cr_assert( norbits == 1 );
      cr_assert( orbitbegins[0] == 0 );
      cr_assert( orbitbegins[1] == 2 );
   }

   SCIPfreeBufferArray(scip, &orbitbegins);
   SCIPfreeBufferArray(scip, &orbits);

   SCIP_CALL( SCIPreleaseExpr(scip, &sumexpr) );
   SCIP_CALL( SCIPreleaseExpr(scip, &powexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &powexpr1) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr2) );
   SCIP_CALL( SCIPreleaseExpr(scip, &varexpr1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var1) );
   SCIP_CALL( SCIPreleaseVar(scip, &var2) );
   SCIP_CALL( SCIPreleaseVar(scip, &var3) );
   SCIP_CALL( SCIPreleaseVar(scip, &var4) );
}

/* TEST SUITE */
TestSuite(test_compute_symmetry, .init = setup, .fini = teardown);

/* TEST 1 */
Test(test_compute_symmetry, basic1, .description = "compute permutation symmetries for a simple example with 4 variables and 2 linear constraints")
{
   simpleExample1(FALSE);
}

/* TEST 2 */
Test(test_compute_symmetry, basic2, .description = "compute signed symmetries for a simple example with 4 variables and 2 linear constraints")
{
   simpleExample1(TRUE);
}

/* TEST 3 */
Test(test_compute_symmetry, basic3, .description = "compute permutation symmetry for a simple example with 4 variables and 4 linear constraints")
{
   simpleExample2(FALSE);
}

/* TEST 4 */
Test(test_compute_symmetry, basic4, .description = "compute signed permutation symmetry for a simple example with 4 variables and 4 linear constraints")
{
   simpleExample2(TRUE);
}

/* TEST 5 */
Test(test_compute_symmetry, basic5, .description = "compute permutation symmetries for a simple example with 5 variables and 2 linear constraints")
{
   simpleExample3(FALSE);
}

/* TEST 6 */
Test(test_compute_symmetry, basic6, .description = "compute signed permutation symmetries for a simple example with 5 variables and 2 linear constraints")
{
   simpleExample3(TRUE);
}

/* TEST 7 */
Test(test_compute_symmetry, special1, .description = "compute permutation symmetries for an example containing bounddisjunction constraints")
{
   exampleBounddisjunction(FALSE);
}

/* TEST 8 */
Test(test_compute_symmetry, special2, .description = "compute signed permutation symmetries for an example containing bounddisjunction constraints")
{
   exampleBounddisjunction(TRUE);
}

/* TEST 9 */
Test(test_compute_symmetry, special3, .description = "compute permutation symmetries for an example containing cardinality constraints")
{
   exampleCardinality(FALSE);
}

/* TEST 10 */
Test(test_compute_symmetry, special4, .description = "compute signed permutation symmetries for an example containing cardinality constraints")
{
   exampleCardinality(TRUE);
}

/* TEST 11 */
Test(test_compute_symmetry, special5, .description = "compute permutation symmetries for an example containing indicator constraints")
{
   exampleIndicator(FALSE);
}

/* TEST 12 */
Test(test_compute_symmetry, special6, .description = "compute signed permutation symmetries for an example containing indicator constraints")
{
   exampleIndicator(TRUE);
}

/* TEST 13 */
Test(test_compute_symmetry, special7, .description = "compute permutation symmetries for an example containing SOS1 constraints")
{
   exampleSOS1(FALSE);
}

/* TEST 14 */
Test(test_compute_symmetry, special8, .description = "compute signed permutation symmetries for an example containing SOS1 constraints")
{
   exampleSOS1(TRUE);
}

/* TEST 15 */
Test(test_compute_symmetry, special9, .description = "compute permutation symmetries for an example containing SOS2 constraints")
{
   exampleSOS2(FALSE);
}

/* TEST 16 */
Test(test_compute_symmetry, special10, .description = "compute signed permutation symmetries for an example containing SOS2 constraints")
{
   exampleSOS2(TRUE);
}

/* TEST 17 */
Test(test_compute_symmetry, expr1, .description = "compute permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr1(FALSE);
}

/* TEST 18 */
Test(test_compute_symmetry, expr2, .description = "compute signed permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr1(TRUE);
}

/* TEST 19 */
Test(test_compute_symmetry, expr3, .description = "compute permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr2(FALSE);
}

/* TEST 20 */
Test(test_compute_symmetry, expr4, .description = "compute signed permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr2(TRUE);
}

/* TEST 21 */
Test(test_compute_symmetry, expr5, .description = "compute permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr3(FALSE);
}

/* TEST 22 */
Test(test_compute_symmetry, expr6, .description = "compute signed permutation symmetries for an example containing nonlinear constraints")
{
   exampleExpr3(TRUE);
}

