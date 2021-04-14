/**############################################################################
 ** TITLE:		CipherSearch
 ** AUTHOR:		Magnus Overbo
 ** ABOUT:		Ciphersearch utilise the GMP library to manage arbitrary sized
 **						numbers and perform an unconstrained approximate row-based bit
 **						parallell search.  It searches for an intercepted bitsequence,
 **						generated by a dessimated LFSR encrypted with a message, and
 **						tries to find this in all possible generated states which is
 **						not dessimated.
 **
 ** Release:	20190313 - 64b version
 ** Release:	20190328 - GMP library version for arbitrary bit size
 ** Release:	20200101 - Add option for shift-OR implementation (default)
 **#########################################################################**/

//-----------------------------------------------------------------------------
// INCLUDES
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>     //strlen
#include <stdint.h>     //64b Int
#include <inttypes.h>   //64b int
#include <gmp.h>        //arbitrary integer size
#include "tpool.h"

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------
size_t num_threads = 16;	// Generally set to # of vCPUs in cloud instances
const int ALPHASIZE = 2;	// Alphabet size, 0 & 1
tpool_t *tm;
int col_acceptance;
int col;
int m;
int n;
int deg;
mpz_t pol;
int slen;
int plain = 0;
mpz_t PLAINTEXT;
mpz_t CIPHER;
int R1STATE;
int R2STATE;
mpz_t max;
mpz_t* B;

//-----------------------------------------------------------------------------
// STRUCTs
//-----------------------------------------------------------------------------
struct LFSR {
	mpz_t POLYNOMIAL; //Polynomial definition
	mpz_t STATE;	  //LFSR state
	int DEGREE;	  //Polynomial degree
};

struct CANDIDATE {
	int istate;		// R2 initial state
	bool match;
	int collisions;
	mpz_t X;		// Undecimated output
};


//-----------------------------------------------------------------------------
// FUNCTION DECLARATIONs
//-----------------------------------------------------------------------------
int search();													// Gen target system
int polyMap(int);
mpz_t* genAlphabet( int );					   													//Gen array of the alphabet
int lfsr_iterate( struct LFSR*);																//Gen next state & output
void lfsrgen(mpz_t, int, int, mpz_t, uint_least64_t, int, mpz_t*); 								//Gen LFRS
mpz_t* arbp_search( mpz_t*, mpz_t, int, int, int );       								       	//Main search function
mpz_t* genError(int, int);  																   	//Gen init error table
void genPrefixes( mpz_t*, mpz_t, int );															//Generate the prefixes
void genEncrypt( mpz_t, mpz_t, mpz_t, mpz_t, int );	        									//Encrypt the plaintext
void mpz_lshift( mpz_t, int );																	//Left shift bin seq by 1
//int match_R1( struct CANDIDATE*, struct CANDIDATE*);		//Exact match for the output of genEncrypt
char* pb( mpz_t, int, int );																	//Print prepending zeros


void match_R1(void *arg) {
	struct CANDIDATE *cand = (struct CANDIDATE *)arg;
	mpz_t LCLK;	mpz_init(LCLK);						//LFSR for dessimating
	mpz_t CIPHER2;	mpz_init( CIPHER2 );			//Gen test ciphertext
	mpz_init(LCLK);	
	mpz_init(CIPHER2);

	int i = 0;
	int c = 0;
	while (i<mpz_get_ui(max)) { // Iterate through all R1States
		#if defined DEBUG
			printf("Generating clocking LFSR (R1) output sequence: \n");
		#endif
		lfsrgen(LCLK, deg, m, pol, i, 0, NULL);				//Clocking LFSR
		
		#if defined DEBUG
			printf("Calculating the Decimated bitsequence and creating ciphertext:\n\n");
		#endif

		genEncrypt(CIPHER2, LCLK, cand->X , PLAINTEXT, m);
		
		// mpz_out_str(stdout, 16, CIPHER); printf(" - ");
		// mpz_out_str(stdout, 16, CIPHER2); printf("\n");
		// printf("%d\t-\t%d\n ", i, cand->istate);

		// Improve matching algorithm.
		if ( mpz_cmp(CIPHER, CIPHER2) == 0 ) { //mpz_get_ui( CIPHER2 ) == mpz_get_ui( CIPHER ) ) {
			if (cand->istate == R2STATE && i == R1STATE) { // CHEATING
				//printf("\t - ACTUAL INIT STATES FOUND for R1 init state %i and R2 init state %i\n", i, cand->istate);
				//exit(0);
			} else {
				//printf("\t - COLLISION FOUND at R1 init state %i and R2 init state %i\n", i, cand->istate);
				//run function to terminate all match_R1 instances.
				col ++;
				cand->collisions++;
				//tpool_destroy(tm);
			}
		}
		i++;
	}

	mpz_clear(LCLK);
	mpz_clear(CIPHER2);

}

