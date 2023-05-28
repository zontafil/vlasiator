
/* This is a unit testing suite for the Vlasiator unified CPU-GPU for loop interface.
 * Compile for device execution with
 *  `USE_CUDA=1 VLASIATOR_ARCH=mahti_cuda make -j12 unit_testing_fields`
 * and for host execution with
 *  `USE_CUDA=0 VLASIATOR_ARCH=mahti_cuda make -j12 unit_testing_fields`
 */

/* Included standard headers */
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <tuple> 
#include <vector>
#include <fsgrid.hpp>

#define ARCH_MAIN 1

/* Include the tested architecture-specific header */
#include "arch_device_api.h"
#include "../mpiconversion.h"
#include "../common.h"

/* Host execution of min() and max() require using std namespace */
using namespace std;

/* Auxiliary function for result evaluation and printing */
void result_eval(std::tuple<bool, double, double> res, const uint test_id){
  std::string success = std::get<0>(res) == true ? "PASSED" : "FAILED";
  printf("Test %d %s - Arch: %9.2f µs, Host: %9.2f µs\n", test_id, success.c_str(), std::get<1>(res), std::get<2>(res));
}

int64_t LocalIDForCoords(int x, int y, int z, int stencil, std::array<int,3> globalSize, std::array<int,3> storageSize) {
    int64_t index=0;
    if(globalSize[2] > 1) {
      index += storageSize[0]*storageSize[1]*(stencil+z);
    }
    if(globalSize[1] > 1) {
      index += storageSize[0]*(stencil+y);
    }
    if(globalSize[0] > 1 ) {
      index += stencil+x;
    }

    return index;
}
/* The test functions are all named as `test`, and only differentiated 
 * by their ascending template id number `I`. This allows executing tests
 * nicely using an array of function pointers, and not calling 
 * each test by a separate name. New tests can be added by inserting 
 * a new function named `test` with the `std::enable_if<I == value, ...` 
 * construct with the next unused value for `I`. 
 */
template<uint I>
typename std::enable_if<I == 0, std::tuple<bool, double, double>>::type test(){

  const std::array<int,3> gridDims = {100, 100, 100};
  uint gridDims3 = gridDims[0] * gridDims[1] * gridDims[2];

  MPI_Comm comm = MPI_COMM_WORLD;
  FsGridCouplingInformation gridCoupling;
  std::array<bool,3> periodicity{true, true, true};

  FsGrid< fsgrids::technical, FS_STENCIL_WIDTH> technicalGrid(gridDims, comm, periodicity,gridCoupling); 
  arch::buf<fsgrids::technical> dataBuffer(technicalGrid.get(0,0,0), gridDims3 * sizeof(fsgrids::technical));  

  std::array<int,3> storageSize = technicalGrid.getStorageSize();

  clock_t arch_start = clock();
  arch::parallel_for({(uint)gridDims[0], (uint)gridDims[1], (uint)gridDims[2] }, ARCH_LOOP_LAMBDA(int i, int j, int k) {
    int64_t id = LocalIDForCoords(i,j,k,FS_STENCIL_WIDTH,gridDims,storageSize);
    dataBuffer[id].maxFsDt=2;
  }); 
  double arch_time = (double)((clock() - arch_start) * 1e6 / CLOCKS_PER_SEC);

  clock_t host_start = clock();
  for (uint k = 0; k < gridDims[2]; ++k){
    for (uint j = 0; j < gridDims[1]; ++j){
      for (uint i = 0; i < gridDims[0]; ++i) {
        technicalGrid.get(i,j,k)->maxFsDt=2;
      }
    } 
  }
  double host_time = (double)((clock() - host_start) * 1e6 / CLOCKS_PER_SEC); 

  bool success = true;
  if (dataBuffer[10,10,10].maxFsDt != 2)
    success = false;
  else if (dataBuffer[10,10,10].maxFsDt != technicalGrid.get(10,10,10)->maxFsDt)
    success = false;

  return std::make_tuple(success, arch_time, host_time);
}

