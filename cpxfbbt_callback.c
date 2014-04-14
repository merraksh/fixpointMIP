/*
 * Fix point FBBT as a cutting plane in Cplex -- real callback
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <sys/time.h>
#include "cplex.h"

//#define DEBUG

#define true  1
#define false 0

#define MAX_DEPTH 0
#define COUENNE_EPS 1e-5
#define DBL_MAX 1e50
#define COUENNE_INFINITY 1e50

extern void createRow (int sign,
		       int indexVar,
		       int nVars,
		       CPXLPptr p,
		       CPXCENVptr env,
		       const int *indices,
		       const double *coe,
		       double rhs,
		       const int nEl,
		       char extMod,
		       int indCon,
		       int nCon);

int fixpointfbbt (CPXCENVptr env,
		  void *cbdata,
		  int wherefrom,
		  void *cbhandle,
		  int *useraction_p) {

  CPXLPptr
    nodeLP,
    fplp;

  CPXCLPptr
    origLP;

  int
    status = cbdata ? CPXgetcallbacknodelp (env, cbdata, wherefrom, &nodeLP) : 0,
    nnz,
    *mbeg,
    *mind,
    *ind,
    ncols,
    nrows,
    suffspace,
    i,j,
    depth;

  double 
    *mval,
    *rhs,
    *rng,
    *rlb,
    *rub,
    *lb,
    *ub,
    *coe,
    time0;

  char
    *sense, extendedModel_ = 0;

  static char firstCall_ = true;

  static int  
    nRuns_ = 0, // number of calls 
    nTiL_  = 0, // number of tightened lower bounds
    nTiU_  = 0; //                     upper

  static double cpuTime_ = 0.;

  {
    struct timeval tv;
    gettimeofday (&tv, NULL);
    time0 = (double) tv. tv_sec + (double) tv. tv_usec / 1e6;
  }

  if ((NULL == cbdata)   &&
      (NULL == cbhandle) &&
      (NULL == useraction_p)) {

    //printf ("ran %d times, tightened %d lower and %d upper bounds, sep time: %g\n", nRuns_, nTiL_, nTiU_, cpuTime_);
    printf ("%g,%d,-1,-1,-1,-1,-1,", cpuTime_, nRuns_);
    return 0;
  }

  status = CPXgetcallbacknodeinfo (env,
				   cbdata,
				   wherefrom,
				   0,
				   CPX_CALLBACK_INFO_NODE_DEPTH,
				   &depth);

  if (depth > MAX_DEPTH) return 0;

  *useraction_p = CPX_CALLBACK_DEFAULT;

  //if (nRuns_ > 10) return 0;

  //printf ("fixpt callback... "); fflush (stdout);

  status = CPXgetcallbacklp (env, cbdata, wherefrom, &origLP);

  char *ctype;

  ncols = CPXgetnumcols (env, nodeLP);
  nrows = CPXgetnumrows (env, nodeLP);
  nnz   = CPXgetnumnz   (env, nodeLP);

  mbeg   = (int    *) malloc ((1 + nrows) * sizeof (int));
  mind   = (int    *) malloc (nnz         * sizeof (int));
  mval   = (double *) malloc (nnz         * sizeof (double));

  rhs    = (double *) malloc (     nrows  * sizeof (double));
  rng    = (double *) malloc (     nrows  * sizeof (double));
  lb     = (double *) malloc (     ncols  * sizeof (double));
  ub     = (double *) malloc (     ncols  * sizeof (double));

  sense  = (char   *) malloc (     nrows  * sizeof (char));
  ctype  = (char   *) malloc (     ncols  * sizeof (char));

  rlb = rhs;
  rub = rng;

  status = CPXgetcallbacknodelb (env, cbdata, wherefrom, lb, 0, ncols-1);
  status = CPXgetcallbacknodeub (env, cbdata, wherefrom, ub, 0, ncols-1);

  //status = CPXgetlb    (env, nodeLP, lb,    0, ncols-1);
  //status = CPXgetub    (env, nodeLP, ub,    0, ncols-1);

  status = CPXgetrhs   (env, nodeLP, rhs,   0, nrows-1);
  status = CPXgetsense (env, nodeLP, sense, 0, nrows-1);

  //if (status) printf ("status:%d\n", status);

  status = CPXgetctype (env, origLP, ctype, 0, ncols-1);

  //if (status) printf ("=> status:%d\n", status);

  /* translate rng, rhs into rlb, rub ************************************/

  for (i=0; i<nrows; ++i)

    switch (sense [i]) {

    case 'L': rub [i] = rhs [i]; rlb [i] = -DBL_MAX; break; /* [a,0] --> [-inf, a]    */
    case 'E': rub [i] = rhs [i];                     break; /* [a,0] --> [a,    a]    */
    case 'G': rub [i] = DBL_MAX;                     break; /* [a,0] --> [a,    +inf] */
    case 'R': rub [i] += rlb [i];                    break; /* [a,b] --> [a,    a+b]  */

    default: printf ("Constraint %d has undefined sense\n", i); 
      exit (-1); 
    }

  status = CPXgetrows (env, nodeLP, &nnz, mbeg, mind, mval, nnz, &suffspace, 0, nrows - 1);

