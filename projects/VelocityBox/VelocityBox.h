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


#ifndef VELOCITYBOX_H
#define VELOCITYBOX_H

#include "../../definitions.h"
#include "../project.h"

namespace projects {
   class VelocityBox: public Project {
    public:
      VelocityBox();
      virtual ~VelocityBox();
      
      virtual bool initialize(void);
      static void addParameters(void);
      virtual void getParameters(void);
      virtual void setProjectBField(
         FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBGrid,
         FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH> & BgBGrid,
         FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH> & technicalGrid
      );
    protected:
      Real getDistribValue(creal& vx, creal& vy, creal& vz, const uint popID) const;
      virtual void calcCellParameters(spatial_cell::SpatialCell* cell,creal& t);
      virtual Real calcPhaseSpaceDensity(
                                         creal& x, creal& y, creal& z,
                                         creal& dx, creal& dy, creal& dz,
                                         creal& vx, creal& vy, creal& vz,
                                         creal& dvx, creal& dvy, creal& dvz,
                                         const uint popID
                                        ) const;
      
      Real rho;
      Real Vx[2];
      Real Vy[2];
      Real Vz[2];
      Real Bx;
      Real By;
      Real Bz;
   }; // class VelocityBox
} //  namespace projects



#endif

