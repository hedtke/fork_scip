/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                           */
/*                  This file is part of the program and library             */
/*         SCIP --- Solving Constraint Integer Programs                      */
/*                                                                           */
/*    Copyright (C) 2002-2018 Konrad-Zuse-Zentrum                            */
/*                            fuer Informationstechnik Berlin                */
/*                                                                           */
/*  SCIP is distributed under the terms of the ZIB Academic License.         */
/*                                                                           */
/*  You should have received a copy of the ZIB Academic License              */
/*  along with SCIP; see the file COPYING. If not email to scip@zib.de.      */
/*                                                                           */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/**@file   circpacking.c
 * @brief  Packing circles in a rectangle of minimal size.
 * @author Jose Salmeron
 * @author Stefan Vigerske
 *
 * This example shows how to setup quadratic constraints in SCIP when using SCIP as callable library.
 * The example implements a model for the computation of a smallest rectangle that contains a number of
 * given circles in the plane or the computation of the maximal number of circles that can be placed
 * into a given rectangle.
 *
 * Given n circles with radii \f$r_i\f$, the task is to find a coordinates \f$(x_i, y_i)\f$ for the
 * circle midpoints and a minimal rectangle of width \f$W \geq 0\f$ and height \f$H \geq 0\f$, such
 * that every circle is places within the rectangle (\f$r_i \leq x_i \leq W-r_i\f$, \f$r_i \leq y_i \leq H-r_i\f$)
 * and circles are not overlapping \f$\left((x_i-x_j)^2 + (y_i-y_j)^2 \geq (r_i + r_j)^2\right)\f$.
 *
 * Alternatively, one may fix the width and the rectangle and maximize the number of circles that
 * can be fit into the rectangle without being overlapping.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "scip/scip.h"
#include "scip/scipdefplugins.h"

/* Model parameters */

/** Number of possible circles **/
#define nCircles 5

/** Radius **/
static const SCIP_Real r[] = {0.25, 0.25, 0.4, 0.7, 0.1};

/* Variables */
SCIP_VAR* x[nCircles]; /**< x coordinates */
SCIP_VAR* y[nCircles]; /**< y coordinates */
SCIP_VAR* b[nCircles]; /**< whether circle is placed into rectangle */
SCIP_VAR* a;           /**< area of rectangle */
SCIP_VAR* w;           /**< width of rectangle */
SCIP_VAR* h;           /**< height of rectangle */
SCIP_Bool minarea;     /**< whether we minimize the area (TRUE) or maximize the number of circles in the rectangle (FALSE) */


/** plots solution by use of Python/Matplotlib */
static
void visualizeSolutionMatplotlib(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOL*             sol                 /**< solution to plot */
)
{
#if _POSIX_C_SOURCE < 2
   SCIPinfoMessage(scip, NULL, "No POSIX version 2. Try http://distrowatch.com/.");
#else
   FILE* stream;
   int i;

   stream = popen("python", "w");
   if( stream == NULL )
   {
      SCIPerrorMessage("Could not open pipe to python.\n");
      return;
   }

   fputs("import numpy as np\n"
      "import matplotlib\n"
      "import matplotlib.pyplot as plt\n",
      stream);

   fputs("fig, ax = plt.subplots()\n"
      "patches = []\n",
      stream);

   for( i = 0; i < nCircles; ++i )
   {
      /* skip circles that are not included in current solution */
      if( !minarea && SCIPgetSolVal(scip, sol, b[i]) < 0.5 )
         continue;

      fprintf(stream, "patches.append(matplotlib.patches.Circle((%g, %g), %g))\n",
         SCIPgetSolVal(scip, sol, x[i]),
         SCIPgetSolVal(scip, sol, y[i]),
         r[i]);
   }

   fputs("colors = 100*np.random.rand(len(patches))\n"
       "p = matplotlib.collections.PatchCollection(patches, alpha=0.4)\n"
       "p.set_array(np.array(colors))\n"
       "ax.add_collection(p)\n",
       stream);

   fprintf(stream, "plt.xlim(xmax=%g)\n", SCIPgetSolVal(scip, sol, w));
   fprintf(stream, "plt.ylim(ymax=%g)\n", SCIPgetSolVal(scip, sol, h));
   if( minarea )
      fprintf(stream, "plt.title('Area = %.4f')\n", SCIPgetSolVal(scip, sol, a));
   else
      fprintf(stream, "plt.title('Number of circles = %d')\n", (int)SCIPround(scip, SCIPgetSolOrigObj(scip, sol)));
   fputs("plt.gca().set_aspect(1)\n", stream);
   fputs("plt.show()\n", stream);

   pclose(stream);
#endif
}

