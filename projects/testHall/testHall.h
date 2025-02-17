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

#ifndef TESTHALL_H
#define TESTHALL_H

#include "../../definitions.h"
#include "../project.h"

namespace projects {
   class TestHall: public Project {
      public:
         TestHall();
         virtual ~TestHall();
         
         virtual bool initialize(void);
         static void addParameters(void);
         virtual void getParameters(void);
         virtual void calcCellParameters(spatial_cell::SpatialCell* cell,creal& t);
         virtual void setProjectBField(
            FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBGrid,
            FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH> & BgBGrid,
            FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH> & technicalGrid
         );
         virtual Real calcPhaseSpaceDensity(
            creal& x, creal& y, creal& z,
            creal& dx, creal& dy, creal& dz,
            creal& vx, creal& vy, creal& vz,
            creal& dvx, creal& dvy, creal& dvz,const uint popID
         ) const;
         
      protected:
         Real getDistribValue(
            creal& x,creal& y, creal& z,
            creal& vx, creal& vy, creal& vz,
            creal& dvx, creal& dvy, creal& dvz
         );
         
         bool noDipoleInSW;
         Real constBgB[3];
         Real dipoleScalingFactor;
         Real dipoleTilt;
         Real BX0;
         Real BY0;
         Real BZ0;
         Real VX0;
         Real VY0;
         Real VZ0;
         Real TEMPERATURE;
         Real DENSITY;
   }; // class TestHall
} // namespace TestHall

#endif
