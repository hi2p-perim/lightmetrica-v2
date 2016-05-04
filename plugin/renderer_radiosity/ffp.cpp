/*
 * $Id: ffp.c,v 1000.9 94/01/14 16:23:46 ps Exp $
 * $Source: /usr/graphics/project/ff/ffp.c,v $
 * $Log:	ffp.c,v $
 * Revision 1000.9  94/01/14  16:23:46  ps
 * still can't compute area... This time the cross[] function was messed up
 * 
 * Revision 1000.8  93/12/28  18:26:15  ps
 * fixed some overflow problems due to large denominators in some of the
 * macros by reordering the multiplies
 * also changed the planarity criterion to check against sqrt of eps
 * 
 * Revision 1000.7  93/12/10  15:15:50  ps
 * fixed typo in det() macro...
 * 
 * Revision 1000.6  93/12/05  15:03:30  ps
 * replaced plane test with a new one and made all mach[] tests much tighter.
 * Also fixed a but in evalint{m|p}()
 * 
 * Revision 1000.5  93/11/30  18:49:24  ps
 * fixed ilog() macro bug in which the boundary case was not carefully enough
 * being dealt with.
 * 
 * Revision 1000.4  93/11/28  13:13:42  ps
 * moved ALLCOEFF to ff.h
 * 
 * Revision 1000.3  93/11/09  10:45:40  ps
 * added _CRAY ifdef to turn on ATAN2BUSTED.
 * 
 * Revision 1000.2  93/11/09  09:50:36  ps
 * created new symbol ATAN2BUSTED for machines on which atan2() returns
 * garbage when called with both arguments zero (such as the Cray YMP).
 * 
 * Revision 1000.1  93/06/18  06:56:06  ps
 * Bump to release, no file changes
 * 
 * Revision 1.1  93/04/14  22:02:17  ps
 * Initial revision
 * 
 * Revision 1.2  93/04/09  15:00:15  ps
 * sanity checkin. It appears to work for all random cases, but planar()
 * doesn't yet.
 * 
 * Revision 1.1  93/04/06  21:24:45  ps
 * Initial revision
 * 
 */
static	char	rcs_id[] = "$Header: /usr/graphics/project/ff/ffp.c,v 1000.9 94/01/14 16:23:46 ps Exp $";

/*
 * to compile on SGI use: -Wf,-XNh1500 flag to keep (acom) from choking...
 */
#include <stdio.h>
#define _USE_MATH_DEFINES
#include <math.h>
//#include <values.h>
#include <limits.h>
#include <float.h>
#include <assert.h>

#include "claussen.h"
#include "ff.h"

#pragma warning(disable:4244)
#define DOUBLE

/*
 * on some machines the atan2() is busted for both arguments == 0
 * e.g. the Cray YMP
 */
#ifdef	_CRAY
#define	ATAN2BUSTED	1
#endif	/* _CRAY */

#ifdef	ATAN2BUSTED
#define	ATAN2(y,x)	(((x)==0)&&((y)==0)?0:atan2(y,x))
#else	/* ATAN2BUSTED */
#define	ATAN2(y,x)	atan2(y,x)
#endif	/* ATAN2BUSTED */

#if	defined SINGLE

#ifndef	HUGE
#define	HUGE	MAXFLOAT
#endif	/* HUGE */

#define	real	float

#define	atan2	atan2f
#define	claussen	claussenf
#define	IDilog	IDilogf
#define	log	logf
#define	atan	atanf
#define	G	Gf
#define	sqrt	sqrtf
#define	H	Hf
#define	Pair	Pairf
#define	Area	Areaf
#define	Bilinear	Bilinearf
#define	acos	acosf
#define	mach	machf
#define	cos	cosf
#define	sin	sinf
#define	fabs	fabsf
#define	IntegralPlanar	IntegralPlanarf
#define	Lcis	Lcisf
#define	LogSelect	LogSelectf
#define	RM	RMf
#define	IDilogPath	IDilogPathf
#define	ILogPart	ILogPartf
#define	ILogIntegral	ILogIntegralf
#define	floor	floorf
#define	Integral	Integralf
#define	FormFactor	FormFactorf

extern	float	logf( float );
extern	float	atan2f( float, float );
extern	float	atanf( float );
extern	float	sqrtf( float );
extern	float	acosf( float );
extern	float	cosf( float );
extern	float	sinf( float );
extern	float	fabsf( float );
extern	float	floorf( float );

#elif	defined DOUBLE

#ifndef	HUGE
//#define	HUGE	MAXDOUBLE]
#define	HUGE	DBL_MAX
#endif	/* HUGE */

int	fferror = NO_FF_ERROR;

#define	real	double

#endif	/* real */

#define	M_PIF	((real)M_PI)
#define	HALF	((real).5)
#define	THREEHALF	((real)1.5)
#define	ONEQUARTER	((real).25)
#define	ONEEIGHTTH	((real).125)
#define	ONESIXTEENTH	((real).0625)
#define	ONETHIRTYSECOND	((real).03125)

