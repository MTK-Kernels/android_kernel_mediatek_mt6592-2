




#define INTRIG 1




#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef INTRIG
#include <math.h>
#endif

#define cot(x) (1.0 / tan(x))

#define TRUE  1
#define FALSE 0

#define max_surfaces 10



static short current_surfaces;
static short paraxial;

static double clear_aperture;

static double aberr_lspher;
static double aberr_osc;
static double aberr_lchrom;

static double max_lspher;
static double max_osc;
static double max_lchrom;

static double radius_of_curvature;
static double object_distance;
static double ray_height;
static double axis_slope_angle;
static double from_index;
static double to_index;

static double spectral_line[9];
static double s[max_surfaces][5];
static double od_sa[2][2];

static char outarr[8][80];	   

int itercount;			   

#ifndef ITERATIONS
#define ITERATIONS   100
#endif
int niter = ITERATIONS; 	   

static char *refarr[] = {	   

        "   Marginal ray          47.09479120920   0.04178472683",
        "   Paraxial ray          47.08372160249   0.04177864821",
        "Longitudinal spherical aberration:        -0.01106960671",
        "    (Maximum permissible):                 0.05306749907",
        "Offense against sine condition (coma):     0.00008954761",
        "    (Maximum permissible):                 0.00250000000",
        "Axial chromatic aberration:                0.00448229032",
        "    (Maximum permissible):                 0.05306749907"
};


static double testcase[4][4] = {
	{27.05, 1.5137, 63.6, 0.52},
	{-16.68, 1, 0, 0.138},
	{-16.68, 1.6164, 36.7, 0.38},
	{-78.1, 1, 0, 0}
};


#ifdef INTRIG


#define sin I_sin
#define cos I_cos
#define tan I_tan
#define sqrt I_sqrt
#define atan I_atan
#define atan2 I_atan2
#define asin I_asin

#define fabs(x)  ((x < 0.0) ? -x : x)

#define pic 3.1415926535897932


static double pi = pic,
	twopi =pic * 2.0,
	piover4 = pic / 4.0,
	fouroverpi = 4.0 / pic,
	piover2 = pic / 2.0;


static double atanc[] = {
	0.0,
	0.4636476090008061165,
	0.7853981633974483094,
	0.98279372324732906714,
	1.1071487177940905022,
	1.1902899496825317322,
	1.2490457723982544262,
	1.2924966677897852673,
	1.3258176636680324644
};


double aint(x)
double x;
{
	long l;


	l = x;
	if ((int)(-0.5) != 0  &&  l < 0 )
	   l++;
	x = l;
	return x;
}


static double sin(x)
double x;
{
	int sign;
	double y, r, z;

	x = (((sign= (x < 0.0)) != 0) ? -x: x);

	if (x > twopi)
	   x -= (aint(x / twopi) * twopi);

	if (x > pi) {
	   x -= pi;
	   sign = !sign;
	}

	if (x > piover2)
	   x = pi - x;

	if (x < piover4) {
	   y = x * fouroverpi;
	   z = y * y;
	   r = y * (((((((-0.202253129293E-13 * z + 0.69481520350522E-11) * z -
	      0.17572474176170806E-8) * z + 0.313361688917325348E-6) * z -
	      0.365762041821464001E-4) * z + 0.249039457019271628E-2) * z -
	      0.0807455121882807815) * z + 0.785398163397448310);
	} else {
	   y = (piover2 - x) * fouroverpi;
	   z = y * y;
	   r = ((((((-0.38577620372E-12 * z + 0.11500497024263E-9) * z -
	      0.2461136382637005E-7) * z + 0.359086044588581953E-5) * z -
	      0.325991886926687550E-3) * z + 0.0158543442438154109) * z -
	      0.308425137534042452) * z + 1.0;
	}
	return sign ? -r : r;
}


static double cos(x)
double x;
{
	x = (x < 0.0) ? -x : x;
	if (x > twopi)		      
	   x = x - (aint(x / twopi) * twopi); 
	return sin(x + piover2);
}


static double tan(x)
double x;
{
	return sin(x) / cos(x);
}


double sqrt(x)
double x;
{
	double c, cl, y;
	int n;

	if (x == 0.0)
	   return 0.0;

	if (x < 0.0) {
	   fprintf(stderr,
              "\nGood work!  You tried to take the square root of %g",
	     x);
	   fprintf(stderr,
              "\nunfortunately, that is too complex for me to handle.\n");
	   exit(1);
	}

	y = (0.154116 + 1.893872 * x) / (1.0 + 1.047988 * x);

	c = (y - x / y) / 2.0;
	cl = 0.0;
	for (n = 50; c != cl && n--;) {
	   y = y - c;
	   cl = c;
	   c = (y - x / y) / 2.0;
	}
	return y;
}