/** plots solution by use of gnuplot */
static
void visualizeSolutionGnuplot(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOL*             sol                 /**< solution to plot */
)
{
#if _POSIX_C_SOURCE < 2
   SCIPinfoMessage(scip, NULL, "No POSIX version 2. Try http://distrowatch.com/.");
#else
   SCIP_Real wval;
   SCIP_Real hval;
   FILE* stream;
   int i;

   /* -p (persist) to keep the plot open after gnuplot terminates */
   stream = popen("gnuplot -p", "w");
   if( stream == NULL )
   {
      SCIPerrorMessage("Could not open pipe to gnuplot.\n");
      return;
   }

   fputs("unset xtics\n"
      "unset ytics\n"
      "unset border\n"
      "set size ratio 1\n",
      stream);

   wval = SCIPgetSolVal(scip, sol, w);
   hval = SCIPgetSolVal(scip, sol, h);

   fprintf(stream, "set xrange [0:%.2f]\n", MAX(wval, hval));
   fprintf(stream, "set yrange [0:%.2f]\n", MAX(wval, hval));
   fprintf(stream, "set object rectangle from 0,0 to %.2f,%.2f\n", wval, hval);
   if( minarea )
      fprintf(stream, "set xlabel \"Area = %.4f\"\n", SCIPgetSolVal(scip, sol, a));
   else
      fprintf(stream, "set xlabel \"Number of circles = %d\"\n", (int)SCIPround(scip, SCIPgetSolOrigObj(scip, sol)));

   fputs("plot '-' with circles notitle\n", stream);
   for( i = 0; i < nCircles; ++i )
   {
      /* skip circles that are not included in current solution */
      if( !minarea && SCIPgetSolVal(scip, sol, b[i]) < 0.5 )
         continue;

      fprintf(stream, "%g %g %g\n",
         SCIPgetSolVal(scip, sol, x[i]),
         SCIPgetSolVal(scip, sol, y[i]),
         r[i]);
   }
   fputs("e\n", stream);

   pclose(stream);
#endif
}

