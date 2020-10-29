/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2020 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not visit scip.zib.de.         */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   expr.c
 * @brief  functions for algebraic expressions
 * @author Ksenia Bestuzheva
 * @author Benjamin Mueller
 * @author Felipe Serrano
 * @author Stefan Vigerske
 */

#include "scip/scip_expr.h"
#include "scip/expr.h"
#include "scip/struct_expr.h"
#include "nlpi/nlpi_ipopt.h" /* for LAPACK */

/*
 * Data structures
 */

/** variable mapping data passed on during copying expressions when copying SCIP instances */
typedef struct
{
   SCIP_HASHMAP*         varmap;             /**< SCIP_HASHMAP mapping variables of the source SCIP to corresponding variables of the target SCIP */
   SCIP_HASHMAP*         consmap;            /**< SCIP_HASHMAP mapping constraints of the source SCIP to corresponding constraints of the target SCIP */
   SCIP_Bool             global;             /**< should a global or a local copy be created */
   SCIP_Bool             valid;              /**< indicates whether every variable copy was valid */
} COPY_MAPVAR_DATA;

/** printing to file data */
struct SCIP_ExprPrintData
{
   FILE*                 file;               /**< file to print to */
   SCIP_EXPRITER*        iterator;           /**< iterator to use */
   SCIP_Bool             closefile;          /**< whether file need to be closed when finished printing */
   SCIP_HASHMAP*         leaveexprs;         /**< hashmap storing leave (no children) expressions */
   SCIP_EXPRPRINT_WHAT   whattoprint;        /**< flags that indicate what to print for each expression */
};

/*
 * Local methods
 */

/** creates an expression */
static
SCIP_RETCODE createExpr(
   SCIP_SET*             set,                /**< global SCIP settings */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   SCIP_EXPR**           expr,               /**< pointer where to store expression */
   SCIP_EXPRHDLR*        exprhdlr,           /**< expression handler */
   SCIP_EXPRDATA*        exprdata,           /**< expression data (expression assumes ownership) */
   int                   nchildren,          /**< number of children */
   SCIP_EXPR**           children,           /**< children (can be NULL if nchildren is 0) */
   )
{
   int c;

   assert(set != NULL);
   assert(blkmem != NULL);
   assert(expr != NULL);
   assert(exprhdlr != NULL);
   assert(children != NULL || nchildren == 0);
   assert(exprdata == NULL || exprhdlr->copydata != NULL); /* copydata must be available if there is expression data */
   assert(exprdata == NULL || exprhdlr->freedata != NULL); /* freedata must be available if there is expression data */

   SCIP_ALLOC( BMSallocClearBlockMemory(blkmem, expr) );

   (*expr)->exprhdlr = exprhdlr;
   (*expr)->exprdata = exprdata;
   (*expr)->curvature = SCIP_EXPRCURV_UNKNOWN;

   /* initialize activity to entire interval */
   SCIPintervalSetEntire(SCIP_INTERVAL_INFINITY, &(*expr)->activity);

   if( nchildren > 0 )
   {
      SCIP_ALLOC( BMSduplicateBlockMemoryArray(blkmem, &(*expr)->children, children, nchildren) );
      (*expr)->nchildren = nchildren;
      (*expr)->childrensize = nchildren;

      for( c = 0; c < nchildren; ++c )
         SCIPcaptureExpr((*expr)->children[c]);
   }

   SCIPcaptureExpr(*expr);

   return SCIP_OKAY;
}

/** initializes the ownerdata of an expression
 *
 * typically called right after creating an expression
 */