void search_thread(void *arg) {

  	struct CANDIDATE *cand = (struct CANDIDATE *)arg;
	mpz_t TEXT; 																// This variable stores the current search text
	mpz_init(TEXT);
	int ci = 0;

	lfsrgen( TEXT, deg, n, pol, cand->istate, 0, NULL );								// Generate undecimated bitseq TEXT for current initial state 

	mpz_t*	MATCH = arbp_search(B, TEXT, slen, m, n);			// Run the ARBP search on TEXT with slen errors allowed and return matches with CLD and position. 

	int j = 0;
	while(j < n){																//For each position
		if( mpz_cmp_ui(MATCH[j], m) < 0 ){								//If match is less than m
			mpz_clear( MATCH[j] );
			ci++;																//increment counter for print
		}
		j++;																	//Next position
	}

	if (ci > 0) {											//If matches exist, add state to Candidate matrix.
		cand->match = true;
		mpz_init(cand->X); mpz_set(cand->X, TEXT);
		ci = 0;
	}
	else {	
		cand->match = false;
		mpz_init(cand->X); mpz_set(cand->X, TEXT);			
	}
	free(MATCH);
	//pthread_exit(NULL);
}

//-----------------------------------------------------------------------------
//	Function to create a target system by encrypting a given plaintext and returning the cipher
//-----------------------------------------------------------------------------
int main(int argc, char *argv[]){
	
	//------------------------------------------------------------
	// Parameter initialization and validation
	//------------------------------------------------------------
	if( argc != 9 ){	      							//Check that required input parameters are met
		printf("Incorrect number of arguments\nUsage: ./main <polynomial degree> <search word length> <search text length> <errors> <init state R1> <init state R2> <collision acceptance> <# CPUs>\n");
		return 1;
	}

	struct stat st = {0}; 
	if (stat("./data", &st) == -1) { 					// Create ./data directory if it does not exist.
		mkdir("./data", 0777);
	}

	deg = atoi( argv[1] );
	m = atoi( argv[2] );
	n = atoi( argv[3] );
	slen = atoi( argv[4] )+1;
	R1STATE = atoi( argv[5] );
	R2STATE = atoi( argv[6] );
	col_acceptance = atoi( argv[7] );
	num_threads = atoi( argv[8] );
	if (n<2*m) {
		printf("Search text must be at least 2 * m...\n");
		return 1;
	}
	//n = 2*m;											//Search text length 2m

	if (slen >= m) {
		printf("Error: Errors cannot be higher than search word (m)...\n");
		return 1;
	}

	mpz_init( pol );
	mpz_set_ui(pol, polyMap(deg));

	mpz_init( max );
	mpz_setbit(max, deg);								//Set max val, eg 2048 in 2^11
	if (R1STATE > mpz_get_ui(max)) {
		printf("Error: R1 initial state (%d) is higher than LFSR capacity (%lu)\n", R1STATE, mpz_get_ui(max));
		return 1;
	} else if (R2STATE > mpz_get_ui(max)) {
		printf("Error: R2 initial state (%d) is higher than LFSR capacity (%lu)\n", R2STATE, mpz_get_ui(max));
		return 1;
	}
		
	mpz_init(PLAINTEXT);	
	mpz_set_ui(PLAINTEXT, plain);							//Default value is 0

	mpz_t R1SEQ;		mpz_init(R1SEQ);						// 
	lfsrgen(R1SEQ, deg, m, pol, R1STATE, 0, NULL);		// Generate R1 output sequence

	mpz_t R2SEQ;		mpz_init(R2SEQ);				//LFSR to be dessimated
	lfsrgen(R2SEQ, deg, n, pol, R2STATE, 0, NULL);		//Dessimated LFSR

	mpz_init( CIPHER );									//Gen intercepted ciphertext
	genEncrypt(	CIPHER, R1SEQ, R2SEQ, PLAINTEXT, m);

	mpz_clear( R1SEQ );									//Cleanup LFSRs
	mpz_clear( R2SEQ );

	//printf("Testing Pol.deg = %d, R1 = %d, R2 = %d, m = %d, k = %d...m-1, CPUs=%zu\n", deg, R1STATE, R2STATE, m, slen, num_threads);

	int r;
	struct timespec start, finish;
	double elapsed;
	clock_gettime(CLOCK_MONOTONIC, &start);
	//while (true) {


	//-----------------------------------------------------------------------------
	// At this point, the target (CIPHER) has been created. 
	// The known-plaintext attack starts here.
	// TODO; Separate this from main function to enable multithreading.
	//
	// Step ONE: Generate prefixes and run ARBP search and choose candidates for R2
	// Candidates are stored in an array in memory in the form:
	// <initial state>, <R2 undeciamted output sequence of length n>
	//-----------------------------------------------------------------------------
	char* FNAME;
	int found;

	B	= genAlphabet( ALPHASIZE );				//Generate alphabet
	genPrefixes(B, CIPHER, m);							//Generate prefixes for the alphabet

	//tpool_t *tm;
	tm   = tpool_create(num_threads);

	struct CANDIDATE* C = malloc( mpz_get_ui(max) * sizeof(struct CANDIDATE) );

	for (int i = 0; i < (mpz_get_ui(max)-1); i++){		// Iterate through all initial states of R2
		C[i].istate = i+1;
		tpool_add_work(tm, search_thread, &C[i]);

	}
	tpool_wait(tm);
	tpool_destroy(tm);

	int u=0;

	struct CANDIDATE* ptr = C;
	struct CANDIDATE* endPtr = C + (sizeof(*C)/sizeof(struct CANDIDATE) * mpz_get_ui(max));

	// Open file for writing. May need to be moved back up if position of edits are to be written
	FNAME = malloc(60*sizeof(char));					//Filename allocation
	sprintf(FNAME, "./data/%d_%d_%d_%d_%d_%d.cand", deg, m, n, slen-1, R1STATE, R2STATE);
	FILE* fh = fopen(FNAME, "w");						// Open output file for writing

	while ( ptr < endPtr ) { // Iterate through all candidates
		if (ptr->match == true){
			fprintf(fh, "\n%i,", ptr->istate); mpz_out_str(fh, 62, ptr->X);
			if (ptr->istate == R2STATE) {
				found = 1;
				fprintf(fh, "%s", "[!]");
			}

			u++;
		}
		ptr++;
	}

	fclose( fh );												//Close data file

	if ( u == 0 ) { // Zero matches, empty dataset
		remove(FNAME);
		printf("%d,%d",-1,u);
		return -1;
	} else if( u >= mpz_get_ui(max)-1 ) {	 // Full dataset, all candidates
		remove(FNAME);
		printf("%d,%d",-2,u);
		return -2;
	} else if ( found != 1 ) { // Dataset not containing the actual candidate (cheat)
		remove(FNAME);
		printf("%d,%d",u,u);
		return u;
	} else  {
		// Check collisions
		// Start brute force attack with intercepted CIPHER and known PLAINTEXT
		// printf("[k=%d] Set of %d/%lu contains actual R2STATE! \n", slen, u, mpz_get_ui(max));
		// printf("Checking for collisions... This may take a long time depending on parameters. \n");

		//tpool_t *tm;
		tm   = tpool_create(num_threads);
		col = 0;
		for (int i = 0; i < (mpz_get_ui(max)-1); i++){		// Iterate through all initial states of R2
			C[i].istate = i+1;
			tpool_add_work(tm, match_R1, &C[i]);
		}
		tpool_wait(tm);
		tpool_destroy(tm);

		if (col > 0) {
			printf("%d,%d",-3,u);
			return -3;
		} else {
			printf("%d,%d",0,u);
		}

		// struct CANDIDATE* ptr = C;
		// while ( ptr < endPtr ) { // Iterate through all candidates
		// 	if ( ptr->collisions > 0 ){
				
		// 		return -3;
		// 	}
		// 	ptr++;
		// }
		return 0;
	}	
		// r = search();
		// printf("%d", r);
	// 	if ( r >= 1 ) { // Not containing actual R2State
	// 		//printf("[k=%d] FAILED: Set of %d/%lu contains no actual R2STATE... \n", slen, r, mpz_get_ui(max)); // CHEAT
	// 		printf("%d",r);
	// 		//slen ++;
	// 	} else if ( r == -2 ) {
	// 		printf("%d",r);

	// 		//printf("[k=%d] FAILED: Too many candidates... \n", slen);
	// 		// m++;
	// 		// mpz_init(R1SEQ);						// 
	// 		// lfsrgen(R1SEQ, deg, m, pol, R1STATE, 0, NULL);		// Generate R1 output sequence
	// 		// mpz_clear(CIPHER);
	// 		// mpz_init( CIPHER );									//Gen intercepted ciphertext
	// 		// genEncrypt(	CIPHER, R1SEQ, R2SEQ, PLAINTEXT, m);
	// 		//break;
	// 	} else if ( r == -1 ) {
	// 		printf("%d",r);
	// 		//printf("[k=%d] FAILED: Zero candidates... \n", slen);
	// 		//slen ++;
	// 	} else if ( r == -3 ) {
	// 		printf("%d",r);
	// 		//printf("[k=%d] FAILED: Collisions found... Exiting\n", slen);
	// 		//m++;
	// 		//break;
	// 		//len ++;
	// 	} else {
	// 		printf("%d",r);
	// 		//printf("[k=%d] SUCCESS!\n", slen);
	// 		//slen ++;
	// 		//break;
	// 	}
	// 	//if ( slen == m ) {
	// 		//break;
	// 	//}
	// //}
			exit(r);

	clock_gettime(CLOCK_MONOTONIC, &finish);

	elapsed = (finish.tv_sec - start.tv_sec);
	elapsed += (finish.tv_nsec - start.tv_nsec) / 1000000000.0;
	printf("%f seconds...\n", elapsed);

	return 0;
}


