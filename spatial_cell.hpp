/*
 * This file is part of Vlasiator.
 * Copyright 2010-2023 Finnish Meteorological Institute, University of Helsinki
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
/*!
  Spatial cell wrapper, maps to newer CUDA version or older CPU version which also has some
  stale VAMR mesh support.
*/

#ifdef USE_CUDA
#include "spatial_cell_cuda.hpp"
#else
#include "spatial_cell_old.hpp"
#endif