static
SCIP_RETCODE createExprOwnerData(
   SCIP_SET*             set,                          /**< global SCIP settings */
   SCIP_EXPR*            expr,                         /**< expression for which to create ownerdata */
   SCIP_DECL_EXPR_OWNERDATACREATE((*ownerdatacreate)), /**< function to call to create ownerdata */
   SCIP_EXPR_OWNERDATACREATEDATA* ownerdatacreatedata, /**< data to pass to ownerdatacreate */
   SCIP_DECL_EXPR_OWNERDATAFREE((*ownerdatafree))      /**< function to call when freeing expression, e.g., to free ownerdata */
   )
{
   assert(set != NULL);
   assert(expr != NULL);

   /* expr should not yet have ownerdata or ownerdatafree
    * (if this becomes an issue some day, we could call the ownerdatafree here instead of the asserts)
    */
   assert(expr->ownerdata == NULL);
   assert(expr->ownerdatafree == NULL);

   if( ownerdatacreate != NULL )
   {
      SCIP_CALL( ownerdatacreate(set->scip, expr, &expr->ownerdata, ownerdatacreatedata) );
   }
   expr->ownerdatafree = ownerdatafree;

   return SCIP_OKAY;
}

/** frees an expression */
static
SCIP_RETCODE freeExpr(
   SCIP_SET*             set,                /**< global SCIP settings */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   SCIP_EXPR**           expr                /**< pointer to free the expression */
   )
{
   assert(scip != NULL);
   assert(expr != NULL);
   assert(*expr != NULL);
   assert((*expr)->nuses == 1);

   /* call ownerdatafree callback, if given
    * we intentially call this also if ownerdata is NULL, so owner can be notified without storing data
    */
   if( (*expr)->ownerdatafree != NULL )
   {
      SCIP_CALL( (*expr)->ownerdatafree(set->scip, *expr, &(*expr)->ownerdata) );
   }
   assert((*expr)->ownerdata == NULL);

   /* free children array, if any */
   BMSfreeBlockMemoryArrayNull(blkmem, &(*expr)->children, (*expr)->childrensize);

   BMSfreeBlockMemory(blkmem, expr);
   assert(*expr == NULL);

   return SCIP_OKAY;
}

/** variable mapping callback to call when copying expressions (within same or different SCIPs) */
static
SCIP_DECL_EXPR_MAPVAR(copyVar)
{
   COPY_MAPVAR_DATA* data;
   SCIP_Bool valid;

   assert(sourcevar != NULL);
   assert(targetvar != NULL);
   assert(mapvardata != NULL);

   data = (COPY_MAPVAR_DATA*)mapvardata;

   SCIP_CALL( SCIPgetVarCopy(sourcescip, targetscip, sourcevar, targetvar, data->varmap, data->consmap, data->global, &valid) );
   assert(*targetvar != NULL);

   /* if copy was not valid, store so in mapvar data */
   if( !valid )
      data->valid = FALSE;

   /* caller assumes that target variable has been captured */
   SCIP_CALL( SCIPcaptureVar(targetscip, *targetvar) );

   return SCIP_OKAY;
}

/** copies an expression including subexpressions
 *
 * @note If copying fails due to an expression handler not being available in the targetscip, then *targetexpr will be set to NULL.
 *
 * Variables can be mapped to different ones by specifying a mapvar callback.
 * For all or some expressions, a mapping to an existing expression can be specified via the mapexpr callback.
 * The mapped expression (including its children) will not be copied in this case and its ownerdata will not be touched.
 * If, however, the mapexpr callback returns NULL for the targetexpr, then the expr will be copied in the usual way.
 */