/*
 * the magic array is called `c' and it has slots named just as
 * in the paper. There are a few extra slots which are symbolically
 * named below
 */
#define	X	0
#define	Y	1
#define	L	0
#define	U	1
#define	PHI	6	/* otherwise unused */
#define	THETA	7	/* otherwise unused */
#define	CONST	8	/* otherwise unused */
#define	PSI	9	/* otherwise unused */
#define	PLANE	PSI	/* only needed initially */
#define	UPPER	19
#define	LOWER	20
/*
 * in order to facilitate custom calling sequences
 * we put the following in ff.h
 */
#if 0
#define	ALLCOEFF	21	/* length of array */
#endif /* 0 */

/*
 * for debugging
 */
#define PRINT(x) (print?fprintf( stderr,"%.16e\n",x):0)

/*
 * return values for Lcis
 */
#define	XSOL	0x1
#define	YSOL	0x2

/*
 * for point data structures
 */
#define	dot(a,b)	((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define	cross(c,a,b)	\
  ((c)[0]=(a)[1]*(b)[2]-(a)[2]*(b)[1],\
   (c)[1]=(a)[2]*(b)[0]-(a)[0]*(b)[2],\
   (c)[2]=(a)[0]*(b)[1]-(a)[1]*(b)[0],(c))
#define	det(a,b,c)	\
  (((a)[1]*(b)[2]-(a)[2]*(b)[1])*(c)[0]+\
   ((a)[2]*(b)[0]-(a)[0]*(b)[2])*(c)[1]+\
   ((a)[0]*(b)[1]-(a)[1]*(b)[0])*(c)[2])
#define	diff(d,a,b)	\
  ((d)[0]=(a)[0]-(b)[0],\
   (d)[1]=(a)[1]-(b)[1],\
   (d)[2]=(a)[2]-(b)[2],(d))
#define	add(d,a,b)	\
  ((d)[0]=(a)[0]+(b)[0],\
   (d)[1]=(a)[1]+(b)[1],\
   (d)[2]=(a)[2]+(b)[2],(d))

/*
 * real and imaginary part of the product of two complex numbers
 */
#define	cmulx(a,b)	((a)[X]*(b)[X]-(a)[Y]*(b)[Y])
#define	cmuly(a,b)	((a)[X]*(b)[Y]+(a)[Y]*(b)[X])
/*
 * complex magnitude squared
 */
#define	cmagsqr(c)	((c)[X]*(c)[X]+(c)[Y]*(c)[Y])
/*
 * real and imaginary part of the square of a complex number
 */
#define	sqrx(z)	((z)[X]*(z)[X]-(z)[Y]*(z)[Y])
#define	sqry(z)	(2*(z)[X]*(z)[Y])

/*
 * on the SGI hypot is broken. Isn't that great?
 */
#define	hypot(a,b)	sqrt((a)*(a)+(b)*(b))

/*
 * imaginary part of the complex logarithm as a function
 * of the particular extension choosen
 */
#define	ilog(j,x,y)	\
  (ATAN2(y,x)+(((j)==1)&&((y)<0)&&((x)<=0)?(real)(2*M_PIF):\
		(((((j)==2)&&(((y)>0)||(((y)==0)&&((x)<0))))||\
		   (((j)==3)&&((y)>=0)&&((x)<0)))? -2*M_PIF : 0)))

/*
 * computes the imaginary part of dilog for complex arguments
 */
real
IDilog( real z[2] )
{
  real r = hypot( z[X], z[Y] );
  real omega = 2 * ATAN2( z[Y], 1 - z[X] );
  real theta = 2 * ATAN2( z[Y], z[X] );
  real result = 0;

  if( r != 0 ){
    result = HALF * ( omega * log( r ) +
		     claussen( omega ) +
		     claussen( theta ) -
		     claussen( theta + omega ) );
  }

  return result;
}

real
G( real a, real b, real c, real t )
{
  real qt = ( a * t + b ) * t + c;
  real qpt = 2 * a * t + b;
  real d = sqrt( 4 * a * c - b * b );

  return qpt / ( 2 * a ) * log( qt ) - 2 * t + d / a * atan( qpt / d );
}

real
H( real a, real b, real c, real t )
{
  real at = a * t;
  real qt = ( at + b ) * t + c;
  real twoa = 2 * a;
  real qpt = twoa * t + b;
  real d = sqrt( 4 * a * c - b * b );

  return ( ( at * t + c ) * twoa - b * b ) / ( twoa * twoa ) * log( qt )
    - t * ( at - b ) / twoa - b * d / ( twoa * a ) * atan( qpt / d );
}

/*
 * compute c_0 to c_5 for a given pair of edges
 */
