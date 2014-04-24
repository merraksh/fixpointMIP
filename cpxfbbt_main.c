/*
 * Fix point FBBT as a cutting plane in Cplex
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cplex.h"
#include "cmdline.h"

int fixpointfbbt (CPXCENVptr env,
		  void *cbdata,
		  int wherefrom,
		  void *cbhandle,
		  int *useraction_p);

int main (int argc, char **argv) {

  int status, i;

  CPXENVptr env = CPXopenCPLEX (&status);

  double maxTime;

  char
    addcuts = 0,
    ifHelp  = 0;

  tpar options [] = {{ 'f',  CSTR() "fixpt",      0, &addcuts,    TTOGGLE, CSTR() "add fixpoint FBBT (default: off)"}
		     ,{'t',  CSTR() "maxtime",   -1, &maxTime,    TDOUBLE, CSTR() "Maximum CPU time (default: no limit)"}
		     ,{'h',  CSTR() "help",       0, &ifHelp,     TTOGGLE, CSTR() "Print this help and exit"}
		     ,{0,    CSTR() "",           0, NULL,        TTOGGLE, CSTR() ""} // THIS ENTRY ALWAYS AT THE END
  };

  char **filenames;

  set_default_args (options); // default parameter values

  filenames = readargs (argc, argv, options);  // parse command line

  if (ifHelp || (argc < 1) || !filenames)
    print_help (argv [0], options);

  if (ifHelp || !filenames) {

    if (!ifHelp) 
      printf ("No file specified, exiting\n");      

    for (i=0; filenames [i]; ++i)
      free (filenames [i]);
    free (filenames);

    return 0;
  }

  /* Turn on output to the screen */

  if (maxTime > 0)
    status = CPXsetdblparam (env, CPX_PARAM_TILIM,  maxTime);

  status = CPXsetintparam (env, CPX_PARAM_SCRIND, CPX_ON);
  status = CPXsetintparam (env, CPX_PARAM_PREIND, CPX_OFF); // turn off presolve. TODO: restore presolve

  CPXLPptr mip = CPXcreateprob (env, &status, "cpx+fbbt");

  if (mip == NULL) {
    printf ("CPXcreateprob, Failed to create LP1, error code %d\n", status);
    for (i=0; filenames [i]; ++i)
      free (filenames [i]);
    free (filenames);
    exit (-1);
  }

  status = CPXreadcopyprob (env, mip, *filenames, NULL); /* Read MIP from file */

  if (addcuts)
    status = CPXsetusercutcallbackfunc (env, fixpointfbbt, NULL);
  
  /*
    status = CPXsetintparam (env, CPX_PARAM_MIPCBREDLP,
    CPX_OFF); //UNNECESSARY -- Let MIP callbacks work on the original
    model, otherwise CPLEX works on the presolved model

    status = CPXsetintparam (env, CPX_PARAM_MIPSEARCH,
    CPX_MIPSEARCH_TRADITIONAL); Turn on traditional search for use
    with control callbacks
  */

  status = CPXmipopt (env, mip); /* Optimize the problem and obtain solution */

  if (status)
    printf ("Failed to optimize MIP, error code %d\n", status);
  else {

    double objval, *x;
    int ncols, i, nprint = 0;

    status = CPXgetobjval (env, mip, &objval);

    if (!status) {
      printf ("Solution value  = %f\n", objval);

      ncols = CPXgetnumcols (env, mip);

      x = (double *) malloc (ncols * sizeof (double));

      status = CPXgetx (env, mip, x, 0, ncols-1);

      for (i=0; i<ncols; ++i) {
	if (fabs (x [i]) > 1e-6) {
	  printf ("(%d,%g) ", i, x [i]);
	  if (!(++nprint % 10)) 
	    printf ("\n");
	}
      }

      if (++nprint % 10) 
	printf ("\n");

      free (x);
    } else 
      printf ("Could not retrieve solution");
  }

  // print first part of the output line

  {
    struct rusage usage;

    double lb, ub;

    int status;

    char
      *summary  = "unknown",
      ubs [50] = "inf";

    CPXgetobjval     (env, mip, &ub);
    CPXgetbestobjval (env, mip, &lb);

    if (getrusage (RUSAGE_SELF, &usage)) {
      fprintf (stderr, "Error: can't run getrusage\n");
      exit (-1);
    }

    printf ("Stats: root,0.%d,%s,%d,%d,%d,-1,-1,-1,-1,%g,-1,%g,%g,%d,-1,-1,-1,-1,-1,-1,-1,", 
	    addcuts,
	    *filenames,
	    CPXgetnumcols (env, mip),
	    CPXgetnumint  (env, mip) +
	    CPXgetnumbin  (env, mip),
	    CPXgetnumrows (env, mip),
	    usage.ru_utime.tv_sec + (double) usage.ru_utime.tv_usec * 1.e-6,
	    lb,
	    ub,
	    CPXgetnodecnt (env, mip));

    // print second part

    fixpointfbbt (env, NULL, 0, NULL, NULL);

    // print final two strings

    CPXgetobjval (env, mip, &ub);

    if (ub < 1e19)
      sprintf (ubs, "%g", ub);

    status = CPXgetstat (env, mip);

    switch (status) {

    case CPXMIP_OPTIMAL:         summary = "done";       break;
    case CPXMIP_INFEASIBLE:      summary = "infeasible"; break;
    case CPXMIP_UNBOUNDED:       summary = "unbounded";  break;
    case CPXMIP_TIME_LIM_FEAS:
    case CPXMIP_TIME_LIM_INFEAS: summary = "timelimit";  break;
    default:                                             break;
    }

    printf ("%s,%s\n",summary,ubs);
  }

  if (mip != NULL) status = CPXfreeprob    (env, &mip);
  if (env != NULL) status = CPXcloseCPLEX (&env);

  for (i=0; filenames [i]; ++i)
    free (filenames [i]);
  free (filenames);

  return status;
}
