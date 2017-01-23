
#include "datatypes.h"

 int dsbmv_(char *uplo, integer *n, integer *k, doublereal *
	alpha, doublereal *a, integer *lda, doublereal *x, integer *incx, 
	doublereal *beta, doublereal *y, integer *incy, ftnlen uplo_len)
{
    
    integer a_dim1, a_offset, i__1, i__2, i__3, i__4;

    
    integer i__, j, l, ix, iy, jx, jy, kx, ky, info;
    doublereal temp1, temp2;
    extern logical lsame_(char *, char *, ftnlen, ftnlen);
    integer kplus1;
    extern  int xerbla_(char *, integer *, ftnlen);























/*           vector y. On exit, Y is overwritten by the updated vector y. */




/*  -- Written on 22-October-1986. */




    
    a_dim1 = *lda;
    a_offset = 1 + a_dim1;
    a -= a_offset;
    --x;
    --y;

    
    info = 0;
    if (! lsame_(uplo, "U", (ftnlen)1, (ftnlen)1) && ! lsame_(uplo, "L", (
	    ftnlen)1, (ftnlen)1)) {
	info = 1;
    } else if (*n < 0) {
	info = 2;
    } else if (*k < 0) {
	info = 3;
    } else if (*lda < *k + 1) {
	info = 6;
    } else if (*incx == 0) {
	info = 8;
    } else if (*incy == 0) {
	info = 11;
    }
    if (info != 0) {
	xerbla_("DSBMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0 || (*alpha == 0. && *beta == 1.)) {
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



    if (*beta != 1.) {
	if (*incy == 1) {
	    if (*beta == 0.) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[i__] = 0.;
		}
	    } else {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[i__] = *beta * y[i__];
		}
	    }
	} else {
	    iy = ky;
	    if (*beta == 0.) {
		i__1 = *n;
		for (i__ = 1; i__ <= i__1; ++i__) {
		    y[iy] = 0.;
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
    if (*alpha == 0.) {
	return 0;
    }
    if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {


	kplus1 = *k + 1;
	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[j];
		temp2 = 0.;
		l = kplus1 - j;
		i__2 = 1, i__3 = j - *k;
		i__4 = j - 1;
		for (i__ = max(i__2,i__3); i__ <= i__4; ++i__) {
		    y[i__] += temp1 * a[l + i__ + j * a_dim1];
		    temp2 += a[l + i__ + j * a_dim1] * x[i__];
		}
		y[j] = y[j] + temp1 * a[kplus1 + j * a_dim1] + *alpha * temp2;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[jx];
		temp2 = 0.;
		ix = kx;
		iy = ky;
		l = kplus1 - j;
		i__4 = 1, i__2 = j - *k;
		i__3 = j - 1;
		for (i__ = max(i__4,i__2); i__ <= i__3; ++i__) {
		    y[iy] += temp1 * a[l + i__ + j * a_dim1];
		    temp2 += a[l + i__ + j * a_dim1] * x[ix];
		    ix += *incx;
		    iy += *incy;
		}
		y[jy] = y[jy] + temp1 * a[kplus1 + j * a_dim1] + *alpha * 
			temp2;
		jx += *incx;
		jy += *incy;
		if (j > *k) {
		    kx += *incx;
		    ky += *incy;
		}
	    }
	}
    } else {


	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[j];
		temp2 = 0.;
		y[j] += temp1 * a[j * a_dim1 + 1];
		l = 1 - j;
		i__4 = *n, i__2 = j + *k;
		i__3 = min(i__4,i__2);
		for (i__ = j + 1; i__ <= i__3; ++i__) {
		    y[i__] += temp1 * a[l + i__ + j * a_dim1];
		    temp2 += a[l + i__ + j * a_dim1] * x[i__];
		}
		y[j] += *alpha * temp2;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		temp1 = *alpha * x[jx];
		temp2 = 0.;
		y[jy] += temp1 * a[j * a_dim1 + 1];
		l = 1 - j;
		ix = jx;
		iy = jy;
		i__4 = *n, i__2 = j + *k;
		i__3 = min(i__4,i__2);
		for (i__ = j + 1; i__ <= i__3; ++i__) {
		    ix += *incx;
		    iy += *incy;
		    y[iy] += temp1 * a[l + i__ + j * a_dim1];
		    temp2 += a[l + i__ + j * a_dim1] * x[ix];
		}
		y[jy] += *alpha * temp2;
		jx += *incx;
		jy += *incy;
	    }
	}
    }

    return 0;


} 