void
Pair( real c[ALLCOEFF][2],
      real p1[3], real p2[3],
      real q1[3], real q2[3] )
{
  real dp[3], dq[3], qp[3];
  real l;

  diff( dp, p2, p1 );
  c[0][X] = sqrt( dot( dp, dp ) ); /* dj */

  l = 1 / c[0][X];
  dp[0] *= l; dp[1] *= l; dp[2] *= l;

  diff( dq, q2, q1 );
  c[2][X] = sqrt( dot( dq, dq ) ); /* di */

  l = 1 / c[2][X];
  dq[0] *= l; dq[1] *= l; dq[2] *= l;

  c[1][X] = -2 * dot( dp, dq );

  diff( qp, q1, p1 );
  c[3][X] = -2 * dot( dp, qp );
  c[4][X] = 2 * dot( dq, qp );
  c[5][X] = dot( qp, qp );

  /*
   * for sanity for later
   */
  c[CONST][X] = sqrt( c[5][X] );
  if( c[CONST][X] > mach[2] ){
    c[THETA][X] = c[3][X] / ( 2 * c[CONST][X] );
    c[PHI][X] = c[4][X] / ( 2 * c[CONST][X] );
    c[THETA][X] > 1 ? c[THETA][X] = 1 :
      ( c[THETA][X] < -1 ? c[THETA][X] = -1 : c[THETA][X] );
    c[PHI][X] > 1 ? c[PHI][X] = 1 :
      ( c[PHI][X] < -1 ? c[PHI][X] = -1 : c[PHI][X] );
  }else{
    c[THETA][X] = c[1][X] * HALF;
    c[THETA][X] > 1 ? c[THETA][X] = 1 :
      ( c[THETA][X] < -1 ? c[THETA][X] = -1 : c[THETA][X] );
  }

  /*
   * see whether the direction vector is parallel to
   * the plane defined by the cross product of the other
   * this is just qp X dp dot dq or the determinant
   */
  c[PLANE][X] = det( qp, dp, dq );
}

/*
 * area of a polygon
 */
real
Area( real ( *p )[3], int np )
{
  int i, ii;
  real d1[3], d2[3];
  real a[3];
  real c[3];

  a[0] = a[1] = a[2] = 0;
  for( i = 1, ii = 2; ii < np; i++, ii++ ){
    diff( d1, p[i], p[0] );
    diff( d2, p[ii], p[0] );
    cross( c, d1, d2 );
    add( a, a, c );
  }

  return sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] ) * HALF;
}

/*
 * compute the quantities necessary if the two edges share a plane
 */
int
Bilinear( real c[ALLCOEFF][2] )
{
  if( c[PLANE][X] * c[PLANE][X] < mach[2] ){
    /*
     * guaranteed to share a plane
     */
    if( c[CONST][X] < mach[2] ){
      c[THETA][Y] = sin( acos( c[THETA][X] ) );
      c[PHI][X] = 1; c[PHI][Y] = 0;
    }else{
      real ts = sin( acos( c[THETA][X] ) );
      real ps = sin( acos( c[PHI][X] ) );
      
      if( c[1][X] * HALF - ( c[3][X] * c[4][X] ) / ( 4 * c[5][X] ) > 0 ){
	c[THETA][Y] = ts; c[PHI][Y] = ps;
      }else{
	c[THETA][Y] = -ts; c[PHI][Y] = ps;
      }
    }
    return 1;
  }else{
    return 0;
  }
}

/*
 * evaluate h^2(log(h)-1.5)
 */
#define	evalintp(f,c,s,t)	\
  (tmp[X]=(s)*(c)[THETA][X]+(t)*(c)[PHI][X]+(c)[CONST][X],\
   tmp[Y]=(s)*(c)[THETA][Y]+(t)*(c)[PHI][Y],\
   tmp2[X]=sqrx(tmp),tmp2[Y]=sqry(tmp),\
   tmp[Y]=ATAN2(tmp[Y],tmp[X]),tmp[X]=cmagsqr(tmp2),\
   tmp[X]=((tmp[X]>mach[0]?ONEQUARTER*log(tmp[X]):0)-THREEHALF),\
   (f)[X]+=cmulx(tmp,tmp2),(f)[Y]+=cmuly(tmp,tmp2))
#define	evalintm(f,c,s,t)	\
  (tmp[X]=(s)*(c)[THETA][X]+(t)*(c)[PHI][X]+(c)[CONST][X],\
   tmp[Y]=(s)*(c)[THETA][Y]+(t)*(c)[PHI][Y],\
   tmp2[X]=sqrx(tmp),tmp2[Y]=sqry(tmp),\
   tmp[Y]=ATAN2(tmp[Y],tmp[X]),tmp[X]=cmagsqr(tmp2),\
   tmp[X]=((tmp[X]>mach[0]?ONEQUARTER*log(tmp[X]):0)-THREEHALF),\
   (f)[X]-=cmulx(tmp,tmp2),(f)[Y]-=cmuly(tmp,tmp2))