int search() { // Attack
	char* FNAME;
	int found;

	//-----------------------------------------------------------------------------
	// At this point, the target (CIPHER) has been created. 
	// The known-plaintext attack starts here.
	// TODO; Separate this from main function to enable multithreading.
	//
	// Step ONE: Generate prefixes and run ARBP search and choose candidates for R2
	// Candidates are stored in an array in memory in the form:
	// <initial state>, <R2 undeciamted output sequence of length n>
	//-----------------------------------------------------------------------------
	
	B	= genAlphabet( ALPHASIZE );				//Generate alphabet
	genPrefixes(B, CIPHER, m);							//Generate prefixes for the alphabet

	//tpool_t *tm;
	tm   = tpool_create(num_threads);

	struct CANDIDATE* C = malloc( mpz_get_ui(max) * sizeof(struct CANDIDATE) );

	for (int i = 0; i < (mpz_get_ui(max)-1); i++){		// Iterate through all initial states of R2
		C[i].istate = i+1;
		tpool_add_work(tm, search_thread, &C[i]);

	}
	tpool_wait(tm);
	tpool_destroy(tm);

	int u=0;

	struct CANDIDATE* ptr = C;
	struct CANDIDATE* endPtr = C + (sizeof(*C)/sizeof(struct CANDIDATE) * mpz_get_ui(max));

	// Open file for writing. May need to be moved back up if position of edits are to be written
	FNAME = malloc(60*sizeof(char));					//Filename allocation
	sprintf(FNAME, "./data/%d_%d_%d_%d_%d.cand", deg, m, slen-1, R1STATE, R2STATE);
	FILE* fh = fopen(FNAME, "w");						// Open output file for writing

	while ( ptr < endPtr ) { // Iterate through all candidates
		if (ptr->match == true){
			fprintf(fh, "\n%i,", ptr->istate); mpz_out_str(fh, 62, ptr->X);
			if (ptr->istate == R2STATE) {
				found = 1;
				fprintf(fh, "%s", "[!]");
			}

			u++;
		}
		ptr++;
	}

	fclose( fh );												//Close data file

	if ( u == 0 ) { // Zero matches, empty dataset
		remove(FNAME);
		return -1;
	} else if( u >= mpz_get_ui(max)-1 ) {	 // Full dataset, all candidates
		remove(FNAME);
		return -2;
	} else if ( found != 1 ) { // Dataset not containing the actual candidate (cheat)
		remove(FNAME);
		return u;
	} else  {
		// Check collisions
		// Start brute force attack with intercepted CIPHER and known PLAINTEXT
		// printf("[k=%d] Set of %d/%lu contains actual R2STATE! \n", slen, u, mpz_get_ui(max));
		// printf("Checking for collisions... This may take a long time depending on parameters. \n");

		//tpool_t *tm;
		tm   = tpool_create(num_threads);
		col = 0;
		for (int i = 0; i < (mpz_get_ui(max)-1); i++){		// Iterate through all initial states of R2
			C[i].istate = i+1;
			tpool_add_work(tm, match_R1, &C[i]);
		}
		tpool_wait(tm);
		tpool_destroy(tm);

		if (col > 0) {
			return -3;
		}

		// struct CANDIDATE* ptr = C;
		// while ( ptr < endPtr ) { // Iterate through all candidates
		// 	if ( ptr->collisions > 0 ){
				
		// 		return -3;
		// 	}
		// 	ptr++;
		// }
		return 0;
	}
}