static
SCIP_RETCODE copyExpr(
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat,               /**< dynamic problem statistics */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   SCIP_SET*             targetset,          /**< global SCIP settings data structure where target expression will live */
   BMS_BLKMEM*           targetblkmem,       /**< block memory in target SCIP */
   SCIP_EXPR*            sourceexpr,         /**< expression to be copied */
   SCIP_EXPR**           targetexpr,         /**< buffer to store pointer to copy of source expression */
   SCIP_DECL_EXPR_MAPVAR((*mapvar)),         /**< variable mapping function, or NULL for identity mapping */
   void*                 mapvardata,         /**< data of variable mapping function */
   SCIP_DECL_EXPR_MAPEXPR((*mapexpr)),       /**< expression mapping function, or NULL for creating new expressions */
   void*                 mapexprdata,        /**< data of expression mapping function */
   SCIP_DECL_EXPR_OWNERDATACREATE((*ownerdatacreate)), /**< function to call on expression copy to create ownerdata */
   SCIP_EXPR_OWNERDATACREATEDATA* ownerdatacreatedata, /**< data to pass to ownerdatacreate */
   SCIP_DECL_EXPR_OWNERDATAFREE((*ownerdatafree)),     /**< function to call when freeing expression, e.g., to free ownerdata */
   )
{
   SCIP_EXPRITER* it;
   SCIP_EXPRITER_USERDATA expriteruserdata;
   SCIP_EXPR* expr;
   SCIP* sourcescip = set->scip;        /* SCIP data structure corresponding to source expression */
   SCIP* targetscip = targetset->scip;  /* SCIP data structure where target expression will live */

   assert(set != NULL);
   assert(stat != NULL);
   assert(blkmem != NULL);
   assert(targetset != NULL);
   assert(sourceexpr != NULL);
   assert(targetexpr != NULL);
   assert(sourcescip != NULL);
   assert(targetscip != NULL);

   SCIP_CALL( SCIPexpriteratorCreate(stat, blkmem, &it) );
   SCIP_CALL( SCIPexpriteratorInit(it, sourceexpr, SCIP_EXPRITER_DFS, TRUE) );  /*TODO use FALSE, i.e., don't duplicate common subexpr? */
   SCIPexpriteratorSetStagesDFS(it, SCIP_EXPRITER_ENTEREXPR | SCIP_EXPRITER_VISITEDCHILD);

   expr = sourceexpr;
   while( !SCIPexpriteratorIsEnd(it) )
   {
      switch( SCIPexpriteratorGetStageDFS(it) )
      {
         case SCIP_EXPRITER_ENTEREXPR :
         {
            /* create expr that will hold the copy */
            SCIP_EXPR* exprcopy = NULL;

            if( mapexprdata != NULL )
            {
               SCIP_CALL( mapexprdata(targetscip, &exprcopy, sourcescip, expr, mapexprdata) );
               if( exprcopy != NULL )
               {
                  /* map callback gave us an expression to use for the copy */
                  /* store targetexpr */
                  expriteruserdata.ptrval = exprcopy;
                  SCIPexpriteratorSetCurrentUserData(it, expriteruserdata);

                  /* skip subexpression (assume that exprcopy is a complete copy) and continue */
                  expr = SCIPexpriteratorSkipDFS(it);
                  continue;
               }
            }

            /* if the source is a variable expression create a variable expression directly; otherwise copy the expression data */
            if( SCIPisExprVar(expr) )
            {
               SCIP_VAR* sourcevar;
               SCIP_VAR* targetvar;

               sourcevar = SCIPgetConsExprExprVarVar(expr);
               assert(sourcevar != NULL);
               targetvar = NULL;

               /* get the corresponding variable in the target SCIP */
               if( mapvar != NULL )
               {
                  SCIP_CALL( mapvar(targetscip, &targetvar, sourcescip, sourcevar, mapvardata) );
                  SCIP_CALL( SCIPcreateConsExprExprVar(targetscip, &exprcopy, targetvar) );

                  /* we need to release once since it has been captured by the mapvar() and createExprVar() call */
                  SCIP_CALL( SCIPreleaseVar(targetscip, &targetvar) );
               }
               else
               {
                  targetvar = sourcevar;
                  SCIP_CALL( SCIPcreateConsExprExprVar(targetscip, &exprcopy, targetvar) );
               }
            }
            else
            {
               SCIP_EXPRHDLR* targetexprhdlr;
               SCIP_EXPRDATA* targetexprdata;

               /* get the exprhdlr of the target scip */
               if( targetscip != sourcescip )
               {
                  targetexprhdlr = SCIPsetFindExprhdlr(targetset, SCIPexprhdlrGetName(SCIPexprGetHdlr(expr)));

                  if( targetexprhdlr == NULL )
                  {
                     /* expression handler not in target scip (probably did not have a copy callback) -> abort */
                     expriteruserdata.ptrval = NULL;
                     SCIPexpriteratorSetCurrentUserData(it, expriteruserdata);

                     expr = SCIPexpriteratorSkipDFS(it);
                     continue;
                  }
               }
               else
               {
                  targetexprhdlr = SCIPexprGetHdlr(expr);
               }
               assert(targetexprhdlr != NULL);

               /* copy expression data */
               if( expr->exprdata != NULL )
               {
                  assert(expr->exprhdlr->copydata != NULL);
                  SCIP_CALL( expr->exprhdlr->copydata(targetscip, targetexprhdlr, &targetexprdata, sourcescip, expr, mapvar, mapvardata) );
               }
               else
               {
                  targetexprdata = NULL;
               }

               /* create in targetexpr an expression of the same type as expr, but without children for now */
               SCIP_CALL( createExpr(targetset, targetblkmem, &exprcopy, targetexprhdlr, targetexprdata, 0, NULL, ownerdatacreate, ownerdatacreatedata, ownerdatafree) );
            }

            /* let future owner creates its data and store its free callback in the expr */
            SCIP_CALL( createExprOwnerData(targetset, exprcopy, ownerdatacreate, ownerdatacreatedata, ownerdatafree) );

            /* store targetexpr */
            expriteruserdata.ptrval = exprcopy;
            SCIPexpriteratorSetCurrentUserData(it, expriteruserdata);

            break;
         }

         case SCIP_EXPRITER_VISITEDCHILD :
         {
            /* just visited child so a copy of himself should be available; append it */
            SCIP_EXPR* exprcopy;
            SCIP_EXPR* childcopy;

            exprcopy = (SCIP_EXPR*)SCIPexpriteratorGetCurrentUserData(it).ptrval;

            /* get copy of child */
            childcopy = (SCIP_EXPR*)SCIPexpriteratorGetChildUserDataDFS(it).ptrval;
            if( childcopy == NULL )
            {
               /* abort */
               /* release exprcopy (should free also the already copied children) */
               SCIP_CALL( SCIPreleaseExpr(targetscip, (SCIP_EXPR**)&exprcopy) );

               expriteruserdata.ptrval = NULL;
               SCIPexpriteratorSetCurrentUserData(it, expriteruserdata);

               expr = SCIPexpriteratorSkipDFS(it);
               continue;
            }

            /* append child to exprcopy */
            SCIP_CALL( SCIPappendExprChild(targetscip, exprcopy, childcopy) );

            /* release childcopy (still captured by exprcopy) */
            SCIP_CALL( SCIPreleaseExpr(targetscip, &childcopy) );

            break;
         }

         default:
            /* we should never be called in this stage */
            SCIPABORT();
            break;
      }

      expr = SCIPexpriteratorGetNext(it);
   }

   /* the target expression should be stored in the userdata of the sourceexpr (can be NULL if aborted) */
   *targetexpr = (SCIP_EXPR*)SCIPexpriteratorGetExprUserData(it, sourceexpr).ptrval;

   SCIPexpriteratorFree(&it);

   return SCIP_OKAY;
}