real
IntegralPlanar( real c[ALLCOEFF][2] )
{
  real tmp[2], tmp2[2];	/* for evalint{p|m}() macro */
  real den[2], f[2];

  den[X] = cmulx( c[THETA], c[PHI] );
  den[Y] = -cmuly( c[THETA], c[PHI] );

  f[X] = f[Y] = 0;
  evalintp( f, c, c[0][X], c[2][X] );
  evalintm( f, c, c[0][X], 0 );
  evalintm( f, c, 0, c[2][X] );
  evalintp( f, c, 0, 0 );
  
  return cmulx( den, f );
}

/*
 * (L)ine (C)ircle (I)nter(s)ection
 */
int
Lcis( real xax[2], real yax[2], real rad[2], real cnt[2] )
{
  /*
   * see whether a circle at c with radius r
   * intersects the x or y axis
   */
  real cabs = cmagsqr( cnt );
  real rabs = cmagsqr( rad );
  real xdisc = cnt[X] * cnt[X] - cabs + rabs;
  real ydisc = cnt[Y] * cnt[Y] - cabs + rabs;
  int ret = 0;
  
  if( xdisc >= 0 ){
    xdisc = sqrt( xdisc );
    xax[0] = cnt[X] + xdisc;
    xax[1] = cnt[X] - xdisc;
    ret |= XSOL;
  }else{
    xax[0] = xax[1] = HUGE;
  }
  if( ydisc >= 0 ){
    ydisc = sqrt( ydisc );
    yax[0] = cnt[Y] + ydisc;
    yax[1] = cnt[Y] - ydisc;
    ret |= YSOL;
  }else{
    yax[0] = yax[1] = HUGE;
  }

  return ret;
}

/*
 * is the given intersection of the circle with one of the axes
 * in the range between l and u?
 */
#define	seghitx(x,psi)	\
  (tmp=ATAN2(-rad[Y]*((x)-cnt[X])-rad[X]*cnt[Y],\
	      rad[X]*((x)-cnt[X])-rad[Y]*cnt[Y]),\
   (psi)[L]<(psi)[U]?(tmp>=(psi)[L]?tmp<=(psi)[U]:0):\
                     (tmp>=(psi)[U]?tmp<=(psi)[L]:0))
#define	seghity(x,psi)	\
  (tmp=ATAN2(rad[Y]*cnt[X]+rad[X]*((x)-cnt[Y]),\
	      -rad[X]*cnt[X]+rad[Y]*((x)-cnt[Y])),\
   (psi)[L]<(psi)[U]?(tmp>=(psi)[L]?tmp<=(psi)[U]:0):\
                     (tmp>=(psi)[U]?tmp<=(psi)[L]:0))

/*
 * based on whether a circle segment between angles (l)ower
 * and (u)pper intersects any of the coordinate axes, return
 * an index for the particular extension of the complex
 * logarithm which does not put the branchcut under our path
 * (of integration).
 */
int
LogSelect( real rad[2], real cnt[2], real psi[2] )
{
  real tmp; /* seghit() needs this one */
  real xax[2], yax[2];
  int sl;

  /*
   * get intersections, if any and return the index of the
   * particular extension of the logarithm to be used
   */
  /*
   * there is some debate whether the below should use
   * <= (>=) or < (>) depending on whether atan(0,0) is
   * considered a well defined point (it is not, but in
   * practice 0 is returned and that seems to work...)
   */
  sl = Lcis( xax, yax, rad, cnt );
  if( !( ( sl & XSOL ) &&
	( ( ( xax[0] <= 0 ) && seghitx( xax[0], psi ) ) ||
	 ( ( xax[1] <= 0 ) && seghitx( xax[1], psi ) ) ) ) ){
    return 0;
  }else if( !( ( sl & YSOL ) &&
	      ( ( ( yax[0] <= 0 ) && seghity( yax[0], psi ) ) ||
	       ( ( yax[1] <= 0 ) && seghity( yax[1], psi ) ) ) ) ){
    return 1;
  }else if( !( ( sl & YSOL ) &&
	      ( ( ( yax[0] >= 0 ) && seghity( yax[0], psi ) ) ||
	       ( ( yax[1] >= 0 ) && seghity( yax[1], psi ) ) ) ) ){
    return 3;
  }else if( !( ( sl & XSOL ) &&
	      ( ( ( xax[0] >= 0 ) && seghitx( xax[0], psi ) ) ||
	       ( ( xax[1] >= 0 ) && seghitx( xax[1], psi ) ) ) ) ){
    return 2;
  }else{
    assert( 0 );
    return 0;
  }
}

/*
 * only the real part of M
 */
