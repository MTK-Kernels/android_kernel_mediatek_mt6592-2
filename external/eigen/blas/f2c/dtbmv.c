
#include "datatypes.h"

 int dtbmv_(char *uplo, char *trans, char *diag, integer *n, 
	integer *k, doublereal *a, integer *lda, doublereal *x, integer *incx,
	 ftnlen uplo_len, ftnlen trans_len, ftnlen diag_len)
{
    
    integer a_dim1, a_offset, i__1, i__2, i__3, i__4;

    
    integer i__, j, l, ix, jx, kx, info;
    doublereal temp;
    extern logical lsame_(char *, char *, ftnlen, ftnlen);
    integer kplus1;
    extern  int xerbla_(char *, integer *, ftnlen);
    logical nounit;




























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
	xerbla_("DTBMV ", &info, (ftnlen)6);
	return 0;
    }


    if (*n == 0) {
	return 0;
    }

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
		    if (x[j] != 0.) {
			temp = x[j];
			l = kplus1 - j;
			i__2 = 1, i__3 = j - *k;
			i__4 = j - 1;
			for (i__ = max(i__2,i__3); i__ <= i__4; ++i__) {
			    x[i__] += temp * a[l + i__ + j * a_dim1];
			}
			if (nounit) {
			    x[j] *= a[kplus1 + j * a_dim1];
			}
		    }
		}
	    } else {
		jx = kx;
		i__1 = *n;
		for (j = 1; j <= i__1; ++j) {
		    if (x[jx] != 0.) {
			temp = x[jx];
			ix = kx;
			l = kplus1 - j;
			i__4 = 1, i__2 = j - *k;
			i__3 = j - 1;
			for (i__ = max(i__4,i__2); i__ <= i__3; ++i__) {
			    x[ix] += temp * a[l + i__ + j * a_dim1];
			    ix += *incx;
			}
			if (nounit) {
			    x[jx] *= a[kplus1 + j * a_dim1];
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
		    if (x[j] != 0.) {
			temp = x[j];
			l = 1 - j;
			i__1 = *n, i__3 = j + *k;
			i__4 = j + 1;
			for (i__ = min(i__1,i__3); i__ >= i__4; --i__) {
			    x[i__] += temp * a[l + i__ + j * a_dim1];
			}
			if (nounit) {
			    x[j] *= a[j * a_dim1 + 1];
			}
		    }
		}
	    } else {
		kx += (*n - 1) * *incx;
		jx = kx;
		for (j = *n; j >= 1; --j) {
		    if (x[jx] != 0.) {
			temp = x[jx];
			ix = kx;
			l = 1 - j;
			i__4 = *n, i__1 = j + *k;
			i__3 = j + 1;
			for (i__ = min(i__4,i__1); i__ >= i__3; --i__) {
			    x[ix] += temp * a[l + i__ + j * a_dim1];
			    ix -= *incx;
			}
			if (nounit) {
			    x[jx] *= a[j * a_dim1 + 1];
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
		    temp = x[j];
		    l = kplus1 - j;
		    if (nounit) {
			temp *= a[kplus1 + j * a_dim1];
		    }
		    i__4 = 1, i__1 = j - *k;
		    i__3 = max(i__4,i__1);
		    for (i__ = j - 1; i__ >= i__3; --i__) {
			temp += a[l + i__ + j * a_dim1] * x[i__];
		    }
		    x[j] = temp;
		}
	    } else {
		kx += (*n - 1) * *incx;
		jx = kx;
		for (j = *n; j >= 1; --j) {
		    temp = x[jx];
		    kx -= *incx;
		    ix = kx;
		    l = kplus1 - j;
		    if (nounit) {
			temp *= a[kplus1 + j * a_dim1];
		    }
		    i__4 = 1, i__1 = j - *k;
		    i__3 = max(i__4,i__1);
		    for (i__ = j - 1; i__ >= i__3; --i__) {
			temp += a[l + i__ + j * a_dim1] * x[ix];
			ix -= *incx;
		    }
		    x[jx] = temp;
		    jx -= *incx;
		}
	    }
	} else {
	    if (*incx == 1) {
		i__3 = *n;
		for (j = 1; j <= i__3; ++j) {
		    temp = x[j];
		    l = 1 - j;
		    if (nounit) {
			temp *= a[j * a_dim1 + 1];
		    }
		    i__1 = *n, i__2 = j + *k;
		    i__4 = min(i__1,i__2);
		    for (i__ = j + 1; i__ <= i__4; ++i__) {
			temp += a[l + i__ + j * a_dim1] * x[i__];
		    }
		    x[j] = temp;
		}
	    } else {
		jx = kx;
		i__3 = *n;
		for (j = 1; j <= i__3; ++j) {
		    temp = x[jx];
		    kx += *incx;
		    ix = kx;
		    l = 1 - j;
		    if (nounit) {
			temp *= a[j * a_dim1 + 1];
		    }
		    i__1 = *n, i__2 = j + *k;
		    i__4 = min(i__1,i__2);
		    for (i__ = j + 1; i__ <= i__4; ++i__) {
			temp += a[l + i__ + j * a_dim1] * x[ix];
			ix += *incx;
		    }
		    x[jx] = temp;
		    jx += *incx;
		}
	    }
	}
    }

    return 0;


} 

