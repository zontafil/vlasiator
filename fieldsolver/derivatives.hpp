/*
 * This file is part of Vlasiator.
 * Copyright 2010-2016 Finnish Meteorological Institute
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

#ifndef DERIVATIVES_HPP
#define DERIVATIVES_HPP

#include <vector>

#include "../definitions.h"
#include "../spatial_cell.hpp"
#include "../sysboundary/sysboundary.h"

#include "fs_limiters.h"

void calculateDerivativesSimple(
   arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBDt2Grid,
   arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsDt2Grid,
   arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   arch::buf<SysBoundary>& sysBoundaries,
   cint& RKCase,
   const bool communicateMoments);


void calculateBVOLDerivatives(
   const arch::buf<FsGrid<Real, fsgrids::volfields::N_VOL, FS_STENCIL_WIDTH>> & volGrid,
   const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   cint i,
   cint j,
   cint k,
   const arch::buf<SysBoundary>& sysBoundaries
);

void calculateBVOLDerivativesSimple(
   arch::buf<FsGrid<Real, fsgrids::volfields::N_VOL, FS_STENCIL_WIDTH>> & volGrid,
   arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   arch::buf<SysBoundary>& sysBoundaries
);


#endif