real
RM( real z[2] )
{
  real t2_1[2]; /* conj(t^2 - 1) */
  real t2_1_2[2]; /* conj(t^2 - 1)^2 */
  real t_1[2], tp1[2]; /* t-1, t+1 */
  real quot;

  t2_1[X] = sqrx( z ) - 1; t2_1[Y] = -sqry( z );
  t2_1_2[X] = sqrx( t2_1 ); t2_1_2[Y] = sqry( t2_1 );
  quot = 1 / cmagsqr( t2_1 );
  t_1[X] = z[X] - 1; t_1[Y] = z[Y];
  tp1[X] = z[X] + 1; tp1[Y] = z[Y];

  return ONEQUARTER * cmulx( z, t2_1_2 ) * quot * quot +
      ONEEIGHTTH * cmulx( z, t2_1 ) * quot +
	ONETHIRTYSECOND * log( cmagsqr( t_1 ) / cmagsqr( tp1 ) );
}

/*
 * turn segment on unit circle into segment on given circle
 */
#define	circlemap(p,t)	\
    ((p)[X]=1-cnt[X]-cmulx(rad,t),\
     (p)[Y]=-cnt[Y]-cmuly(rad,t))

real
IDilogPath( int k, real rad[2], real cnt[2], real c[ALLCOEFF][2] )
{
  real xax[2], yax[2], end[2], start[2];
  real tmp; /* seghit() needs this one */
  real parg, p[2];
  real f;
  int sl;

  /*
   * if we don't walk over the branchcut we just apply
   * the dilog
   */
  circlemap( end, c[UPPER] );
  circlemap( start, c[LOWER] );
  f = IDilog( end ) - IDilog( start );

  /*
   * return the points where the given circle cuts the real axis
   */
  sl = Lcis( xax, yax, rad, cnt );

  /*
   * we have to correct for the branchcut:
   * 
   * only if there are any solutions,
   * the solutions are on the negative real axis,
   * they are within the segment of interest,
   * there is only one intersection (we actually
   * cross)
   */
  if( ( ( k == 1 ) || ( k == 3 ) ) &&
     ( ( ( xax[0] <= 0 ) && seghitx( xax[0], c[PSI] ) ) !=
      ( ( xax[1] <= 0 ) && seghitx( xax[1], c[PSI] ) ) ) ){
    /*
     * for the dilog we have an argument of the form
     * 1-z, compute it and its arg
     */
    if( ( xax[0] <= 0 ) && seghitx( xax[0], c[PSI] ) ){
      p[X] = 1 - xax[0]; p[Y] = 0;
      parg = ATAN2( -rad[Y] * ( xax[0] - cnt[X] ) - rad[X] * cnt[Y],
		    rad[X] * ( xax[0] - cnt[X] ) - rad[Y] * cnt[Y] );
    }else{
      p[X] = 1 - xax[1]; p[Y] = 0;
      parg = ATAN2( -rad[Y] * ( xax[1] - cnt[X] ) - rad[X] * cnt[Y],
		    rad[X] * ( xax[1] - cnt[X] ) - rad[Y] * cnt[Y] );
    }

    if( k == 1 ){
      if( parg * ( c[PSI][U] - c[PSI][L] ) * ( p[X] - cnt[X] ) < 0 ){
	f += -M_PIF *
	  ( 2 * log( cmagsqr( p ) ) - log( cmagsqr( start ) ) );
      }else{
	f += -M_PIF * log( cmagsqr( end ) );
      }
    }else if( k == 3 ){
      if( parg * ( c[PSI][U] - c[PSI][L] ) * ( p[X] - cnt[X] ) < 0 ){
	f += -M_PIF *
	  ( 2 * log( cmagsqr( p ) ) - log( cmagsqr( end ) ) );
      }else{
	f += -M_PIF * log( cmagsqr( start ) );
      }
    }
  }

  return f;
}

/*
 * 2(b-t)/((b^2-1)(t^2-1))+
 */
#define	subexpr1(c)	\
  (tmp[X]=sqrx(z)-1,tmp[Y]=sqry(z),\
   tmp2[X]=sqrx(c)-1,tmp2[Y]=sqry(c),\
   denom[X]=cmulx(tmp,tmp2),denom[Y]=-cmuly(tmp,tmp2),\
   fac[X]=2*(z[X]-(c)[X]),fac[Y]=2*(z[Y]-(c)[Y]),\
   cmuly(fac,denom)/cmagsqr(denom))

/*
 * -p1[t-1]b/(1+b)^2
 */
#define	subexpr2(c)	\
  (tmp[X]=z[X]+1,tmp[Y]=z[Y],\
   tmp2[X]=sqrx(tmp),tmp2[Y]=-sqry(tmp),\
   fac[X]=-cmulx(z,tmp2),fac[Y]=-cmuly(z,tmp2),\
   tmp[X]=(c)[X]-1,tmp[Y]=(c)[Y],\
   (fac[X]*ilog(p1,tmp[X],tmp[Y])+\
    fac[Y]*HALF*log(cmagsqr(tmp)))/cmagsqr(tmp2))

/*
 * -p1[1+t]b/(b-1)^2
 */