static double atan(x)
double x;
{
	int sign, l, y;
	double a, b, z;

	x = (((sign = (x < 0.0)) != 0) ? -x : x);
	l = 0;

	if (x >= 4.0) {
	   l = -1;
	   x = 1.0 / x;
	   y = 0;
	   goto atl;
	} else {
	   if (x < 0.25) {
	      y = 0;
	      goto atl;
	   }
	}

	y = aint(x / 0.5);
	z = y * 0.5;
	x = (x - z) / (x * z + 1);

atl:
	z = x * x;
	b = ((((893025.0 * z + 49116375.0) * z + 425675250.0) * z +
	    1277025750.0) * z + 1550674125.0) * z + 654729075.0;
	a = (((13852575.0 * z + 216602100.0) * z + 891080190.0) * z +
	    1332431100.0) * z + 654729075.0;
	a = (a / b) * x + atanc[y];
	if (l)
	   a=piover2 - a;
	return sign ? -a : a;
}


static double atan2(y, x)
double y, x;
{
	double temp;

	if (x == 0.0) {
	   if (y == 0.0)   
	      return 0.0;
	   else if (y > 0)
	      return piover2;
	   else
	      return -piover2;
	}
	temp = atan(y / x);
	if (x < 0.0) {
	   if (y >= 0.0)
	      temp += pic;
	   else
	      temp -= pic;
	}
	return temp;
}


static double asin(x)
double x;
{
	if (fabs(x)>1.0) {
	   fprintf(stderr,
              "\nInverse trig functions lose much of their gloss when");
	   fprintf(stderr,
              "\ntheir arguments are greater than 1, such as the");
	   fprintf(stderr,
              "\nvalue %g you passed.\n", x);
	   exit(1);
	}
	return atan2(x, sqrt(1 - x * x));
}
#endif


static void transit_surface() {
	double iang,		   
	       rang,		   
	       iang_sin,	   
	       rang_sin,	   
	       old_axis_slope_angle, sagitta;

	if (paraxial) {
	   if (radius_of_curvature != 0.0) {
	      if (object_distance == 0.0) {
		 axis_slope_angle = 0.0;
		 iang_sin = ray_height / radius_of_curvature;
	      } else
		 iang_sin = ((object_distance -
		    radius_of_curvature) / radius_of_curvature) *
		    axis_slope_angle;

	      rang_sin = (from_index / to_index) *
		 iang_sin;
	      old_axis_slope_angle = axis_slope_angle;
	      axis_slope_angle = axis_slope_angle +
		 iang_sin - rang_sin;
	      if (object_distance != 0.0)
		 ray_height = object_distance * old_axis_slope_angle;
	      object_distance = ray_height / axis_slope_angle;
	      return;
	   }
	   object_distance = object_distance * (to_index / from_index);
	   axis_slope_angle = axis_slope_angle * (from_index / to_index);
	   return;
	}

	if (radius_of_curvature != 0.0) {
	   if (object_distance == 0.0) {
	      axis_slope_angle = 0.0;
	      iang_sin = ray_height / radius_of_curvature;
	   } else {
	      iang_sin = ((object_distance -
		 radius_of_curvature) / radius_of_curvature) *
		 sin(axis_slope_angle);
	   }
	   iang = asin(iang_sin);
	   rang_sin = (from_index / to_index) *
	      iang_sin;
	   old_axis_slope_angle = axis_slope_angle;
	   axis_slope_angle = axis_slope_angle +
	      iang - asin(rang_sin);
	   sagitta = sin((old_axis_slope_angle + iang) / 2.0);
	   sagitta = 2.0 * radius_of_curvature*sagitta*sagitta;
	   object_distance = ((radius_of_curvature * sin(
	      old_axis_slope_angle + iang)) *
	      cot(axis_slope_angle)) + sagitta;
	   return;
	}

	rang = -asin((from_index / to_index) *
	   sin(axis_slope_angle));
	object_distance = object_distance * ((to_index *
	   cos(-rang)) / (from_index *
	   cos(axis_slope_angle)));
	axis_slope_angle = -rang;
}


static void trace_line(line, ray_h)
int line;
double ray_h;
{
	int i;

	object_distance = 0.0;
	ray_height = ray_h;
	from_index = 1.0;

	for (i = 1; i <= current_surfaces; i++) {
	   radius_of_curvature = s[i][1];
	   to_index = s[i][2];
	   if (to_index > 1.0)
	      to_index = to_index + ((spectral_line[4] -
		 spectral_line[line]) /
		 (spectral_line[3] - spectral_line[6])) * ((s[i][2] - 1.0) /
		 s[i][3]);
	   transit_surface();
	   from_index = to_index;
	   if (i < current_surfaces)
	      object_distance = object_distance - s[i][4];
	}
}