/**############################################################################
 **
 **	Helper functions
 **
 **#########################################################################**/

/*-----------------------------------------------------------------------------
 * Generates the cipher which then becomes the prefix.
 *	Y is the clocking LFSR
 *	X is the encrypting LFSR
 *	Implementation of the BRM
-----------------------------------------------------------------------------*/
void genEncrypt(mpz_t rop, mpz_t CLK, mpz_t DES, mpz_t PLAINTEXT, int m){
	int i =	0;
	int	j = 0;
	int x, y;
	char* t;
	mpz_t CIPHER; mpz_init(CIPHER);
	while( i < m ){						//Counter for clocking lfsr 
		x = 0;		
		y = 0;
		#if defined DEBUG
			printf("\tCLK(%d)=%d	", i, mpz_tstbit(CLK, i) );
		#endif

		if( mpz_tstbit(CLK, i) == 1 ){
				j++;					//Decimate LFSR by skipping a bit
				#if defined DEBUG
					printf("\tDES(%d)=%d --> \tDES(%d)=%d", j-1, mpz_tstbit(DES, j-1), j, mpz_tstbit(DES, j) );
				#endif
		}
		#if defined DEBUG
		else{
			printf("\t\t\tDES(%d)=%d", j, mpz_tstbit(DES,j));
		}
		#endif

		//Val of dessimated LFSR output
		if( mpz_tstbit(DES, j) == 1 )	x = 1;	//grab value of bit
		if( mpz_tstbit(PLAINTEXT, i) == 1 )		y = 1;	//Grab value of bit
		if( (y^x) == 1 )	mpz_setbit(CIPHER, i);			//Xor to get CIPHER

		#if defined DEBUG
		printf(" ^ P(%d)=%d == C(%d)=%d\n", i, mpz_tstbit(PLAINTEXT, i), i, mpz_tstbit(CIPHER, i) );
		#endif
		i++;		
		j++;	//Increment counters
	}
	#if defined DEBUG
		printf("\n");
		t = pb(CIPHER, m, 0);
		printf("\tCIPHERTEXT: %s", t); mpz_out_str(stdout, 2, CIPHER);
			printf("try\n");

		free(t);
		printf("\n\n");
	#endif
	mpz_set(rop, CIPHER);								//Set var to generated cipher
	mpz_clear( CIPHER );
}

