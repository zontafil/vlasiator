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

#ifndef SETBYUSER_H
#define SETBYUSER_H

#include <vector>
#include "../definitions.h"
#include "../readparameters.h"
#include "../spatial_cell.hpp"
#include "sysboundarycondition.h"
#include "setbyuserFieldBoundary.h"

namespace SBC {

   struct UserSpeciesParameters {
      /*! Vector containing a vector for each face which has the current boundary condition. Each of these vectors has one line per input data line (time point). The length of the lines is nParams.*/
      std::vector<std::vector<Real> > inputData[6];
      /*! Input files for the user-set boundary conditions. */
      std::string files[6];

      /*! Number of space- and velocityspace samples used when creating phase space densities */
      uint nSpaceSamples;
      uint nVelocitySamples;

      /*! Number of parameters per input file line. */
      uint nParams;
   };

   /*!\brief Base class for system boundary conditions with user-set settings and parameters read from file.
    * 
    * SetByUser is a base class for e.g. SysBoundaryConditon::SetMaxwellian.
    * It defines the managing functions to set boundary conditions on the faces of the
    * simulation domain.
    * 
    * This class handles the import and interpolation in time of the input parameters read
    * from file as well as the assignment of the state from the template cells.
    * 
    * The daughter classes have then to handle parameters and generate the template cells as
    * wished from the data returned.
    */
   class SetByUser: public SysBoundaryCondition {
   public:
      SetByUser();
      virtual ~SetByUser();
      
      virtual void getParameters() = 0;
      
      virtual bool initSysBoundary(
         creal& t,
         Project &project
      );
      bool initFieldBoundary();
      SetByUserFieldBoundary* getFieldBoundary() {return fieldBoundary;}

      virtual bool assignSysBoundary(dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                                     FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH> & technicalGrid);
      virtual bool applyInitialState(
         const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
         FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBGrid,
         Project &project
      );
      ARCH_HOSTDEV Real fieldSolverBoundaryCondMagneticField(
         const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & bGrid,
         const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
         cint i,
         cint j,
         cint k,
         creal& dt,
         cuint& component
      ) {
         return fieldBoundary->fieldSolverBoundaryCondMagneticField(bGrid, technicalGrid, i, j, k, dt, component);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondMagneticFieldProjection(
         const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & bGrid,
         const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
         cint i,
         cint j,
         cint k
      ) {
         fieldBoundary->fieldSolverBoundaryCondMagneticFieldProjection(bGrid, technicalGrid, i, j, k);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondElectricField(
         const arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      ) {
         fieldBoundary->fieldSolverBoundaryCondElectricField(EGrid, i, j, k, component);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondHallElectricField(
         const arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      ) {
         fieldBoundary->fieldSolverBoundaryCondHallElectricField(EHallGrid, i, j, k, component);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondGradPeElectricField(
         const arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
         cint i,
         cint j,
         cint k,
         cuint component
      ) {
         fieldBoundary->fieldSolverBoundaryCondGradPeElectricField(EGradPeGrid, i, j, k, component);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondDerivatives(
         const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
         const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
         cint i,
         cint j,
         cint k,
         cuint& RKCase,
         cuint& component
      ) {
         fieldBoundary->fieldSolverBoundaryCondDerivatives(dPerBGrid, dMomentsGrid, i, j, k, RKCase, component);
      }
      ARCH_HOSTDEV void fieldSolverBoundaryCondBVOLDerivatives(
         const arch::buf<FsGrid<Real, fsgrids::volfields::N_VOL, FS_STENCIL_WIDTH>> & volGrid,
         cint i,
         cint j,
         cint k,
         cuint& component
      ) {
         fieldBoundary->fieldSolverBoundaryCondBVOLDerivatives(volGrid, i, j, k, component);
      }
      virtual void vlasovBoundaryCondition(
         const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
         const CellID& cellID,
         const uint popID,
         const bool calculate_V_moments
      );
      
      virtual void getFaces(bool* faces);
      
      virtual std::string getName() const = 0;
      virtual uint getIndex() const = 0;
      
   protected:
      bool loadInputData(const uint popID);
      std::vector<std::vector<Real> > loadFile(const char* file, unsigned int nParams);
      void interpolate(const int inputDataIndex, const uint popID, creal t, Real* outputData);
      
      bool generateTemplateCells(creal& t);
      virtual void generateTemplateCell(spatial_cell::SpatialCell& templateCell, Real B[3], int inputDataIndex, creal& t) = 0;
      bool setCellsFromTemplate(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,const uint popID);
      bool setBFromTemplate(const dccrg::Dccrg<SpatialCell,dccrg::Cartesian_Geometry>& mpiGrid,
                            FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH> & perBGrid);
      
      /*! Array of bool telling which faces are going to be processed by the system boundary condition.*/
      bool facesToProcess[6];
      /*! Array of template spatial cells replicated over the corresponding simulation volume face. Only the template for an active face is actually being touched at all by the code. */
      spatial_cell::SpatialCell templateCells[6];
      Real templateB[6][3];
      /*! List of faces on which user-set boundary conditions are to be applied ([xyz][+-]). */
      std::vector<std::string> faceList;

      std::vector<UserSpeciesParameters> speciesParams;

      SetByUserFieldBoundary* fieldBoundary;
   };
}

#endif
