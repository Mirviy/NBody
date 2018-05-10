#include<stdint.h>
#include<time.h>
#include<cmath>
#include"random32.h"

uint32_t random32(){
	static uint64_t rnd=time(NULL);
	const uint64_t a=6364136223846793005,c=1442695040888963407;
	rnd=rnd*a+c;
	return rnd>>32;
}
double random(){
	return (random32()+0.5)/4294967296;
}
double randomN(){
	const double dpi=6.2831853071795864;
	return sqrt(-2*log(random()))*cos(dpi*random());
}