#define	subexpr3(c)	\
  (tmp[X]=z[X]-1,tmp[Y]=z[Y],\
   tmp2[X]=sqrx(tmp),tmp2[Y]=-sqry(tmp),\
   fac[X]=-cmulx(z,tmp2),fac[Y]=-cmuly(z,tmp2),\
   tmp[X]=(c)[X]+1,tmp[Y]=(c)[Y],\
   (fac[X]*ilog(p1,tmp[X],tmp[Y])+\
    fac[Y]*HALF*log(cmagsqr(tmp)))/cmagsqr(tmp2))

/*
 * 2(b+t)(1+b t)((b-t)^2+(b t-1)^2)/((b^2-1)^2(t^2-1)^2)
 */
#define	subexpr4(sum,c)	\
  (tmp[X]=sqrx(z)-1,tmp[Y]=sqry(z),\
   tmp2[X]=sqrx(c)-1,tmp2[Y]=sqry(c),\
   fac[X]=sqrx(tmp),fac[Y]=sqry(tmp),\
   tmp[X]=sqrx(tmp2),tmp[Y]=sqry(tmp2),\
   denom[X]=cmulx(fac,tmp),denom[Y]=-cmuly(fac,tmp),\
   quot=1/cmagsqr(denom),\
   denom[X]*=quot,denom[Y]*=quot,\
   tmp[X]=z[X]+(c)[X],tmp[Y]=z[Y]+(c)[Y],\
   tmp2[X]=cmulx(z,c)+1,tmp2[Y]=cmuly(z,c),\
   fac[X]=2*cmulx(tmp,tmp2),fac[Y]=2*cmuly(tmp,tmp2),\
   tmp2[X]-=2,\
   tmp[X]=sqrx(tmp2),tmp[Y]=sqry(tmp2),\
   tmp2[X]=z[X]-(c)[X],tmp2[Y]=z[Y]-(c)[Y],\
   tmp[X]+=sqrx(tmp2),tmp[Y]+=sqry(tmp2),\
   tmp2[X]=cmulx(fac,tmp),tmp2[Y]=cmuly(fac,tmp),\
   (sum)[X]+=cmulx(tmp2,denom),(sum)[Y]=cmuly(tmp2,denom))

/*
 * p2[(1-t)/(1+b)]
 */
#define	subexpr5(sum,c)	\
  (tmp[X]=1-(c)[X],tmp[Y]=-(c)[Y],\
   tmp2[X]=1+z[X],tmp2[Y]=-z[Y],\
   quot=1/cmagsqr(tmp2),\
   tmp2[0]*=quot,tmp2[1]*=quot,\
   fac[X]=cmulx(tmp,tmp2),fac[Y]=cmuly(tmp,tmp2),\
   (sum)[X]+=HALF*log(cmagsqr(fac)),(sum)[Y]+=ilog(p2,fac[X],fac[Y]))

/*
 * -p3[(1+t)/(1-b)]
 */
#define	subexpr6(sum,c)	\
  (tmp[X]=1+(c)[X],tmp[Y]=(c)[Y],\
   tmp2[X]=1-z[X],tmp2[Y]=z[Y],\
   quot=1/cmagsqr(tmp2),\
   tmp2[0]*=quot,tmp2[1]*=quot,\
   fac[X]=cmulx(tmp,tmp2),fac[Y]=cmuly(tmp,tmp2),\
   (sum)[X]-=HALF*log(cmagsqr(fac)),(sum)[Y]-=ilog(p3,fac[X],fac[Y]))

/*
 * sum*p1[b+t]
 */
#define	subexpr7(sum,c)	\
  (tmp[X]=z[X]+(c)[X],tmp[Y]=z[Y]+(c)[Y],\
   (sum)[X]*ilog(p1,tmp[X],tmp[Y])+HALF*(sum)[Y]*log(cmagsqr(tmp)))

