/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */
#define REAL float
#define REAL8 float8
#define REAL3 float4
#define ZERO3 {0.0f, 0.0f, 0.0f, 0.0f}

REAL3 bodyBodyInteraction(REAL3 ai, REAL8 bi, REAL8 bj, REAL softeningSquared, REAL damping) 
{
    REAL8 r;

    r.s0 = bi.s0 - bj.s0;
    r.s1 = bi.s1 - bj.s1;
    r.s2 = bi.s2 - bj.s2;
    r.s4 = bi.s4 - bj.s4;
    r.s5 = bi.s5 - bj.s5;
    r.s6 = bi.s6 - bj.s6;

    REAL distSqr = r.s0 * r.s0 + r.s1 * r.s1 + r.s2 * r.s2;
    REAL distVel = r.s0 * r.s4 + r.s1 * r.s5 + r.s2 * r.s6;
    distSqr += softeningSquared;

    REAL invDist = rsqrt((float)distSqr);
    REAL invDistSqr = invDist * invDist;

    REAL s = (bj.s3  + damping * distVel * invDistSqr) * invDist * invDistSqr;

    ai.x += r.x * s;
    ai.y += r.y * s;
    ai.z += r.z * s;

    return ai;
}

// This is the "tile_calculation" function from the GPUG3 article.
REAL3 gravitation(REAL8 myPos, REAL3 accel, REAL softeningSquared, REAL damping, __local REAL8* sharedPos)
{
    // The CUDA 1.1 compiler cannot determine that i is not going to 
    // overflow in the loop below.  Therefore if int is used on 64-bit linux 
    // or windows (or long instead of long long on win64), the compiler
    // generates suboptimal code.  Therefore we use long long on win64 and
    // long on everything else. (Workaround for Bug ID 347697)
#ifdef _Win64
    unsigned long long i = 0;
#else
    unsigned long i = 0;
#endif

    // Here we unroll the loop

    // Note that having an unsigned int loop counter and an unsigned
    // long index helps the compiler generate efficient code on 64-bit
    // OSes.  The compiler can't assume the 64-bit index won't overflow
    // so it incurs extra integer operations.  This is a standard issue
    // in porting 32-bit code to 64-bit OSes.
    int blockDimx = get_local_size(0);
    for (unsigned int counter = 0; counter < blockDimx; ) 
    {
        accel = bodyBodyInteraction(accel, sharedPos[i++], myPos, softeningSquared, damping); 
        accel = bodyBodyInteraction(accel, sharedPos[i++], myPos, softeningSquared, damping); 
        accel = bodyBodyInteraction(accel, sharedPos[i++], myPos, softeningSquared, damping); 
        accel = bodyBodyInteraction(accel, sharedPos[i++], myPos, softeningSquared, damping);
		counter += 4;
    }

    return accel;
}

// WRAP is used to force each block to start working on a different 
// chunk (and wrap around back to the beginning of the array) so that
// not all multiprocessors try to read the same memory locations at 
// once.
#define WRAP(x,m) (((x)<m)?(x):(x-m))  // Mod without divide, works on values from 0 up to 2m

REAL3 computeBodyAccel_noMT(REAL8 bodyPos, 
                             __global REAL8* positions, 
                             int numBodies, 
                             REAL softeningSquared, 
							 REAL damping,
                             __local REAL8* sharedPos)
{
    REAL3 acc = ZERO3;
    
    unsigned int threadIdxx = get_local_id(0);
    unsigned int blockIdxx = get_group_id(0);
    unsigned int gridDimx = get_num_groups(0);
    unsigned int blockDimx = get_local_size(0);
    unsigned int numTiles = numBodies / blockDimx;

    for (unsigned int tile = 0; tile < numTiles ; tile++) 
    {
        sharedPos[threadIdxx] = 
            positions[WRAP(blockIdxx + tile, gridDimx) * blockDimx + threadIdxx];
       
        barrier(CLK_LOCAL_MEM_FENCE);// __syncthreads();

        acc = gravitation(bodyPos, acc, softeningSquared, damping, sharedPos);
        
        barrier(CLK_LOCAL_MEM_FENCE);// __syncthreads();
    }
	
    return acc;
}

__kernel void integrateBodies_noMT(
            __global REAL8* newPos,
            __global REAL8* oldPos,
            REAL deltaTime,
            REAL damping,
            REAL softeningSquared,
            int numBodies,
            __local REAL8* sharedPos)
{
    unsigned int threadIdxx = get_local_id(0);
    unsigned int blockIdxx = get_group_id(0);
    unsigned int blockDimx = get_local_size(0);

    unsigned int index = mul24(blockIdxx, blockDimx) + threadIdxx;
    REAL8 pos = oldPos[index];   
    REAL3 accel = computeBodyAccel_noMT(pos, oldPos, numBodies, softeningSquared, damping, sharedPos);
	
    pos.s4 += accel.s0 * deltaTime;
    pos.s5 += accel.s1 * deltaTime;
    pos.s6 += accel.s2 * deltaTime;  

    pos.s0 += pos.s4 * deltaTime;
    pos.s1 += pos.s5 * deltaTime;
    pos.s2 += pos.s6 * deltaTime;

    newPos[index] = pos;
}

