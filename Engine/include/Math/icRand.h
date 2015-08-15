#ifndef __IC_RAND_H__
#define __IC_RAND_H__

int icRandAutoSeed(void);
void icRandSeed(int seed);

int icRandI(void);
int icRandI(int min, int max);

uint icRandUI(void);
uint icRandUI(uint min, uint max);

int64 icRandI64(void);
int64 icRandI64(int64 min, int64 max);

uint64 icRandUI64(void);
uint64 icRandUI64(uint64 min, uint64 max);

float icRandF(void);
float icRandF(float min, float max);

double icRandD(void);
double icRandD(double min, double max);

icReal icRandR(void);
icReal icRandR(icReal min, icReal max);


#include "icRand.inl"

#endif //__IC_RAND_H__