/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2004 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2004 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the SCIP Academic Licence.        */
/*                                                                           */
/*  You should have received a copy of the SCIP Academic License             */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: struct_lp.h,v 1.21 2004/08/06 08:18:03 bzfpfend Exp $"

/**@file   struct_lp.h
 * @brief  datastructures for LP management
 * @author Tobias Achterberg
 *
 *  In SCIP, the LP is defined as follows:
 *
 *   min       obj * x
 *      lhs <=   A * x + const <= rhs
 *      lb  <=       x         <= ub
 *
 *  The row activities are defined as 
 *     activity = A * x + const
 *  and must therefore be in the range of [lhs,rhs].
 *
 *  The reduced costs are defined as
 *     redcost = obj - A^T * y
 *  and must be   nonnegative, if the corresponding lb is nonnegative,
 *                zero,        if the corresponging lb is negative.
 *
 *  The main datastructures for storing an LP are the rows and the columns.
 *  A row can live on its own (if it was created by a separator), or as LP
 *  relaxation of a constraint. Thus, it has a nuses-counter, and is
 *  deleted, if not needed any more.
 *  A column cannot live on its own. It is always connected to a problem
 *  variable. Because pricing is always problem specific, it cannot create
 *  LP columns without introducing new variables. Thus, each column is
 *  connected to exactly one variable, and is deleted, if the variable
 *  is deleted.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __STRUCT_LP_H__
#define __STRUCT_LP_H__


#include "def.h"
#include "type_lpi.h"
#include "type_lp.h"
#include "type_var.h"



/** LP column;
 *  The row vector of the LP column is partitioned into two parts: The first col->nlprows rows in the rows array
 *  are the ones that belong to the current LP (col->rows[j]->lppos >= 0) and that are linked to the column
 *  (col->linkpos[j] >= 0). The remaining col->len - col->nlprows rows in the rows array are the ones that
 *  don't belong to the current LP (col->rows[j]->lppos == -1) or that are not linked to the column
 *  (col->linkpos[j] == -1).
 */
struct Col
{
   Real             obj;                /**< current objective value of column in LP */
   Real             lb;                 /**< current lower bound of column in LP */
   Real             ub;                 /**< current upper bound of column in LP */
   Real             primsol;            /**< primal solution value in LP, is 0 if col is not in LP */
   Real             redcost;            /**< reduced cost value in LP, or SCIP_INVALID if not yet calculated */
   Real             farkascoef;         /**< coefficient in dual farkas infeasibility proof (== dualfarkas^T A_c) */
   Real             minprimsol;         /**< minimal LP solution value, this column ever assumed */
   Real             maxprimsol;         /**< maximal LP solution value, this column ever assumed */
   Real             strongbranchdown;   /**< strong branching information for downwards branching */
   Real             strongbranchup;     /**< strong branching information for upwards branching */
   Real             strongbranchsolval; /**< LP solution value of column at last strong branching call */
   Real             strongbranchlpobjval;/**< LP objective value at last strong branching call on the column */
   Longint          strongbranchnode;   /**< node number of the last strong branching call on this column */
   Longint          obsoletenode;       /**< last node where this column was removed due to aging */
   VAR*             var;                /**< variable, this column represents; there cannot be a column without variable */
   ROW**            rows;               /**< rows of column entries, that may have a nonzero dual solution value */
   Real*            vals;               /**< coefficients of column entries */
   int*             linkpos;            /**< position of col in col vector of the row, or -1 if not yet linked */
   int              index;              /**< consecutively numbered column identifier */
   int              size;               /**< size of the row- and val-arrays */
   int              len;                /**< number of nonzeros in column */
   int              nlprows;            /**< number of linked rows in column, that belong to the current LP */
   int              nunlinked;          /**< number of column entries, where the rows don't know about the column */
   int              lppos;              /**< column position number in current LP, or -1 if not in current LP */
   int              lpipos;             /**< column position number in LP solver, or -1 if not in LP solver */
   int              validredcostlp;     /**< LP number for which reduced cost value is valid */
   int              validfarkaslp;      /**< LP number for which farkas coefficient is valid */
   int              validstrongbranchlp;/**< LP number for which strong branching values are valid */
   int              strongbranchitlim;  /**< strong branching iteration limit used to get strongbranch values, or -1 */
   int              age;                /**< number of successive times this variable was in LP and was 0.0 in solution */
   int              var_probindex;      /**< copy of var->probindex for avoiding expensive dereferencing */
   unsigned int     lprowssorted:1;     /**< are the linked LP rows in the rows array sorted by non-decreasing index? */
   unsigned int     nonlprowssorted:1;  /**< are the non-LP/not linked rows sorted by non-decreasing index? */
   unsigned int     objchanged:1;       /**< has objective value changed, and has data of LP solver to be updated? */
   unsigned int     lbchanged:1;        /**< has lower bound changed, and has data of LP solver to be updated? */
   unsigned int     ubchanged:1;        /**< has upper bound changed, and has data of LP solver to be updated? */
   unsigned int     coefchanged:1;      /**< has the coefficient vector changed, and has LP solver to be updated? */
   unsigned int     integral:1;         /**< is associated variable of integral type? */
   unsigned int     removeable:1;       /**< is column removeable from the LP (due to aging or cleanup)? */
};