/** returns an equivalent expression for a given expression if possible
 *
 * it adds the expression to key2expr if the map does not contain the key
 */
static
SCIP_RETCODE findEqualExpr(
//   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_EXPR*            expr,               /**< expression to replace */
   SCIP_MULTIHASH*       key2expr,           /**< mapping of hashes to expressions */
   SCIP_EXPR**           newexpr             /**< pointer to store an equivalent expression (NULL if there is none) */
   )
{  /*lint --e{438}*/
   SCIP_MULTIHASHLIST* multihashlist;

//   assert(set != NULL);
   assert(expr != NULL);
   assert(key2expr != NULL);
   assert(newexpr != NULL);

   *newexpr = NULL;
   multihashlist = NULL;
   do
   {
      /* search for an equivalent expression */
      *newexpr = (SCIP_EXPR*)(SCIPmultihashRetrieveNext(key2expr, &multihashlist, (void*)expr));

      if( *newexpr == NULL )
      {
         /* processed all expressions like expr from hash table, so insert expr */
         SCIP_CALL( SCIPmultihashInsert(key2expr, (void*) expr) );
         break;
      }
      else if( expr != *newexpr )
      {
         assert(SCIPexprCompare(expr, *newexpr) == 0);
         break;
      }
      else
      {
         /* can not replace expr since it is already contained in the hashtablelist */
         assert(expr == *newexpr);
         *newexpr = NULL;
         break;
      }
   }
   while( TRUE ); /*lint !e506*/

   return SCIP_OKAY;
}

