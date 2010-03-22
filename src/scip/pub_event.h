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
#pragma ident "@(#) $Id: pub_event.h,v 1.12.2.2 2010/03/22 16:05:32 bzfwolte Exp $"

/**@file   pub_event.h
 * @ingroup PUBLICMETHODS
 * @brief  public methods for managing events
 * @author Tobias Achterberg
 */

/*---+----1----+----2----+----3----+----4----+----5----+----6----+----7----+----8----+----9----+----0----+----1----+----2*/

#ifndef __SCIP_PUB_EVENT_H__
#define __SCIP_PUB_EVENT_H__


#include "scip/def.h"
#include "scip/type_retcode.h"
#include "scip/type_event.h"
#include "scip/type_var.h"
#include "scip/type_sol.h"
#include "scip/type_tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Event handler methods
 */

/** gets name of event handler */
extern
const char* SCIPeventhdlrGetName(
   SCIP_EVENTHDLR*       eventhdlr           /**< event handler */
   );

/** gets user data of event handler */
extern
SCIP_EVENTHDLRDATA* SCIPeventhdlrGetData(
   SCIP_EVENTHDLR*       eventhdlr           /**< event handler */
   );

/** sets user data of event handler; user has to free old data in advance! */
extern
void SCIPeventhdlrSetData(
   SCIP_EVENTHDLR*       eventhdlr,          /**< event handler */
   SCIP_EVENTHDLRDATA*   eventhdlrdata       /**< new event handler user data */
   );

/** is event handler initialized? */
extern
SCIP_Bool SCIPeventhdlrIsInitialized(
   SCIP_EVENTHDLR*       eventhdlr           /**< event handler */
   );



/*
 * Event methods
 */

/** gets type of event */
extern
SCIP_EVENTTYPE SCIPeventGetType(
   SCIP_EVENT*           event               /**< event */
   );

/** gets variable for a variable event (var added, var deleted, var fixed, objective value or domain change) */
extern
SCIP_VAR* SCIPeventGetVar(
   SCIP_EVENT*           event               /**< event */
   );

/** gets old objective value for an objective value change event */
extern
SCIP_Real SCIPeventGetOldobj(
   SCIP_EVENT*           event               /**< event */
   );

/** gets new objective value for an objective value change event */
extern
SCIP_Real SCIPeventGetNewobj(
   SCIP_EVENT*           event               /**< event */
   );

/** gets old bound for a bound change event */
extern
SCIP_Real SCIPeventGetOldbound(
   SCIP_EVENT*           event               /**< event */
   );

/** gets new bound for a bound change event */
extern
SCIP_Real SCIPeventGetNewbound(
   SCIP_EVENT*           event               /**< event */
   );

/** gets node for a node or LP event */
extern
SCIP_NODE* SCIPeventGetNode(
   SCIP_EVENT*           event               /**< event */
   );

/** gets solution for a primal solution event */
extern
SCIP_SOL* SCIPeventGetSol(
   SCIP_EVENT*           event               /**< event */
   );

#ifdef __cplusplus
}
#endif

#endif
