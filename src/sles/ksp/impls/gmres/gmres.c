
/*
    This implements gmres.  It may be called recurrsively as long as 
    all of the user-supplied routines can. 

    This routine is meant to be compatible with execution on a parallel
    processor.  As such, it expects to be given routines for 
    all operations as well as a user-defined pointer to a distributed
    data structure.  THIS IS A DATA-STRUCTURE NEUTRAL IMPLEMENTATION.
  
    A context variable is used to hold internal data (the Hessenberg
    matrix and various parameters).

    Here are the routines that must be provided.  The generic parameters
    are:
	 itP   - Iterative context.  See the generic iterative method
	         information

    Special routines needed only for gmres:

    void   orthog(  itP, it )
        perform the orthogonalization of the vectors VV to VV+it.  A 
        basic version of this, defined in terms of vdot and maxpy, is
        available (in borthog.c) called void GMRESBasicOrthog;
	The user may use this routine to try alternate approaches.

    The calling sequence is the same as for all of the iterative methods.
    The special values (specific to GMRES) are:

    Some comments on left vs. right preconditioning, and restarts.
    Left and right preconditioning.
    If right preconditioning is chosen, then the problem being solved
    by gmres is actually
       My =  AB^-1 y = f
    so the initial residual is 
          r = f - Mx
    Note that B^-1 y = x or y = B x, and if x is non-zero, the initial
    residual is
          r = f - A x
    The final solution is then
          x = B^-1 y 

    If left preconditioning is chosen, then the problem being solved is
       My = B^-1 A x = B^-1 f,
    and the initial residual is
       r  = B^-1(f - Ax)

    Restarts:  Restarts are basically solves with x0 not equal to zero.
    Note that we can elliminate an extra application of B^-1 between
    restarts as long as we don't require that the solution at the end
    of a unsuccessful gmres iteration always be the solution x.
 */

#include <math.h>
#include <stdio.h>
#include "gmresp.h"
#define GMRES_DELTA_DIRECTIONS 5
#define GMRES_DEFAULT_MAXK 10
int  BasicMultiMaxpy( Vec *,int,double *,Vec);
static int GMRESGetNewVectors( KSP ,int );
static double GMRESUpdateHessenberg( KSP , int );
static int BuildGmresSoln(double* ,Vec,Vec ,KSP, int);
static int KSPiGMRESSetUp(KSP itP )
{
  unsigned int size, hh, hes, rs, cc;
  int      ierr,  max_k, k;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)itP->MethodPrivate;

  if (ierr = KSPCheckDef( itP )) return ierr;
  max_k         = gmresP->max_k;
  hh            = (max_k + 2) * (max_k + 1);
  hes           = (max_k + 1) * (max_k + 1);
  rs            = (max_k + 2);
  cc            = (max_k + 1);
  size          = (hh + hes + rs + 2*cc) * sizeof(double);

  gmresP->hh_origin  = (double *) MALLOC( size );
  CHKPTR(gmresP->hh_origin);
  gmresP->hes_origin = gmresP->hh_origin + hh;
  gmresP->rs_origin  = gmresP->hes_origin + hes;
  gmresP->cc_origin  = gmresP->rs_origin + rs;
  gmresP->ss_origin  = gmresP->cc_origin + cc;

  /* Allocate array to hold pointers to user vectors.  Note that we need
   4 + max_k + 1 (since we need it+1 vectors, and it <= max_k) */
  gmresP->vecs = (Vec *) MALLOC((VEC_OFFSET+2+max_k)*sizeof(void *));
  CHKPTR(gmresP->vecs);
  gmresP->vecs_allocated = VEC_OFFSET + 2 + max_k;
  gmresP->user_work = (Vec **)MALLOC((VEC_OFFSET+2+max_k)*sizeof(void *));
  CHKPTR(gmresP->user_work);
  gmresP->mwork_alloc = (int *) MALLOC( (VEC_OFFSET+2+max_k)*sizeof(int) );
  CHKPTR(gmresP->mwork_alloc);

  if (gmresP->q_preallocate) {
    gmresP->vv_allocated   = VEC_OFFSET + 2 + max_k;
 ierr = VecGetVecs(itP->vec_rhs, gmresP->vv_allocated,&gmresP->user_work[0]);
    CHKERR(ierr);
    gmresP->mwork_alloc[0] = gmresP->vv_allocated;
    gmresP->nwork_alloc    = 1;
    for (k=0; k<gmresP->vv_allocated; k++)
	gmresP->vecs[k] = gmresP->user_work[0][k];
  }
  else {
    gmresP->vv_allocated    = 5;
    VecGetVecs(itP->vec_rhs, 5,    &gmresP->user_work[0]);
    gmresP->mwork_alloc[0]  = 5;
    gmresP->nwork_alloc     = 1;
    for (k=0; k<gmresP->vv_allocated; k++)
	gmresP->vecs[k] = gmresP->user_work[0][k];
  }
  return 0;
}
/* 
    This routine computes the initial residual without making any assumptions
    about the solution.
 */