/** get key of hash element */
static
SCIP_DECL_HASHGETKEY(hashCommonSubexprGetKey)
{
   return elem;
}  /*lint !e715*/

/** checks if two expressions are structurally the same */
static
SCIP_DECL_HASHKEYEQ(hashCommonSubexprEq)
{
   SCIP_EXPR* expr1;
   SCIP_EXPR* expr2;

   expr1 = (SCIP_EXPR*)key1;
   expr2 = (SCIP_EXPR*)key2;
   assert(expr1 != NULL);
   assert(expr2 != NULL);

   return expr1 == expr2 || SCIPexprCompare(expr1, expr2) == 0;
}  /*lint !e715*/

/** get value of hash element when comparing with another expression */
static
SCIP_DECL_HASHKEYVAL(hashCommonSubexprKeyval)
{
   SCIP_EXPR* expr;
   SCIP_EXPRITER* hashiterator;

   expr = (SCIP_EXPR*) key;
   assert(expr != NULL);

   hashiterator = (SCIP_EXPRITER*) userptr;
   assert(hashiterator != NULL);

   return SCIPexpriteratorGetExprUserData(hashiterator, expr).uintval;
}  /*lint !e715*/

/** hashes an expression using an already existing iterator
 *
 * The iterator must by of type DFS with allowrevisit=FALSE and only the leaveexpr stage enabled.
 * The hashes of all visited expressions will be stored in the iterators expression data.
 */
static
SCIP_RETCODE hashExpr(
   SCIP_SET*             set,                /**< global SCIP settings */
   BMS_BUFMEM*           bufmem,             /**< buffer memory */
   SCIP_EXPR*            expr,               /**< expression to hash */
   SCIP_EXPRITER*        hashiterator,       /**< iterator to use for hashing */
   int*                  nvisitedexprs       /**< counter to increment by the number of expressions visited, or NULL */
   )
{
   SCIP_EXPRITER_USERDATA iterdata;
   unsigned int* childrenhashes;
   int childrenhashessize;
   int i;

   assert(set != NULL);
   assert(expr != NULL);
   assert(hashiterator != NULL);

   childrenhashessize = 5;
   SCIP_ALLOC( BMSallocBufferMemoryArray(bufmem, &childrenhashes, childrenhashessize) );

   for( expr = SCIPexpriteratorRestartDFS(hashiterator, expr); !SCIPexpriteratorIsEnd(hashiterator); expr = SCIPexpriteratorGetNext(hashiterator) ) /*lint !e441*/
   {
      assert(SCIPexpriteratorGetStageDFS(hashiterator) == SCIP_EXPRITER_LEAVEEXPR);

      if( nvisitedexprs != NULL )
         ++*nvisitedexprs;

      /* collect hashes of children */
      if( childrenhashessize < expr->nchildren )
      {
         childrenhashessize = SCIPsetCalcMemGrowSize(set, expr->nchildren);
         SCIP_ALLOC( BMSreallocBufferMemoryArray(bufmem, &childrenhashes, childrenhashessize) );
      }
      for( i = 0; i < expr->nchildren; ++i )
         childrenhashes[i] = SCIPexpriteratorGetExprUserData(hashiterator, expr->children[i]).uintval;

      SCIP_CALL( SCIPcallExprhdlrHash(set->scip, expr, &iterdata.uintval, childrenhashes) );

      SCIPexpriteratorSetCurrentUserData(hashiterator, iterdata);
   }

   BMSfreeBufferMemoryArray(bufmem, &childrenhashes);

   return SCIP_OKAY;
}

