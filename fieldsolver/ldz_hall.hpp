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

#ifndef LDZ_HALL_HPP
#define LDZ_HALL_HPP

#include "../definitions.h"

void calculateHallTermSimple(
   FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBGrid,
   FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBDt2Grid,
   FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH> & EHallGrid,
   FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH> & momentsGrid,
   FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH> & momentsDt2Grid,
   FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH> & dPerBGrid,
   FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH> & dMomentsGrid,
   FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH> & BgBGrid,
   FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH> & technicalGrid,
   SysBoundary& sysBoundaries,
   cint& RKCase,
   const bool communicateMomentsDerivatives
);

#endif
