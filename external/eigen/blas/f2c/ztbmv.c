
#include "datatypes.h"

 int ztbmv_(char *uplo, char *trans, char *diag, integer *n, 
	integer *k, doublecomplex *a, integer *lda, doublecomplex *x, integer 
	*incx, ftnlen uplo_len, ftnlen trans_len, ftnlen diag_len)
{
    
    integer a_dim1, a_offset, i__1, i__2, i__3, i__4, i__5;
    doublecomplex z__1, z__2, z__3;

    
    void d_cnjg(doublecomplex *, doublecomplex *);

    
    integer i__, j, l, ix, jx, kx, info;
    doublecomplex temp;
    extern logical lsame_(char *, char *, ftnlen, ftnlen);
    integer kplus1;
    extern  int xerbla_(char *, integer *, ftnlen);
    logical noconj, nounit;




























/*           element vector x. On exit, X is overwritten with the */




/*  -- Written on 22-October-1986. */




    
    a_dim1 = *lda;
    a_offset = 1 + a_dim1;
    a -= a_offset;
    --x;

    
    info = 0;
    if (! lsame_(uplo, "U", (ftnlen)1, (ftnlen)1) && ! lsame_(uplo, "L", (
	    ftnlen)1, (ftnlen)1)) {
	info = 1;
    } else if (! lsame_(trans, "N", (ftnlen)1, (ftnlen)1) && ! lsame_(trans, 
	    "T", (ftnlen)1, (ftnlen)1) && ! lsame_(trans, "C", (ftnlen)1, (
	    ftnlen)1)) {
	info = 2;
    } else if (! lsame_(diag, "U", (ftnlen)1, (ftnlen)1) && ! lsame_(diag, 
	    "N", (ftnlen)1, (ftnlen)1)) {
	info = 3;
    } else if (*n < 0) {
	info = 4;
    } else if (*k < 0) {
	info = 5;
    } else if (*lda < *k + 1) {
	info = 7;
    } else if (*incx == 0) {
	info = 9;
    }
    if (info != 0) {
	xerbla_("ZTBMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0) {
	return 0;
    }

    noconj = lsame_(trans, "T", (ftnlen)1, (ftnlen)1);
    nounit = lsame_(diag, "N", (ftnlen)1, (ftnlen)1);


    if (*incx <= 0) {
	kx = 1 - (*n - 1) * *incx;
    } else if (*incx != 1) {
	kx = 1;
    }


    if (lsame_(trans, "N", (ftnlen)1, (ftnlen)1)) {


	if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {
	    kplus1 = *k + 1;
	    if (*incx == 1) {
		i__1 = *n;
		for (j = 1; j <= i__1; ++j) {
		    i__2 = j;
		    if (x[i__2].r != 0. || x[i__2].i != 0.) {
			i__2 = j;
			temp.r = x[i__2].r, temp.i = x[i__2].i;
			l = kplus1 - j;
			i__2 = 1, i__3 = j - *k;
			i__4 = j - 1;
			for (i__ = max(i__2,i__3); i__ <= i__4; ++i__) {
			    i__2 = i__;
			    i__3 = i__;
			    i__5 = l + i__ + j * a_dim1;
			    z__2.r = temp.r * a[i__5].r - temp.i * a[i__5].i, 
				    z__2.i = temp.r * a[i__5].i + temp.i * a[
				    i__5].r;
			    z__1.r = x[i__3].r + z__2.r, z__1.i = x[i__3].i + 
				    z__2.i;
			    x[i__2].r = z__1.r, x[i__2].i = z__1.i;
			}
			if (nounit) {
			    i__4 = j;
			    i__2 = j;
			    i__3 = kplus1 + j * a_dim1;
			    z__1.r = x[i__2].r * a[i__3].r - x[i__2].i * a[
				    i__3].i, z__1.i = x[i__2].r * a[i__3].i + 
				    x[i__2].i * a[i__3].r;
			    x[i__4].r = z__1.r, x[i__4].i = z__1.i;
			}
		    }
		}
	    } else {
		jx = kx;
		i__1 = *n;
		for (j = 1; j <= i__1; ++j) {
		    i__4 = jx;
		    if (x[i__4].r != 0. || x[i__4].i != 0.) {
			i__4 = jx;
			temp.r = x[i__4].r, temp.i = x[i__4].i;
			ix = kx;
			l = kplus1 - j;
			i__4 = 1, i__2 = j - *k;
			i__3 = j - 1;
			for (i__ = max(i__4,i__2); i__ <= i__3; ++i__) {
			    i__4 = ix;
			    i__2 = ix;
			    i__5 = l + i__ + j * a_dim1;
			    z__2.r = temp.r * a[i__5].r - temp.i * a[i__5].i, 
				    z__2.i = temp.r * a[i__5].i + temp.i * a[
				    i__5].r;
			    z__1.r = x[i__2].r + z__2.r, z__1.i = x[i__2].i + 
				    z__2.i;
			    x[i__4].r = z__1.r, x[i__4].i = z__1.i;
			    ix += *incx;
			}
			if (nounit) {
			    i__3 = jx;
			    i__4 = jx;
			    i__2 = kplus1 + j * a_dim1;
			    z__1.r = x[i__4].r * a[i__2].r - x[i__4].i * a[
				    i__2].i, z__1.i = x[i__4].r * a[i__2].i + 
				    x[i__4].i * a[i__2].r;
			    x[i__3].r = z__1.r, x[i__3].i = z__1.i;
			}
		    }
		    jx += *incx;
		    if (j > *k) {
			kx += *incx;
		    }
		}
	    }
	} else {
	    if (*incx == 1) {
		for (j = *n; j >= 1; --j) {
		    i__1 = j;
		    if (x[i__1].r != 0. || x[i__1].i != 0.) {
			i__1 = j;
			temp.r = x[i__1].r, temp.i = x[i__1].i;
			l = 1 - j;
			i__1 = *n, i__3 = j + *k;
			i__4 = j + 1;
			for (i__ = min(i__1,i__3); i__ >= i__4; --i__) {
			    i__1 = i__;
			    i__3 = i__;
			    i__2 = l + i__ + j * a_dim1;
			    z__2.r = temp.r * a[i__2].r - temp.i * a[i__2].i, 
				    z__2.i = temp.r * a[i__2].i + temp.i * a[
				    i__2].r;
			    z__1.r = x[i__3].r + z__2.r, z__1.i = x[i__3].i + 
				    z__2.i;
			    x[i__1].r = z__1.r, x[i__1].i = z__1.i;
			}
			if (nounit) {
			    i__4 = j;
			    i__1 = j;
			    i__3 = j * a_dim1 + 1;
			    z__1.r = x[i__1].r * a[i__3].r - x[i__1].i * a[
				    i__3].i, z__1.i = x[i__1].r * a[i__3].i + 
				    x[i__1].i * a[i__3].r;
			    x[i__4].r = z__1.r, x[i__4].i = z__1.i;
			}
		    }
		}
	    } else {
		kx += (*n - 1) * *incx;
		jx = kx;
		for (j = *n; j >= 1; --j) {
		    i__4 = jx;
		    if (x[i__4].r != 0. || x[i__4].i != 0.) {
			i__4 = jx;
			temp.r = x[i__4].r, temp.i = x[i__4].i;
			ix = kx;
			l = 1 - j;
			i__4 = *n, i__1 = j + *k;
			i__3 = j + 1;
			for (i__ = min(i__4,i__1); i__ >= i__3; --i__) {
			    i__4 = ix;
			    i__1 = ix;
			    i__2 = l + i__ + j * a_dim1;
			    z__2.r = temp.r * a[i__2].r - temp.i * a[i__2].i, 
				    z__2.i = temp.r * a[i__2].i + temp.i * a[
				    i__2].r;
			    z__1.r = x[i__1].r + z__2.r, z__1.i = x[i__1].i + 
				    z__2.i;
			    x[i__4].r = z__1.r, x[i__4].i = z__1.i;
			    ix -= *incx;
			}
			if (nounit) {
			    i__3 = jx;
			    i__4 = jx;
			    i__1 = j * a_dim1 + 1;
			    z__1.r = x[i__4].r * a[i__1].r - x[i__4].i * a[
				    i__1].i, z__1.i = x[i__4].r * a[i__1].i + 
				    x[i__4].i * a[i__1].r;
			    x[i__3].r = z__1.r, x[i__3].i = z__1.i;
			}
		    }
		    jx -= *incx;
		    if (*n - j >= *k) {
			kx -= *incx;
		    }
		}
	    }
	}
    } else {


	if (lsame_(uplo, "U", (ftnlen)1, (ftnlen)1)) {
	    kplus1 = *k + 1;
	    if (*incx == 1) {
		for (j = *n; j >= 1; --j) {
		    i__3 = j;
		    temp.r = x[i__3].r, temp.i = x[i__3].i;
		    l = kplus1 - j;
		    if (noconj) {
			if (nounit) {
			    i__3 = kplus1 + j * a_dim1;
			    z__1.r = temp.r * a[i__3].r - temp.i * a[i__3].i, 
				    z__1.i = temp.r * a[i__3].i + temp.i * a[
				    i__3].r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__4 = 1, i__1 = j - *k;
			i__3 = max(i__4,i__1);
			for (i__ = j - 1; i__ >= i__3; --i__) {
			    i__4 = l + i__ + j * a_dim1;
			    i__1 = i__;
			    z__2.r = a[i__4].r * x[i__1].r - a[i__4].i * x[
				    i__1].i, z__2.i = a[i__4].r * x[i__1].i + 
				    a[i__4].i * x[i__1].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
		    } else {
			if (nounit) {
			    d_cnjg(&z__2, &a[kplus1 + j * a_dim1]);
			    z__1.r = temp.r * z__2.r - temp.i * z__2.i, 
				    z__1.i = temp.r * z__2.i + temp.i * 
				    z__2.r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__4 = 1, i__1 = j - *k;
			i__3 = max(i__4,i__1);
			for (i__ = j - 1; i__ >= i__3; --i__) {
			    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
			    i__4 = i__;
			    z__2.r = z__3.r * x[i__4].r - z__3.i * x[i__4].i, 
				    z__2.i = z__3.r * x[i__4].i + z__3.i * x[
				    i__4].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
		    }
		    i__3 = j;
		    x[i__3].r = temp.r, x[i__3].i = temp.i;
		}
	    } else {
		kx += (*n - 1) * *incx;
		jx = kx;
		for (j = *n; j >= 1; --j) {
		    i__3 = jx;
		    temp.r = x[i__3].r, temp.i = x[i__3].i;
		    kx -= *incx;
		    ix = kx;
		    l = kplus1 - j;
		    if (noconj) {
			if (nounit) {
			    i__3 = kplus1 + j * a_dim1;
			    z__1.r = temp.r * a[i__3].r - temp.i * a[i__3].i, 
				    z__1.i = temp.r * a[i__3].i + temp.i * a[
				    i__3].r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__4 = 1, i__1 = j - *k;
			i__3 = max(i__4,i__1);
			for (i__ = j - 1; i__ >= i__3; --i__) {
			    i__4 = l + i__ + j * a_dim1;
			    i__1 = ix;
			    z__2.r = a[i__4].r * x[i__1].r - a[i__4].i * x[
				    i__1].i, z__2.i = a[i__4].r * x[i__1].i + 
				    a[i__4].i * x[i__1].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			    ix -= *incx;
			}
		    } else {
			if (nounit) {
			    d_cnjg(&z__2, &a[kplus1 + j * a_dim1]);
			    z__1.r = temp.r * z__2.r - temp.i * z__2.i, 
				    z__1.i = temp.r * z__2.i + temp.i * 
				    z__2.r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__4 = 1, i__1 = j - *k;
			i__3 = max(i__4,i__1);
			for (i__ = j - 1; i__ >= i__3; --i__) {
			    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
			    i__4 = ix;
			    z__2.r = z__3.r * x[i__4].r - z__3.i * x[i__4].i, 
				    z__2.i = z__3.r * x[i__4].i + z__3.i * x[
				    i__4].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			    ix -= *incx;
			}
		    }
		    i__3 = jx;
		    x[i__3].r = temp.r, x[i__3].i = temp.i;
		    jx -= *incx;
		}
	    }
	} else {
	    if (*incx == 1) {
		i__3 = *n;
		for (j = 1; j <= i__3; ++j) {
		    i__4 = j;
		    temp.r = x[i__4].r, temp.i = x[i__4].i;
		    l = 1 - j;
		    if (noconj) {
			if (nounit) {
			    i__4 = j * a_dim1 + 1;
			    z__1.r = temp.r * a[i__4].r - temp.i * a[i__4].i, 
				    z__1.i = temp.r * a[i__4].i + temp.i * a[
				    i__4].r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__1 = *n, i__2 = j + *k;
			i__4 = min(i__1,i__2);
			for (i__ = j + 1; i__ <= i__4; ++i__) {
			    i__1 = l + i__ + j * a_dim1;
			    i__2 = i__;
			    z__2.r = a[i__1].r * x[i__2].r - a[i__1].i * x[
				    i__2].i, z__2.i = a[i__1].r * x[i__2].i + 
				    a[i__1].i * x[i__2].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
		    } else {
			if (nounit) {
			    d_cnjg(&z__2, &a[j * a_dim1 + 1]);
			    z__1.r = temp.r * z__2.r - temp.i * z__2.i, 
				    z__1.i = temp.r * z__2.i + temp.i * 
				    z__2.r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__1 = *n, i__2 = j + *k;
			i__4 = min(i__1,i__2);
			for (i__ = j + 1; i__ <= i__4; ++i__) {
			    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
			    i__1 = i__;
			    z__2.r = z__3.r * x[i__1].r - z__3.i * x[i__1].i, 
				    z__2.i = z__3.r * x[i__1].i + z__3.i * x[
				    i__1].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
		    }
		    i__4 = j;
		    x[i__4].r = temp.r, x[i__4].i = temp.i;
		}
	    } else {
		jx = kx;
		i__3 = *n;
		for (j = 1; j <= i__3; ++j) {
		    i__4 = jx;
		    temp.r = x[i__4].r, temp.i = x[i__4].i;
		    kx += *incx;
		    ix = kx;
		    l = 1 - j;
		    if (noconj) {
			if (nounit) {
			    i__4 = j * a_dim1 + 1;
			    z__1.r = temp.r * a[i__4].r - temp.i * a[i__4].i, 
				    z__1.i = temp.r * a[i__4].i + temp.i * a[
				    i__4].r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__1 = *n, i__2 = j + *k;
			i__4 = min(i__1,i__2);
			for (i__ = j + 1; i__ <= i__4; ++i__) {
			    i__1 = l + i__ + j * a_dim1;
			    i__2 = ix;
			    z__2.r = a[i__1].r * x[i__2].r - a[i__1].i * x[
				    i__2].i, z__2.i = a[i__1].r * x[i__2].i + 
				    a[i__1].i * x[i__2].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			    ix += *incx;
			}
		    } else {
			if (nounit) {
			    d_cnjg(&z__2, &a[j * a_dim1 + 1]);
			    z__1.r = temp.r * z__2.r - temp.i * z__2.i, 
				    z__1.i = temp.r * z__2.i + temp.i * 
				    z__2.r;
			    temp.r = z__1.r, temp.i = z__1.i;
			}
			i__1 = *n, i__2 = j + *k;
			i__4 = min(i__1,i__2);
			for (i__ = j + 1; i__ <= i__4; ++i__) {
			    d_cnjg(&z__3, &a[l + i__ + j * a_dim1]);
			    i__1 = ix;
			    z__2.r = z__3.r * x[i__1].r - z__3.i * x[i__1].i, 
				    z__2.i = z__3.r * x[i__1].i + z__3.i * x[
				    i__1].r;
			    z__1.r = temp.r + z__2.r, z__1.i = temp.i + 
				    z__2.i;
			    temp.r = z__1.r, temp.i = z__1.i;
			    ix += *incx;
			}
		    }
		    i__4 = jx;
		    x[i__4].r = temp.r, x[i__4].i = temp.i;
		    jx += *incx;
		}
	    }
	}
    }

    return 0;


} 