/** plots solution by use of ascii graphics */
static
SCIP_RETCODE visualizeSolutionAscii(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_SOL*             sol                 /**< solution to plot */
)
{
   SCIP_Real wval;
   SCIP_Real hval;
   SCIP_Real xval;
   SCIP_Real yval;
   SCIP_Real radius;
   SCIP_Real scale;
   int dispwidth;
   int width;
   int height;
   char* picture;
   int i;

   wval = SCIPgetSolVal(scip, sol, w);
   hval = SCIPgetSolVal(scip, sol, h);

   /* scale so that picture is about as wide as SCIP B&B log */
   SCIP_CALL( SCIPgetIntParam(scip, "display/width", &dispwidth) );
   scale = (dispwidth-3) / wval;

   width = SCIPceil(scip, scale*wval)+3;  /* +2 for left and right border and +1 for \n */
   height = SCIPceil(scip, scale*hval)+2; /* +2 for top and bottom border */

   SCIP_CALL( SCIPallocBufferArray(scip, &picture, width * height + 1) );

   /* initialize with white space and termination */
   memset(picture, ' ', width * height);
   picture[width*height] = '\0';

   /* add border and linebreaks */
   memset(picture, '*', width-1); /* top border */
   memset(picture + (height-1) * width, '*', width-1);  /* bottom border */
   for( i = 0; i < height; ++i )
   {
      picture[i*width] = '*';  /* left border */
      picture[i*width + width-2] = '*';  /* right border */
      picture[i*width + width-1] = '\n';
   }

   /* add circles */
   for( i = 0; i < nCircles; ++i )
   {
      SCIP_Real phi;
      int xcoord;
      int ycoord;

      /* skip circles that are not included in current solution */
      if( !minarea && SCIPgetSolVal(scip, sol, b[i]) < 0.5 )
         continue;

      xval = SCIPgetSolVal(scip, sol, x[i]);
      yval = SCIPgetSolVal(scip, sol, y[i]);
      radius = r[i];

      for( phi = 0.0; phi < 2.0 * M_PI; phi += 0.01 )
      {
         xcoord = SCIPround(scip, scale * (xval + radius * cos(phi))) + 1; /* +1 for border */
         ycoord = SCIPround(scip, scale * (yval + radius * sin(phi))) + 1; /* +1 for border */

         /* feasible solutions should be within box
          * due to rounding, they can be on the border
          */
         assert(xcoord >= 0);
         assert(ycoord >= 0);
         assert(xcoord < width);
         assert(ycoord < height);

         picture[ycoord * width + xcoord] = 'a' + i;
      }
   }

   /* print objective value inside top border */
   i = SCIPsnprintf(picture + width/2 - 8, width/2 + 8,
      minarea ? " Area = %g " : " #Circles = %.0f ", SCIPgetSolOrigObj(scip, sol));
   picture[width/2-8+i] = '*';

   /* show plot */
   SCIPinfoMessage(scip, NULL, picture);

   SCIPfreeBufferArray(scip, &picture);

   return SCIP_OKAY;
}

/** initialization method of event handler (called after problem was transformed) */
static
SCIP_DECL_EVENTINIT(eventInitDispsol)
{
   SCIP_CALL( SCIPcatchEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, NULL, NULL) );

   return SCIP_OKAY;
}

/** deinitialization method of event handler (called before transformed problem is freed) */
static
SCIP_DECL_EVENTEXIT(eventExitDispsol)
{
   SCIP_CALL( SCIPdropEvent(scip, SCIP_EVENTTYPE_BESTSOLFOUND, eventhdlr, NULL, -1) );

   return SCIP_OKAY;
}

/** execution method of event handler */
static
SCIP_DECL_EVENTEXEC(eventExecDispsol)
{  /*lint --e{715}*/
   SCIP_SOL* sol;

   assert(SCIPeventGetType(event) == SCIP_EVENTTYPE_BESTSOLFOUND);

   sol = SCIPeventGetSol(event);
   assert(sol != NULL);

   SCIP_CALL( visualizeSolutionAscii(scip, sol) );

   return SCIP_OKAY;
}

/** creates event handler for dispsol event */
static
SCIP_RETCODE includeEventHdlrDispsol(
   SCIP*                 scip                /**< SCIP data structure */
   )
{
   SCIP_EVENTHDLR* eventhdlr = NULL;

   /* include event handler into SCIP */
   SCIP_CALL( SCIPincludeEventhdlrBasic(scip, &eventhdlr, "dispsol", "displays new solutions",
         eventExecDispsol, NULL) );
   assert(eventhdlr != NULL);

   /* set non fundamental callbacks via setter functions */
   SCIP_CALL( SCIPsetEventhdlrInit(scip, eventhdlr, eventInitDispsol) );
   SCIP_CALL( SCIPsetEventhdlrExit(scip, eventhdlr, eventExitDispsol) );

   return SCIP_OKAY;
}