static int GMRESResidual(  KSP itP,int restart )
{
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)(itP->MethodPrivate);
  double        mone = -1.0;

  /* compute initial residual: f - M*x */
  /* (inv(b)*a)*x or (a*inv(b)*b)*x into dest */
  if (itP->right_pre) {
    /* we want a * binv * b * x, or just a * x for the first step */
    /* a*x into temp */
    MM(itP, VEC_SOLN, VEC_TEMP );
  }
  else {
    /* else we do binv * a * x */
    MATOP(itP, VEC_SOLN, VEC_TEMP, VEC_TEMP_MATOP );
  }
  /* This is an extra copy for the right-inverse case */
  VecCopy( VEC_BINVF, VEC_VV(0) );
  VecAXPY( &mone, VEC_TEMP, VEC_VV(0) );
      /* inv(b)(f - a*x) into dest */
  return 0;
}
/*
    Run gmres, possibly with restart.  Return residual history if requested.
    input parameters:
.        restart - 1 if restarting gmres, 0 otherwise
.	gmresP  - structure containing parameters and work areas
.	itsSoFar- total number of iterations so far (from previous cycles)

    output parameters:
.        nres    - residuals (from preconditioned system) at each step.
                  If restarting, consider passing nres+it.  If null, 
                  ignored
.        itcount - number of iterations used.  nres[0] to nres[itcount]
                  are defined.  If null, ignored.
    Returns:
    0 on success, 1 on failure (did not converge)

    Notes:
    On entry, the value in vector VEC_VV(0) should be the initial residual
    (this allows shortcuts where the initial preconditioned residual is 0).
 */