/** LP row
 *  The column vector of the LP row is partitioned into two parts: The first row->nlpcols columns in the cols array
 *  are the ones that belong to the current LP (row->cols[j]->lppos >= 0) and that are linked to the row   
 *  (row->linkpos[j] >= 0). The remaining row->len - row->nlpcols columns in the cols array are the ones that
 *  don't belong to the current LP (row->cols[j]->lppos == -1) or that are not linked to the row   
 *  (row->linkpos[j] == -1).
 */
struct Row
{
   Real             constant;           /**< constant shift c in row lhs <= ax + c <= rhs */
   Real             lhs;                /**< left hand side of row */
   Real             rhs;                /**< right hand side of row */
   Real             sqrnorm;            /**< squared euclidean norm of row vector */
   Real             maxval;             /**< maximal absolute value of row vector, only valid if nummaxval > 0 */
   Real             minval;             /**< minimal absolute non-zero value of row vector, only valid if numminval > 0 */
   Real             dualsol;            /**< dual solution value in LP, is 0 if row is not in LP */
   Real             activity;           /**< row activity value in LP, or SCIP_INVALID if not yet calculated */
   Real             dualfarkas;         /**< multiplier value in dual farkas infeasibility proof */
   Real             pseudoactivity;     /**< row activity value in pseudo solution, or SCIP_INVALID if not yet calculated */
   Real             minactivity;        /**< minimal activity value w.r.t. the column's bounds, or SCIP_INVALID */
   Real             maxactivity;        /**< maximal activity value w.r.t. the column's bounds, or SCIP_INVALID */
   Longint          validpsactivitydomchg; /**< domain change number for which pseudo activity value is valid */
   Longint          validactivitybdsdomchg;/**< domain change number for which activity bound values are valid */
   Longint          obsoletenode;       /**< last node where this row was removed due to aging */
   char*            name;               /**< name of the row */
   COL**            cols;               /**< columns of row entries, that may have a nonzero primal solution value */
   int*             cols_probindex;     /**< copy of cols[i]->var->probindex for avoiding expensive dereferencing */
   Real*            vals;               /**< coefficients of row entries */
   int*             linkpos;            /**< position of row in row vector of the column, or -1 if not yet linked */
   int              index;              /**< consecutively numbered row identifier */
   int              size;               /**< size of the col- and val-arrays */
   int              len;                /**< number of nonzeros in row */
   int              nlpcols;            /**< number of linked columns in row, that belong to the current LP */
   int              nunlinked;          /**< number of row entries, where the columns don't know about the row */
   int              nuses;              /**< number of times, this row is referenced */
   int              lppos;              /**< row position number in current LP, or -1 if not in current LP */
   int              lpipos;             /**< row position number in LP solver, or -1 if not in LP solver */
   int              minidx;             /**< minimal column index of row entries */
   int              maxidx;             /**< maximal column index of row entries */
   int              nummaxval;          /**< number of coefs with absolute value equal to maxval, zero if maxval invalid */
   int              numminval;          /**< number of coefs with absolute value equal to minval, zero if minval invalid */
   int              validactivitylp;    /**< LP number for which activity value is valid */
   int              age;                /**< number of successive times this row was in LP and was not sharp in solution */
   unsigned int     lpcolssorted:1;     /**< are the linked LP columns in the cols array sorted by non-decreasing index? */
   unsigned int     nonlpcolssorted:1;  /**< are the non-LP/not linked columns sorted by non-decreasing index? */
   unsigned int     delaysort:1;        /**< should the row sorting be delayed and done in a lazy fashion? */
   unsigned int     validminmaxidx:1;   /**< are minimal and maximal column index valid? */
   unsigned int     lhschanged:1;       /**< was left hand side or constant changed, and has LP solver to be updated? */
   unsigned int     rhschanged:1;       /**< was right hand side or constant changed, and has LP solver to be updated? */
   unsigned int     coefchanged:1;      /**< was the coefficient vector changed, and has LP solver to be updated? */
   unsigned int     integral:1;         /**< is activity (without constant) of row always integral in feasible solution? */
   unsigned int     local:1;            /**< is row only valid locally? */
   unsigned int     modifiable:1;       /**< is row modifiable during node processing (subject to column generation)? */
   unsigned int     removeable:1;       /**< is row removeable from the LP (due to aging or cleanup)? */
   unsigned int     nlocks:24;          /**< number of sealed locks of an unmodifiable row */
};

