/*************************************************************************/
/*                                                                       */
/* FILE NAME                                                             */
/*     atm/sarmath.c 				                                     */
/*                                                                       */
/* DESCRIPTION  :                                                        */
/*          S3C2510 ATM SAR Math Funct.,  Ver. 1.0,  T.H.KIM             */
/*                                                                       */
/*-----------------------------------------------------------------------*/
/*              Copyrigth (C) 1999 Samsung Electronics. Inc.             */
/*-----------------------------------------------------------------------*/
/* REVIEW                                                                */
/*                                                                       */
/*         NAME            DATE                    REMARKS               */
/*                                                                       */
/*************************************************************************/

#define DOMAIN          1       /* argument domain error */
#define SING            2       /* argument singularity */
#define OVERFLOW        3       /* overflow range error */
#define UNDERFLOW       4       /* underflow range error */
#define TLOSS           5       /* total loss of precision */
#define PLOSS           6       /* partial loss of precision */
#define EDOM            33
#define ERANGE          34
#define MIEEE           1
#define ANSIC           1
#define mtherr(a,b)

typedef struct {
	float r;
	float i;
}cmplxf;

/* for log function */
#define M_PIf 3.1415926535f

/* for floor function */
#define DENORMAL 1
#define EXPMSK 0x807f
#define MEXP 255
#define NBITS 24

float floor_ss( float );
float frexp( float, int *);
float ldexp( float, int );
float powi( float, int );
float log_ss( float xx );

float MAXNUMF = 1.7014117331926442990585209174225846272e38f;
/* Bit clearing masks: */
static unsigned short bmask[] = {
	0xffff, 0xfffe, 0xfffc, 0xfff8, 0xfff0, 0xffe0,
	0xffc0, 0xff80, 0xff00, 0xfe00, 0xfc00, 0xf800,
	0xf000, 0xe000, 0xc000, 0x8000, 0x0000,
};

float floor_ss( float x )
{
#if	0
	unsigned short *p;
	float y;
	int e;

	y = x; 
	p = (unsigned short *)&y;
	e = (( *p >> 7) & 0xff) - 0x7f;
	p += 1;

	if( e < 0 ) {
		if( y < 0 ) return( -1.0f );
		else return( 0.0f );
	}
	e = (NBITS -1) - e;
	/* clean out 16 bits at a time */
	while( e >= 16 ) {
		*p-- = 0;
		e -= 16;
	}
	/* clear the remaining bits */
	if( e > 0 ) *p &= bmask[e];
	if( (x < 0) && (y != x) ) y -= 1.0f;
	return(y);
#endif
/*	printk("DBTH : Floor func. exec.\n");*/
	return(2.0);
}


float frexp( float x, int *pw2 )
{
	float y;
	int i, k;
	short *q;

	y = x;
	q = (short *)&y;
	i  = ( *q >> 7) & 0xff;
	if( i == 0 ) {
		if( y == 0.0f ) {
			*pw2 = 0;
			return(0.0f);
		}

		do {
			y *= 2.0f;
			i -= 1;
			k  = ( *q >> 7) & 0xff;
		}
		while( k == 0 );
		i = i + k;
	}
	i -= 0x7e;
	*pw2 = i;
	*q &= 0x807f;	/* strip all exponent bits */
	*q |= 0x3f00;	/* mantissa between 0.5 and 1 */
	return( y );
}

float ldexp( float x, int pw2 )
{
	float y;
	short *q;
	int e;

	y = x;
	q = (short *)&y;
	while( (e = ( *q >> 7) & 0xff) == 0 )
	{
		if( y == 0.0f ) return( 0.0f );
		if( pw2 > 0 ) {
			y *= 2.0f;
			pw2 -= 1;
		}
		if( pw2 < 0 ) {
			if( pw2 < -24 ) return( 0.0f );
			y *= 0.5f;
			pw2 += 1;
		}
		if( pw2 == 0 ) return(y);
	}
	e += pw2;
	if( e > MEXP ) return( MAXNUMF );
	*q &= 0x807f;
	if( e < 1 ) {
		if( e < -24 ) return( 0.0f );
		*q |= 0x80;
		while( e < 1 ) {
			y *= 0.5f;
			e += 1;
		}
		e = 0;
	}
	*q |= (e & 0xff) << 7;
	return(y);
}

