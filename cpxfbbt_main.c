/*
 * Fix point FBBT as a cutting plane in Cplex
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <time.h>
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

  fixpointfbbt (env, NULL, 0, NULL, NULL);

  if (mip != NULL) status = CPXfreeprob    (env, &mip);
  if (env != NULL) status = CPXcloseCPLEX (&env);

  for (i=0; filenames [i]; ++i)
    free (filenames [i]);
  free (filenames);

  return status;
}
