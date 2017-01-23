
#include "datatypes.h"

 int sspmv_(char *uplo, integer *n, real *alpha, real *ap, 
	real *x, integer *incx, real *beta, real *y, integer *incy, ftnlen 
	uplo_len)
{
    
    integer i__1, i__2;

    
    integer i__, j, k, kk, ix, iy, jx, jy, kx, ky, info;
    real temp1, temp2;
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
	xerbla_("SSPMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0 || (*alpha == 0.f && *beta == 1.f)) {
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



    if (*beta != 1.f) {
	if (*incy == 1) {
	    if (*beta == 0.f) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[i__] = 0.f;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[i__] = *beta * y[i__];
		}
	    }
	} else {
	    iy = ky;
	    if (*beta == 0.f) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[iy] = 0.f;
		    iy += *incy;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[iy] = *beta * y[iy];
		    iy += *incy;
		}
	    }
	}
    }
    if (*alpha == 0.f) {
	return 0;
    }
    kk = 1;
    if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[j];
		temp2 = 0.f;
		k = kk;
		i__2 = j - 1;
		for (i__ = 1; i__ <= i__2; ++i__) {
		    y[i__] += temp1 * ap[k];
		    temp2 += ap[k] * x[i__];
		    ++k;
		}
		y[j] = y[j] + temp1 * ap[kk + j - 1] + *alpha * temp2;
		kk += j;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[jx];
		temp2 = 0.f;
		ix = kx;
		iy = ky;
		i__2 = kk + j - 2;
		for (k = kk; k <= i__2; ++k) {
		    y[iy] += temp1 * ap[k];
		    temp2 += ap[k] * x[ix];
		    ix += *incx;
		    iy += *incy;
		}
		y[jy] = y[jy] + temp1 * ap[kk + j - 1] + *alpha * temp2;
		jx += *incx;
		jy += *incy;
		kk += j;
	    }
	}
    } else {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[j];
		temp2 = 0.f;
		y[j] += temp1 * ap[kk];
		k = kk + 1;
		i__2 = *n;
		for (i__ = j + 1; i__ <= i__2; ++i__) {
		    y[i__] += temp1 * ap[k];
		    temp2 += ap[k] * x[i__];
		    ++k;
		}
		y[j] += *alpha * temp2;
		kk += *n - j + 1;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[jx];
		temp2 = 0.f;
		y[jy] += temp1 * ap[kk];
		ix = jx;
		iy = jy;
		i__2 = kk + *n - j;
		for (k = kk + 1; k <= i__2; ++k) {
		    ix += *incx;
		    iy += *incy;
		    y[iy] += temp1 * ap[k];
		    temp2 += ap[k] * x[ix];
		}
		y[jy] += *alpha * temp2;
		jx += *incx;
		jy += *incy;
		kk += *n - j + 1;
	    }
	}
    }

    return 0;


} 