int GMREScycle(int *  itcount, int itsSoFar,int restart,KSP itP )
{
  double  res_norm, res, rtol, tt, hapbnd, hist_len= itP->res_hist_size, cerr;
  double  *nres = itP->residual_history, tmp;
  /* Note that hapend is ignored in the code */
  int     it, hapend, converged;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)(itP->MethodPrivate);
  int     max_k = gmresP->max_k;
  int     max_it = itP->max_it;

  /* Question: on restart, compute the residual?  No; provide a restart 
     driver */

  it  = 0;

  /* dest . dest */
  VecNorm(VEC_VV(0),&res_norm);
  res         = res_norm;
  *RS(0)      = res_norm;

  /* Do-nothing case: */
  if (res_norm == 0.0) {
    if (itcount) *itcount = 0;
    return 0;
  }
  /* scale VEC_VV (the initial residual) */
  tmp = 1.0/res_norm; VecScale(&tmp , VEC_VV(0) );

  if (!restart) {
    rtol      = itP->rtol * res_norm;
    itP->ttol = (itP->atol > rtol) ? itP->atol : rtol;
  }
  rtol= itP->ttol;
  gmresP->it = (it-1);  /* For converged */
  while ( !(converged = CONVERGED(itP,res,it+itsSoFar)) && it < max_k && 
        it + itsSoFar < max_it) {
    if (nres && hist_len > it + itsSoFar) nres[it+itsSoFar]   = res;
    if (itP->usr_monitor) {
	gmresP->it = (it - 1);
        (*itP->usr_monitor)( itP,  it + itsSoFar, res,itP->monP );
	}
    if (gmresP->vv_allocated <= it + VEC_OFFSET + 1) {
	/* get more vectors */
	GMRESGetNewVectors(  itP, it+1 );
	}
    MATOP(itP, VEC_VV(it), VEC_VV(it+1), VEC_TEMP_MATOP );

    /* update hessenberg matrix and do Gram-Schmidt */
    (*gmresP->orthog)(  itP, it );

    /* vv(i+1) . vv(i+1) */
    VecNorm(VEC_VV(it+1),&tt);
    /* save the magnitude */
    *HH(it+1,it)    = tt;
    *HES(it+1,it)   = tt;

    /* check for the happy breakdown */
    hapbnd  = gmresP->epsabs * fabs( *HH(it,it) / *RS(it) );
    if (hapbnd > gmresP->haptol) hapbnd = gmresP->haptol;
    if (tt > hapbnd) {
        tmp = 1.0/tt; VecScale( &tmp , VEC_VV(it+1) );
    }
    else {
        /* We SHOULD probably abort the gmres step
           here.  This happens when the solution is exactly reached. */
      hapend  = 1;
    }
    res = GMRESUpdateHessenberg( itP, it );
    it++;
    gmresP->it = (it-1);  /* For converged */
  }
  itP->nmatop   += it;
  itP->nvectors += 3 + it * (3 + (it - 1));

  if (nres && hist_len > it + itsSoFar) nres[it + itsSoFar]   = res; 
  if (nres) 
    itP->res_act_size = (hist_len < it + itsSoFar) ? hist_len : 
	it + itsSoFar + 1;
  if (itP->usr_monitor) {
    gmresP->it = it - 1;
    (*itP->usr_monitor)( itP,  it + itsSoFar, res, itP->monP );
  }
  if (itcount) *itcount    = it;

  /*
    Down here we have to solve for the "best" coefficients of the Krylov
    columns, add the solution values together, and possibly unwind the
    preconditioning from the solution
   */
  if (it == 0) {
    /* exited at the top before doing ANYTHING */
    return 0;
  }

  /* Form the solution (or the solution so far) */
  BuildGmresSoln(  RS(0), VEC_SOLN, VEC_SOLN, itP, it-1 );

  /* Return correct status (Failed on iteration test (failed to converge)) */
  return !converged;
}

static int KSPiGMRESSolve(KSP itP,int *outits )
{
  int maxit, err;
  int restart, its, itcount, it;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)itP->MethodPrivate;

  it      = 0;
  restart = 0;
  itcount = 0;
  /* Save binv*f */
  if (!itP->right_pre) {
    /* inv(b)*f */
    PRE(itP, VEC_RHS, VEC_BINVF );
  }
  else 
    VecCopy( VEC_RHS, VEC_BINVF );
  /* Compute the initial (preconditioned) residual */
  if (!itP->guess_zero) {
    if (err=GMRESResidual(  itP, restart )) return err;
  }
  else VecCopy( VEC_BINVF, VEC_VV(0) );
    
  while (err = GMREScycle(  &its, itcount, restart, itP )) {
    restart = 1;
    itcount += its;
    if( err = GMRESResidual(  itP, restart )) return err;
    if (itcount > itP->max_it) break;
    /* need another check to make sure that gmres breaks out 
       at precisely the number of iterations chosen */
  }
  itcount += its;      /* add in last call to GMREScycle */
  *outits = itcount;  return 0;
}

static int KSPiGMRESAdjustWork(KSP itP )
{
  KSPiGMRESCntx *gmresP;
  int          i;

  if ( itP->adjust_work_vectors ) {
   gmresP = (KSPiGMRESCntx *) itP->MethodPrivate;
   for (i=0; i<gmresP->vv_allocated; i++) 
       if ( (*itP->adjust_work_vectors)(itP,gmresP->user_work[i],
					     gmresP->mwork_alloc[i] ) ) 
	   SETERR(1,"Could not allocate work vectors in GMRES");
 }
  return 0;
}