int main(argc, argv)
int argc;
char *argv[];
{
	int i, j, k, errors;
	double od_fline, od_cline;
#ifdef ACCURACY
	long passes;
#endif

	spectral_line[1] = 7621.0;	 
	spectral_line[2] = 6869.955;	 
	spectral_line[3] = 6562.816;	 
	spectral_line[4] = 5895.944;	 
	spectral_line[5] = 5269.557;	 
	spectral_line[6] = 4861.344;	 
        spectral_line[7] = 4340.477;     
	spectral_line[8] = 3968.494;	 

	

	if (argc > 1) {
	   niter = atoi(argv[1]);
           if (*argv[1] == '-' || niter < 1) {
              printf("This is John Walker's floating point accuracy and\n");
              printf("performance benchmark program.  You call it with\n");
              printf("\nfbench <itercount>\n\n");
              printf("where <itercount> is the number of iterations\n");
              printf("to be executed.  Archival timings should be made\n");
              printf("with the iteration count set so that roughly five\n");
              printf("minutes of execution is timed.\n");
	      exit(0);
	   }
	}

	

	clear_aperture = 4.0;
	current_surfaces = 4;
	for (i = 0; i < current_surfaces; i++)
	   for (j = 0; j < 4; j++)
	      s[i + 1][j + 1] = testcase[i][j];

#ifdef ACCURACY
        printf("Beginning execution of floating point accuracy test...\n");
	passes = 0;
#else
        printf("Ready to begin John Walker's floating point accuracy\n");
        printf("and performance benchmark.  %d iterations will be made.\n\n",
	   niter);

        printf("\nMeasured run time in seconds should be divided by %.f\n", niter / 1000.0);
        printf("to normalise for reporting results.  For archival results,\n");
        printf("adjust iteration count so the benchmark runs about five minutes.\n\n");

        
	
#endif

	

#ifdef ACCURACY
	while (TRUE) {
	   passes++;
	   if ((passes % 100L) == 0) {
              printf("Pass %ld.\n", passes);
	   }
#else
	for (itercount = 0; itercount < niter; itercount++) {
#endif

	   for (paraxial = 0; paraxial <= 1; paraxial++) {

	      

	      trace_line(4, clear_aperture / 2.0);
	      od_sa[paraxial][0] = object_distance;
	      od_sa[paraxial][1] = axis_slope_angle;
	   }
	   paraxial = FALSE;

	   

	   trace_line(3, clear_aperture / 2.0);
	   od_cline = object_distance;

	   

	   trace_line(6, clear_aperture / 2.0);
	   od_fline = object_distance;

	   aberr_lspher = od_sa[1][0] - od_sa[0][0];
	   aberr_osc = 1.0 - (od_sa[1][0] * od_sa[1][1]) /
	      (sin(od_sa[0][1]) * od_sa[0][0]);
	   aberr_lchrom = od_fline - od_cline;
	   max_lspher = sin(od_sa[0][1]);

	   

	   max_lspher = 0.0000926 / (max_lspher * max_lspher);
	   max_osc = 0.0025;
	   max_lchrom = max_lspher;
#ifndef ACCURACY
	}

        
	
#endif

	

        sprintf(outarr[0], "%15s   %21.11f  %14.11f",
           "Marginal ray", od_sa[0][0], od_sa[0][1]);
        sprintf(outarr[1], "%15s   %21.11f  %14.11f",
           "Paraxial ray", od_sa[1][0], od_sa[1][1]);
	sprintf(outarr[2],
           "Longitudinal spherical aberration:      %16.11f",
	   aberr_lspher);
	sprintf(outarr[3],
           "    (Maximum permissible):              %16.11f",
	   max_lspher);
	sprintf(outarr[4],
           "Offense against sine condition (coma):  %16.11f",
	   aberr_osc);
	sprintf(outarr[5],
           "    (Maximum permissible):              %16.11f",
	   max_osc);
	sprintf(outarr[6],
           "Axial chromatic aberration:             %16.11f",
	   aberr_lchrom);
	sprintf(outarr[7],
           "    (Maximum permissible):              %16.11f",
	   max_lchrom);


	errors = 0;
	for (i = 0; i < 8; i++) {
	   if (strcmp(outarr[i], refarr[i]) != 0) {
#ifdef ACCURACY
              printf("\nError in pass %ld for results on line %d...\n",
		     passes, i + 1);
#else
              printf("\nError in results on line %d...\n", i + 1);
#endif
              printf("Expected:  \"%s\"\n", refarr[i]);
              printf("Received:  \"%s\"\n", outarr[i]);
              printf("(Errors)    ");
	      k = strlen(refarr[i]);
	      for (j = 0; j < k; j++) {
                 printf("%c", refarr[i][j] == outarr[i][j] ? ' ' : '^');
		 if (refarr[i][j] != outarr[i][j])
		    errors++;
	      }
              printf("\n");
	   }
	}
#ifdef ACCURACY
	}
#else
	if (errors > 0) {
           printf("\n%d error%s in results.  This is VERY SERIOUS.\n",
              errors, errors > 1 ? "s" : "");
	} else
           printf("\nNo errors in results.\n");
#endif
	return 0;
}
