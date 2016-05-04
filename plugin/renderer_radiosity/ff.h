/*
 * $Id: ff.h,v 1000.4 93/12/28 18:23:17 ps Exp $
 * $Source: /usr/graphics/project/ff/ff.h,v $
 * $Log:	ff.h,v $
 * Revision 1000.4  93/12/28  18:23:17  ps
 * removed not used warning by wrapping up rcs string
 * 
 * Revision 1000.3  93/12/05  15:03:25  ps
 * replaced plane test with a new one and made all mach[] tests much tighter.
 * Also fixed a but in evalint{m|p}()
 * 
 * Revision 1000.2  93/11/28  13:14:37  ps
 * moved ALLCOEFF to ff.h
 * 
 * Revision 1000.1  93/06/18  06:56:09  ps
 * Bump to release, no file changes
 * 
 * Revision 1.1  93/04/14  22:03:29  ps
 * Initial revision
 * 
 */
#ifndef	FF_H
#define	FF_H

//static char
//__rcs_ref_ff_h( void )
//{
//  static char rcs_id_h[] = "$Header: /usr/graphics/project/ff/ff.h,v 1000.4 93/12/28 18:23:17 ps Exp $";
//  return rcs_id_h[0];
//}

extern	float	IDilogf( float z[2] );
extern	double	IDilog( double z[2] );
extern	float	Gf( float a, float b, float c, float t );
extern	double	G( double a, double b, double c, double t );
extern	float	Hf( float a, float b, float c, float t );
extern	double	H( double a, double b, double c, double t );
extern	void	Pairf( float ( *c )[2],
		      float p1[3], float p2[3],
		      float q1[3], float q2[3] );
extern	void	Pair( double ( *c )[2],
		      double p1[3], double p2[3],
		      double q1[3], double q2[3] );
extern	float	Areaf( float ( *p )[3], int np );
extern	double	Area( double ( *p )[3], int np );
extern	int	Bilinearf( float ( *c )[2] );
extern	int	Bilinear( double ( *c )[2] );
extern	float	IntegralPlanarf( float ( *c )[2] );
extern	double	IntegralPlanar( double ( *c )[2] );
extern	int	Lcisf( float xax[2], float yax[2],
		      float rad[2], float cnt[2] );
extern	int	Lcis( double xax[2], double yax[2],
		      double rad[2], double cnt[2] );
extern	int	LogSelectf( float rad[2], float cnt[2], float psi[2] );
extern	int	LogSelect( double rad[2], double cnt[2], double psi[2] );
extern	float	RMf( float z[2] );
extern	double	RM( double z[2] );
extern	float	IDilogPathf( int k, float rad[2], float cnt[2],
			    float ( *c )[2] );
extern	double	IDilogPath( int k, double rad[2], double cnt[2],
			    double ( *c )[2] );
extern	float	ILogPartf( int p1, float z[2], float ( *c )[2] );
extern	double	ILogPart( int p1, double z[2], double ( *c )[2] );
extern	float	ILogIntegralf( float ( *c )[2], float s );
extern	double	ILogIntegral( double ( *c )[2], double s );
extern	float	Integralf( float ( *c )[2] );
extern	double	Integral( double ( *c )[2] );
extern	float	FormFactorf( float ( *p )[3], int np,
			    float ( *q )[3], int nq );
extern	double	FormFactor( double ( *p )[3], int np,
			    double ( *q )[3], int nq );

extern	int	fferror;
#define	NO_FF_ERROR	0
#define	INCONSISTENT_K	1
#define	OUT_OF_RANGE_K	2

/*
 * this is really defined in ffp.c but moved
 * out here to support custom calling sequences
 */
#define	ALLCOEFF	21	/* length of c array */

#endif	/* FF_H */