#ifdef DEBUG
  printf ("problem: %d rows, %d cols, %d nz\n", nrows, ncols, nnz);
#endif

  if (suffspace < 0) {

    printf ("not enough room for getrows\n");
    exit (-1);
  }

  //x = (double *) malloc (ncols * sizeof (double));

  if (firstCall_) 
    firstCall_ = false;

  //  double startTime = CoinCpuTime ();

  //printf ("Fixed Point FBBT: "); fflush (stdout);

  ++nRuns_;

  /******************************************************************

    An LP relaxation of a MINLP problem is available. Let us suppose
    that this LP relaxation is of the form

    LP = {x in R^n: Ax <= b}
 
    for suitable nxm matrix A, rhs vector b, and variable vector
    x. Our purpose is that of creating a much larger LP that will
    help us find the interval [l,u] corresponding to the fixpoint of
    an FBBT algorithm. To this purpose, consider a single constraint
    of the above system:

    sum {i=1..n} a_ji x_i <= b_j

    According to two schools of thought (Leo's and mine), this
    single constraint can give rise to a number of FBBT
    constraints. The two schools of thoughts differ in the meaning
    of b: in mine, it is constant. In Leo's, it is a variable.

    We need to perform the following steps:

    define variables xL and xU
    define variables gL and gU for constraints (downward variables)

    add objective function sum_j (u_j - l_j)

    for each constraint a^j x <= b_j in Ax <= b:
    for each variable x_i contained:
    depending on sign of a_ji, add constraint on x_i^L or x_i^U
    (*) add constraints on g_j as well

    solve LP

    If new bounds are better than si's old bounds
    add OsiColCuts

    PS: We prove in the paper that our schools of thought are very
    close, and the only two people between them are Fourier and
    Motzkin

  ******************************************************************/

  /// Get the original problem's coefficient matrix and rhs vector, A and b

  fplp = CPXcreateprob (env, &status, "FixPointLP");

#ifdef DEBUG
  for (i=0; i<ncols; i++) 
    printf ("----------- x_%d in [%g,%g]\n", i, lb [i], ub [i]);
#endif

  double
    plus_one  =  1.,
    minus_one = -1.,
    zero      =  0.,
    pInf      =  DBL_MAX,
    mInf      = -DBL_MAX;

  // add lvars and uvars to the new problem
  for (i=0; i<ncols; i++)   status = CPXnewcols (env, fplp, 1, &minus_one, lb + i, ub + i, NULL, NULL); /*fplp -> addCol (0, NULL, NULL, lb [i], ub [i], -1.); // xL_i*/
  for (i=0; i<ncols; i++)   status = CPXnewcols (env, fplp, 1,  &plus_one, lb + i, ub + i, NULL, NULL); /*fplp -> addCol (0, NULL, NULL, lb [i], ub [i], +1.); // xU_i*/

  if (extendedModel_) {

    for (j=0; j<nrows; j++) status = CPXnewcols (env, fplp, 1, &zero, rlb + j, &pInf, NULL, NULL); /*fplp -> addCol (0, NULL, NULL, rlb [j],      DBL_MAX, 0.); // bL_j*/
    for (j=0; j<nrows; j++) status = CPXnewcols (env, fplp, 1, &zero, &mInf, rub + j, NULL, NULL); /*fplp -> addCol (0, NULL, NULL, -DBL_MAX, rub [j],     0.); // bU_j*/
  }

  // Scan each row of the matrix 

  coe = mval;
  ind = mind;

  for (j=0; j<nrows; j++) { // for each row

    //printf ("checking mbeg[%d]-mbeg[%d]\n", j+1, j);
    //printf ("--> %d-%d\n", mbeg[j+1], mbeg[j]);

    int nEl = (j==nrows-1) ? (nnz - mbeg [j]) : (mbeg [j+1] - mbeg [j]);

    if (!nEl)
      continue;

#ifdef DEBUG

    printf ("row %4d, %4d elements: ", j, nEl);

    for (i=0; i<nEl; i++) {
      printf ("%+g x%d ", coe [i], ind [i]);
      fflush (stdout);
    }

    printf ("in [%g,%g]\n", rlb [j], rub [j]);
#endif

    // create cuts for the xL and xU elements //////////////////////

    if (extendedModel_ || (rlb [j] > -COUENNE_INFINITY))
      for (i=0; i<nEl; i++) 
	createRow (-1, ind [i], ncols, fplp, env, ind, coe, rlb [j], nEl, extendedModel_, j, nrows); // downward constraints -- on x_i

    if (extendedModel_ || (rub [j] <  COUENNE_INFINITY))
      for (i=0; i<nEl; i++) 
	createRow (+1, ind [i], ncols, fplp, env, ind, coe, rub [j], nEl, extendedModel_, j, nrows); // downward constraints -- on x_i

    // create (at most 2) cuts for the bL and bU elements //////////////////////

    if (extendedModel_) {
      createRow (-1, 2*ncols         + j, ncols, fplp, env, ind, coe, rlb [j], nEl, extendedModel_, j, nrows); // upward constraints -- on bL_i
      createRow (+1, 2*ncols + nrows + j, ncols, fplp, env, ind, coe, rub [j], nEl, extendedModel_, j, nrows); // upward constraints -- on bU_i
    }

    ind += nEl;
    coe += nEl;
  }

  // finally, add consistency cuts, bL <= bU

  if (extendedModel_)

    for (j=0; j<nrows; j++) { // for each row

      int    ind [2] = {2*ncols + j, 2*ncols + nrows + j};
      double coe [2] = {1.,      -1.};
      int    beg [2] = {0,2};
      char sense = 'L';

      status = CPXaddrows (env, fplp, 0, 1, 2, &zero, &sense, beg, ind, coe, NULL, NULL);
    }

  /// Now we have an fbbt-fixpoint LP problem. Solve it to get
  /// (possibly) better bounds

  status = CPXchgobjsen (env, fplp, CPX_MAX);

