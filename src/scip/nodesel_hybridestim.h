/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2010 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#pragma ident "@(#) $Id: nodesel_hybridestim.h,v 1.2.2.2 2010/03/22 16:05:29 bzfwolte Exp $"

/**@file   nodesel_hybridestim.h
 * @brief  node selector for hybrid best estimate / best bound search
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_NODESEL_HYBRIDESTIM_H__
#define __SCIP_NODESEL_HYBRIDESTIM_H__


#include "scip/scip.h"

#ifdef __cplusplus
extern "C" {
#endif

/** creates the node selector for hybrid best estim / best bound search and includes it in SCIP */
extern
SCIP_RETCODE SCIPincludeNodeselHybridestim(
   SCIP*                 scip                /**< SCIP data structure */
   );

#ifdef __cplusplus
}
#endif

#endif
