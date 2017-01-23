
#include "datatypes.h"

 int chpmv_(char *uplo, integer *n, complex *alpha, complex *
	ap, complex *x, integer *incx, complex *beta, complex *y, integer *
	incy, ftnlen uplo_len)
{
    
    integer i__1, i__2, i__3, i__4, i__5;
    real r__1;
    complex q__1, q__2, q__3, q__4;

    
    void r_cnjg(complex *, complex *);

    
    integer i__, j, k, kk, ix, iy, jx, jy, kx, ky, info;
    complex temp1, temp2;
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
	xerbla_("CHPMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0 || (alpha->r == 0.f && alpha->i == 0.f && (beta->r == 1.f && 
                                                           beta->i == 0.f))) {
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



    if (beta->r != 1.f || beta->i != 0.f) {
	if (*incy == 1) {
	    if (beta->r == 0.f && beta->i == 0.f) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = i__;
		    y[i__2].r = 0.f, y[i__2].i = 0.f;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = i__;
		    i__3 = i__;
		    q__1.r = beta->r * y[i__3].r - beta->i * y[i__3].i, 
			    q__1.i = beta->r * y[i__3].i + beta->i * y[i__3]
			    .r;
		    y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		}
	    }
	} else {
	    iy = ky;
	    if (beta->r == 0.f && beta->i == 0.f) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = iy;
		    y[i__2].r = 0.f, y[i__2].i = 0.f;
		    iy += *incy;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    i__2 = iy;
		    i__3 = iy;
		    q__1.r = beta->r * y[i__3].r - beta->i * y[i__3].i, 
			    q__1.i = beta->r * y[i__3].i + beta->i * y[i__3]
			    .r;
		    y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		    iy += *incy;
		}
	    }
	}
    }
    if (alpha->r == 0.f && alpha->i == 0.f) {
	return 0;
    }
    kk = 1;
    if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = j;
		q__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, q__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = q__1.r, temp1.i = q__1.i;
		temp2.r = 0.f, temp2.i = 0.f;
		k = kk;
		i__2 = j - 1;
		for (i__ = 1; i__ <= i__2; ++i__) {
		    i__3 = i__;
		    i__4 = i__;
		    i__5 = k;
		    q__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    q__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    q__1.r = y[i__4].r + q__2.r, q__1.i = y[i__4].i + q__2.i;
		    y[i__3].r = q__1.r, y[i__3].i = q__1.i;
		    r_cnjg(&q__3, &ap[k]);
		    i__3 = i__;
		    q__2.r = q__3.r * x[i__3].r - q__3.i * x[i__3].i, q__2.i =
			     q__3.r * x[i__3].i + q__3.i * x[i__3].r;
		    q__1.r = temp2.r + q__2.r, q__1.i = temp2.i + q__2.i;
		    temp2.r = q__1.r, temp2.i = q__1.i;
		    ++k;
		}
		i__2 = j;
		i__3 = j;
		i__4 = kk + j - 1;
		r__1 = ap[i__4].r;
		q__3.r = r__1 * temp1.r, q__3.i = r__1 * temp1.i;
		q__2.r = y[i__3].r + q__3.r, q__2.i = y[i__3].i + q__3.i;
		q__4.r = alpha->r * temp2.r - alpha->i * temp2.i, q__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		q__1.r = q__2.r + q__4.r, q__1.i = q__2.i + q__4.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		kk += j;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = jx;
		q__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, q__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = q__1.r, temp1.i = q__1.i;
		temp2.r = 0.f, temp2.i = 0.f;
		ix = kx;
		iy = ky;
		i__2 = kk + j - 2;
		for (k = kk; k <= i__2; ++k) {
		    i__3 = iy;
		    i__4 = iy;
		    i__5 = k;
		    q__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    q__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    q__1.r = y[i__4].r + q__2.r, q__1.i = y[i__4].i + q__2.i;
		    y[i__3].r = q__1.r, y[i__3].i = q__1.i;
		    r_cnjg(&q__3, &ap[k]);
		    i__3 = ix;
		    q__2.r = q__3.r * x[i__3].r - q__3.i * x[i__3].i, q__2.i =
			     q__3.r * x[i__3].i + q__3.i * x[i__3].r;
		    q__1.r = temp2.r + q__2.r, q__1.i = temp2.i + q__2.i;
		    temp2.r = q__1.r, temp2.i = q__1.i;
		    ix += *incx;
		    iy += *incy;
		}
		i__2 = jy;
		i__3 = jy;
		i__4 = kk + j - 1;
		r__1 = ap[i__4].r;
		q__3.r = r__1 * temp1.r, q__3.i = r__1 * temp1.i;
		q__2.r = y[i__3].r + q__3.r, q__2.i = y[i__3].i + q__3.i;
		q__4.r = alpha->r * temp2.r - alpha->i * temp2.i, q__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		q__1.r = q__2.r + q__4.r, q__1.i = q__2.i + q__4.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
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
		q__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, q__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = q__1.r, temp1.i = q__1.i;
		temp2.r = 0.f, temp2.i = 0.f;
		i__2 = j;
		i__3 = j;
		i__4 = kk;
		r__1 = ap[i__4].r;
		q__2.r = r__1 * temp1.r, q__2.i = r__1 * temp1.i;
		q__1.r = y[i__3].r + q__2.r, q__1.i = y[i__3].i + q__2.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		k = kk + 1;
		i__2 = *n;
		for (i__ = j + 1; i__ <= i__2; ++i__) {
		    i__3 = i__;
		    i__4 = i__;
		    i__5 = k;
		    q__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    q__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    q__1.r = y[i__4].r + q__2.r, q__1.i = y[i__4].i + q__2.i;
		    y[i__3].r = q__1.r, y[i__3].i = q__1.i;
		    r_cnjg(&q__3, &ap[k]);
		    i__3 = i__;
		    q__2.r = q__3.r * x[i__3].r - q__3.i * x[i__3].i, q__2.i =
			     q__3.r * x[i__3].i + q__3.i * x[i__3].r;
		    q__1.r = temp2.r + q__2.r, q__1.i = temp2.i + q__2.i;
		    temp2.r = q__1.r, temp2.i = q__1.i;
		    ++k;
		}
		i__2 = j;
		i__3 = j;
		q__2.r = alpha->r * temp2.r - alpha->i * temp2.i, q__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		q__1.r = y[i__3].r + q__2.r, q__1.i = y[i__3].i + q__2.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		kk += *n - j + 1;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = jx;
		q__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, q__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = q__1.r, temp1.i = q__1.i;
		temp2.r = 0.f, temp2.i = 0.f;
		i__2 = jy;
		i__3 = jy;
		i__4 = kk;
		r__1 = ap[i__4].r;
		q__2.r = r__1 * temp1.r, q__2.i = r__1 * temp1.i;
		q__1.r = y[i__3].r + q__2.r, q__1.i = y[i__3].i + q__2.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		ix = jx;
		iy = jy;
		i__2 = kk + *n - j;
		for (k = kk + 1; k <= i__2; ++k) {
		    ix += *incx;
		    iy += *incy;
		    i__3 = iy;
		    i__4 = iy;
		    i__5 = k;
		    q__2.r = temp1.r * ap[i__5].r - temp1.i * ap[i__5].i, 
			    q__2.i = temp1.r * ap[i__5].i + temp1.i * ap[i__5]
			    .r;
		    q__1.r = y[i__4].r + q__2.r, q__1.i = y[i__4].i + q__2.i;
		    y[i__3].r = q__1.r, y[i__3].i = q__1.i;
		    r_cnjg(&q__3, &ap[k]);
		    i__3 = ix;
		    q__2.r = q__3.r * x[i__3].r - q__3.i * x[i__3].i, q__2.i =
			     q__3.r * x[i__3].i + q__3.i * x[i__3].r;
		    q__1.r = temp2.r + q__2.r, q__1.i = temp2.i + q__2.i;
		    temp2.r = q__1.r, temp2.i = q__1.i;
		}
		i__2 = jy;
		i__3 = jy;
		q__2.r = alpha->r * temp2.r - alpha->i * temp2.i, q__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		q__1.r = y[i__3].r + q__2.r, q__1.i = y[i__3].i + q__2.i;
		y[i__2].r = q__1.r, y[i__2].i = q__1.i;
		jx += *incx;
		jy += *incy;
		kk += *n - j + 1;
	    }
	}
    }

    return 0;


} 

