
#include "datatypes.h"

 int zhpmv_(char *uplo, integer *n, doublecomplex *alpha, 
	doublecomplex *ap, doublecomplex *x, integer *incx, doublecomplex *
	beta, doublecomplex *y, integer *incy, ftnlen uplo_len)
{
    
    integer i__1, i__2, i__3, i__4, i__5;
    doublereal d__1;
    doublecomplex z__1, z__2, z__3, z__4;

    
    void d_cnjg(doublecomplex *, doublecomplex *);

    
    integer i__, j, k, kk, ix, iy, jx, jy, kx, ky, info;
    doublecomplex temp1, temp2;
    extern logical lsame_(char *, char *, ftnlen, ftnlen);
    extern  int xerbla_(char *, integer *, ftnlen);

















/*           element vector y. On exit, Y is overwritten by the updated */




/*  -- Written on 22-October-1986. */




    
    --y;
    --x;
    --ap;

    
    info = 0;
    if (! lsame_(uplo, "U", (ftnlen)1, (ftnlen)1) && ! lsame_(uplo, "L", (
	    ftnlen)1, (ftnlen)1)) {
	info = 1;
    } else if (*n < 0) {
	info = 2;
    } else if (*incx == 0) {
	info = 6;
    } else if (*incy == 0) {
	info = 9;
    }
    if (info != 0) {
	xerbla_("ZHPMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0 || (alpha->r == 0. && alpha->i == 0. && (beta->r == 1. && 
                                                         beta->i == 0.))) {
	return 0;
    }


    if (*incx > 0) {
	kx = 1;
    } else {
	kx = 1 - (*n - 1) * *incx;
    }
    if (*incy > 0) {
	ky = 1;
    } else {
	ky = 1 - (*n - 1) * *incy;
    }



    if (beta->r != 1. || beta->i != 0.) {
	if (*incy == 1) {
	    if (beta->r == 0. && beta->i == 0.) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = i__;
		    y[i__2].r = 0., y[i__2].i = 0.;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = i__;
		    i__3 = i__;
		    z__1.r = beta->r * y[i__3].r - beta->i * y[i__3].i, 
			    z__1.i = beta->r * y[i__3].i + beta->i * y[i__3]
			    .r;
		    y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		}
	    }
	} else {
	    iy = ky;
	    if (beta->r == 0. && beta->i == 0.) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = iy;
		    y[i__2].r = 0., y[i__2].i = 0.;
		    iy += *incy;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = iy;
		    i__3 = iy;
		    z__1.r = beta->r * y[i__3].r - beta->i * y[i__3].i, 
			    z__1.i = beta->r * y[i__3].i + beta->i * y[i__3]
			    .r;
		    y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		    iy += *incy;
		}
	    }
	}
    }
    if (alpha->r == 0. && alpha->i == 0.) {
	return 0;
    }
    kk = 1;
    if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = j;
		z__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, z__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		k = kk;
		i__2 = j - 1;
		for (i__ = 1; i__ <= i__2; ++i__) {
		    i__3 = i__;
		    i__4 = i__;
		    i__5 = k;
		    z__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    z__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		    y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		    d_cnjg(&z__3, &ap[k]);
		    i__3 = i__;
		    z__2.r = z__3.r * x[i__3].r - z__3.i * x[i__3].i, z__2.i =
			     z__3.r * x[i__3].i + z__3.i * x[i__3].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		    ++k;
		}
		i__2 = j;
		i__3 = j;
		i__4 = kk + j - 1;
		d__1 = ap[i__4].r;
		z__3.r = d__1 * temp1.r, z__3.i = d__1 * temp1.i;
		z__2.r = y[i__3].r + z__3.r, z__2.i = y[i__3].i + z__3.i;
		z__4.r = alpha->r * temp2.r - alpha->i * temp2.i, z__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = z__2.r + z__4.r, z__1.i = z__2.i + z__4.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		kk += j;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = jx;
		z__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, z__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		ix = kx;
		iy = ky;
		i__2 = kk + j - 2;
		for (k = kk; k <= i__2; ++k) {
		    i__3 = iy;
		    i__4 = iy;
		    i__5 = k;
		    z__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    z__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		    y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		    d_cnjg(&z__3, &ap[k]);
		    i__3 = ix;
		    z__2.r = z__3.r * x[i__3].r - z__3.i * x[i__3].i, z__2.i =
			     z__3.r * x[i__3].i + z__3.i * x[i__3].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		    ix += *incx;
		    iy += *incy;
		}
		i__2 = jy;
		i__3 = jy;
		i__4 = kk + j - 1;
		d__1 = ap[i__4].r;
		z__3.r = d__1 * temp1.r, z__3.i = d__1 * temp1.i;
		z__2.r = y[i__3].r + z__3.r, z__2.i = y[i__3].i + z__3.i;
		z__4.r = alpha->r * temp2.r - alpha->i * temp2.i, z__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = z__2.r + z__4.r, z__1.i = z__2.i + z__4.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		jx += *incx;
		jy += *incy;
		kk += j;
	    }
	}
    } else {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = j;
		z__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, z__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		i__2 = j;
		i__3 = j;
		i__4 = kk;
		d__1 = ap[i__4].r;
		z__2.r = d__1 * temp1.r, z__2.i = d__1 * temp1.i;
		z__1.r = y[i__3].r + z__2.r, z__1.i = y[i__3].i + z__2.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		k = kk + 1;
		i__2 = *n;
		for (i__ = j + 1; i__ <= i__2; ++i__) {
		    i__3 = i__;
		    i__4 = i__;
		    i__5 = k;
		    z__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    z__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		    y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		    d_cnjg(&z__3, &ap[k]);
		    i__3 = i__;
		    z__2.r = z__3.r * x[i__3].r - z__3.i * x[i__3].i, z__2.i =
			     z__3.r * x[i__3].i + z__3.i * x[i__3].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		    ++k;
		}
		i__2 = j;
		i__3 = j;
		z__2.r = alpha->r * temp2.r - alpha->i * temp2.i, z__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = y[i__3].r + z__2.r, z__1.i = y[i__3].i + z__2.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		kk += *n - j + 1;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = jx;
		z__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, z__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		i__2 = jy;
		i__3 = jy;
		i__4 = kk;
		d__1 = ap[i__4].r;
		z__2.r = d__1 * temp1.r, z__2.i = d__1 * temp1.i;
		z__1.r = y[i__3].r + z__2.r, z__1.i = y[i__3].i + z__2.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		ix = jx;
		iy = jy;
		i__2 = kk + *n - j;
		for (k = kk + 1; k <= i__2; ++k) {
		    ix += *incx;
		    iy += *incy;
		    i__3 = iy;
		    i__4 = iy;
		    i__5 = k;
		    z__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    z__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		    y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		    d_cnjg(&z__3, &ap[k]);
		    i__3 = ix;
		    z__2.r = z__3.r * x[i__3].r - z__3.i * x[i__3].i, z__2.i =
			     z__3.r * x[i__3].i + z__3.i * x[i__3].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		}
		i__2 = jy;
		i__3 = jy;
		z__2.r = alpha->r * temp2.r - alpha->i * temp2.i, z__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = y[i__3].r + z__2.r, z__1.i = y[i__3].i + z__2.i;
		y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		jx += *incx;
		jy += *incy;
		kk += *n - j + 1;
	    }
	}
    }

    return 0;


} 

