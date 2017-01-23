
#include "datatypes.h"

logical lsame_(char *ca, char *cb, ftnlen ca_len, ftnlen cb_len)
{
    
    logical ret_val;

    
    integer inta, intb, zcode;












    ret_val = *(unsigned char *)ca == *(unsigned char *)cb;
    if (ret_val) {
	return ret_val;
    }


    zcode = 'Z';


    inta = *(unsigned char *)ca;
    intb = *(unsigned char *)cb;

    if (zcode == 90 || zcode == 122) {


	if (inta >= 97 && inta <= 122) {
	    inta += -32;
	}
	if (intb >= 97 && intb <= 122) {
	    intb += -32;
	}

    } else if (zcode == 233 || zcode == 169) {


	if ((inta >= 129 && inta <= 137) || (inta >= 145 && inta <= 153) || 
            (inta >= 162 && inta <= 169)) {
	    inta += 64;
	}
	if ((intb >= 129 && intb <= 137) || (intb >= 145 && intb <= 153) || 
            (intb >= 162 && intb <= 169)) {
	    intb += 64;
	}

    } else if (zcode == 218 || zcode == 250) {


	if (inta >= 225 && inta <= 250) {
	    inta += -32;
	}
	if (intb >= 225 && intb <= 250) {
	    intb += -32;
	}
    }
    ret_val = inta == intb;



    return ret_val;
} 

