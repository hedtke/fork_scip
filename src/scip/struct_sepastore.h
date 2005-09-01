/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2005 Tobias Achterberg                              */
/*                                                                           */
/*                  2002-2005 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: struct_sepastore.h,v 1.18 2005/09/01 18:19:21 bzfpfend Exp $"

/**@file   struct_sepastore.h
 * @brief  datastructures for storing separated cuts
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_STRUCT_SEPASTORE_H__
#define __SCIP_STRUCT_SEPASTORE_H__


#include "scip/def.h"
#include "scip/type_lp.h"
#include "scip/type_var.h"
#include "scip/type_sepastore.h"


/** storage for separated cuts
 *  cuts with only one variable are treated specifically as bound changes, and they are not counted for exceeding the
 *  separation limit
 */
struct SCIP_SepaStore
{
   SCIP_ROW**            cuts;               /**< array with separated cuts sorted by score */
   SCIP_Real*            efficacies;         /**< efficacy for each separated cut: distance of hyperplane to LP solution */
   SCIP_Real*            objparallelisms;    /**< parallelism of cut to the objective function */
   SCIP_Real*            orthogonalities;    /**< minimal orthogonality of cut with all other cuts of larger score */
   SCIP_Real*            scores;             /**< score for each separated cut: weighted sum of efficacy and orthogonality */
   SCIP_VAR**            bdchgvars;          /**< variables to change bound for */
   SCIP_Real*            bdchgvals;          /**< new bounds of variables */
   SCIP_BOUNDTYPE*       bdchgtypes;         /**< type of bounds to change */
   int                   cutssize;           /**< size of cuts and score arrays */
   int                   ncuts;              /**< number of separated cuts (max. is set->sepa_maxcuts) */
   int                   nforcedcuts;        /**< number of forced separated cuts */
   int                   bdchgssize;         /**< size of bdchgvars, bdchgvals, and bdchgtypes arrays */
   int                   nbdchgs;            /**< number of bound changes */
   int                   ncutsfound;         /**< total number of cuts (and bound changes) found so far */
   int                   ncutsfoundround;    /**< number of cuts (and bound changes) found so far in this separation round */
   int                   ncutsstored;        /**< total number of cuts stored (and possibly removed again) in this round */
   int                   ncutsapplied;       /**< total number of cuts (and bound changes) applied to the LPs */
   SCIP_Bool             initiallp;          /**< is the separation storage currently being filled with the initial LP rows? */
   SCIP_Bool             forcecuts;          /**< should the cuts be used despite the number of cuts parameter limit? */
};


#endif