real
ILogPart( int p1, real z[2], real c[ALLCOEFF][2] )
{
  real rad[2], cnt[2]; /* radius, center */
  real tmp[2], tmp2[2]; /* the macros need these */
  real fac[2]; /* generic factor holding slot */
  real denom[2]; /* generic denominator holding slot */
  real f = 0; /* the final result */
  real quot; /* for quotients */
  real sum[2];
  int p2, p3;
  
  /*
   * get the right extensions of the log
   */
  cnt[X] = 1 + z[X]; cnt[Y] = -z[Y];
  quot = 1 / cmagsqr( cnt );
  rad[X] = -( cnt[X] *= quot ); rad[Y] = -( cnt[Y] *= quot );
  p2 = LogSelect( rad, cnt, c[PSI] );
  f += IDilogPath( p2, rad, cnt, c );

  cnt[X] = 1 - z[X]; cnt[Y] = z[Y];
  quot = 1 / cmagsqr( cnt );
  rad[X] = cnt[X] *= quot; rad[Y] = cnt[Y] *= quot;
  p3 = LogSelect( rad, cnt, c[PSI] );
  f -= IDilogPath( p3, rad, cnt, c );

  /*
   * 2(b-t)/((b^2-1)(t^2-1))
   */
  f += subexpr1( c[UPPER] );
  
  /*
   * -p1[t-1]b/(1+b)^2
   */
  f += subexpr2( c[UPPER] );

  /*
   * -p1[1+t]b/(b-1)^2
   */
  f += subexpr3( c[UPPER] );

  /*
   * sum += 2(b+t)(1+b t)((b-t)^2+(b t-1)^2)/((b^2-1)^2(t^2-1)^2)
   */
  sum[X] = sum[Y] = 0;
  subexpr4( sum, c[UPPER] );

  /*
   * sum += p2[(1-t)/(1+b)]
   */
  subexpr5( sum, c[UPPER] );

  /*
   * sum += -p3[(1+t)/(1-b)]
   */
  subexpr6( sum, c[UPPER] );

  /*
   * sum*p1[b+t]
   */
  f += subexpr7( sum, c[UPPER] );

  f -= subexpr1( c[LOWER] );
  f -= subexpr2( c[LOWER] );
  f -= subexpr3( c[LOWER] );
  sum[X] = sum[Y] = 0;
  subexpr4( sum, c[LOWER] );
  subexpr5( sum, c[LOWER] );
  subexpr6( sum, c[LOWER] );
  f -= subexpr7( sum, c[LOWER] );

  return ONESIXTEENTH * f;
}

/*
 * the part of the overall integral due to the value of k
 */
#define	kpart(k,c)	\
  ((2*(k)+1)*M_PIF*(RM((c)[UPPER])-RM((c)[LOWER])))

/*
 * the nasty integral
 */
real
ILogIntegral( real c[ALLCOEFF][2], real s )
{
  static real unit[2] = { 1, 0 };
  real mc17[2]; /* -c[17] */
  real mc18[2]; /* -c[18] */
  real logu, logl;
  /*
   * to find k we are lazy. Evaluate the original
   * form of the inverse tangent at the beginning
   * and end of the path
   */
  real tanu = -ATAN2( 2 * s + c[1][X] * c[2][X] + c[3][X],
		     sqrt( ( c[10][X] * c[2][X] + c[11][X] ) *
			  c[2][X] + c[12][X] ) );
  real tanl = -ATAN2( 2 * s + c[3][X], sqrt( c[12][X] ) );
  int p1, p2, p3, p4, ku, kl, k;
  
  /*
   * after decomposing the inverse tangent into 4 logarithms
   * first find out which of the extensions we need to use
   */
  mc17[X] = -c[17][X]; mc17[Y] = -c[17][Y];
  p1 = LogSelect( unit, mc17, c[PSI] );
  mc18[X] = -c[18][X]; mc18[Y] = -c[18][Y];
  p2 = LogSelect( unit, mc18, c[PSI] );
  p3 = LogSelect( unit, c[17], c[PSI] );
  p4 = LogSelect( unit, c[18], c[PSI] );
  
  /*
   * now evaluate this alternative form at the two endpoints
   */
  logu =
    HALF * ( M_PIF +
	   ilog( p1, c[UPPER][X] - c[17][X], c[UPPER][Y] - c[17][Y] ) +
	   ilog( p2, c[UPPER][X] - c[18][X], c[UPPER][Y] - c[18][Y] ) -
	   ilog( p3, c[UPPER][X] + c[17][X], c[UPPER][Y] + c[17][Y] ) -
	   ilog( p4, c[UPPER][X] + c[18][X], c[UPPER][Y] + c[18][Y] ) );
  logl =
    HALF * ( M_PIF +
	   ilog( p1, c[LOWER][X] - c[17][X], c[LOWER][Y] - c[17][Y] ) +
	   ilog( p2, c[LOWER][X] - c[18][X], c[LOWER][Y] - c[18][Y] ) -
	   ilog( p3, c[LOWER][X] + c[17][X], c[LOWER][Y] + c[17][Y] ) -
	   ilog( p4, c[LOWER][X] + c[18][X], c[LOWER][Y] + c[18][Y] ) );

  /*
   * and compare to find k. This whole shebang should be
   * done more intelligently by figuring out exactly what
   * happens to the integration path and chosing k directly.
   */
  ku = floor( ( tanu - logu ) / M_PIF + HALF );
  kl = floor( ( tanl - logl ) / M_PIF + HALF );

  /*
   * needless to say this error check should not be necessary...
   */
  assert( ku == kl );
  assert( ku == -1 || ku == 0 || ku == 1 );

  k = ku;

  return kpart( k, c ) +
    ILogPart( p1, mc17, c ) + ILogPart( p2, mc18, c ) -
      ILogPart( p3, c[17], c ) - ILogPart( p4, c[18], c );
}

/*
 * everything but the tricky integral
 */
