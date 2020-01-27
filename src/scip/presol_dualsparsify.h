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

/**@file   presol_dualsparsify.h
 * @brief  cancel nonzeros of the constraint matrix based on the columns
 * @author Dieter Weninger
 * @author Leona Gottwald
 * @author Ambros Gleixner
 * @author Weikun Chen
 *
 * This presolver attempts to cancel non-zero entries of the constraint
 * matrix by adding scaled columns to other columns.
 *
 * In more detail, for two columns A_{j.} and A_{k.}, suppose for a given value s, we have
 *
 *                  | A_{j.} | - | A_{j.} - s*A_{k.} | > eta,
 *
 * where eta is an nonnegative integer. Then we introduce a new variable y := s*x_j + x_k
 * and aggregate the variable x_k = y - s*x_j. After aggregation, the column of the variable
 * x_j is A_{j.} + s*A_{j.} which is sparser than A_{j.}. In the case that x_k is no implied
 * free variable, we need to add a new constraint l_k <= y - weight*x_j <= u_k into the problem
 * to keep the bounds constraints of variable x_k.
 *
 * Further information can be found in
 * Chen et al. "Two-row and two-column mixed-integer presolve using hasing-based pairing methods".
 *
 * @todo add infrastructure to SCIP for handling aggregated binary variables
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_PRESOL_DUALSPARSIFY_H__
#define __SCIP_PRESOL_DUALSPARSIFY_H__

#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the dual sparsify presolver and includes it in SCIP */
SCIP_EXPORT
SCIP_RETCODE SCIPincludePresolDualsparsify(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