/*-----------------------------------------------------------------------------
 * 
 * Return an irreducible polynomial based on the given degree.
 * 
-----------------------------------------------------------------------------*/
int polyMap(int deg) {
	int pol;
	switch(deg) {
		case 11:
			pol = 1209;
			break;
		case 16:
			pol = 33262;
			break;
	}
	return pol;
}

/*-----------------------------------------------------------------------------
 * Prepend bits to a binary representation of a number.
 *	returns a char pointer to an array filled with missing bits 
 * or nothing if it is already full.
-----------------------------------------------------------------------------*/
char* pb( mpz_t num, int len, int b ){
	size_t plen = len+1 - mpz_sizeinbase( num, 2 );
	if( plen == 0 )
		return "";
	char* pre = malloc( plen * sizeof(char) );			//Allocate plen length array
	int i = 0;
	while( i < plen-1 ){
		if( b == 1){
			pre[i++] = '1';
		}
		else{
			pre[i++] = '0';
		}
	}
	pre[i]='\0';
	return pre;
}

/*-----------------------------------------------------------------------------
 *	Left shift MPZ_T variable to the left
 *		n number of shift
 *		rop is mpz_t value to shift
-----------------------------------------------------------------------------*/
void mpz_lshift( mpz_t rop, int len ) {
	mpz_t	tmp;	
	mpz_init( tmp );	//temp variable
	int i = len-1;
	while( i > 0 ){
		if(mpz_tstbit(rop, i-1) == 1){
			mpz_setbit( tmp, i );		//set bit if it
		}
		i--;
	}
	mpz_set(rop, tmp);					//Set original var to tmp
	mpz_clear(tmp);
}
                        
/*-----------------------------------------------------------------------------
 *	Generate initial m-bitmasks for the alphabet of ALPHASIZE
 *	Shift-OR Complements 1
-----------------------------------------------------------------------------*/
mpz_t* genAlphabet( int ALPHASIZE ){
	mpz_t* B = malloc((ALPHASIZE)*sizeof( mpz_t ));
	#if defined DEBUG
		printf( "Generating masks for the alphabet\n" );
	#endif

	int i=0;
	while( i < ALPHASIZE ){
		mpz_init( B[i] );
		mpz_set_ui( B[i], 0 );
		
		#if defined DEBUG
			printf("\tB[%d] ",i);
			mpz_out_str(stdout, 2, B[i]);
			printf("\n");
		#endif
		i++;
	}
	#if defined DEBUG
		printf( "\tDone\n\n" );
	#endif
	return B;
}

/*-----------------------------------------------------------------------------
 * LFSR iteration function
 *		Calculates the next state of the LFSR by first grabbing the MSB as output
 *		value.  Then it calculates the AND of cur state and the polynomial before
 *		running an XOR on all bits that are set in the temp var to generate the
 *		feedback value.
 *
 *		Finally it left shifts the entire original state and sets the LSB to the
 *		value of the feedback polynomial.
 *
 *		The feedback value is then set
-----------------------------------------------------------------------------*/
int lfsr_iterate( struct LFSR* lfsr) {
	int	i = 0;											//Counter
	int	fbck = 0;										//XOR calculated value
	int	ret	= mpz_tstbit( lfsr->STATE, lfsr->DEGREE-1 );

	mpz_t tmp;		
	mpz_init( tmp );
	mpz_and(tmp, lfsr->STATE, lfsr->POLYNOMIAL); //AND taps the LFSR according to the given POLYNOMIAL

	while( i < lfsr->DEGREE ){				//Calc feedback
		fbck = fbck + mpz_tstbit( tmp, i );
		i++;
	}

	fbck = fbck % 2; // XOR the feedback bits
	mpz_lshift(lfsr->STATE, lfsr->DEGREE);	//Left shift
	if( fbck == 1 )	mpz_setbit( lfsr->STATE, 0 );
	mpz_clear( tmp );
	return ret;											//Return output character
}

