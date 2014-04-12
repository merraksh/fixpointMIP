
// single cut creation. Parameters:
//
//  1) sign:     tells us whether this is a <= or a >= (part of a) constraint.
//  2) indexVar: index of variable we want to do upward or downward bt
//  3) nVars:    number of variables in the original problems (original +
//               auxiliaries). Used to understand if we are adding an
//               up or a down constraint
//  4) p:        solver interface to which we are adding constraints
//  5) indices:  vector containing indices of the linearization constraint (the    i's)
//  6) coe:                        coeffs                                       a_ji's
//  7) rhs:      right-hand side of constraint
//  8) nEl:      number of elements of this linearization cut
//  9) extMod:   extendedModel_
// 10) indCon:   index of constraint being treated (and corresponding bL, bU)
// 11) nCon:     number of constraints

#include "cplex.h"

#define COUENNE_EPS 1e-8
#define DBL_MAX 1e50
#define COUENNE_INFINITY 1e50

//#define DEBUG

void createRow (int sign,
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
		int nCon) {

  ///////////////////////////////////////////////////////////////////////////////////////////////////////
  ///
  /// Define
  ///
  /// I+ the subset of {1..n} such that a_ji > 0 and i != indexVar;
  /// I- the subset of {1..n} such that a_ji < 0 and i != indexVar.
  ///
  /// Consider
  /// 
  /// sum {i=1..n} a_ji x_i = b_j,      (=)
  ///
  /// equivalent to the two constraints
  ///
  /// sum {i=1..n} a_ji x_i >= b_j.     (>) -- sign will be -1 (rlb)
  /// sum {i=1..n} a_ji x_i <= b_j      (<) -- sign will be +1 (rub)
  ///
  /// Hence we should consider both (<) and (>) when we have an
  /// equality constraint. The resulting set of upward FBBT is as
  /// follows:
  ///
  /// sum {i in I+} a_ji xU_i + sum {i in I-} a_ji xL_i >= bU_j  (only if (<) enforced, i.e., sign ==  1)
  /// sum {i in I+} a_ji xL_i + sum {i in I-} a_ji xU_i <= bL_j  (only if (>) enforced, i.e., sign == -1)
  ///
  /// together with the constraints defining the initial bounding
  /// interval of the auxiliary variable (already included):
  ///
  /// bU_j <= bU0_j (<)
  /// bL_j >= bL0_j (>)
  ///
  /// The set of downward FBBT constraints is instead:
  ///
  /// xL_i >= (bL_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k) / a_ji   (if a_ji > 0 and (>))
  /// xU_i <= (bU_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k) / a_ji   (if a_ji > 0 and (<))
  ///
  /// xU_i <= (bL_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k) / a_ji   (if a_ji < 0 and (>))
  /// xL_i >= (bU_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k) / a_ji   (if a_ji < 0 and (<))
  ///
  /// probably better rewritten, to avoid (further) numerical issues, as
  ///
  ///   a_ji xL_i >=   bL_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k   (if a_ji > 0 and (>))
  ///   a_ji xU_i <=   bU_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k   (if a_ji > 0 and (<))
  ///
  /// - a_ji xU_i <= - bL_j + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k   (if a_ji < 0 and (>))
  /// - a_ji xL_i >= - bU_j + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k   (if a_ji < 0 and (<))
  ///
  /// or even better, to keep the old coefficients (but not the indices!), like this:
  ///
  /// a_ji xL_i + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k - bL_j >= 0  (if a_ji > 0 and (>))
  /// a_ji xU_i + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k - bU_j <= 0  (if a_ji > 0 and (<))
  ///
  /// a_ji xU_i + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k - bL_j >= 0  (if a_ji < 0 and (>))
  /// a_ji xL_i + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k - bU_j <= 0  (if a_ji < 0 and (<))
  ///
  /// and finally we need initial lower/upper bounds:
  ///
  /// xL_i >= xL0_i
  /// xU_i <= xU0_i
  ///
  /// and some consistency constraints
  ///
  /// bL_i <= bU_i
  ///
  /// (these and the two bound constraints above are already added in
  /// the main function above).
  ///
  /////////////////////////////////////////////////////////////////////////////////////////////////////
  ///
  /// According to my school of thought, instead, there is no
  /// upward/downward directions to simulate. Hence, considering again
  ///
  /// sum {i=1..n} a_ji x_i >= b_j      (>) -- sign will be -1 (rlb)
  /// sum {i=1..n} a_ji x_i <= b_j      (<) -- sign will be +1 (rub)
  ///
  /// we'll have similar constraints, where bL and bU are replaced by
  /// the original rhs.
  ///
  /// xL_i >= (b_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k) / a_ji   (if a_ji > 0 and (>))
  /// xU_i <= (b_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k) / a_ji   (if a_ji > 0 and (<))
  ///										     
  /// xU_i <= (b_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k) / a_ji   (if a_ji < 0 and (>))
  /// xL_i >= (b_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k) / a_ji   (if a_ji < 0 and (<))
  ///
  /// once again rewritten as 
  ///
  ///   a_ji xL_i >=   b_j - sum {k in I+} a_jk xU_k - sum {k in I-} a_jk xL_k   (if a_ji > 0 and (>))
  ///   a_ji xU_i <=   b_j - sum {k in I+} a_jk xL_k - sum {k in I-} a_jk xU_k   (if a_ji > 0 and (<))
  ///										     
  /// - a_ji xU_i <= - b_j + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k   (if a_ji < 0 and (>))
  /// - a_ji xL_i >= - b_j + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k   (if a_ji < 0 and (<))
  ///
  /// or even better:
  ///
  /// a_ji xL_i + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k >= b_j       (if a_ji > 0 and (>))
  /// a_ji xU_i + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k <= b_j       (if a_ji > 0 and (<))
  ///										     
  /// a_ji xU_i + sum {k in I+} a_jk xU_k + sum {k in I-} a_jk xL_k >= b_j       (if a_ji < 0 and (>))
  /// a_ji xL_i + sum {k in I+} a_jk xL_k + sum {k in I-} a_jk xU_k <= b_j       (if a_ji < 0 and (<))
  ///
  /// No other cuts are needed.
  ///
  /////////////////////////////////////////////////////////////////////////////////////////////////////


  int nTerms = extMod ? nEl+1 : nEl, i, k;

#ifdef DEBUG
  double
    lb = sign > 0 ? -DBL_MAX : extMod ? 0. : rhs,
    ub = sign < 0 ? +DBL_MAX : extMod ? 0. : rhs;
#endif

  int    *iInd = (int    *) malloc (nTerms * sizeof (int)),
    beg [2] = {0, nTerms};
  double *elem = (double *) malloc (nTerms * sizeof (double));

  char sense = (sign > 0) ? 'L' : 'G'; // TODO: it was (sign < 0) before... :-O

#ifdef DEBUG
  printf ("creating constraint from: ");
  for (i=0; i<nEl; i++)
    printf ("%+g x%d ", coe [i], indices [i]);
  printf ("%c= %g for variable index %d: ", sign > 0 ? '<' : '>', rhs, indexVar);
#endif

  // coefficients are really easy

  for (i=0; i<nEl; ++i)
    elem [i] = coe [i];/* CoinCopyN (coe, nEl, elem); */

  if (extMod) {
    elem [nEl] = -1.; // extended model, coefficient for bL or bU
    iInd [nEl] = 2*nVars + indCon + ((sign > 0) ? nCon : 0);
  }

  // indices are not so easy...

  for (k=0; k<nEl; k++) {

    int curInd = indices [k];

    iInd [k] = curInd; // Begin with xL_i, same index as x_i in the
                       // original model. Should add n depending on a
                       // few things... 

    if (curInd == indexVar) { // x_k is x_i itself
      if (((sign > 0) && (coe [k] > 0.)) || 
	  ((sign < 0) && (coe [k] < 0.)))

      iInd [k] += nVars;

    } else if (((coe [k] > 0.) && (sign < 0)) ||
	       ((coe [k] < 0.) && (sign > 0)))

      iInd [k] += nVars;
  }

  //CoinPackedVector vec (nTerms, iInd, elem);

  if (extMod) rhs = 0;

  CPXaddrows (env, p, 0, 1, nEl, &rhs, &sense, beg, iInd, elem, NULL, NULL);

  // Update time spent doing this

#ifdef DEBUG
  for (i=0; i<nEl; i++)
    printf ("%+g x%d ", elem [i], iInd [i]);

  printf ("in [%g,%g]\n", lb, ub);
#endif

  free (iInd);
  free (elem);
}