/** sets up problem */
static SCIP_RETCODE setupProblem(
   SCIP*                 scip,               /**< SCIP data structure */
   SCIP_Real             fixwidth,           /**< a given fixed width for the rectangle, or SCIP_INVALID if not fixed */
   SCIP_Real             fixheight           /**< a given fixed height for the rectangle, or SCIP_INVALID if not fixed */
)
{
	SCIP_CONS* xrw[nCircles];
	SCIP_CONS* yrh[nCircles];
	SCIP_CONS* wha;
	SCIP_CONS* quad[nCircles][nCircles];

	int i, j;
	char name[SCIP_MAXSTRLEN];
	SCIP_Real one = 1.0;

	/* if both width and height are fixed, then we don't optimize the area anymore, but the number of circles */
   minarea = (fixwidth == SCIP_INVALID || fixheight == SCIP_INVALID);

	/* create empty problem */
	SCIP_CALL( SCIPcreateProbBasic(scip, "Packing circles") );

	/* change to maximization if optimizing number of circles instead of rectangle area */
	if( !minarea )
	{
	   SCIP_CALL( SCIPsetObjsense(scip, SCIP_OBJSENSE_MAXIMIZE) );
	}

	/* create variables */
	for( i = 0; i < nCircles; ++i )
	{
		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "x_%d", i);
		SCIP_CALL( SCIPcreateVarBasic(scip, &x[i], name, r[i], SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );

		(void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "y_%d", i);
		SCIP_CALL( SCIPcreateVarBasic(scip, &y[i], name, r[i], SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );

		if( !minarea )
		{
	      (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "b_%d", i);
	      SCIP_CALL( SCIPcreateVarBasic(scip, &b[i], name, 0.0, 1.0, 1.0, SCIP_VARTYPE_BINARY) );
		}
	}
	SCIP_CALL( SCIPcreateVarBasic(scip, &a, "a", 0.0, SCIPinfinity(scip), minarea ? 1.0 : 0.0, SCIP_VARTYPE_CONTINUOUS) );
	SCIP_CALL( SCIPcreateVarBasic(scip, &w, "w", 0.0, SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );
	SCIP_CALL( SCIPcreateVarBasic(scip, &h, "h", 0.0, SCIPinfinity(scip), 0.0, SCIP_VARTYPE_CONTINUOUS) );

	/* fix width if desired */
	if( fixwidth != SCIP_INVALID )
	{
	   SCIP_Bool infeas;
	   SCIP_Bool fixed;

	   SCIP_CALL( SCIPfixVar(scip, w, fixwidth, &infeas, &fixed) );

	   assert(!infeas);
	   assert(fixed);
	}

   /* fix height if desired */
   if( fixheight != SCIP_INVALID )
   {
      SCIP_Bool infeas;
      SCIP_Bool fixed;

      SCIP_CALL( SCIPfixVar(scip, h, fixheight, &infeas, &fixed) );

      assert(!infeas);
      assert(fixed);
   }

   /* fix area if both width and height are given */
   if( !minarea )
   {
      SCIP_Bool infeas;
      SCIP_Bool fixed;

      SCIP_CALL( SCIPfixVar(scip, a, fixwidth * fixheight, &infeas, &fixed) );

      assert(!infeas);
      assert(fixed);
   }

	/* add variables to problem */
	for( i = 0; i < nCircles; ++i )
	{
		SCIP_CALL( SCIPaddVar(scip, x[i]) );
		SCIP_CALL( SCIPaddVar(scip, y[i]) );
		if( !minarea )
		{
	      SCIP_CALL( SCIPaddVar(scip, b[i]) );
		}
	}
	SCIP_CALL( SCIPaddVar(scip, a) );
	SCIP_CALL( SCIPaddVar(scip, w) );
	SCIP_CALL( SCIPaddVar(scip, h) );

	/* circles must be within rectangle, if decided to be put into rectangle
	 * - lower and left bounds are already passed in by variable bounds
	 * - upper and right bounds need to be satisfied only if circles are placed into rectangle
	 * - however, for circles that are not placed into the rectangle, it is easy to satisfy the
	 *   same constraints as long as the rectangle is not too small
	 * - thus, if not minimizing area, we add these constraints on the circle coordinates,
	 *   unless the circle is too big, in which case ensure that it is not placed into the rectangle
	 */
	for( i = 0; i < nCircles; ++i )
	{
	   if( !minarea && SCIPisLT(scip, MIN(fixwidth, fixheight), 2*r[i]) )
	   {
         SCIP_Bool infeas;
	      SCIP_Bool fixed;

	      SCIP_CALL( SCIPfixVar(scip, b[i], 0.0, &infeas, &fixed) );

	      assert(!infeas);
	      assert(fixed);

	      xrw[i] = NULL;
	      yrh[i] = NULL;

	      continue;
	   }

	   /* linear constraint: x_i + r_i <= w --> r_i <= w - x_i */
	   (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "boundaryright_%d", i, i);
	   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &xrw[i], name, 0, NULL, NULL, r[i], SCIPinfinity(scip)) );
	   SCIP_CALL( SCIPaddCoefLinear(scip, xrw[i], w, 1.0) );
	   SCIP_CALL( SCIPaddCoefLinear(scip, xrw[i], x[i], -1.0) );

	   /* linear constraint: y_i + r_i <= h --> r_i <= h - y_i */
	   (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "boundarytop_%d", i, i);
	   SCIP_CALL( SCIPcreateConsBasicLinear(scip, &yrh[i], name, 0, NULL, NULL, r[i], SCIPinfinity(scip)) );
	   SCIP_CALL( SCIPaddCoefLinear(scip, yrh[i], h, 1.0) );
	   SCIP_CALL( SCIPaddCoefLinear(scip, yrh[i], y[i], -1.0) );
	}

	/* quadratic constraint: w * h <= a --> w * h - a <= 0 */
	SCIP_CALL( SCIPcreateConsBasicQuadratic(scip, &wha, "area", 0, NULL, NULL, 1, &w, &h, &one, -SCIPinfinity(scip), 0.0) );
	SCIP_CALL( SCIPaddLinearVarQuadratic(scip, wha, a, -1.0) );

	/* quadratic constraint: (x_i - x_j)^2 + (y_i - y_j)^2 >= (r_i + r_j)^2  (if minarea)
    *   x_i^2 - 2 x_i x_j + x_j^2 + y_i^2 - 2 y_i y_j + y_j^2 >= (r_i + r_j)^2
    * if not minarea, then rhs becomes (r_i + r_j)^2 * (b_i+b_j-1)
	 */
	for( i = 0; i < nCircles; ++i )
	{
	   for( j = i + 1; j < nCircles; ++j )
	   {
	      /* create empty quadratic constraint with right-hand-side +/- (r_i - r_j)^2 */
	      (void) SCIPsnprintf(name, SCIP_MAXSTRLEN, "nooverlap_%d,%d", i, j);
	      SCIP_CALL( SCIPcreateConsBasicQuadratic(scip, &quad[i][j], name, 0, NULL, NULL, 0, NULL, NULL, NULL, (minarea ? 1.0 : -1.0) * SQR(r[i] + r[j]), SCIPinfinity(scip)) );

	      SCIP_CALL( SCIPaddSquareCoefQuadratic(scip, quad[i][j], x[i], 1.0) ); /* x_i^2 */
         SCIP_CALL( SCIPaddSquareCoefQuadratic(scip, quad[i][j], x[j], 1.0) ); /* x_j^2 */
	      SCIP_CALL( SCIPaddBilinTermQuadratic(scip, quad[i][j], x[i], x[j], -2.0) ); /* - 2 x_i x_j */

	      SCIP_CALL( SCIPaddSquareCoefQuadratic(scip, quad[i][j], y[i], 1.0) ); /* y_i^2 */
         SCIP_CALL( SCIPaddSquareCoefQuadratic(scip, quad[i][j], y[j], 1.0) ); /* y_j^2 */
	      SCIP_CALL( SCIPaddBilinTermQuadratic(scip, quad[i][j], y[i], y[j], -2.0) ); /* - 2 y_i y_j */

	      if( !minarea )
	      {
	         /* add -(r_i+r_j)^2 (b_i + b_j) */
	         SCIP_CALL( SCIPaddLinearVarQuadratic(scip, quad[i][j], b[i], -SQR(r[i] + r[j])) );
            SCIP_CALL( SCIPaddLinearVarQuadratic(scip, quad[i][j], b[j], -SQR(r[i] + r[j])) );
	      }
	   }
	}

	/* add constraints to problem */
	for( i = 0; i < nCircles; ++i )
	{
	   if( xrw[i] != NULL )
	   {
	      SCIP_CALL( SCIPaddCons(scip, xrw[i]) );
	   }
	   if( yrh[i] != NULL )
	   {
	      SCIP_CALL( SCIPaddCons(scip, yrh[i]) );
	   }
	}

	SCIP_CALL( SCIPaddCons(scip, wha) );

	for( i = 0; i < nCircles; ++i )
	{
		for( j = i + 1; j < nCircles; ++j )
		{
			SCIP_CALL( SCIPaddCons(scip, quad[i][j]) );
		}
	}

	/* release constraints
	 * the problem has them captured, and we do not require them anymore
	 */
	for( i = 0; i < nCircles; ++i )
	{
	   if( xrw[i] != NULL )
	   {
	      SCIP_CALL( SCIPreleaseCons(scip, &xrw[i]) );
	   }
	   if( yrh[i] != NULL )
	   {
	      SCIP_CALL( SCIPreleaseCons(scip, &yrh[i]) );
	   }

      for( j = i + 1; j < nCircles; ++j )
      {
         SCIP_CALL( SCIPreleaseCons(scip, &quad[i][j]) );
      }
	}
	SCIP_CALL( SCIPreleaseCons(scip, &wha) );

	return SCIP_OKAY;
}