float MINLOGF = -103.278929903431851103f;
float SQRTHF = 0.707106781186547524f;
float log_ss( float xx )
{
	register float y;
	float x, z, fe;
	int e;

	x = xx;
	fe = 0.0f;
	
/*	printk("DBKTH : log func. s0\n");*/
	x = frexp( x, &e );
/*	printk("DBKTH : log func. s1\n");*/
	if( x < SQRTHF ) {
		e -= 1;
		x = x + x - 1.0f; /*  2x - 1  */
	}	
	else x = x - 1.0f;

	z = x * x;
	y = (((((((( 7.0376836292E-2f * x - 1.1514610310E-1f) * x
	    + 1.1676998740E-1f) * x - 1.2420140846E-1f) * x
	    + 1.4249322787E-1f) * x - 1.6668057665E-1f) * x
	    + 2.0000714765E-1f) * x - 2.4999993993E-1f) * x
	    + 3.3333331174E-1f) * x * z;

/*	printk("DBKTH : log func. s2\n");*/
	if( e ) {
		fe = e;
		y += -2.12194440e-4f * fe;
	}
	y +=  -0.5f * z;  /* y - 0.5 x^2 */
	z = x + y;   /* ... + x  */
	if( e ) z += 0.693359375f * fe;

/*	printk("DBKTH : Log func. exec.\n");*/
	return( z );
}


float LOGE2F = 0.693147180559945309f;
float MAXLOGF = 88.02969187150841f;

/* 2^(-i/16)
 * The decimal values are rounded to 24-bit precision
 */

float frexp( float, int * );

static float A[] = {
	1.00000000000000000000E0,     9.57603275775909423828125E-1,
	9.17004048824310302734375E-1, 8.78126084804534912109375E-1,
	8.40896427631378173828125E-1, 8.05245161056518554687500E-1,
	7.71105408668518066406250E-1, 7.38413095474243164062500E-1,
	7.07106769084930419921875E-1, 6.77127778530120849609375E-1,
	6.48419797420501708984375E-1, 6.20928883552551269531250E-1,
	5.94603538513183593750000E-1, 5.69394290447235107421875E-1,
	5.45253872871398925781250E-1, 5.22136867046356201171875E-1,
	5.00000000000000000000E-1 };

/* continuation, for even i only
 * 2^(i/16)  =  A[i] + B[i/2]
 */
static float B[] = {
	 0.00000000000000000000E0,     -5.61963907099083340520586E-9,
	-1.23776636307969995237668E-8,  4.03545234539989593104537E-9,
	 1.21016171044789693621048E-8, -2.00949968760174979411038E-8,
	 1.89881769396087499852802E-8, -6.53877009617774467211965E-9,
	 0.00000000000000000000E0 };

/* 1 / A[i]
 * The decimal values are full precision
 */
static float Ainv[] = {
	1.00000000000000000000000E0, 1.04427378242741384032197E0,
	1.09050773266525765920701E0, 1.13878863475669165370383E0,
	1.18920711500272106671750E0, 1.24185781207348404859368E0,
	1.29683955465100966593375E0, 1.35425554693689272829801E0,
	1.41421356237309504880169E0, 1.47682614593949931138691E0,
	1.54221082540794082361229E0, 1.61049033194925430817952E0,
	1.68179283050742908606225E0, 1.75625216037329948311216E0,
	1.83400808640934246348708E0, 1.91520656139714729387261E0,
	2.00000000000000000000000E0 };

#define PMEXP 2031.0f
#define MNEXP -2031.0f

/* log2(e) - 1 */
#define LOG2EA 0.44269504088896340736f

#define F W
#define Fa Wa
#define Fb Wb
#define G W
#define Ga Wa
#define Gb u
#define H W
#define Ha Wb
#define Hb Wb


/* Find a multiple of 1/16 that is within 1/16 of x. */
#define reduc(x)  0.0625f * floor_ss( 16.0f * (x) )

