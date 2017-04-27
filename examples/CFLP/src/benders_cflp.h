/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2017 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   benders_cflp.h
 * @ingroup BENDERS
 * @brief  Benders' decomposition algorithm for the capacitated facility location problem
 * @author Stephen J. Maher
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_BENDERS_CFLP_H__
#define __SCIP_BENDERS_CFLP_H__


#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the cflp Benders' decomposition and includes it in SCIP
 *
 *  @ingroup BendersIncludes
 */
EXTERN
SCIP_RETCODE SCIPincludeBendersCflp(
   SCIP*                 scip,               /**< SCIP data structure */
   int                   nsubproblems        /**< the number of subproblems in the Benders' decomposition */
   );

/**@addtogroup BENDERSS
 *
 * @{
 */

/** TODO: add public methods to this group for documentation purposes */

/* @} */

#ifdef __cplusplus
}
#endif

#endif