/* runs packing circles */
static SCIP_RETCODE runPacking(
   SCIP_Real             fixwidth,           /**< a given fixed width for the rectangle, or SCIP_INVALID if not fixed */
   SCIP_Real             fixheight,          /**< a given fixed height for the rectangle, or SCIP_INVALID if not fixed */
   SCIP_Bool             dognuplot,          /**< whether to draw best solution with gnuplot */
   SCIP_Bool             domatplotlib        /**< whether to draw best solution with python/matplotlib */
   )
{
	SCIP* scip;
	int i;

   SCIP_CALL( SCIPcreate(&scip) );
   SCIP_CALL( SCIPincludeDefaultPlugins(scip) );
   SCIP_CALL( includeEventHdlrDispsol(scip) );

   SCIPinfoMessage(scip, NULL, "\n");
   SCIPinfoMessage(scip, NULL, "***************************\n");
   SCIPinfoMessage(scip, NULL, "* Running Packing Circles *\n");
   SCIPinfoMessage(scip, NULL, "***************************\n");
   SCIPinfoMessage(scip, NULL, "\n");

	SCIP_CALL( setupProblem(scip, fixwidth, fixheight) );

	SCIPinfoMessage(scip, NULL, "Original problem:\n");
	SCIP_CALL( SCIPprintOrigProblem(scip, NULL, "cip", FALSE) );

	/* closing the last bit of the gap can take very long */
	SCIP_CALL( SCIPsetRealParam(scip, "limits/gap", 1e-4) );

	SCIPinfoMessage(scip, NULL, "\nSolving...\n");
	SCIP_CALL( SCIPsolve(scip) );

	if( SCIPgetNSols(scip) > 0 )
	{
		SCIPinfoMessage(scip, NULL, "\nSolution:\n");
		SCIPinfoMessage(scip, NULL, "Name: Packing Circles\n");
		SCIPinfoMessage(scip, NULL, "N %d\n", nCircles);
		SCIPinfoMessage(scip, NULL, "r ");
		for( i = 0; i < nCircles; ++i )
		{
			SCIPinfoMessage(scip, NULL, "%f ", r[i]);
		}
		SCIPinfoMessage(scip, NULL, "\n");
		SCIP_CALL( SCIPprintSol(scip, SCIPgetBestSol(scip), NULL, FALSE) );

		if( dognuplot )
	      visualizeSolutionGnuplot(scip, SCIPgetBestSol(scip));

		if( domatplotlib )
		   visualizeSolutionMatplotlib(scip, SCIPgetBestSol(scip));
	}

   /* release variables */
   SCIP_CALL( SCIPreleaseVar(scip, &a) );
   SCIP_CALL( SCIPreleaseVar(scip, &w) );
   SCIP_CALL( SCIPreleaseVar(scip, &h) );
   for( i = 0; i < nCircles; ++i )
   {
      SCIP_CALL( SCIPreleaseVar(scip, &x[i]) );
      SCIP_CALL( SCIPreleaseVar(scip, &y[i]) );
      if( !minarea )
      {
         SCIP_CALL( SCIPreleaseVar(scip, &b[i]) );
      }
   }

	SCIP_CALL( SCIPfree(&scip) );

	return SCIP_OKAY;
}

