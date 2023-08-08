/*
 * This file is part of Vlasiator.
 * Copyright 2010-2016 Finnish Meteorological Institute
 *
 * For details of usage, see the COPYING file and read the "Rules of the Road"
 * at http://www.physics.helsinki.fi/vlasiator/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef GPU_MOMENTS_H
#define GPU_MOMENTS_H

#include <vector>
#include <limits>
#include <dccrg.hpp>
#include <dccrg_cartesian_geometry.hpp>

#include "../definitions.h"
#include "../common.h"
#include "../spatial_cell.hpp"

using namespace spatial_cell;

#define nMoments1 (5)
#define nMoments2 (3)
#define MaxPopulations (10)

struct MomentInfo
{
   vmesh::VelocityBlockContainer *blockContainer;
   Real mass;
   Real charge;
   uint blockCount;
};

void calculateMoments_V(
        dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
        const std::vector<CellID>& cells,
        const bool& computeSecond);

void calculateMoments_R(
        dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
        const std::vector<CellID>& cells,
        const bool& computeSecond);

extern void gpu_allocateMomentCalculations(
   const uint nPopulations,
   const uint maxThreads
   );

inline __host__ __device__ Real divideIfNonZeroHD(
   Real numerator,
   Real denominator
) {
   if(denominator == 0.0) {
      return 0.0;
   } else {
      return numerator / denominator;
   }
}

/* void calculate_firstMoments_glue( */
/*    MomentInfo *dev_momentInfos, */
/*    Real* dev_momentArrays1, */
/*    const int nPopulations, */
/*    gpuStream_t stream */
/*    ); */

/* void calculate_secondMoments_glue( */
/*    MomentInfo *dev_momentInfos, */
/*    Real* dev_momentArrays2, */
/*    const int nPopulations, */
/*    const Real bulkVX, */
/*    const Real bulkVY, */
/*    const Real bulkVZ, */
/*    gpuStream_t stream */
/*    ); */

extern MomentInfo *dev_momentInfos[];
extern MomentInfo *host_momentInfos[];

extern Real *dev_momentArrays1[];
extern Real *host_momentArrays1[];
extern Real *dev_momentArrays2[];
extern Real *host_momentArrays2[];

extern bool isGpuMomentsAllocated;

#endif
