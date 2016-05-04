/*
 * $Id: claussen.h,v 1000.2 93/12/05 15:03:07 ps Exp $
 * $Source: /usr/graphics/project/ff/claussen.h,v $
 * $Log:	claussen.h,v $
 * Revision 1000.2  93/12/05  15:03:07  ps
 * replaced plane test with a new one and made all mach[] tests much tighter.
 * Also fixed a but in evalint{m|p}()
 * 
 * Revision 1000.1  93/06/18  06:56:08  ps
 * Bump to release, no file changes
 * 
 * Revision 1.2  93/04/14  22:04:04  ps
 * header file for claussen's function
 * 
 * Revision 1.1  93/04/06  21:25:48  ps
 * Initial revision
 * 
 */
#ifndef	CLAUSSEN_H
#define	CLAUSSEN_H

static	char	rcs_id_claussen_h[] = "$Header: /usr/graphics/project/ff/claussen.h,v 1000.2 93/12/05 15:03:07 ps Exp $";

extern	float	*machf;
extern	double	mach[5];
extern	float	claussenf( float x );
extern	double	claussen( double x );

#endif	/* CLAUSSEN_H */