#ifdef DEBUG
  {
    char fplpname [10];
    sprintf (fplpname, "fplp-%d.lp", nRuns_);
    printf ("(writing lp %s) ", fplpname);
    status = CPXwriteprob (env, fplp, fplpname, NULL);
  }
#endif

                                   //  /|-----------+
  status = CPXlpopt (env, fplp);   // < |           |
                                   //  \|-----------+

  status = CPXgetstat (env, fplp);

  *useraction_p = CPX_CALLBACK_DEFAULT;

  if (status == CPX_STAT_OPTIMAL) {

    // if problem not solved to optimality, bounds are useless

    double 
      *newLB = (double *) malloc (2 * ncols * sizeof (double)),
      *newUB,
      *oldLB = lb,
      *oldUB = ub,
      newbd = 1.,
      *x = (double *) malloc (ncols * sizeof (double)); // solution to the node LP


    status = CPXgetcallbacknodex (env, cbdata, wherefrom, x, 0, ncols-1); 

    status = CPXgetx (env, fplp, newLB, 0, 2 * ncols - 1);

    newUB = newLB + ncols;

    // check old and new bounds

    for (i=0; i<ncols; i++) {

      if ((CPX_BINARY  == ctype [i]) ||
	  (CPX_INTEGER == ctype [i])) {

	newLB [i] = ceil  (newLB [i] - COUENNE_EPS);
	newUB [i] = floor (newUB [i] + COUENNE_EPS);
      }

      if             ((newLB [i] > x [i] + COUENNE_EPS) && (newLB [i] > oldLB [i] + COUENNE_EPS))  {status = CPXcutcallbackadd (env, cbdata, wherefrom, 1, newLB [i], 'G', &i, &newbd, CPX_USECUT_PURGE); *useraction_p = CPX_CALLBACK_SET; ++nTiL_;}
      if (!status && ((newUB [i] < x [i] - COUENNE_EPS) && (newUB [i] < oldUB [i] - COUENNE_EPS))) {status = CPXcutcallbackadd (env, cbdata, wherefrom, 1, newUB [i], 'L', &i, &newbd, CPX_USECUT_PURGE); *useraction_p = CPX_CALLBACK_SET; ++nTiU_;}

      if (status)
	printf ("status:%d\n", status);

#define DEBUG
#ifdef DEBUG
      if (((newLB [i] > x [i] + COUENNE_EPS) && (newLB [i] > oldLB [i] + COUENNE_EPS))  ||
	  ((newUB [i] < x [i] - COUENNE_EPS) && (newUB [i] < oldUB [i] - COUENNE_EPS)))
	printf ("x%d=%g: [%g,%g] --> [%g,%g]\n", i, x [i],
		oldLB [i], oldUB [i],
		newLB [i], newUB [i]);
#endif
    }

    free (x);
    free (newLB);

  } else printf ("FPLP infeasible or unbounded.\n");

  CPXfreeprob (env, &fplp);

  free (mbeg);
  free (mind);
  free (mval);
  free (rhs);
  free (rng);
  free (lb);
  free (ub);
  free (sense);
  free (ctype);

  //printf ("\rrun %d done", nRuns_); fflush (stdout);

  {
    struct timeval tv;
    gettimeofday (&tv, NULL);
    cpuTime_ += ((double) tv. tv_sec + (double) tv. tv_usec / 1e6 - time0);
    //printf ("%g this call\n", (double) tv. tv_sec + (double) tv. tv_usec / 1e6 - time0);
  }

  return 0;
}
