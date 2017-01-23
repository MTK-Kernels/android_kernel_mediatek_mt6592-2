
#include "datatypes.h"

 int zhbmv_(char *uplo, integer *n, integer *k, doublecomplex 
	*alpha, doublecomplex *a, integer *lda, doublecomplex *x, integer *
	incx, doublecomplex *beta, doublecomplex *y, integer *incy, ftnlen 
	uplo_len)
{
    
    integer a_dim1, a_offset, i__1, i__2, i__3, i__4, i__5;
    doublereal d__1;
    doublecomplex z__1, z__2, z__3, z__4;

    
    void d_cnjg(doublecomplex *, doublecomplex *);

    
    integer i__, j, l, ix, iy, jx, jy, kx, ky, info;
    doublecomplex temp1, temp2;
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
	xerbla_("ZHBMV ", &info, (ftnlen)6);
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
    if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {


	kplus1 = *k + 1;
	if (*incx == 1 && *incy == 1) {
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__2 = j;
		z__1.r = alpha->r * x[i__2].r - alpha->i * x[i__2].i, z__1.i =
			 alpha->r * x[i__2].i + alpha->i * x[i__2].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		l = kplus1 - j;
		i__2 = 1, i__3 = j - *k;
		i__4 = j - 1;
		for (i__ = max(i__2,i__3); i__ <= i__4; ++i__) {
		    i__2 = i__;
		    i__3 = i__;
		    i__5 = l + i__ + j * a_dim1;
		    z__2.r = temp1.r * a[i__5].r - temp1.i * a[i__5].i, 
			    z__2.i = temp1.r * a[i__5].i + temp1.i * a[i__5]
			    .r;
		    z__1.r = y[i__3].r + z__2.r, z__1.i = y[i__3].i + z__2.i;
		    y[i__2].r = z__1.r, y[i__2].i = z__1.i;
		    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
		    i__2 = i__;
		    z__2.r = z__3.r * x[i__2].r - z__3.i * x[i__2].i, z__2.i =
			     z__3.r * x[i__2].i + z__3.i * x[i__2].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		}
		i__4 = j;
		i__2 = j;
		i__3 = kplus1 + j * a_dim1;
		d__1 = a[i__3].r;
		z__3.r = d__1 * temp1.r, z__3.i = d__1 * temp1.i;
		z__2.r = y[i__2].r + z__3.r, z__2.i = y[i__2].i + z__3.i;
		z__4.r = alpha->r * temp2.r - alpha->i * temp2.i, z__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = z__2.r + z__4.r, z__1.i = z__2.i + z__4.i;
		y[i__4].r = z__1.r, y[i__4].i = z__1.i;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__4 = jx;
		z__1.r = alpha->r * x[i__4].r - alpha->i * x[i__4].i, z__1.i =
			 alpha->r * x[i__4].i + alpha->i * x[i__4].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		ix = kx;
		iy = ky;
		l = kplus1 - j;
		i__4 = 1, i__2 = j - *k;
		i__3 = j - 1;
		for (i__ = max(i__4,i__2); i__ <= i__3; ++i__) {
		    i__4 = iy;
		    i__2 = iy;
		    i__5 = l + i__ + j * a_dim1;
		    z__2.r = temp1.r * a[i__5].r - temp1.i * a[i__5].i, 
			    z__2.i = temp1.r * a[i__5].i + temp1.i * a[i__5]
			    .r;
		    z__1.r = y[i__2].r + z__2.r, z__1.i = y[i__2].i + z__2.i;
		    y[i__4].r = z__1.r, y[i__4].i = z__1.i;
		    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
		    i__4 = ix;
		    z__2.r = z__3.r * x[i__4].r - z__3.i * x[i__4].i, z__2.i =
			     z__3.r * x[i__4].i + z__3.i * x[i__4].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		    ix += *incx;
		    iy += *incy;
		}
		i__3 = jy;
		i__4 = jy;
		i__2 = kplus1 + j * a_dim1;
		d__1 = a[i__2].r;
		z__3.r = d__1 * temp1.r, z__3.i = d__1 * temp1.i;
		z__2.r = y[i__4].r + z__3.r, z__2.i = y[i__4].i + z__3.i;
		z__4.r = alpha->r * temp2.r - alpha->i * temp2.i, z__4.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = z__2.r + z__4.r, z__1.i = z__2.i + z__4.i;
		y[i__3].r = z__1.r, y[i__3].i = z__1.i;
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
		i__3 = j;
		z__1.r = alpha->r * x[i__3].r - alpha->i * x[i__3].i, z__1.i =
			 alpha->r * x[i__3].i + alpha->i * x[i__3].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		i__3 = j;
		i__4 = j;
		i__2 = j * a_dim1 + 1;
		d__1 = a[i__2].r;
		z__2.r = d__1 * temp1.r, z__2.i = d__1 * temp1.i;
		z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		l = 1 - j;
		i__4 = *n, i__2 = j + *k;
		i__3 = min(i__4,i__2);
		for (i__ = j + 1; i__ <= i__3; ++i__) {
		    i__4 = i__;
		    i__2 = i__;
		    i__5 = l + i__ + j * a_dim1;
		    z__2.r = temp1.r * a[i__5].r - temp1.i * a[i__5].i, 
			    z__2.i = temp1.r * a[i__5].i + temp1.i * a[i__5]
			    .r;
		    z__1.r = y[i__2].r + z__2.r, z__1.i = y[i__2].i + z__2.i;
		    y[i__4].r = z__1.r, y[i__4].i = z__1.i;
		    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
		    i__4 = i__;
		    z__2.r = z__3.r * x[i__4].r - z__3.i * x[i__4].i, z__2.i =
			     z__3.r * x[i__4].i + z__3.i * x[i__4].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		}
		i__3 = j;
		i__4 = j;
		z__2.r = alpha->r * temp2.r - alpha->i * temp2.i, z__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		y[i__3].r = z__1.r, y[i__3].i = z__1.i;
	    }
	} else {
	    jx = kx;
	    jy = ky;
	    i__1 = *n;
	    for (j = 1; j <= i__1; ++j) {
		i__3 = jx;
		z__1.r = alpha->r * x[i__3].r - alpha->i * x[i__3].i, z__1.i =
			 alpha->r * x[i__3].i + alpha->i * x[i__3].r;
		temp1.r = z__1.r, temp1.i = z__1.i;
		temp2.r = 0., temp2.i = 0.;
		i__3 = jy;
		i__4 = jy;
		i__2 = j * a_dim1 + 1;
		d__1 = a[i__2].r;
		z__2.r = d__1 * temp1.r, z__2.i = d__1 * temp1.i;
		z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		l = 1 - j;
		ix = jx;
		iy = jy;
		i__4 = *n, i__2 = j + *k;
		i__3 = min(i__4,i__2);
		for (i__ = j + 1; i__ <= i__3; ++i__) {
		    ix += *incx;
		    iy += *incy;
		    i__4 = iy;
		    i__2 = iy;
		    i__5 = l + i__ + j * a_dim1;
		    z__2.r = temp1.r * a[i__5].r - temp1.i * a[i__5].i, 
			    z__2.i = temp1.r * a[i__5].i + temp1.i * a[i__5]
			    .r;
		    z__1.r = y[i__2].r + z__2.r, z__1.i = y[i__2].i + z__2.i;
		    y[i__4].r = z__1.r, y[i__4].i = z__1.i;
		    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
		    i__4 = ix;
		    z__2.r = z__3.r * x[i__4].r - z__3.i * x[i__4].i, z__2.i =
			     z__3.r * x[i__4].i + z__3.i * x[i__4].r;
		    z__1.r = temp2.r + z__2.r, z__1.i = temp2.i + z__2.i;
		    temp2.r = z__1.r, temp2.i = z__1.i;
		}
		i__3 = jy;
		i__4 = jy;
		z__2.r = alpha->r * temp2.r - alpha->i * temp2.i, z__2.i = 
			alpha->r * temp2.i + alpha->i * temp2.r;
		z__1.r = y[i__4].r + z__2.r, z__1.i = y[i__4].i + z__2.i;
		y[i__3].r = z__1.r, y[i__3].i = z__1.i;
		jx += *incx;
		jy += *incy;
	    }
	}
    }

    return 0;


} 