static int KSPiGMRESDestroy(PetscObject obj)
{
  KSP itP = (KSP) obj;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *) itP->MethodPrivate;
  int          i;

  /* Free the matrix */
  FREE( gmresP->hh_origin );

  /* Free the pointer to user variables */
  FREE( gmresP->vecs );

  /* free work vectors */
  for (i=0; i<gmresP->nwork_alloc; i++) 
    VecFreeVecs(gmresP->user_work[i], gmresP->mwork_alloc[i] );
  FREE( gmresP->user_work );
  FREE( gmresP->mwork_alloc );
  if (gmresP->nrs) FREE( gmresP->nrs );
  FREE( gmresP ); 
  FREE( itP );
  return 0;
}
/*
    BuildGmresSoln - create the solution from the starting vector and the
    current iterates.

    Input parameters:
        nrs - work area of size it + 1.
	vs  - index of initial guess
	vdest - index of result.  Note that vs may == vdest (replace
	        guess with the solution).

     This is an internal routine that knows about the GMRES internals.
 */
static int BuildGmresSoln(double* nrs,Vec vs,Vec vdest,KSP itP, int it )
{
  double  tt, zero = 0.0, one = 1.0;
  int     ii, k, j;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)(itP->MethodPrivate);

  /* Solve for solution vector that minimizes the residual */

  /* If it is < 0, no gmres steps have been performed */
  if (it < 0) {
    if (vdest != vs) {
	VecCopy( vs, vdest );
    }
    return 0;
  }
  nrs[it] = *RS(it) / *HH(it,it);
  for (ii=1; ii<=it; ii++) {
    k   = it - ii;
    tt  = *RS(k);
    for (j=k+1; j<=it; j++) tt  = tt - *HH(k,j) * nrs[j];
    nrs[k]   = tt / *HH(k,k);
  }

  /* Accumulate the correction to the solution of the preconditioned problem
    in TEMP */
  VecSet( &zero, VEC_TEMP );
  BasicMultiMaxpy(  &VEC_VV(0), it, nrs, VEC_TEMP );

  /* If we preconditioned on the right, we need to solve for the correction to
     the unpreconditioned problem */
  if (itP->right_pre) {
    if (vdest != vs) {
	  PRE(itP, VEC_TEMP, vdest );
	  VecAXPY( &one, vs, vdest );
    }
    else {
	  PRE(itP, VEC_TEMP, VEC_TEMP_MATOP );
	  VecAXPY( &one, VEC_TEMP_MATOP, vdest );
    }
  }
  else {
    if (vdest != vs) {
	VecCopy( VEC_TEMP, vdest );
	VecAXPY( &one, vs, vdest );
    }
    else 
	VecAXPY( &one, VEC_TEMP, vdest );
 }
  return 0;
}
/*
   Do the scalar work for the orthogonalization.  Return new residual.
 */
static double GMRESUpdateHessenberg( KSP itP, int it )
{
  register double *hh, *cc, *ss, tt;
  register int    j;
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)(itP->MethodPrivate);

  hh  = HH(0,it);
  cc  = CC(0);
  ss  = SS(0);

  /* Apply all the previously computed plane rotations to the new column
     of the Hessenberg matrix */
  for (j=1; j<=it; j++) {
    tt  = *hh;
    *hh = *cc * tt + *ss * *(hh+1);
    hh++;
    *hh = *cc++ * *hh - ( *ss++ * tt );
  }

  /*
    compute the new plane rotation, and apply it to:
     1) the right hand side of the Hessenberg system
     2) the new column of the Hessenberg matrix
    thus obtaining the updated value of the residual
  */
  tt        = sqrt( *hh * *hh + *(hh+1) * *(hh+1) );
  *cc       = *hh / tt;
  *ss       = *(hh+1) / tt;
  *RS(it+1) = - ( *ss * *RS(it) );
  *RS(it)   = *cc * *RS(it);
  *hh       = *cc * *hh + *ss * *(hh+1);
  return fabs( *RS(it+1) );
}
/*
   This routine allocates more work vectors, starting from VEC_VV(it).
 */
static int GMRESGetNewVectors( KSP itP,int it )
{
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)itP->MethodPrivate;
  int nwork = gmresP->nwork_alloc;
  int k, nalloc;

  nalloc = gmresP->delta_allocate;
  /* Adjust the number to allocate to make sure that we don't exceed the
    number of available slots */
  if (it + VEC_OFFSET + nalloc >= gmresP->vecs_allocated)
      nalloc = gmresP->vecs_allocated - it - VEC_OFFSET;
  /* CHKPTR(nalloc); */
  if (nalloc == 0) return 0;

  gmresP->vv_allocated += nalloc;
  VecGetVecs(itP->vec_rhs, nalloc,&gmresP->user_work[nwork] );
  CHKPTR(gmresP->user_work[nwork]);
  gmresP->mwork_alloc[nwork] = nalloc;
  for (k=0; k<nalloc; k++)
    gmresP->vecs[it+VEC_OFFSET+k] = gmresP->user_work[nwork][k];
  gmresP->nwork_alloc++;
}





