
#include "datatypes.h"

 int srotmg_(real *sd1, real *sd2, real *sx1, real *sy1, real 
	*sparam)
{
    

    static real zero = 0.f;
    static real one = 1.f;
    static real two = 2.f;
    static real gam = 4096.f;
    static real gamsq = 16777200.f;
    static real rgamsq = 5.96046e-8f;

    
    static char fmt_120[] = "";
    static char fmt_150[] = "";
    static char fmt_180[] = "";
    static char fmt_210[] = "";

    
    real r__1;

    
    real su, sp1, sp2, sq1, sq2, sh11, sh12, sh21, sh22;
    integer igo;
    real sflag, stemp;

    
    static char *igo_fmt;


















    
    --sparam;

    
    if (! (*sd1 < zero)) {
	goto L10;
    }
    goto L60;
L10:
    sp2 = *sd2 * *sy1;
    if (! (sp2 == zero)) {
	goto L20;
    }
    sflag = -two;
    goto L260;
L20:
    sp1 = *sd1 * *sx1;
    sq2 = sp2 * *sy1;
    sq1 = sp1 * *sx1;

    if (! (dabs(sq1) > dabs(sq2))) {
	goto L40;
    }
    sh21 = -(*sy1) / *sx1;
    sh12 = sp2 / sp1;

    su = one - sh12 * sh21;

    if (! (su <= zero)) {
	goto L30;
    }
    goto L60;
L30:
    sflag = zero;
    *sd1 /= su;
    *sd2 /= su;
    *sx1 *= su;
    goto L100;
L40:
    if (! (sq2 < zero)) {
	goto L50;
    }
    goto L60;
L50:
    sflag = one;
    sh11 = sp1 / sp2;
    sh22 = *sx1 / *sy1;
    su = one + sh11 * sh22;
    stemp = *sd2 / su;
    *sd2 = *sd1 / su;
    *sd1 = stemp;
    *sx1 = *sy1 * su;
    goto L100;
L60:
    sflag = -one;
    sh11 = zero;
    sh12 = zero;
    sh21 = zero;
    sh22 = zero;

    *sd1 = zero;
    *sd2 = zero;
    *sx1 = zero;
    goto L220;
L70:
    if (! (sflag >= zero)) {
	goto L90;
    }

    if (! (sflag == zero)) {
	goto L80;
    }
    sh11 = one;
    sh22 = one;
    sflag = -one;
    goto L90;
L80:
    sh21 = -one;
    sh12 = one;
    sflag = -one;
L90:
    switch (igo) {
	case 0: goto L120;
	case 1: goto L150;
	case 2: goto L180;
	case 3: goto L210;
    }
L100:
L110:
    if (! (*sd1 <= rgamsq)) {
	goto L130;
    }
    if (*sd1 == zero) {
	goto L160;
    }
    igo = 0;
    igo_fmt = fmt_120;
    goto L70;
L120:
    r__1 = gam;
    *sd1 *= r__1 * r__1;
    *sx1 /= gam;
    sh11 /= gam;
    sh12 /= gam;
    goto L110;
L130:
L140:
    if (! (*sd1 >= gamsq)) {
	goto L160;
    }
    igo = 1;
    igo_fmt = fmt_150;
    goto L70;
L150:
    r__1 = gam;
    *sd1 /= r__1 * r__1;
    *sx1 *= gam;
    sh11 *= gam;
    sh12 *= gam;
    goto L140;
L160:
L170:
    if (! (dabs(*sd2) <= rgamsq)) {
	goto L190;
    }
    if (*sd2 == zero) {
	goto L220;
    }
    igo = 2;
    igo_fmt = fmt_180;
    goto L70;
L180:
    r__1 = gam;
    *sd2 *= r__1 * r__1;
    sh21 /= gam;
    sh22 /= gam;
    goto L170;
L190:
L200:
    if (! (dabs(*sd2) >= gamsq)) {
	goto L220;
    }
    igo = 3;
    igo_fmt = fmt_210;
    goto L70;
L210:
    r__1 = gam;
    *sd2 /= r__1 * r__1;
    sh21 *= gam;
    sh22 *= gam;
    goto L200;
L220:
    if (sflag < 0.f) {
	goto L250;
    } else if (sflag == 0) {
	goto L230;
    } else {
	goto L240;
    }
L230:
    sparam[3] = sh21;
    sparam[4] = sh12;
    goto L260;
L240:
    sparam[2] = sh11;
    sparam[5] = sh22;
    goto L260;
L250:
    sparam[2] = sh11;
    sparam[3] = sh21;
    sparam[4] = sh12;
    sparam[5] = sh22;
L260:
    sparam[1] = sflag;
    return 0;
} 