/** main method starting SCIP */
int main(
   int                        argc,          /**< number of arguments from the shell */
   char**                     argv           /**< array of shell arguments */
   )  /*lint --e{715}*/
{
   SCIP_RETCODE retcode;
   SCIP_Bool dognuplot = FALSE;
   SCIP_Bool domatplotlib = FALSE;
   SCIP_Real fixwidth = SCIP_INVALID;
   SCIP_Real fixheight = SCIP_INVALID;
   int i;

   for( i = 1; i < argc; ++i )
   {
      if( strcmp(argv[i], "--help") == 0 )
      {
         printf("usage: %s [--help] [-w <width>] [-h <height>]", argv[0]);
#if _POSIX_C_SOURCE >= 2
         printf(" [-g] [-m]");
#endif
         printf("\n");
         printf("  --help shows this help and exits\n");
         printf("  -w <width> fix rectangle width to given value\n");
         printf("  -h <height> fix rectangle height to given value\n");
#if _POSIX_C_SOURCE >= 2
         printf("  -g show final solution with gnuplot\n");
         printf("  -m show final solution with matplotlib\n");
#endif

         return EXIT_SUCCESS;
      }

#if _POSIX_C_SOURCE >= 2
      if( strcmp(argv[i], "-g") == 0 )
      {
         dognuplot = TRUE;
         continue;
      }

      if( strcmp(argv[i], "-m") == 0 )
      {
         domatplotlib = TRUE;
         continue;
      }
#endif

      if( strcmp(argv[i], "-w") == 0 )
      {
         char* endptr;

         if( i == argc-1 )
         {
            fprintf(stderr, "ERROR: Missing argument for -w.\n");
            return EXIT_FAILURE;
         }

         fixwidth = strtod(argv[i+1], &endptr);
         if( *endptr != '\0' )
         {
            fprintf(stderr, "ERROR: Could not parse argument %s into a number.\n", argv[i+1]);
            return EXIT_FAILURE;
         }

         ++i;
         continue;
      }

      if( strcmp(argv[i], "-h") == 0 )
      {
         char* endptr;

         if( i == argc-1 )
         {
            fprintf(stderr, "ERROR: Missing argument for -h.\n");
            return EXIT_FAILURE;
         }

         fixheight = strtod(argv[i+1], &endptr);
         if( *endptr != '\0' )
         {
            fprintf(stderr, "ERROR: Could not parse argument %s into a number.\n", argv[i+1]);
            return EXIT_FAILURE;
         }

         ++i;
         continue;
      }

      fprintf(stderr, "ERROR: Unknown option %s.\n", argv[i]);
      return EXIT_FAILURE;
   }

   retcode = runPacking(fixwidth, fixheight, dognuplot, domatplotlib);

   /* evaluate return code of the SCIP process */
   if( retcode != SCIP_OKAY )
   {
      /* write error back trace */
      SCIPprintError(retcode);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}