/** replaces common sub-expressions in a given expression graph by using a hash key for each expression
 *
 *  The algorithm consists of two steps:
 *
 *  1. traverse through all given expressions and compute for each of them a (not necessarily unique) hash
 *
 *  2. initialize an empty hash table and traverse through all expression; check for each of them if we can find a
 *     structural equivalent expression in the hash table; if yes we replace the expression by the expression inside the
 *     hash table, otherwise we add it to the hash table
 *
 *  @note the hash keys of the expressions are used for the hashing inside the hash table; to compute if two expressions
 *  (with the same hash) are structurally the same we use the function SCIPexprCompare()
 */
static
SCIP_RETCODE replaceCommonSubexpressions(
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat,               /**< dynamic problem statistics */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   BMS_BUFMEM*           bufmem,             /**< buffer memory */
   SCIP_EXPR**           exprs,              /**< expressions (possibly replaced by equivalent on output) */
   int                   nexprs,             /**< total number of expressions */
   SCIP_Bool*            replacedroot        /**< buffer to store whether any root expression (expression in exprs) was replaced */
   )
{
   SCIP_EXPRITER* hashiterator;
   SCIP_EXPRITER* repliterator;
   SCIP_MULTIHASH* key2expr;
   SCIP_CONSDATA* consdata;
   int i;
   int nvisitedexprs = 0;

   assert(set != NULL);
   assert(stat != NULL);
   assert(exprs != NULL);
   assert(nexprs >= 0);
   assert(replacedroot != NULL);

   if( nexprs == 0 )
      return SCIP_OKAY;

   SCIP_CALL( SCIPexpriteratorCreate(stat, blkmem, &hashiterator) );
   SCIP_CALL( SCIPexpriteratorInit(hashiterator, NULL, SCIP_EXPRITER_DFS, FALSE) );
   SCIPexpriteratorSetStagesDFS(hashiterator, SCIP_EXPRITER_LEAVEEXPR);

   /* compute all hashes for each sub-expression */
   for( i = 0; i < nexprs; ++i )
   {
      assert(exprs[i] != NULL);
      SCIP_CALL( hashExpr(set, bufmem, exprs[i], hashiterator, &nvisitedexprs) );
   }

   /* replace equivalent sub-expressions */
   SCIP_CALL( SCIPmultihashCreate(&key2expr, blkmem, nvisitedexprs,
         hashCommonSubexprGetKey, hashCommonSubexprEq, hashCommonSubexprKeyval, (void*)hashiterator) );

   SCIP_CALL( SCIPexpriteratorCreate(stat, blkmem, &repliterator) );

   for( i = 0; i < nexprs; ++i )
   {
      SCIP_EXPR* newroot;
      SCIP_EXPR* newchild;
      SCIP_EXPR* child;

      /* check the root for equivalence separately first */
      SCIP_CALL( findEqualExpr(exprs[i], key2expr, &newroot) );

      if( newroot != NULL )
      {
         assert(newroot != exprs[i]);
         assert(SCIPexprCompare(exprs[i], newroot) == 0);

         SCIPdebugMsg(scip, "replacing common root expression of %dth expr: %p -> %p\n", i, (void*)exprs[i], (void*)newroot);

         SCIP_CALL( SCIPreleaseExpr(set->scip, &exprs[i]) );

         exprs[i] = newroot;
         SCIPcaptureExpr(newroot);

         *replacedroot = TRUE;

         continue;
      }

      /* replace equivalent sub-expressions in the tree */
      SCIP_CALL( SCIPexpriteratorInit(repliterator, exprs[i], SCIP_EXPRITER_DFS, FALSE) );
      SCIPexpriteratorSetStagesDFS(repliterator, SCIP_EXPRITER_VISITINGCHILD);

      while( !SCIPexpriteratorIsEnd(repliterator) )
      {
         child = SCIPexpriteratorGetChildExprDFS(repliterator);
         assert(child != NULL);

         /* try to find an equivalent expression */
         SCIP_CALL( findEqualExpr(child, key2expr, &newchild) );

         /* replace child with newchild */
         if( newchild != NULL )
         {
            assert(child != newchild);
            assert(SCIPexprCompare(child, newchild) == 0);

            SCIPdebugMsg(scip, "replacing common child expression %p -> %p\n", (void*)child, (void*)newchild);

            SCIP_CALL( SCIPreplaceExprChild(scip, SCIPexpriteratorGetCurrent(repliterator), SCIPexpriteratorGetChildIdxDFS(repliterator), newchild) );

            (void) SCIPexpriteratorSkipDFS(repliterator);
         }
         else
         {
            (void) SCIPexpriteratorGetNext(repliterator);
         }
      }
   }

   /* free memory */
   SCIPexpriteratorFree(&repliterator);
   SCIPmultihashFree(&key2expr);
   SCIPexpriteratorFree(&hashiterator);

   return SCIP_OKAY;
}