float pow_ss( float x, float y )
{
#if	0
	float u, w, z, W, Wa, Wb, ya, yb;
	int e, i, nflg;

	nflg = 0;	/* flag = 1 if x<0 raised to integer power */
	w = floor_ss(y);
	if( w < 0 ) z = -w;
	else z = w;
	if( (w == y) && (z < 32768.0f) ) {
		i = w;
		w = powi( x, i );
		return( w );
	}

	if( x <= 0.0f ) {
		if( x == 0.0f ) {
			if( y == 0.0f ) return( 1.0f );  /*   0**0   */
			else  return( 0.0f );  /*   0**y   */
		}
		else {
			if( w != y ) { 
				/* noninteger power of negative number */
				return(0.0f);
			}
			nflg = 1;
			if( x < 0 ) x = -x;
		}
	}

	/* separate significand from exponent */
	x = frexp( x, &e );

	/* find significand in antilog table A[] */
	i = 1;
	if( x <= A[9] ) i = 9;
	if( x <= A[i+4] ) i += 4;
	if( x <= A[i+2] ) i += 2;
	if( x >= A[1] ) i = -1;
	i += 1;

	/* Find (x - A[i])/A[i]
 	 * in order to compute log(x/A[i]):
 	 * log(x) = log( a x/a ) = log(a) + log(x/a)
 	 * log(x/a) = log(1+v),  v = x/a - 1 = (x-a)/a
 	 */
	x -= A[i];
	x -= B[ i >> 1 ];
	x *= Ainv[i];


	/* rational approximation for log(1+v):
 	 * log(1+v)  =  v  -  0.5 v^2  +  v^3 P(v)
 	 * Theoretical relative error of the approximation is 3.5e-11
 	 * on the interval 2^(1/16) - 1  > v > 2^(-1/16) - 1
 	 */
	z = x*x;
	w = (((-0.1663883081054895f  * x + 0.2003770364206271f) * x
		- 0.2500006373383951f) * x + 0.3333331095506474f) * x * z;
	w -= 0.5f * z;

	/* Convert to base 2 logarithm:
 	 * multiply by log2(e)
 	 */
	w = w + LOG2EA * w;

	/* Note x was not yet added in
 	 * to above rational approximation,
 	 * so do it now, while multiplying
 	 * by log2(e).
 	 */
	z = w + LOG2EA * x;
	z = z + x;

	/* Compute exponent term of the base 2 logarithm. */
	w = -i;
	w *= 0.0625f;  /* divide by 16 */
	w += e;
	/* Now base 2 log of x is w + z. */

	/* Multiply base 2 log by y, in extended precision. */
	
	/* separate y into large part ya
 	 * and small part yb less than 1/16
 	 */
	ya = reduc(y);
	yb = y - ya;

	F = z * y  +  w * yb;
	Fa = reduc(F);
	Fb = F - Fa;

	G = Fa + w * ya;
	Ga = reduc(G);
	Gb = G - Ga;

	H = Fb + Gb;
	Ha = reduc(H);
	w = 16.0f * (Ga + Ha);

	/* Test the power of 2 for overflow */
	if( w > PMEXP ) return( MAXNUMF );
	if( w < MNEXP ) return( 0.0f );
	
	e = w;
	Hb = H - Ha;

	if( Hb > 0.0f ) {
		e += 1;
		Hb -= 0.0625f;
	}

	/* Now the product y * log2(x)  =  Hb + e/16.0.
 	 *
 	 * Compute base 2 exponential of Hb,
 	 * where -0.0625 <= Hb <= 0.
 	 * Theoretical relative error of the approximation is 2.8e-12.
 	 */
	/*  z  =  2**Hb - 1    */
	z = ((( 9.416993633606397E-003f * Hb + 5.549356188719141E-002f) * Hb
		+ 2.402262883964191E-001f) * Hb + 6.931471791490764E-001f) * Hb;

	/* Express e/16 as an integer plus a negative number of 16ths.
 	 * Find lookup table entry for the fractional power of 2.
 	 */
	if( e < 0 ) i = -( -e >> 4 );
	else i = (e >> 4) + 1;
	e = (i << 4) - e;
	w = A[e];
	z = w + w * z;      /*    2**-e * ( 1 + (2**Hb-1) )    */
	z = ldexp( z, i );  /* multiply by integer power of 2 */

	if( nflg ) {
		/* For negative x,
 		 * find out if the integer exponent
 		 * is odd or even.
 		 */
		w = 2.0f * floor_ss( 0.5f * w );
		if( w != y ) z = -z; /* odd exponent */
	}

	return( z );
#endif
/*	printk("DBKTH : Power func. exec.\n");*/
	return(1.0);
}