/** current LP data */
struct Lp
{
   Real             lpobjval;           /**< objective value of LP without loose variables, or SCIP_INVALID */
   Real             looseobjval;        /**< current solution value of all loose variables set to their best bounds,
                                         *   ignoring variables, with infinite best bound */
   Real             pseudoobjval;       /**< current pseudo solution value with all variables set to their best bounds,
                                         *   ignoring variables, with infinite best bound */
   Real             cutoffbound;        /**< upper objective limit of LP (copy of primal->cutoffbound) */
   Real             lpiuobjlim;         /**< current upper objective limit in LPI */
   Real             lpifeastol;         /**< current feasibility tolerance in LPI */
   Real             lpidualfeastol;     /**< current reduced costs feasibility tolerance in LPI */
   LPI*             lpi;                /**< LP solver interface */
   COL**            lpicols;            /**< array with columns currently stored in the LP solver */
   ROW**            lpirows;            /**< array with rows currently stored in the LP solver */
   COL**            chgcols;            /**< array of changed columns not yet applied to the LP solver */
   ROW**            chgrows;            /**< array of changed rows not yet applied to the LP solver */
   COL**            cols;               /**< array with current LP columns in correct order */
   ROW**            rows;               /**< array with current LP rows in correct order */
   LPISTATE*        divelpistate;       /**< stores LPI state (basis information) before diving starts */
   int              lpicolssize;        /**< available slots in lpicols vector */
   int              nlpicols;           /**< number of columns in the LP solver */
   int              lpifirstchgcol;     /**< first column of the LP which differs from the column in the LP solver */
   int              lpirowssize;        /**< available slots in lpirows vector */
   int              nlpirows;           /**< number of rows in the LP solver */
   int              lpifirstchgrow;     /**< first row of the LP which differs from the row in the LP solver */
   int              chgcolssize;        /**< available slots in chgcols vector */
   int              nchgcols;           /**< current number of chgcols (number of used slots in chgcols vector) */
   int              chgrowssize;        /**< available slots in chgrows vector */
   int              nchgrows;           /**< current number of chgrows (number of used slots in chgrows vector) */
   int              colssize;           /**< available slots in cols vector */
   int              ncols;              /**< current number of LP columns (number of used slots in cols vector) */
   int              nremoveablecols;    /**< number of removeable columns in the LP */
   int              firstnewcol;        /**< first column added at the active node */
   int              rowssize;           /**< available slots in rows vector */
   int              nrows;              /**< current number of LP rows (number of used slots in rows vector) */
   int              nremoveablerows;    /**< number of removeable rows in the LP */
   int              firstnewrow;        /**< first row added at the active node */
   int              looseobjvalinf;     /**< number of loose variables with infinite best bound in current solution */
   int              nloosevars;         /**< number of loose variables in LP */
   int              pseudoobjvalinf;    /**< number of variables with infinite best bound in current pseudo solution */
   int              validsollp;         /**< LP number for which the currently stored solution values are valid */
   int              validfarkaslp;      /**< LP number for which the currently stored farkas row multipliers are valid */
   int              lpiitlim;           /**< current iteration limit setting in LPI */
   LPSOLSTAT        lpsolstat;          /**< solution status of last LP solution */
   Bool             flushdeletedcols;   /**< have LPI-columns been deleted in the last lpFlush() call? */
   Bool             flushaddedcols;     /**< have LPI-columns been added in the last lpFlush() call? */
   Bool             flushdeletedrows;   /**< have LPI-rows been deleted in the last lpFlush() call? */
   Bool             flushaddedrows;     /**< have LPI-rows been added in the last lpFlush() call? */
   Bool             flushed;            /**< are all cached changes applied to the LP solver? */
   Bool             solved;             /**< is current LP solved? */
   Bool             primalfeasible;     /**< is current LP basis primal feasible? */
   Bool             dualfeasible;       /**< is current LP basis dual feasible? */
   Bool             diving;             /**< LP is used for diving: col bounds and obj don't corresond to variables */
   Bool             divingobjchg;       /**< objective values were changed in diving: LP objective is invalid */
   Bool             lpifromscratch;     /**< current FROMSCRATCH setting in LPI */
   Bool             lpifastmip;         /**< current FASTMIP setting in LPI */
   Bool             lpiscaling;         /**< current SCALING setting in LPI */
   Bool             lpilpinfo;          /**< current LPINFO setting in LPI */
   Bool             lastwasprimal;      /**< was the last simplex call a call to the primal simplex? */
};


#endif