// This test function compares the performance of two 
// different implementations of a loop that sets the value of a field in a 3D grid.
template<uint I>
typename std::enable_if<I == 1, std::tuple<bool, double, double>>::type test(){

  // Define the size of the 3D grid
  const std::array<int,3> gridDims = {100, 100, 100};
  uint gridDims3 = gridDims[0] * gridDims[1] * gridDims[2];

  // Initialize MPI and grid coupling information
  MPI_Comm comm = MPI_COMM_WORLD;
  FsGridCouplingInformation gridCoupling;

  // Set the periodicity of the grid
  std::array<bool,3> periodicity{true, true, true};

  // Create the 3D grid with a field of type std::array<Real, fsgrids::bfield::N_BFIELD>
  FsGrid< std::array<Real, fsgrids::bfield::N_BFIELD>, FS_STENCIL_WIDTH> perBGrid(gridDims, comm, periodicity,gridCoupling); 

  // Create a buffer object that provides a convenient interface for accessing the grid data on the device
  arch::buf<FsGrid< std::array<Real, fsgrids::bfield::N_BFIELD>, FS_STENCIL_WIDTH>> perBGridBuf(&perBGrid);

  // Execute the loop in parallel on the device using CUDA
  clock_t arch_start = clock();
  arch::parallel_for({(uint)gridDims[0], (uint)gridDims[1], (uint)gridDims[2] }, ARCH_LOOP_LAMBDA(int i, int j, int k) {
    perBGridBuf.get(i,j,k)->at(fsgrids::bfield::PERBX) = 2;
  }); 
  double arch_time = (double)((clock() - arch_start) * 1e6 / CLOCKS_PER_SEC);

  // Execute the loop on the host
  clock_t host_start = clock();
  for (uint k = 0; k < gridDims[2]; ++k){
    for (uint j = 0; j < gridDims[1]; ++j){
      for (uint i = 0; i < gridDims[0]; ++i) {
        perBGrid.get(i,j,k)->at(fsgrids::bfield::PERBX) = 2;
      }
    } 
  }
  double host_time = (double)((clock() - host_start) * 1e6 / CLOCKS_PER_SEC); 

  // Check whether the test was successful
  bool success = true;
  if (perBGridBuf.get(10,10,10)->at(fsgrids::bfield::PERBX) != 2)
    success = false;
  else if (perBGridBuf.get(10,10,10)->at(fsgrids::bfield::PERBX) != perBGrid.get(10,10,10)->at(fsgrids::bfield::PERBX))
    success = false;

  // Return a tuple containing the test result and the execution times of the CUDA and host implementations
  return std::make_tuple(success, arch_time, host_time); 
}

/* Instantiate each test function by recursively calling the
 * driver function in a descending order beginning from `N - 1`
 */
template<uint N, uint I>
struct test_instatiator {
  static void driver(std::tuple<bool, double, double>(*fptr_test[N])()){
    fptr_test[I - 1] = &test<I - 1>;
    test_instatiator<N, I - 1>::driver(fptr_test);
  }
};

/* Specialization for the instantiation end condition `I = 0` */
template<uint N>
struct test_instatiator<N, 0> {
  static void driver(std::tuple<bool, double, double>(*fptr_test[N])()){}
};

/* The main function */
int main(int argn,char* args[]) {

  // init MPI, required for grids initialization
  int required=MPI_THREAD_FUNNELED;
  int provided;
  MPI_Init_thread(&argn,&args,required,&provided);
    
  /* Specify the number of tests and set function pointers */
  constexpr uint n_tests = 2;
  std::tuple<bool, double, double>(*fptr_test[n_tests])();
  test_instatiator<n_tests, n_tests>::driver(fptr_test);

  /* Indicate for what backend option the test suite is compiled */
  #ifdef USE_CUDA
    printf("Run tests for Arch = CUDA (USE_CUDA defined)\n");
  #else
    printf("Run tests for Arch = HOST (USE_CUDA not defined)\n");
  #endif

  
  /* Evaluate all test cases using the array of function pointers */
  for(uint i = 0; i < n_tests; i++) {
    printf("Running Test %d\n", i);
    fflush(stdout);
    result_eval(fptr_test[i](), i);
  }

  /* Finalize MPI */
  MPI_Finalize(); 

  return 0;
}
