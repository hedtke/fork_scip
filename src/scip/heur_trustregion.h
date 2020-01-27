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

/**@file   heur_trustregion.h
 * @ingroup PRIMALHEURISTICS
 * @brief  Large neighborhood search heuristic for Benders' decomposition based on trust region methods
 * @author Stephen J. Maher
 *
 * The Trust Region heuristic draws upon trust region methods for solving optimization problems, especially in the
 * context of Benders' decomposition. This heuristic has been developed to improve the heuristic performance of the
 * Benders' decomposition algorithm within SCIP.
 *
 * The Trust Region heuristic copies the original SCIP instance and adds a constraint to penalize changes from the
 * incumbent solution. Consider a problem that includes a set of binary variables \f$\mathcal{B}\f$. Given a feasible
 * solution \f$\hat{x}\f$ to the original problem, we define the set \f$\mathcal{B}^{+}\f$ as the index set for the
 * binary variables that are 1 in the input solution and \f$\mathcal{B}^{-}\f$ as the index set for binary variables
 * that are 0. The trust region constraint, which is added to the sub-SCIP, is given by
 *
 * \f[
 *    \sum_{i \in \mathcal{B}^{+}}(1 - x_{i}) + \sum_{i \in \mathcal{B}^{-}}x_{i} \le \theta
 * \f]
 *
 * The variable \f$\theta\f$ measure the distance, in terms of the binary variables, of candidate solutions to the input
 * solution.
 *
 * In addition, an upper bounding constraint is explicitly added to enforce a minimum improvement from the heuristic,
 * given by \f$f(x) \le f(\hat{x}) - \epsilon\f$. The parameter \f$\epsilon \ge 0\f$ denotes the minimum improvement
 * that must be achieved by the heuristic.
 *
 * The objective function is then modified to \f$f(x) + M\theta\f$, where \f$M\f$ is a parameter for penalizing the
 * distance of solutions from the input solution \f$\hat{x}\f$.
 *
 * If a new incumbent solution is found by this heuristic, then the Trust Region heuristic is immediately
 * re-executed with this new incumbent solution.
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_HEUR_TRUSTREGION_H__
#define __SCIP_HEUR_TRUSTREGION_H__

#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates local branching primal heuristic and includes it in SCIP
 *
 *  @ingroup PrimalHeuristicIncludes
 */
SCIP_EXPORT
SCIP_RETCODE SCIPincludeHeurTrustregion(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