/*@
    KSPGMRESSetRestart - Sets the number of search directions 
    for GMRES before restart.

    Input Parameters:
.   itP - the iterative context
.   max_k - the number of directions
@*/
int KSPGMRESSetRestart(KSP itP,int max_k )
{
  KSPiGMRESCntx *gmresP;
  VALIDHEADER(itP,KSP_COOKIE);
  gmresP = (KSPiGMRESCntx *)itP->MethodPrivate;
  if (itP->method != KSPGMRES) return 0;
  gmresP->max_k = max_k;
  return 0;
}

int KSPiGMRESDefaultConverged(KSP itP,int n,double rnorm,void *dummy)
{
  if ( rnorm <= itP->ttol ) return(1);
  else return(0);
}


/*
  GMRESBuildSolution -Build the solution for GMRES 
 */
static int GMRESBuildSolution(KSP itP,Vec  ptr,Vec *result )
{
  KSPiGMRESCntx *gmresP = (KSPiGMRESCntx *)itP->MethodPrivate; 

  if (ptr == 0) {
    /* if (!gmresP->sol_temp)  need to allocate */
    ptr = gmresP->sol_temp;
  }
  if (!gmresP->nrs) {
    /* allocate the work area */
    gmresP->nrs = (double *)
	               MALLOC( (unsigned)(gmresP->max_k * sizeof(double) ) );
  }

  BuildGmresSoln(  gmresP->nrs, VEC_SOLN, ptr, itP, gmresP->it );
  *result = ptr; return 0;
}


/*@
  KSPGMRESSetOrthogRoutine - Sets the orthogonalization routine used by GMRES

  Input Parameters:
.   itP   - iterative context obtained from KSPCreate
.   fcn   - Orthogonalization function.  See iter/gmres/borthog.c for examples

  Notes:
  The functions GMRESBasicOrthog and GMRESUnmodifiedOrthog are predefined.
  The default is GMRESBasicOrthog; GMRESUnmodifiedOrthog is a simple 
  Gramm-Schmidt (NOT modified Gramm-Schmidt).  The GMRESUnmodifiedOrthog is 
  NOT recommended; however, for some problems, particularly when using 
  parallel distributed vectors, this may be significantly faster.
@*/
int KSPGMRESSetOrthogRoutine( KSP itP,int (*fcn)(KSP,int) )
{
  VALIDHEADER(itP,KSP_COOKIE);
  if (itP->method == KSPGMRES) {
    ((KSPiGMRESCntx *)itP->MethodPrivate)->orthog = fcn;
  }
  return 0;
}
int KSPiGMRESCreate(KSP itP)
{
  KSPiGMRESCntx *gmresP;
  int         GMRESBasicOrthog(KSP,int);

  gmresP = NEW(KSPiGMRESCntx); CHKPTR(gmresP);

  itP->MethodPrivate = (void *) gmresP;
  itP->method        = KSPGMRES;
  itP->converged     = KSPiGMRESDefaultConverged;
  itP->BuildSolution = GMRESBuildSolution;

  itP->setup         = KSPiGMRESSetUp;
  itP->solver        = KSPiGMRESSolve;
  itP->adjustwork    = KSPiGMRESAdjustWork;
  itP->destroy       = KSPiGMRESDestroy;

  gmresP->haptol    = 1.0e-8;
  gmresP->epsabs    = 1.0e-8;
  gmresP->q_preallocate = 0;
  gmresP->delta_allocate = GMRES_DELTA_DIRECTIONS;
  gmresP->orthog    = GMRESBasicOrthog;
  gmresP->nrs       = 0;
  gmresP->sol_temp  = 0;
  gmresP->max_k     = GMRES_DEFAULT_MAXK;
  return 0;
}
