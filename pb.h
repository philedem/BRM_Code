#include <gmp.h>
#include <stdlib.h>

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