/** helper function to simplify an expression and its subexpressions */
static
SCIP_RETCODE simplifyConsExprExpr(
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat,               /**< dynamic problem statistics */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   SCIP_EXPR*            rootexpr,           /**< expression to be simplified */
   SCIP_EXPR**           simplified,         /**< buffer to store simplified expression */
   SCIP_Bool*            changed,            /**< buffer to store if rootexpr actually changed */
   SCIP_Bool*            infeasible          /**< buffer to store whether infeasibility has been detected */
   )
{
   SCIP_EXPR* expr;
   SCIP_EXPRITER* it;

   assert(set != NULL);
   assert(stat != NULL);
   assert(blkmem != NULL);
   assert(rootexpr != NULL);
   assert(simplified != NULL);
   assert(changed != NULL);
   assert(infeasible != NULL);

   /* simplify bottom up
    * when leaving an expression it simplifies it and stores the simplified expr in its iterators expression data
    * after the child was visited, it is replaced with the simplified expr
    */
   SCIP_CALL( SCIPexpriteratorCreate(stat, blkmem, &it) );
   SCIP_CALL( SCIPexpriteratorInit(it, rootexpr, SCIP_EXPRITER_DFS, TRUE) );  /* TODO can we set allowrevisited to FALSE?*/
   SCIPexpriteratorSetStagesDFS(it, SCIP_EXPRITER_VISITEDCHILD | SCIP_EXPRITER_LEAVEEXPR);

   *changed = FALSE;
   *infeasible = FALSE;
   for( expr = SCIPexpriteratorGetCurrent(it); !SCIPexpriteratorIsEnd(it); expr = SCIPexpriteratorGetNext(it) ) /*lint !e441*/
   {
      switch( SCIPexpriteratorGetStageDFS(it) )
      {
         case SCIP_EXPRITER_VISITEDCHILD:
         {
            SCIP_EXPR* newchild;
            SCIP_EXPR* child;

            newchild = (SCIP_EXPR*)SCIPexpriteratorGetChildUserDataDFS(it).ptrval;
            child = SCIPexpriteratorGetChildExprDFS(it);
            assert(newchild != NULL);

            /* if child got simplified, replace it with the new child */
            if( newchild != child )
            {
               SCIP_CALL( SCIPreplaceExprChild(scip, expr, SCIPexpriteratorGetChildIdxDFS(it), newchild) );
            }

            /* we do not need to hold newchild anymore */
            SCIP_CALL( SCIPreleaseExpr(scip, &newchild) );

            break;
         }

         case SCIP_EXPRITER_LEAVEEXPR:
         {
            SCIP_EXPR* refexpr = NULL;
            SCIP_EXPRITER_USERDATA iterdata;

            /* TODO we should do constant folding (handle that all children are value-expressions) here in a generic way
             * instead of reimplementing it in every handler
             */

            /* use simplification of expression handlers */
            if( SCIPexprhdlrHasSimplify(expr->exprhdlr) )
            {
               SCIP_CALL( SCIPcallExprhdlrSimplify(scip, conshdlr, expr, &refexpr) );
               if( expr != refexpr )
                  *changed = TRUE;
            }
            else
            {
               /* if an expression handler doesn't implement simplify, we assume all those type of expressions are simplified
                * we have to capture it, since it must simulate a "normal" simplified call in which a new expression is created
                */
               refexpr = expr;
               SCIPcaptureExpr(refexpr);
            }
            assert(refexpr != NULL);

            iterdata.ptrval = (void*) refexpr;
            SCIPexpriteratorSetCurrentUserData(it, iterdata);

            break;
         }

         default:
            SCIPABORT(); /* we should never be called in this stage */
            break;
      }
   }

   *simplified = (SCIP_EXPR*)SCIPexpriteratorGetExprUserData(it, rootexpr).ptrval;
   assert(*simplified != NULL);

   SCIPexpriteratorFree(&it);

   return SCIP_OKAY;
}