/*-----------------------------------------------------------------------------
 * Generate LFSR and output an n-length bitsequence
 * With all arbitrary skips until first prefix is met
-----------------------------------------------------------------------------*/
void lfsrgen(mpz_t rop, int psize, int olen, mpz_t p,
	 					uint_least64_t iv, int skip, mpz_t* B){
	int i;											//Counter var
	int initmatch = 0;								//Check if first prefix is found
	struct LFSR lfsr;								//Create struct variable
	char* t;

	lfsr.DEGREE = psize;							//Set polynomial degree

	mpz_init( lfsr.POLYNOMIAL );
	mpz_set(lfsr.POLYNOMIAL, p);					//Set polynomial

	mpz_init( lfsr.STATE );
	mpz_set_ui(lfsr.STATE, iv);						//Set initial vector (seed)
	
	#if defined DEBUG
		t = pb(lfsr.STATE, psize, 0);
		printf("\n\tINIT STT:\t%s",t); mpz_out_str( stdout, 2, lfsr.STATE );
		printf("\n");
		free(t);
	#endif
	
	mpz_t OUTPUT;
	mpz_init( OUTPUT );
	int	tmpOUT	= 0;								//temp char holder
	i = 0;											//Zero out counter
	while( i < olen ){								//Iterate the LFSR for the set output length
		tmpOUT = lfsr_iterate(&lfsr);
		if( initmatch == 0 && skip == 1) {			// Iterate until we match the first bit of the sequence

			#if defined SHIFTOR
			if( mpz_tstbit(B[tmpOUT], 0) == 0 ) {
			//Set output to tmpvar
			#else
			if( mpz_tstbit(B[tmpOUT], 0) == 1 ) {
			#endif
				initmatch = 1;						//Set state to found
				if( tmpOUT == 1 )
					mpz_setbit(OUTPUT, i);			//Set output to tmpvar

				i++;								//inc counter

				// #if defined DEBUG
				// 	t = pb(B[0],m);
				// 	printf("\tinitmatch found\t\t%d |\t0: %s", tmpOUT, t);
				// 	mpz_out_str( stdout, 2, B[0] );
				// 	free(t); t = pb(B[1],m);
				// 	printf("\n\t\t\t\t\t1: %s", t);		
				// 	mpz_out_str( stdout, 2, B[1] );
				// 	printf("\n");
				// 	free(t);
				// #endif
			}

			// #if defined DEBUG
			// else{
			// 	t = pb(B[0],m);
			// 	printf("\tinitmatch not found\t%d |\t0: %s", tmpOUT, t);
			// 	mpz_out_str( stdout, 2, B[0] );
			// 	free(t); t = pb(B[1],m);
			// 	printf("\n\t\t\t\t\t1: %s", t);			
			// 	mpz_out_str( stdout, 2, B[1]);
			// 	printf("\n");
			// 	free(t);
			// }
			// #endif
		}
		else {
			if( tmpOUT == 1)	mpz_setbit(OUTPUT, i);
			i++;															//Next bit
		}
	}

	//Print LFSR information
	#if defined DEBUG
	printf("\tDEGREE:\t\t%d\n", psize);
	printf("\tLENGTH:\t\t%d\n", olen);
	printf("\tPOLYNOMIAL:\t"); mpz_out_str( stdout, 2, lfsr.POLYNOMIAL );
	t = pb(lfsr.STATE, psize, 0);
	printf("\n\tFINAL STT:\t%s", t); mpz_out_str( stdout, 2, lfsr.STATE );
	free(t); t = pb(OUTPUT, olen, 0);
	printf("\n\tSEQUENCE:\t%s", t); mpz_out_str( stdout, 2, OUTPUT);
	free(t);
	printf("\n\n");
	#endif
	mpz_set(rop, OUTPUT);
	mpz_clear(OUTPUT);
	mpz_clear( lfsr.POLYNOMIAL );
	mpz_clear( lfsr.STATE );
	//free( lfsr.DEGREE );
	//free( lfsr ); 
}

/*-----------------------------------------------------------------------------
 * Creates the prefixes
-----------------------------------------------------------------------------*/
void genPrefixes( mpz_t* B, mpz_t P, int m ){
	int	 j = 0;

	mpz_t tmp;  
	mpz_init(tmp); 
	mpz_set_ui( tmp, 1);

	while( j<m ){
		int ci = mpz_tstbit(P, j);				//Char value 0/1

		#if defined DEBUG
			printf("\tj=%d\tB[%d] |\t%s", j, mpz_tstbit(P,j), pb(B[ci],m,0) );
			mpz_out_str(stdout, 2, B[ci]);
		#endif

		mpz_ior( B[ci], B[ci], tmp );			//current value or-ed with 10^(j-1)
		mpz_lshift( tmp, m );

		#if defined DEBUG
			printf( "\n\t\t\t%s", pb(B[ci],m,0));
			mpz_out_str( stdout, 2, B[ci]);		printf( "\n" );
		#endif

		j++;									//Next pattern character
	}

	// Invert prefixes for Shift-OR mode
	#if defined SHIFTOR
		int i = 0;
		mpz_t mask;
		mpz_init(mask);
		mpz_ui_pow_ui(mask, ALPHASIZE, m);
		mpz_sub_ui(mask, mask, 1);
		while( i<ALPHASIZE ){
			mpz_xor( B[i], B[i], mask );
			i++;
		}
	#endif

	mpz_clear( tmp );

}

/*-----------------------------------------------------------------------------
 * Creates the error list of K-size
-----------------------------------------------------------------------------*/
mpz_t* genError(int K, int m) {
	mpz_t*	R	= malloc( K*sizeof(mpz_t) );		//Allocate memory for array
	#if defined DEBUG
		printf("Gen error R[%d..%d]\n", 0, K-1);
	#endif
	int k = 0;																//Set counter
	while( k<K ){
		int i = 0;
		mpz_init( R[k] );
		while( i < k ){
			mpz_setbit( R[k], i );
			i++;
		}
		#if defined SHIFTOR
		mpz_t mask;
		mpz_init(mask);
		mpz_ui_pow_ui(mask, ALPHASIZE, m);
		mpz_sub_ui(mask, mask, 1);
		mpz_xor( R[k], R[k], mask );
		#endif

		#if defined DEBUG
		char* t;
		t = pb(R[i],m,0);
		printf("\tR[%d]\t= %s", k, t);
		mpz_out_str( stdout, 2, R[k]);
		printf( "\n" );
		#endif
		k++;
	}

	return R;
}

/*-----------------------------------------------------------------------------
 * Perform search on TEXT and PREFIX
-----------------------------------------------------------------------------*/
mpz_t* arbp_search(mpz_t* B, mpz_t TEXT, int K, int m, int n) {
	mpz_t tmp1;	
	mpz_init( tmp1 );									//Tmp variables
	mpz_t tmp2;		
	mpz_init( tmp2 );
	mpz_t tmp3;		
	mpz_init( tmp3 );
														//Match for each position in text
	mpz_t* R = genError(K, m);							//Gen error-table
	mpz_t* MATCHES = malloc( n * sizeof(mpz_t) );

	mpz_t oldR, newR;									//Create and init variables
	mpz_init( oldR );
	mpz_init( newR );

	char* t;
	char* b;

	#if defined DEBUG
		printf( "\nBeginning search\n");					//Debug
	#endif

	uint_least64_t pos	= 0;							//Start search from char 1

	while( pos < n ){									//Search entire TEXT
		
		#if defined DEBUG
			printf("-------------- %llu", pos);
		#endif

		int Ti	= mpz_tstbit( TEXT, pos );				//Grab current chars int value

		mpz_clear( oldR );	mpz_init( oldR );			//Reset variables
		mpz_clear( newR );	mpz_init( newR );

		mpz_set( oldR, R[0] );							//Init oldR to cur R[0] (R[i])
		mpz_set( tmp1, R[0] );

		#if defined SHIFTOR
			mpz_lshift( tmp1, m );						//lshift
			//b = pb(tmp1,m,0); printf("\nLSHFT: \t%s", b); mpz_out_str(stdout, 2, tmp1);
			mpz_ior( tmp1, tmp1, B[Ti] );				//OR with B[Ti]
			//b = pb(tmp1,m,0); printf("\nORWB: \t%s", b); mpz_out_str(stdout, 2, tmp1);

		#else
			mpz_lshift( tmp1, m );						//lshift
			//b = pb(tmp1,m,0); printf("\nLSHFT: \t%s", b); mpz_out_str(stdout, 2, tmp1);
			mpz_setbit( tmp1, 0 );						//OR with 1
			//b = pb(tmp1,m,0); printf("\nORW1: \t%s", b); mpz_out_str(stdout, 2, tmp1);
			mpz_and( tmp1, tmp1, B[Ti] );				//AND with B[Ti]
			//b = pb(tmp1,m,0); printf("\nANDWB: \t%s", b); mpz_out_str(stdout, 2, tmp1);
		#endif


		#if defined DEBUG
			b = pb(B[Ti],m,0);
			printf("\n\nB[%d]: %s", Ti, b);
			mpz_out_str(stdout, 2, B[Ti]);		
		#endif

		mpz_set( newR, tmp1 );							//Set newR to tmp
		mpz_set(R[0], newR);							//Set R[0] to R'[i]

		#if defined (DEBUG) && defined (SHIFTOR)
			if(mpz_tstbit(R[0], m-1) > 0){ 
				t = pb(R[0],m,1);
			}
			else{
				t = pb(R[0],m,0);
			}
			printf("\nR[0]: %s", t);
			mpz_out_str(stdout, 2, R[0]);
			if( mpz_tstbit(R[0], m-1) == 0 ) printf(" [!]"); // Print indicator if match
			printf("\n");
		#elif defined DEBUG
			t = pb(R[0],m,0);
			printf("\nR[0]: %s", t);
			mpz_out_str(stdout, 2, R[0]);
			if( mpz_tstbit(R[0], m-1) == 1 ) printf(" [!]"); // Print indicator if match
			printf("\n");
		#endif


		uint_least64_t i = 1;								//Calc matches with K allowed errors
		while( i < K ) {
			mpz_clear( tmp1 );mpz_clear(tmp2); mpz_clear(tmp3);
			mpz_init(tmp1);	mpz_init(tmp2);	mpz_init(tmp3);	//reset and initialise temp variables

															//Substitute and deletion
			#if defined SHIFTOR
				mpz_set(tmp3, oldR);						//tmp3 = oldR
				mpz_lshift(tmp3, m);						//tmp3 = <tmp3> << 1
				mpz_ior(tmp2, tmp3, B[Ti]);

				mpz_and(tmp2, tmp2, tmp3);

				#if defined INC_INSERT						//Insertion
					mpz_and(tmp2, oldR, tmp2);					//tmp2 = oldR | <tmp2>
				#endif

				mpz_set(tmp1, R[i]);						//Copy value
				mpz_lshift(tmp1, m);						//tmp1 = R[i]<<1
				mpz_ior(tmp1, tmp1, B[Ti]);					//tmp1 = <tmp1> & B[Ti]

				mpz_and(tmp1, tmp1, tmp2);					//tmp1 = <tmp1> | <tmp2>

				mpz_set(newR, tmp1);						//newR = <tmp1>
				mpz_set(oldR, R[i]);						//Store R[i] for next error
				mpz_set(R[i], newR);						//R[i] == R'[i]

			#else											// Shift-AND
				mpz_ior(tmp2, oldR, newR);					//tmp2 = (oldR|newR)
				mpz_lshift(tmp2, m);						//tmp2 = <tmp2> << 1
				mpz_setbit( tmp2, 0 );

				#if defined INC_INSERT						//Insertion
					mpz_ior(tmp2, oldR, tmp2);					//tmp2 = oldR | <tmp2>
				#endif

				mpz_set(tmp1, R[i]);						//Copy value
				mpz_lshift(tmp1, m);						//tmp1 = R[i]<<1
				mpz_and(tmp1, tmp1, B[Ti]);					//tmp1 = <tmp1> & B[Ti]

				mpz_ior(tmp1, tmp1, tmp2);					//tmp1 = <tmp1> | <tmp2>

				mpz_set(newR, tmp1);						//newR = <tmp1>
				mpz_set(oldR, R[i]);						//Store R[i] for next error
				mpz_set(R[i], newR);						//R[i] == R'[i]
			#endif


			
			#if defined (DEBUG) && defined (SHIFTOR)
				if(mpz_tstbit(R[i], m-1) > 0){ 
					t = pb(R[i],m,1);
				}
				else{
					t = pb(R[i],m,0);
				}
				//t = pb(R[i],m,1);
				printf("R[%llu]: %s", i, t );
				mpz_out_str(stdout, 2, R[i]);
				if( mpz_tstbit(newR, m-1) == 0 ) printf(" [!]");
				printf("\n");	

			#elif defined DEBUG
				t = pb(R[i],m,0);
				printf("R[%llu]: %s", i, t );
				mpz_out_str(stdout, 2, R[i]);
				if( mpz_tstbit(newR, m-1) == 1 ) printf(" [!]"); // Print indicator if match
				printf("\n");	

			#endif

			i++;										//Next error
		}

		// #if !defined DEBUG
		// if( CSTATE == SSTATE ){
		// #endif
		mpz_init( MATCHES[pos] );
		mpz_set_ui( MATCHES[pos], m );				//Init val of match at cur pos
		int j	= 0;								//Init counter
		#if defined SHIFTOR
		if( mpz_tstbit(newR, m-1) == 0 ){			//Check if R-table has a match
		#else				
		if( mpz_tstbit(newR, m-1) == 1 ){			//Check if R-table has a match
		#endif

		while( j<K ){							//Loop R-table for matches (MSB set)
				#if defined SHIFTOR
				if(mpz_tstbit(R[j], m-1) == 0){		//Check if MSB zero
				#else
				if(mpz_tstbit(R[j], m-1) == 1){		//Check if MSB set
				#endif
					mpz_set_ui(MATCHES[pos], j);	//Set match to the R-level (0-K)
					j = K;												//Skip to end
				}
				j++;														//Next error value
			}
		}

		// #if !defined DEBUG
		// }
		// #endif
		#if defined DEBUG
		printf("\n");
		#endif
		pos += 1;															//Next position in search text
	}

	#if defined DEBUG
		printf("Search done.\n");
	#endif
	mpz_clear( oldR );
	mpz_clear( newR );
	free(R);

	//#if defined DEBUG
	return MATCHES;
	//#else
	//	if( CSTATE == SSTATE )	return MATCHES;
	//	else return NULL;
	//#endif
}



