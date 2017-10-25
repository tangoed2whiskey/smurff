#include "omp_util.h"

// _OPENMP will be enabled if -fopenmp flag is passed to the compiler (use cmake release build)
#if defined(_OPENMP)

#include <iostream>
#include <omp.h>

int nthreads() 
{
   return omp_get_num_threads(); 
}

int thread_limit() 
{
   static int nt = -1;
   if (nt < 0) 
   #pragma omp parallel
   {
      #pragma omp single
      nt = omp_get_num_threads();
   }
   return nt;
}

int thread_num() 
{
   return omp_get_thread_num(); 
}

void threads_init() 
{
   std::cout << "Using OpenMP with up to " << thread_limit() << " threads.\n";
   #ifdef OPENBLAS
   std::cout << "Using BLAS with up to " << openblas_get_num_threads() << " threads.\n";
   #endif
}

#else

int thread_num() 
{
   return 0;
}

int nthreads() 
{
   return 1; 
}

int thread_limit() 
{
   return 1; 
}

void threads_init() 
{ 

}

#endif