/** evaluate and forward-differentiate expression */
static
SCIP_RETCODE evalAndDiff(
   SCIP_SET*             set,                /**< global SCIP settings */
   SCIP_STAT*            stat,               /**< dynamic problem statistics */
   BMS_BLKMEM*           blkmem,             /**< block memory */
   SCIP_EXPR*            expr,               /**< expression to be evaluated */
   SCIP_SOL*             sol,                /**< solution to be evaluated */
   SCIP_Longint          soltag              /**< tag that uniquely identifies the solution (with its values), or 0. */
   )
{
   SCIP_EXPRITER* it;

   assert(set != NULL);
   assert(stat != NULL);
   assert(blkmem != NULL);
   assert(expr != NULL);

   /* assume we'll get a domain error, so we don't have to get this expr back if we abort the iteration
    * if there is no domain error, then we will overwrite the evalvalue in the last leaveexpr stage
    */
   expr->evalvalue = SCIP_INVALID;
   expr->evaltag = soltag;
   expr->dot = SCIP_INVALID;

   SCIP_CALL( SCIPexpriteratorCreate(stat, blkmem, &it) );
   SCIP_CALL( SCIPexpriteratorInit(it, expr, SCIP_EXPRITER_DFS, TRUE) );
   SCIPexpriteratorSetStagesDFS(it, SCIP_EXPRITER_LEAVEEXPR);

   for( expr = SCIPexpriteratorGetCurrent(it); !SCIPexpriteratorIsEnd(it); expr = SCIPexpriteratorGetNext(it) )  /*lint !e441*/
   {
      /* evaluate expression only if necessary */
      if( soltag == 0 || expr->evaltag != soltag )
      {
         SCIP_CALL( SCIPcallExprhdlrEval(set->scip, expr, &expr->evalvalue, NULL, sol) );

         expr->evaltag = soltag;
      }

      if( expr->evalvalue == SCIP_INVALID ) /*lint !e777*/
         break;

      /* compute forward diff */
      SCIP_CALL( SCIPcallExprhdlrFwdiff(set->scip, expr, &expr->dot) );

      if( expr->dot == SCIP_INVALID ) /*lint !e777*/
         break;
   }

   SCIPexpriteratorFree(&it);

   return SCIP_OKAY;
}