float powi( float x, int nn )
{
	int n, e, sign, asign, lx;
	float w, y, s;

	if( x == 0.0f ) {
		if( nn == 0 ) return( 1.0f );
		else if( nn < 0 )
			return( MAXNUMF );
		else return( 0.0f );
	}

	if( nn == 0 ) return( 1.0f );

	if( x < 0.0f ) {
		asign = -1;
		x = -x;
	}
	else asign = 0;

	if( nn < 0 ) {
		sign = -1;
		n = -nn;
	}
	else {
		sign = 0;
		n = nn;
	}

	/* Overflow detection */

	/* Calculate approximate logarithm of answer */
	s = frexp( x, &lx );
	e = (lx - 1)*n;
	if( (e == 0) || (e > 64) || (e < -64) ) {
		s = (s - 7.0710678118654752e-1f) / (s +  7.0710678118654752e-1f);
		s = (2.9142135623730950f * s - 0.5f + lx) * nn * LOGE2F;
	}
	else {
		s = LOGE2F * e;
	}

	if( s > MAXLOGF ) {
		y = MAXNUMF;
		goto done;
	}

	if( s < MINLOGF ) return(0.0f);

	/* Handle tiny denormal answer, but with less accuracy
 	 * since roundoff error in 1.0/x will be amplified.
 	 * The precise demarcation should be the gradual underflow threshold.
 	 */
	if( s < (-MAXLOGF+2.0f) ) {
		x = 1.0f/x;
		sign = 0;
	}

	/* First bit of the power */
	if( n & 1 ) y = x;
	else {
		y = 1.0f;
		asign = 0;
	}

	w = x;
	n >>= 1;
	while( n ) {
		w = w * w;	/* arg to the 2-to-the-kth power */
		if( n & 1 )	/* if that bit is set, then include in product */
			y *= w;
		n >>= 1;
	}

done:
	if( asign )
		y = -y; /* odd power of negative number */
	if( sign )
		y = 1.0f/y;
	return(y);
}


/*-----------------------------Memory copy routine for word, half word & byte size */
void qcopy( void *dst, void *src, unsigned long length )
{
        unsigned char   *dstb, *srcb;
        unsigned long   cnt, rem;

	if ( !(((unsigned long)dst)%4) && !(((unsigned long)src)%4) ) {
                unsigned long   *dstw, *srcw, *endw;
                dstw = (unsigned long *)dst; srcw = (unsigned long *)src;
                cnt = length/4; rem = length%4;
                endw = srcw+cnt;
                while( srcw<endw ) { *dstw++ = *srcw++; }
                if ( rem ) {
                        dstb = (unsigned char *)dstw; srcb = (unsigned char *)srcw;
                        while ( rem-- ) { *dstb++ = *srcb++; }	                
		}
        }
        else if ( !(((unsigned long)dst)%2) && !(((unsigned long)src)%2) ) {
                unsigned short  *dsth, *srch, *endh;
                dsth = (unsigned short *)dst; srch = (unsigned short *)src;
                cnt = length/2; rem = length%2;
                endh = srch+cnt;
                while( srch<endh ) { *dsth++ = *srch++; }
                if ( rem ) {
                        *((unsigned char *)dsth) = *((unsigned char *)srch);
                }
        }
	else {
                dstb = (unsigned char *)dst; srcb = (unsigned char *)src;
                cnt = length; 
                while ( cnt-- ) { *dstb++ = *srcb++; }
        }
}
