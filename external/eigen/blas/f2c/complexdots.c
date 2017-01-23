

#include "datatypes.h"

complex cdotc_(integer *n, complex *cx, integer 
	*incx, complex *cy, integer *incy)
{
    complex res;
    extern  int cdotcw_(integer *, complex *, integer *, 
	    complex *, integer *, complex *);

    
    --cy;
    --cx;

    
    cdotcw_(n, &cx[1], incx, &cy[1], incy, &res);
    return res;
} 

complex cdotu_(integer *n, complex *cx, integer 
	*incx, complex *cy, integer *incy)
{
    complex res;
    extern  int cdotuw_(integer *, complex *, integer *, 
	    complex *, integer *, complex *);

    
    --cy;
    --cx;

    
    cdotuw_(n, &cx[1], incx, &cy[1], incy, &res);
    return res;
} 

doublecomplex zdotc_(integer *n, doublecomplex *cx, integer *incx, 
                     doublecomplex *cy, integer *incy)
{
    doublecomplex res;
    extern  int zdotcw_(integer *, doublecomplex *, integer *,
	     doublecomplex *, integer *, doublecomplex *);

    
    --cy;
    --cx;

    
    zdotcw_(n, &cx[1], incx, &cy[1], incy, &res);
    return res;
} 

doublecomplex zdotu_(integer *n, doublecomplex *cx, integer *incx, 
                     doublecomplex *cy, integer *incy)
{
    doublecomplex res;
    extern  int zdotuw_(integer *, doublecomplex *, integer *,
	     doublecomplex *, integer *, doublecomplex *);

    
    --cy;
    --cx;

    
    zdotuw_(n, &cx[1], incx, &cy[1], incy, &res);
    return res;
} 