#define	firstpart(c,s,t)	\
  (((s)+(c)[3][X]*HALF)*\
   G(1,(c)[4][X]+(c)[1][X]*(s),((s)+(c)[3][X])*(s)+(c)[5][X],t)+\
   (c)[1][X]*HALF*\
   H(1,(c)[4][X]+(c)[1][X]*(s),((s)+(c)[3][X])*(s)+(c)[5][X],t))

real
Integral( real c[ALLCOEFF][2] )
{
  real f;

  if( Bilinear( c ) ){
    f = IntegralPlanar( c );
  }else{
    real tmp, c16magsqr;
    
    f = firstpart( c, c[0][X], c[2][X] ) - firstpart( c, 0, c[2][X] ) -
      firstpart( c, c[0][X], 0 ) + firstpart( c, 0, 0 ) -
	2 * c[0][X] * c[2][X];

    /*
     * all the same for t = lower/upper
     */
    c[10][X] = 4 - c[1][X] * c[1][X];
    c[11][X] = 4 * c[4][X] - 2 * c[1][X] * c[3][X];
    c[12][X] = 4 * c[5][X] - c[3][X] * c[3][X];
    c[13][X] = c[11][X] / ( 2 * c[10][X] );
    c[13][Y] = -sqrt( 4 * c[10][X] * c[12][X] - c[11][X] * c[11][X] ) /
      ( 2 * c[10][X] );
    tmp = 1 / hypot( c[13][X], c[13][Y] );
    c[LOWER][X] = c[13][X] * tmp;
    c[LOWER][Y] = c[13][Y] * tmp;
    if( c[LOWER][Y] < 0 ){
      c[LOWER][X] = -c[LOWER][X]; c[LOWER][Y] = -c[LOWER][Y];
    }
    c[PSI][L] = ATAN2( c[LOWER][Y], c[LOWER][X] );
    tmp = 1 / hypot( c[13][X] + c[2][X], c[13][Y] );
    c[UPPER][X] = ( c[13][X] + c[2][X] ) * tmp;
    c[UPPER][Y] = c[13][Y] * tmp;
    if( c[UPPER][Y] < 0 ){
      c[UPPER][X] = -c[UPPER][X]; c[UPPER][Y] = -c[UPPER][Y];
    }
    c[PSI][U] = ATAN2( c[UPPER][Y], c[UPPER][X] );
    c[14][Y] = c[13][Y] * -2;
    c[15][Y] = sqrt( c[10][X] ) * c[14][Y];

    /*
     * s = c[0]
     */
    c[16][X] = c[1][X] * c[13][X] - c[3][X] - 2 * c[0][X];
    c[16][Y] = c[1][X] * c[13][Y];
    tmp = 1 / ( c16magsqr = cmagsqr( c[16] ) );
    if( c16magsqr > mach[0] ){
      c[17][X] = c[18][X] = -HALF * c[15][Y] * c[16][X] * tmp;
      c[17][Y] = c[18][Y] = -HALF * c[15][Y] * c[16][Y] * tmp;
      tmp *= HALF * sqrt( c[15][Y] * c[15][Y] + 4 * c16magsqr );
      c[17][X] += tmp * c[16][X];
      c[17][Y] += tmp * c[16][Y];
      c[18][X] -= tmp * c[16][X];
      c[18][Y] -= tmp * c[16][Y];
      
      f -= c[14][Y] * c[15][Y] * ILogIntegral( c, c[0][X] );
    }

    /*
     * s = 0
     */
    c[16][X] = c[1][X] * c[13][X] - c[3][X];
    c[16][Y] = c[1][X] * c[13][Y];
    tmp = 1 / ( c16magsqr = cmagsqr( c[16] ) );
    if( c16magsqr > mach[0] ){
      c[17][X] = c[18][X] = -HALF * c[15][Y] * c[16][X] * tmp;
      c[17][Y] = c[18][Y] = -HALF * c[15][Y] * c[16][Y] * tmp;
      tmp *= HALF * sqrt( c[15][Y] * c[15][Y] + 4 * c16magsqr );
      c[17][X] += tmp * c[16][X];
      c[17][Y] += tmp * c[16][Y];
      c[18][X] -= tmp * c[16][X];
      c[18][Y] -= tmp * c[16][Y];
      
      f += c[14][Y] * c[15][Y] * ILogIntegral( c, 0 );
    }
  }

  return f;
}

real
FormFactor( real ( *p )[3], int np, real ( *q )[3], int nq )
{
  int i, j, ii, jj;
  real ff = 0;
  real c[ALLCOEFF][2];

  for( i = 0, ii = 1; i < np; i++, ii = ( i + 1 ) % np ){
    for( j = 0, jj = 1; j < nq; j++, jj = ( j + 1 ) % nq ){
      Pair( c, p[i], p[ii], q[j], q[jj] );
      if( fabs( c[1][X] ) > mach[2] ){
	ff -= c[1][X] * Integral( c );
      }
    }
  }

  return ff / ( ( ( real )( M_PIF * 8 ) ) * Area( p, np ) );
}
