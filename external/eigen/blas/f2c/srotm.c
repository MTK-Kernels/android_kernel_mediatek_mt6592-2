
#include "datatypes.h"

 int srotm_(integer *n, real *sx, integer *incx, real *sy, 
	integer *incy, real *sparam)
{
    

    static real zero = 0.f;
    static real two = 2.f;

    
    integer i__1, i__2;

    
    integer i__;
    real w, z__;
    integer kx, ky;
    real sh11, sh12, sh21, sh22, sflag;
    integer nsteps;

















    
    --sparam;
    --sy;
    --sx;

    

    sflag = sparam[1];
    if (*n <= 0 || sflag + two == zero) {
	goto L140;
    }
    if (! (*incx == *incy && *incx > 0)) {
	goto L70;
    }

    nsteps = *n * *incx;
    if (sflag < 0.f) {
	goto L50;
    } else if (sflag == 0) {
	goto L10;
    } else {
	goto L30;
    }
L10:
    sh12 = sparam[4];
    sh21 = sparam[3];
    i__1 = nsteps;
    i__2 = *incx;
    for (i__ = 1; i__2 < 0 ? i__ >= i__1 : i__ <= i__1; i__ += i__2) {
	w = sx[i__];
	z__ = sy[i__];
	sx[i__] = w + z__ * sh12;
	sy[i__] = w * sh21 + z__;
    }
    goto L140;
L30:
    sh11 = sparam[2];
    sh22 = sparam[5];
    i__2 = nsteps;
    i__1 = *incx;
    for (i__ = 1; i__1 < 0 ? i__ >= i__2 : i__ <= i__2; i__ += i__1) {
	w = sx[i__];
	z__ = sy[i__];
	sx[i__] = w * sh11 + z__;
	sy[i__] = -w + sh22 * z__;
    }
    goto L140;
L50:
    sh11 = sparam[2];
    sh12 = sparam[4];
    sh21 = sparam[3];
    sh22 = sparam[5];
    i__1 = nsteps;
    i__2 = *incx;
    for (i__ = 1; i__2 < 0 ? i__ >= i__1 : i__ <= i__1; i__ += i__2) {
	w = sx[i__];
	z__ = sy[i__];
	sx[i__] = w * sh11 + z__ * sh12;
	sy[i__] = w * sh21 + z__ * sh22;
    }
    goto L140;
L70:
    kx = 1;
    ky = 1;
    if (*incx < 0) {
	kx = (1 - *n) * *incx + 1;
    }
    if (*incy < 0) {
	ky = (1 - *n) * *incy + 1;
    }

    if (sflag < 0.f) {
	goto L120;
    } else if (sflag == 0) {
	goto L80;
    } else {
	goto L100;
    }
L80:
    sh12 = sparam[4];
    sh21 = sparam[3];
    i__2 = *n;
    for (i__ = 1; i__ <= i__2; ++i__) {
	w = sx[kx];
	z__ = sy[ky];
	sx[kx] = w + z__ * sh12;
	sy[ky] = w * sh21 + z__;
	kx += *incx;
	ky += *incy;
    }
    goto L140;
L100:
    sh11 = sparam[2];
    sh22 = sparam[5];
    i__2 = *n;
    for (i__ = 1; i__ <= i__2; ++i__) {
	w = sx[kx];
	z__ = sy[ky];
	sx[kx] = w * sh11 + z__;
	sy[ky] = -w + sh22 * z__;
	kx += *incx;
	ky += *incy;
    }
    goto L140;
L120:
    sh11 = sparam[2];
    sh12 = sparam[4];
    sh21 = sparam[3];
    sh22 = sparam[5];
    i__2 = *n;
    for (i__ = 1; i__ <= i__2; ++i__) {
	w = sx[kx];
	z__ = sy[ky];
	sx[kx] = w * sh11 + z__ * sh12;
	sy[ky] = w * sh21 + z__ * sh22;
	kx += *incx;
	ky += *incy;
    }
L140:
    return 0;
} 

