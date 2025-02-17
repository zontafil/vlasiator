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

#include <cstdlib>

#include "fs_common.h"
#include "ldz_electric_field.hpp"

#ifndef NDEBUG
   #define DEBUG_FSOLVER
#endif

namespace pc = physicalconstants;
using namespace std;

#include "../arch/arch_sysboundary_api.h"

/*! \brief Low-level helper function.
 *
 * Computes the correct combination of speeds to determine the CFL limits.
 *
 * It should be in-plane, but we use the complete wave speeds from calculateWaveSpeed??().
 * 
 * At the moment it computes the geometric mean of both bulk velocity components
 * and takes the maximum of that plus either the magnetosonic or the whistler speed.
 * 
 * \sa calculateWaveSpeedYZ calculateWaveSpeedXY calculateWaveSpeedZX
 *
 * \param v0 Flow in first direction
 * \param v1 Flow in second direction
 * \param vA Alfven speed
 * \param vS Sound speed
 * \param vW Whistler speed
 */
ARCH_HOSTDEV Real calculateCflSpeed(
   const Real& v0,
   const Real& v1,
   const Real& vA,
   const Real& vS,
   const Real& vW
) {
   const Real v = sqrt(v0*v0 + v1*v1);
   const Real vMS = sqrt(vA*vA + vS*vS);
   return max(v + vMS, v + vW);
}

/*! \brief Low-level helper function.
 * 
 * Computes the magnetosonic speed in the YZ plane. Used in upwinding the electric field X component,
 * at the interface between cell (i,j,k) and (nbi,nbj,nbk).
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the
 * stage of the Runge-Kutta time stepping method.
 * 
 * If fields are not propagated, returns 0.0 as there is no information propagating.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities
 * \param momentsGrid fsGrid holding the moment quantities
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param i,j,k fsGrid cell coordinates for the current cell
 * \param nbi,nbj,nbk fsGrid cell coordinates for the adjacent cell
 * \param By Current cell's By
 * \param Bz Current cell's Bz
 * \param dBydx dBydx derivative
 * \param dBydz dBydz derivative
 * \param dBzdx dBzdx derivative
 * \param dBzdy dBzdy derivative
 * \param ydir +1 or -1 depending on the interpolation direction in y
 * \param zdir +1 or -1 depending on the interpolation direction in z
 * \param minRhom Minimum mass density allowed from the neighborhood
 * \param maxRhom Maximum mass density allowed from the neighborhood
 * \param ret_vA Alfven speed returned
 * \param ret_vS Sound speed returned
 * \param ret_vW Whistler speed returned
 */
ARCH_HOSTDEV void calculateWaveSpeedYZ(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   cint i,
   cint j,
   cint k,
   cint nbi,
   cint nbj,
   cint nbk,
   const Real& By,
   const Real& Bz,
   const Real& dBydx,
   const Real& dBydz,
   const Real& dBzdx,
   const Real& dBzdy,
   const Real& ydir,
   const Real& zdir,
   const Real& minRhom,
   const Real& maxRhom,
   Real& ret_vA,
   Real& ret_vS,
   Real& ret_vW
) {
   Real* perb = perBGrid.get(i,j,k);
   Real* nbr_perb = perBGrid.get(nbi,nbj,nbk);
   Real* moments = momentsGrid.get(i,j,k);
   Real* dmoments = dMomentsGrid.get(i,j,k);
   Real* dperb = dPerBGrid.get(i,j,k);
   Real* nbr_dperb = dPerBGrid.get(nbi,nbj,nbk);
   Real* bgb = BgBGrid.get(i,j,k);
   Real*  nbr_bgb = BgBGrid.get(nbi,nbj,nbk);
   
   Real A_0, A_X, rhom, p11, p22, p33;
   A_0  = HALF*(nbr_perb[fsgrids::bfield::PERBX] + nbr_bgb[fsgrids::bgbfield::BGBX] + perb[fsgrids::bfield::PERBX] + bgb[fsgrids::bgbfield::BGBX]);
   A_X  = (nbr_perb[fsgrids::bfield::PERBX] + nbr_bgb[fsgrids::bgbfield::BGBX]) - (perb[fsgrids::bfield::PERBX] + bgb[fsgrids::bgbfield::BGBX]);
   rhom = moments[fsgrids::moments::RHOM] + ydir*HALF*dmoments[fsgrids::dmoments::drhomdy] + zdir*HALF*dmoments[fsgrids::dmoments::drhomdz];
   p11 = moments[fsgrids::moments::P_11] + ydir*HALF*dmoments[fsgrids::dmoments::dp11dy] + zdir*HALF*dmoments[fsgrids::dmoments::dp11dz];
   p22 = moments[fsgrids::moments::P_22] + ydir*HALF*dmoments[fsgrids::dmoments::dp22dy] + zdir*HALF*dmoments[fsgrids::dmoments::dp22dz];
   p33 = moments[fsgrids::moments::P_33] + ydir*HALF*dmoments[fsgrids::dmoments::dp33dy] + zdir*HALF*dmoments[fsgrids::dmoments::dp33dz];
   
   if (rhom < minRhom) {
      rhom = minRhom;
   } else if (rhom > maxRhom) {
      rhom = maxRhom;
   }
   
   const Real A_Y  = nbr_dperb[fsgrids::dperb::dPERBxdy] + nbr_bgb[fsgrids::bgbfield::dBGBxdy] + dperb[fsgrids::dperb::dPERBxdy] + bgb[fsgrids::bgbfield::dBGBxdy];
   const Real A_XY = nbr_dperb[fsgrids::dperb::dPERBxdy] + nbr_bgb[fsgrids::bgbfield::dBGBxdy] - (dperb[fsgrids::dperb::dPERBxdy] + bgb[fsgrids::bgbfield::dBGBxdy]);
   const Real A_Z  = nbr_dperb[fsgrids::dperb::dPERBxdz] + nbr_bgb[fsgrids::bgbfield::dBGBxdz] + dperb[fsgrids::dperb::dPERBxdz] + bgb[fsgrids::bgbfield::dBGBxdz];
   const Real A_XZ = nbr_dperb[fsgrids::dperb::dPERBxdz] + nbr_bgb[fsgrids::bgbfield::dBGBxdz] - (dperb[fsgrids::dperb::dPERBxdz] + bgb[fsgrids::bgbfield::dBGBxdz]);

   const Real Bx2  = (A_0 + ydir*HALF*A_Y + zdir*HALF*A_Z)*(A_0 + ydir*HALF*A_Y + zdir*HALF*A_Z)
     + TWELWTH*(A_X + ydir*HALF*A_XY + zdir*HALF*A_XZ)*(A_X + ydir*HALF*A_XY + zdir*HALF*A_XZ); // OK
   const Real By2  = (By + zdir*HALF*dBydz)*(By + zdir*HALF*dBydz) + TWELWTH*dBydx*dBydx; // OK
   const Real Bz2  = (Bz + ydir*HALF*dBzdy)*(Bz + ydir*HALF*dBzdy) + TWELWTH*dBzdx*dBzdx; // OK
   
   const Real Bmag2 = Bx2 + By2 + Bz2;
   
   p11 = p11 < 0.0 ? 0.0 : p11;
   p22 = p22 < 0.0 ? 0.0 : p22;
   p33 = p33 < 0.0 ? 0.0 : p33;

   // Effective wave speeds for advection and CFL calculation
   // Note that these are calculated as if the plasma is purely made up of hydrogen, which
   // is a reasonable approximation if it is proton-dominant.
   // Simulations which predominantly contain heavier ion species will have to change this!
   //
   // See
   // T E Stringer, Low-frequency waves in an unbounded plasma
   // Journal of Nuclear Energy. Part C, Plasma Physics, Accelerators, Thermonuclear Research
   // Volume 5, Number 2, page 89
   // Section BC in Fig. 1 and Table 1, referenced later e.g. by Gary, for the current one,
   // and the below for the heavy ions
   // https://www.ann-geophys.net/26/1605/2008/  (Whistler waves)
   // and
   // http://iopscience.iop.org/article/10.1088/0253-6102/43/2/026/meta (Alfven waves)
   // for details.
   const Real vA2 = divideIfNonZero(Bmag2, pc::MU_0*rhom); // Alfven speed
   const Real vS2 = divideIfNonZero(p11+p22+p33, 2.0*rhom); // sound speed, adiabatic coefficient 3/2, P=1/3*trace in sound speed
//   const Real vW = meshParams.ohmHallTerm > 0 ? divideIfNonZero(2.0*M_PI*vA2*pc::MASS_PROTON, perBGrid.DX*pc::CHARGE*sqrt(Bmag2)) : 0.0; // whistler speed
   const Real vW = meshParams.ohmHallTerm > 0 ?
      sqrt(vA2) * (1 + divideIfNonZero(2*M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)
            / sqrt(1 + divideIfNonZero(  M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)))
      : 0.0; // whistler speed
   
   ret_vA = sqrt(vA2);
   ret_vS = sqrt(vS2);
   ret_vW = vW;
}

/*! \brief Low-level helper function.
 * 
 * Computes the magnetosonic speed in the XZ plane. Used in upwinding the electric field Y component,
 * at the interface between cell (i,j,k) and (nbi,nbj,nbk).
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the stage of the Runge-Kutta time stepping method.
 * 
 * If fields are not propagated, returns 0.0 as there is no information propagating.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities
 * \param momentsGrid fsGrid holding the moment quantities
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param i,j,k fsGrid cell coordinates for the current cell
 * \param nbi,nbj,nbk fsGrid cell coordinates for the adjacent cell
 * \param Bx Current cell's Bx
 * \param Bz Current cell's Bz
 * \param dBxdy dBxdy derivative
 * \param dBxdz dBxdz derivative
 * \param dBzdx dBzdx derivative
 * \param dBzdy dBzdy derivative
 * \param xdir +1 or -1 depending on the interpolation direction in x
 * \param zdir +1 or -1 depending on the interpolation direction in z
 * \param minRhom Minimum mass density allowed from the neighborhood
 * \param maxRhom Maximum mass density allowed from the neighborhood
 * \param ret_vA Alfven speed returned
 * \param ret_vS Sound speed returned
 * \param ret_vW Whistler speed returned
 */
ARCH_HOSTDEV void calculateWaveSpeedXZ(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   cint i,
   cint j,
   cint k,
   cint nbi,
   cint nbj,
   cint nbk,
   const Real& Bx,
   const Real& Bz,
   const Real& dBxdy,
   const Real& dBxdz,
   const Real& dBzdx,
   const Real& dBzdy,
   const Real& xdir,
   const Real& zdir,
   const Real& minRhom,
   const Real& maxRhom,
   Real& ret_vA,
   Real& ret_vS,
   Real& ret_vW
) {
   Real* perb = perBGrid.get(i,j,k);
   Real* nbr_perb = perBGrid.get(nbi,nbj,nbk);
   Real* moments = momentsGrid.get(i,j,k);
   Real* dmoments = dMomentsGrid.get(i,j,k);
   Real* dperb = dPerBGrid.get(i,j,k);
   Real* nbr_dperb = dPerBGrid.get(nbi,nbj,nbk);
   Real* bgb = BgBGrid.get(i,j,k);
   Real*  nbr_bgb = BgBGrid.get(nbi,nbj,nbk);
   
   Real B_0, B_Y, rhom, p11, p22, p33;
   B_0  = HALF*(nbr_perb[fsgrids::bfield::PERBY] + nbr_bgb[fsgrids::bgbfield::BGBY] + perb[fsgrids::bfield::PERBY] + bgb[fsgrids::bgbfield::BGBY]);
   B_Y  = (nbr_perb[fsgrids::bfield::PERBY] + nbr_bgb[fsgrids::bgbfield::BGBY]) - (perb[fsgrids::bfield::PERBY] + bgb[fsgrids::bgbfield::BGBY]);
   rhom = moments[fsgrids::moments::RHOM] + xdir*HALF*dmoments[fsgrids::dmoments::drhomdx] + zdir*HALF*dmoments[fsgrids::dmoments::drhomdz];
   p11 = moments[fsgrids::moments::P_11] + xdir*HALF*dmoments[fsgrids::dmoments::dp11dx] + zdir*HALF*dmoments[fsgrids::dmoments::dp11dz];
   p22 = moments[fsgrids::moments::P_22] + xdir*HALF*dmoments[fsgrids::dmoments::dp22dx] + zdir*HALF*dmoments[fsgrids::dmoments::dp22dz];
   p33 = moments[fsgrids::moments::P_33] + xdir*HALF*dmoments[fsgrids::dmoments::dp33dx] + zdir*HALF*dmoments[fsgrids::dmoments::dp33dz];
   
   if (rhom < minRhom) {
      rhom = minRhom;
   } else if (rhom > maxRhom) {
      rhom = maxRhom;
   }
   
   const Real B_X  = nbr_dperb[fsgrids::dperb::dPERBydx] + nbr_bgb[fsgrids::bgbfield::dBGBydx] + dperb[fsgrids::dperb::dPERBydx] + bgb[fsgrids::bgbfield::dBGBydx];
   const Real B_XY = nbr_dperb[fsgrids::dperb::dPERBydx] + nbr_bgb[fsgrids::bgbfield::dBGBydx] - (dperb[fsgrids::dperb::dPERBydx] + bgb[fsgrids::bgbfield::dBGBydx]);
   const Real B_Z  = nbr_dperb[fsgrids::dperb::dPERBydz] + nbr_bgb[fsgrids::bgbfield::dBGBydz] + dperb[fsgrids::dperb::dPERBydz] + bgb[fsgrids::bgbfield::dBGBydz];
   const Real B_YZ = nbr_dperb[fsgrids::dperb::dPERBydz] + nbr_bgb[fsgrids::bgbfield::dBGBydz] - (dperb[fsgrids::dperb::dPERBydz] + bgb[fsgrids::bgbfield::dBGBydz]);
      
   const Real By2  = (B_0 + xdir*HALF*B_X + zdir*HALF*B_Z)*(B_0 + xdir*HALF*B_X + zdir*HALF*B_Z)
     + TWELWTH*(B_Y + xdir*HALF*B_XY + zdir*HALF*B_YZ)*(B_Y + xdir*HALF*B_XY + zdir*HALF*B_YZ); // OK
   const Real Bx2  = (Bx + zdir*HALF*dBxdz)*(Bx + zdir*HALF*dBxdz) + TWELWTH*dBxdy*dBxdy; // OK
   const Real Bz2  = (Bz + xdir*HALF*dBzdx)*(Bz + xdir*HALF*dBzdx) + TWELWTH*dBzdy*dBzdy; // OK
   
   const Real Bmag2 = Bx2 + By2 + Bz2;
   
   p11 = p11 < 0.0 ? 0.0 : p11;
   p22 = p22 < 0.0 ? 0.0 : p22;
   p33 = p33 < 0.0 ? 0.0 : p33;
   
   // Effective wave speeds for advection and CFL calculation
   // Note that these are calculated as if the plasma is purely made up of hydrogen, which
   // is a reasonable approximation if it is proton-dominant.
   // Simulations which predominantly contain heavier ion species will have to change this!
   //
   // See
   // T E Stringer, Low-frequency waves in an unbounded plasma
   // Journal of Nuclear Energy. Part C, Plasma Physics, Accelerators, Thermonuclear Research
   // Volume 5, Number 2, page 89
   // Section BC in Fig. 1 and Table 1, referenced later e.g. by Gary, for the current one,
   // and the below for the heavy ions
   // https://www.ann-geophys.net/26/1605/2008/  (Whistler waves)
   // and
   // http://iopscience.iop.org/article/10.1088/0253-6102/43/2/026/meta (Alfven waves)
   // for details.
   const Real vA2 = divideIfNonZero(Bmag2, pc::MU_0*rhom); // Alfven speed
   const Real vS2 = divideIfNonZero(p11+p22+p33, 2.0*rhom); // sound speed, adiabatic coefficient 3/2, P=1/3*trace in sound speed
//   const Real vW = meshParams.ohmHallTerm > 0 ? divideIfNonZero(2.0*M_PI*vA2*pc::MASS_PROTON, perBGrid.DX*pc::CHARGE*sqrt(Bmag2)) : 0.0; // whistler speed
   const Real vW = meshParams.ohmHallTerm > 0 ?
      sqrt(vA2) * (1 + divideIfNonZero(2*M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)
            / sqrt(1 + divideIfNonZero(  M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)))
      : 0.0; // whistler speed
   
   ret_vA = sqrt(vA2);
   ret_vS = sqrt(vS2);
   ret_vW = vW;
}

/*! \brief Low-level helper function.
 * 
 * Computes the magnetosonic speed in the XY plane. Used in upwinding the electric field Z component,
 * at the interface between cell (i,j,k) and (nbi,nbj,nbk).
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the stage of the Runge-Kutta time stepping method.
 * 
 * If fields are not propagated, returns 0.0 as there is no information propagating.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities
 * \param momentsGrid fsGrid holding the moment quantities
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param i,j,k fsGrid cell coordinates for the current cell
 * \param nbi,nbj,nbk fsGrid cell coordinates for the adjacent cell
 * \param Bx Current cell's Bx
 * \param By Current cell's By
 * \param dBxdy dBxdy derivative
 * \param dBxdz dBxdz derivative
 * \param dBydx dBydx derivative
 * \param dBydz dBydz derivative
 * \param xdir +1 or -1 depending on the interpolation direction in x
 * \param ydir +1 or -1 depending on the interpolation direction in y
 * \param minRhom Minimum mass density allowed from the neighborhood
 * \param maxRhom Maximum mass density allowed from the neighborhood
 * \param ret_vA Alfven speed returned
 * \param ret_vS Sound speed returned
 * \param ret_vW Whistler speed returned
 */
ARCH_HOSTDEV void calculateWaveSpeedXY(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   cint i,
   cint j,
   cint k,
   cint nbi,
   cint nbj,
   cint nbk,
   const Real& Bx,
   const Real& By,
   const Real& dBxdy,
   const Real& dBxdz,
   const Real& dBydx,
   const Real& dBydz,
   const Real& xdir,
   const Real& ydir,
   const Real& minRhom,
   const Real& maxRhom,
   Real& ret_vA,
   Real& ret_vS,
   Real& ret_vW
) {
   Real* perb = perBGrid.get(i,j,k);
   Real* nbr_perb = perBGrid.get(nbi,nbj,nbk);
   Real* moments = momentsGrid.get(i,j,k);
   Real* dmoments = dMomentsGrid.get(i,j,k);
   Real* dperb = dPerBGrid.get(i,j,k);
   Real* nbr_dperb = dPerBGrid.get(nbi,nbj,nbk);
   Real* bgb = BgBGrid.get(i,j,k);
   Real*  nbr_bgb = BgBGrid.get(nbi,nbj,nbk);
   
   Real C_0, C_Z, rhom, p11, p22, p33;
   C_0  = HALF*(nbr_perb[fsgrids::bfield::PERBZ] + nbr_bgb[fsgrids::bgbfield::BGBZ] + perb[fsgrids::bfield::PERBZ] + bgb[fsgrids::bgbfield::BGBZ]);
   C_Z  = (nbr_perb[fsgrids::bfield::PERBZ] + nbr_bgb[fsgrids::bgbfield::BGBZ]) - (perb[fsgrids::bfield::PERBZ] + bgb[fsgrids::bgbfield::BGBZ]);
   rhom = moments[fsgrids::moments::RHOM] + xdir*HALF*dmoments[fsgrids::dmoments::drhomdx] + ydir*HALF*dmoments[fsgrids::dmoments::drhomdy];
   p11 = moments[fsgrids::moments::P_11] + xdir*HALF*dmoments[fsgrids::dmoments::dp11dx] + ydir*HALF*dmoments[fsgrids::dmoments::dp11dy];
   p22 = moments[fsgrids::moments::P_22] + xdir*HALF*dmoments[fsgrids::dmoments::dp22dx] + ydir*HALF*dmoments[fsgrids::dmoments::dp22dy];
   p33 = moments[fsgrids::moments::P_33] + xdir*HALF*dmoments[fsgrids::dmoments::dp33dx] + ydir*HALF*dmoments[fsgrids::dmoments::dp33dy];
   
   if (rhom < minRhom) {
      rhom = minRhom;
   } else if (rhom > maxRhom) {
      rhom = maxRhom;
   }
   
   const Real C_X  = nbr_dperb[fsgrids::dperb::dPERBzdx] + nbr_bgb[fsgrids::bgbfield::dBGBzdx] + dperb[fsgrids::dperb::dPERBzdx] + bgb[fsgrids::bgbfield::dBGBzdx];
   const Real C_XZ = nbr_dperb[fsgrids::dperb::dPERBzdx] + nbr_bgb[fsgrids::bgbfield::dBGBzdx] - (dperb[fsgrids::dperb::dPERBzdx] + bgb[fsgrids::bgbfield::dBGBzdx]);
   const Real C_Y  = nbr_dperb[fsgrids::dperb::dPERBzdy] + nbr_bgb[fsgrids::bgbfield::dBGBzdy] + dperb[fsgrids::dperb::dPERBzdy] + bgb[fsgrids::bgbfield::dBGBzdy];
   const Real C_YZ = nbr_dperb[fsgrids::dperb::dPERBzdy] + nbr_bgb[fsgrids::bgbfield::dBGBzdy] - (dperb[fsgrids::dperb::dPERBzdy] + bgb[fsgrids::bgbfield::dBGBzdy]);
   
   const Real Bz2  = (C_0 + xdir*HALF*C_X + ydir*HALF*C_Y)*(C_0 + xdir*HALF*C_X + ydir*HALF*C_Y)
     + TWELWTH*(C_Z + xdir*HALF*C_XZ + ydir*HALF*C_YZ)*(C_Z + xdir*HALF*C_XZ + ydir*HALF*C_YZ);
   const Real Bx2  = (Bx + ydir*HALF*dBxdy)*(Bx + ydir*HALF*dBxdy) + TWELWTH*dBxdz*dBxdz;
   const Real By2  = (By + xdir*HALF*dBydx)*(By + xdir*HALF*dBydx) + TWELWTH*dBydz*dBydz;
   
   const Real Bmag2 = Bx2 + By2 + Bz2;
   
   p11 = p11 < 0.0 ? 0.0 : p11;
   p22 = p22 < 0.0 ? 0.0 : p22;
   p33 = p33 < 0.0 ? 0.0 : p33;
      
   // Effective wave speeds for advection and CFL calculation
   // Note that these are calculated as if the plasma is purely made up of hydrogen, which
   // is a reasonable approximation if it is proton-dominant.
   // Simulations which predominantly contain heavier ion species will have to change this!
   //
   // See
   // T E Stringer, Low-frequency waves in an unbounded plasma
   // Journal of Nuclear Energy. Part C, Plasma Physics, Accelerators, Thermonuclear Research
   // Volume 5, Number 2, page 89
   // Section BC in Fig. 1 and Table 1, referenced later e.g. by Gary, for the current one,
   // and the below for the heavy ions
   // https://www.ann-geophys.net/26/1605/2008/  (Whistler waves)
   // and
   // http://iopscience.iop.org/article/10.1088/0253-6102/43/2/026/meta (Alfven waves)
   // for details.
   const Real vA2 = divideIfNonZero(Bmag2, pc::MU_0*rhom); // Alfven speed
   const Real vS2 = divideIfNonZero(p11+p22+p33, 2.0*rhom); // sound speed, adiabatic coefficient 3/2, P=1/3*trace in sound speed
//   const Real vW = meshParams.ohmHallTerm > 0 ? divideIfNonZero(2.0*M_PI*vA2*pc::MASS_PROTON, perBGrid.DX*pc::CHARGE*sqrt(Bmag2)) : 0.0; // whistler speed
   const Real vW = meshParams.ohmHallTerm > 0 ?
      sqrt(vA2) * (1 + divideIfNonZero(2*M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)
            / sqrt(1 + divideIfNonZero(  M_PI*M_PI*pc::MASS_PROTON*pc::MASS_PROTON, perBGrid.grid()->DX*perBGrid.grid()->DX*rhom*pc::CHARGE*pc::CHARGE*pc::MU_0)))
      : 0.0; // whistler speed
   
   ret_vA = sqrt(vA2);
   ret_vS = sqrt(vS2);
   ret_vW = vW;
}

/*! \brief Low-level electric field propagation function.
 * 
 * Computes the upwinded electric field X component along the cell's corresponding edge as the cross product of B and V in the YZ plane. Also includes the calculation of the maximally allowed time step.
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the stage of the Runge-Kutta time stepping method.
 * 
 * Note that the background B field is excluded from the diffusive term calculations because they are equivalent to a current term and the background field is curl-free.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities
 * \param EGrid fsGrid holding the electric field
 * \param EHallGrid fsGrid holding the Hall contributions to the electric field
 * \param EGradPeGrid fsGrid holding the electron pressure gradient E field
 * \param momentsGrid fsGrid holding the moment quantities
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param technicalGrid fsGrid holding technical information (such as boundary types)
 * \param i,j,k fsGrid cell coordinates for the current cell
 * \param RKCase Element in the enum defining the Runge-Kutta method steps
 */
ARCH_HOSTDEV void calculateEdgeElectricFieldX(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
   const arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
   const arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   cint i,
   cint j,
   cint k,
   cint& RKCase
) {
   #ifdef DEBUG_FSOLVER
   bool ok = true;
   if (technicalGrid.get(i,j,k) == NULL) ok = false;
   if (technicalGrid.get(i,j-1,k) == NULL) ok = false;
   if (technicalGrid.get(i,j-1,k-1) == NULL) ok = false;
   if (technicalGrid.get(i,j,k-1) == NULL) ok = false;
   if (ok == false) {
      cerr << "NULL pointer in " << __FILE__ << ":" << __LINE__ << std::endl;
      exit(1);
   }
   #endif

   // An edge has four neighbouring spatial cells. Calculate
   // electric field in each of the four cells per edge.
   Real ay_pos,ay_neg;              // Max. characteristic velocities to y-direction
   Real az_pos,az_neg;              // Max. characteristic velocities to z-direction
   Real Vy0,Vz0;                    // Reconstructed V
   Real vA, vS, vW;                 // Alfven, sound, whistler speed
   Real maxV = 0.0;                 // Max velocity for CFL purposes
   Real c_y, c_z;                   // Wave speeds to yz-directions

   // Get values at all four neighbours, result is written to SW.
   Real* perb_SW = perBGrid.get(i  ,j  ,k  );
   Real* perb_SE = perBGrid.get(i  ,j-1,k  );
   Real* perb_NE = perBGrid.get(i  ,j-1,k-1);
   Real* perb_NW = perBGrid.get(i  ,j  ,k-1);
   Real* bgb_SW = BgBGrid.get(i,j  ,k  );
   Real* bgb_SE = BgBGrid.get(i,j-1,k  );
   Real* bgb_NE = BgBGrid.get(i,j-1,k-1);
   Real* bgb_NW = BgBGrid.get(i,j  ,k-1);
   Real* moments_SW = momentsGrid.get(i  ,j  ,k  );
   Real* moments_SE = momentsGrid.get(i  ,j-1,k  );
   Real* moments_NE = momentsGrid.get(i  ,j-1,k-1);
   Real* moments_NW = momentsGrid.get(i  ,j  ,k-1);
   Real* dmoments_SW = dMomentsGrid.get(i  ,j  ,k  );
   Real* dmoments_SE = dMomentsGrid.get(i  ,j-1,k  );
   Real* dmoments_NE = dMomentsGrid.get(i  ,j-1,k-1);
   Real* dmoments_NW = dMomentsGrid.get(i  ,j  ,k-1);
   Real* dperb_SW = dPerBGrid.get(i  ,j  ,k  );
   Real* dperb_SE = dPerBGrid.get(i  ,j-1,k  );
   Real* dperb_NE = dPerBGrid.get(i  ,j-1,k-1);
   Real* dperb_NW = dPerBGrid.get(i  ,j  ,k-1);
   
   Real* efield_SW = EGrid.get(i,j,k);
   
   Real By_S, Bz_W, Bz_E, By_N, perBy_S, perBz_W, perBz_E, perBy_N;
   Real minRhom = std::numeric_limits<Real>::max();
   Real maxRhom = std::numeric_limits<Real>::min();

   By_S = perb_SW[fsgrids::bfield::PERBY]+bgb_SW[fsgrids::bgbfield::BGBY];
   Bz_W = perb_SW[fsgrids::bfield::PERBZ]+bgb_SW[fsgrids::bgbfield::BGBZ];
   Bz_E = perb_SE[fsgrids::bfield::PERBZ]+bgb_SE[fsgrids::bgbfield::BGBZ];
   By_N = perb_NW[fsgrids::bfield::PERBY]+bgb_NW[fsgrids::bgbfield::BGBY];
   perBy_S = perb_SW[fsgrids::bfield::PERBY];
   perBz_W = perb_SW[fsgrids::bfield::PERBZ];
   perBz_E = perb_SE[fsgrids::bfield::PERBZ];
   perBy_N = perb_NW[fsgrids::bfield::PERBY];
   Vy0  = moments_SW[fsgrids::moments::VY];
   Vz0  = moments_SW[fsgrids::moments::VZ];
   minRhom = min(minRhom,
       min(moments_SW[fsgrids::moments::RHOM],
         min(moments_SE[fsgrids::moments::RHOM],
           min(moments_NW[fsgrids::moments::RHOM],
             moments_NE[fsgrids::moments::RHOM])
           )
         )
       );
   maxRhom = max(maxRhom,
       max(moments_SW[fsgrids::moments::RHOM],
         max(moments_SE[fsgrids::moments::RHOM],
           max(moments_NW[fsgrids::moments::RHOM],
             moments_NE[fsgrids::moments::RHOM])
           )
         )
       );
   
   creal dBydx_S = dperb_SW[fsgrids::dperb::dPERBydx] + bgb_SW[fsgrids::bgbfield::dBGBydx];
   creal dBydz_S = dperb_SW[fsgrids::dperb::dPERBydz] + bgb_SW[fsgrids::bgbfield::dBGBydz];
   creal dBzdx_W = dperb_SW[fsgrids::dperb::dPERBzdx] + bgb_SW[fsgrids::bgbfield::dBGBzdx];
   creal dBzdy_W = dperb_SW[fsgrids::dperb::dPERBzdy] + bgb_SW[fsgrids::bgbfield::dBGBzdy];
   creal dBzdx_E = dperb_SE[fsgrids::dperb::dPERBzdx] + bgb_SE[fsgrids::bgbfield::dBGBzdx];
   creal dBzdy_E = dperb_SE[fsgrids::dperb::dPERBzdy] + bgb_SE[fsgrids::bgbfield::dBGBzdy];
   creal dBydx_N = dperb_NW[fsgrids::dperb::dPERBydx] + bgb_NW[fsgrids::bgbfield::dBGBydx];
   creal dBydz_N = dperb_NW[fsgrids::dperb::dPERBydz] + bgb_NW[fsgrids::bgbfield::dBGBydz];
   creal dperBydz_S = dperb_SW[fsgrids::dperb::dPERBydz];
   creal dperBydz_N = dperb_NW[fsgrids::dperb::dPERBydz];
   creal dperBzdy_W = dperb_SW[fsgrids::dperb::dPERBzdy];
   creal dperBzdy_E = dperb_SE[fsgrids::dperb::dPERBzdy];

   // Ex and characteristic speeds on this cell:
   // 1st order terms:
   Real Ex_SW = By_S*Vz0 - Bz_W*Vy0;

   // Resistive term
   if (meshParams.resistivity > 0) {
     Ex_SW += meshParams.resistivity *
       sqrt((bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX])*
            (bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX]) +
            (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY])*
            (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY]) +
            (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])*
            (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])
           ) /
       moments_SW[fsgrids::moments::RHOQ] /
       physicalconstants::MU_0 *
       (dperb_SW[fsgrids::dperb::dPERBzdy]/technicalGrid.grid()->DY - dperb_SW[fsgrids::dperb::dPERBydz]/technicalGrid.grid()->DZ);
   }
   
   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ex_SW += EHallGrid.get(i,j,k)[fsgrids::ehall::EXHALL_000_100];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ex_SW += EGradPeGrid.get(i,j,k)[fsgrids::egradpe::EXGRADPE];
   }

   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ex_SW += +HALF*((By_S - HALF*dBydz_S)*(-dmoments_SW[fsgrids::dmoments::dVzdy] - dmoments_SW[fsgrids::dmoments::dVzdz]) - dBydz_S*Vz0 + SIXTH*dBydx_S*dmoments_SW[fsgrids::dmoments::dVzdx]);
      Ex_SW += -HALF*((Bz_W - HALF*dBzdy_W)*(-dmoments_SW[fsgrids::dmoments::dVydy] - dmoments_SW[fsgrids::dmoments::dVydz]) - dBzdy_W*Vy0 + SIXTH*dBzdx_W*dmoments_SW[fsgrids::dmoments::dVydx]);
   #endif
   calculateWaveSpeedYZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i  , j, k,
      i+1, j, k,
      By_S, Bz_W, dBydx_S, dBydz_S, dBzdx_W, dBzdy_W, MINUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_y = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_z = c_y;
   ay_neg   = max(ZERO,-Vy0 + c_y);
   ay_pos   = max(ZERO,+Vy0 + c_y);
   az_neg   = max(ZERO,-Vz0 + c_z);
   az_pos   = max(ZERO,+Vz0 + c_z);
   maxV = max(maxV, calculateCflSpeed(Vy0, Vz0, vA, vS, vW));

   // Ex and characteristic speeds on j-1 neighbour:
   Vy0  = moments_SE[fsgrids::moments::VY];
   Vz0  = moments_SE[fsgrids::moments::VZ];
   
   // 1st order terms:
   Real Ex_SE = By_S*Vz0 - Bz_E*Vy0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
     Ex_SE += meshParams.resistivity *
       sqrt((bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX])*
            (bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX]) +
            (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY])*
            (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY]) +
            (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])*
            (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])
           ) /
       moments_SE[fsgrids::moments::RHOQ] /
       physicalconstants::MU_0 *
       (dperb_SE[fsgrids::dperb::dPERBzdy]/technicalGrid.grid()->DY - dperb_SE[fsgrids::dperb::dPERBydz]/technicalGrid.grid()->DZ);
   }

   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ex_SE += EHallGrid.get(i,j-1,k)[fsgrids::ehall::EXHALL_010_110];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ex_SE += EGradPeGrid.get(i,j-1,k)[fsgrids::egradpe::EXGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ex_SE += +HALF*((By_S - HALF*dBydz_S)*(+dmoments_SE[fsgrids::dmoments::dVzdy] - dmoments_SE[fsgrids::dmoments::dVzdz]) - dBydz_S*Vz0 + SIXTH*dBydx_S*dmoments_SE[fsgrids::dmoments::dVzdx]);
      Ex_SE += -HALF*((Bz_E + HALF*dBzdy_E)*(+dmoments_SE[fsgrids::dmoments::dVydy] - dmoments_SE[fsgrids::dmoments::dVydz]) + dBzdy_E*Vy0 + SIXTH*dBzdx_E*dmoments_SE[fsgrids::dmoments::dVydx]);
   #endif
   
   calculateWaveSpeedYZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i  , j-1, k,
      i+1, j-1, k,
      By_S, Bz_E, dBydx_S, dBydz_S, dBzdx_E, dBzdy_E, PLUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_y = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_z = c_y;
   ay_neg   = max(ay_neg,-Vy0 + c_y);
   ay_pos   = max(ay_pos,+Vy0 + c_y);
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   maxV = max(maxV, calculateCflSpeed(Vy0, Vz0, vA, vS, vW));

   // Ex and characteristic speeds on k-1 neighbour:
   Vy0  = moments_NW[fsgrids::moments::VY];
   Vz0  = moments_NW[fsgrids::moments::VZ];
   
   // 1st order terms:
   Real Ex_NW    = By_N*Vz0 - Bz_W*Vy0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
     Ex_NW += meshParams.resistivity *
       sqrt((bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX])*
            (bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX]) +
            (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY])*
            (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY]) +
            (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])*
            (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])
           ) /
       moments_NW[fsgrids::moments::RHOQ] /
       physicalconstants::MU_0 *
       (dperb_NW[fsgrids::dperb::dPERBzdy]/technicalGrid.grid()->DY - dperb_NW[fsgrids::dperb::dPERBydz]/technicalGrid.grid()->DZ);
   }
   
   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ex_NW += EHallGrid.get(i,j,k-1)[fsgrids::ehall::EXHALL_001_101];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ex_NW += EGradPeGrid.get(i,j,k-1)[fsgrids::egradpe::EXGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ex_NW += +HALF*((By_N + HALF*dBydz_N)*(-dmoments_NW[fsgrids::dmoments::dVzdy] + dmoments_NW[fsgrids::dmoments::dVzdz]) + dBydz_N*Vz0 + SIXTH*dBydx_N*dmoments_NW[fsgrids::dmoments::dVzdx]);
      Ex_NW += -HALF*((Bz_W - HALF*dBzdy_W)*(-dmoments_NW[fsgrids::dmoments::dVydy] + dmoments_NW[fsgrids::dmoments::dVydz]) - dBzdy_W*Vy0 + SIXTH*dBzdx_W*dmoments_NW[fsgrids::dmoments::dVydx]);
   #endif
   
   calculateWaveSpeedYZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i  , j, k-1,
      i+1, j, k-1,
      By_N, Bz_W, dBydx_N, dBydz_N, dBzdx_W, dBzdy_W, MINUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_y = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_z = c_y;
   ay_neg   = max(ay_neg,-Vy0 + c_y);
   ay_pos   = max(ay_pos,+Vy0 + c_y);
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   maxV = max(maxV, calculateCflSpeed(Vy0, Vz0, vA, vS, vW));

   // Ex and characteristic speeds on j-1,k-1 neighbour:
   Vy0 = moments_NE[fsgrids::moments::VY];
   Vz0 = moments_NE[fsgrids::moments::VZ];
   
   // 1st order terms:
   Real Ex_NE    = By_N*Vz0 - Bz_E*Vy0;

   // Resistive term
   if (meshParams.resistivity > 0) {
      Ex_NE += meshParams.resistivity *
               sqrt((bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX])*
                    (bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX]) +
                    (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY])*
                    (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY]) +
                    (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])*
                    (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])
                   ) /
               moments_NE[fsgrids::moments::RHOQ] /
               physicalconstants::MU_0 *
               (dperb_NE[fsgrids::dperb::dPERBzdy]/technicalGrid.grid()->DY - dperb_NE[fsgrids::dperb::dPERBydz]/technicalGrid.grid()->DZ);
   }

   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ex_NE += EHallGrid.get(i,j-1,k-1)[fsgrids::ehall::EXHALL_011_111];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ex_NE += EGradPeGrid.get(i,j-1,k-1)[fsgrids::egradpe::EXGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ex_NE += +HALF*((By_N + HALF*dBydz_N)*(+dmoments_NE[fsgrids::dmoments::dVzdy] + dmoments_NE[fsgrids::dmoments::dVzdz]) + dBydz_N*Vz0 + SIXTH*dBydx_N*dmoments_NE[fsgrids::dmoments::dVzdx]);
      Ex_NE += -HALF*((Bz_E + HALF*dBzdy_E)*(+dmoments_NE[fsgrids::dmoments::dVydy] + dmoments_NE[fsgrids::dmoments::dVydz]) + dBzdy_E*Vy0 + SIXTH*dBzdx_E*dmoments_NE[fsgrids::dmoments::dVydx]);
   #endif
   
   calculateWaveSpeedYZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i  ,j-1,k-1,
      i+1,j-1,k-1,
      By_N, Bz_E, dBydx_N, dBydz_N, dBzdx_E, dBzdy_E, PLUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_y = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_z = c_y;
   ay_neg   = max(ay_neg,-Vy0 + c_y);
   ay_pos   = max(ay_pos,+Vy0 + c_y);
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   maxV = max(maxV, calculateCflSpeed(Vy0, Vz0, vA, vS, vW));
   // Calculate properly upwinded edge-averaged Ex:
   efield_SW[fsgrids::efield::EX]  = ay_pos*az_pos*Ex_NE + ay_pos*az_neg*Ex_SE + ay_neg*az_pos*Ex_NW + ay_neg*az_neg*Ex_SW;
   efield_SW[fsgrids::efield::EX] /= ((ay_pos+ay_neg)*(az_pos+az_neg)+EPS);
   if (meshParams.fieldSolverDiffusiveEterms) {
#ifdef FS_1ST_ORDER_SPACE
      // 1st order diffusive terms:
      efield_SW[fsgrids::efield::EX] -= az_pos*az_neg/(az_pos+az_neg+EPS)*(perBy_S-perBy_N);
      efield_SW[fsgrids::efield::EX] += ay_pos*ay_neg/(ay_pos+ay_neg+EPS)*(perBz_W-perBz_E);
#else
      // 2nd     order diffusive terms
      efield_SW[fsgrids::efield::EX] -= az_pos*az_neg/(az_pos+az_neg+EPS)*((perBy_S-HALF*dperBydz_S) - (perBy_N+HALF*dperBydz_N));
      efield_SW[fsgrids::efield::EX] += ay_pos*ay_neg/(ay_pos+ay_neg+EPS)*((perBz_W-HALF*dperBzdy_W) - (perBz_E+HALF*dperBzdy_E));
#endif
   }
   
   if ((RKCase == RK_ORDER1) || (RKCase == RK_ORDER2_STEP2)) {
      //compute maximum timestep for fieldsolver in this cell (CFL=1)
      Real min_dx=std::numeric_limits<Real>::max();
      min_dx=min(min_dx,technicalGrid.grid()->DY);
      min_dx=min(min_dx,technicalGrid.grid()->DZ);
      //update max allowed timestep for field propagation in this cell, which is the minimum of CFL=1 timesteps
      if (maxV != ZERO) technicalGrid.get(i,j,k)->maxFsDt = min(technicalGrid.get(i,j,k)->maxFsDt,min_dx/maxV);
   }
}

/*! \brief Low-level electric field propagation function.
 * 
 * Computes the upwinded electric field Y component along the cell's corresponding edge as the cross product of B and V in the XZ plane. Also includes the calculation of the maximally allowed time step.
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the stage of the Runge-Kutta time stepping method.
 * 
 * Note that the background B field is excluded from the diffusive term calculations because they are equivalent to a current term and the background field is curl-free.
 * 
 * \param RKCase Element in the enum defining the Runge-Kutta method steps
 */
ARCH_HOSTDEV void calculateEdgeElectricFieldY(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
   const arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
   const arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   cint i,
   cint j,
   cint k,
   cint& RKCase
) {
   #ifdef DEBUG_FSOLVER
   bool ok = true;
   if (technicalGrid.get(i,j,k) == NULL) ok = false;
   if (technicalGrid.get(i,j,k-1) == NULL) ok = false;
   if (technicalGrid.get(i-1,j,k) == NULL) ok = false;
   if (technicalGrid.get(i-1,j,k-1) == NULL) ok = false;
   if (ok == false) {
      cerr << "NULL pointer in " << __FILE__ << ":" << __LINE__ << std::endl;
      exit(1);
   }
   #endif
   
   // An edge has four neighbouring spatial cells. Calculate
   // electric field in each of the four cells per edge.
   Real ax_pos,ax_neg;              // Max. characteristic velocities to x-direction
   Real az_pos,az_neg;              // Max. characteristic velocities to z-direction
   Real Vx0,Vz0;                    // Reconstructed V
   Real vA, vS, vW;                 // Alfven, sound, whistler speed
   Real maxV = 0.0;                 // Max velocity for CFL purposes
   Real c_x,c_z;                    // Wave speeds to xz-directions
   
   Real* perb_SW = perBGrid.get(i  ,j  ,k  );
   Real* perb_SE = perBGrid.get(i  ,j  ,k-1);
   Real* perb_NW = perBGrid.get(i-1,j  ,k  );
   Real* perb_NE = perBGrid.get(i-1,j  ,k-1);
   Real* bgb_SW = BgBGrid.get(i  ,j  ,k  );
   Real* bgb_SE = BgBGrid.get(i  ,j  ,k-1);
   Real* bgb_NW = BgBGrid.get(i-1,j  ,k  );
   Real* bgb_NE = BgBGrid.get(i-1,j  ,k-1);
   Real* moments_SW = momentsGrid.get(i  ,j  ,k  );
   Real* moments_SE = momentsGrid.get(i  ,j  ,k-1);
   Real* moments_NW = momentsGrid.get(i-1,j  ,k  );
   Real* moments_NE = momentsGrid.get(i-1,j  ,k-1);
   Real* dmoments_SW = dMomentsGrid.get(i  ,j  ,k  );
   Real* dmoments_SE = dMomentsGrid.get(i  ,j  ,k-1);
   Real* dmoments_NW = dMomentsGrid.get(i-1,j  ,k  );
   Real* dmoments_NE = dMomentsGrid.get(i-1,j  ,k-1);
   Real* dperb_SW = dPerBGrid.get(i  ,j  ,k  );
   Real* dperb_SE = dPerBGrid.get(i  ,j  ,k-1);
   Real* dperb_NW = dPerBGrid.get(i-1,j  ,k  );
   Real* dperb_NE = dPerBGrid.get(i-1,j  ,k-1);
   
   Real* efield_SW = EGrid.get(i,j,k);
   
   // Fetch required plasma parameters:
   Real Bz_S, Bx_W, Bx_E, Bz_N, perBz_S, perBx_W, perBx_E, perBz_N;
   Real minRhom = std::numeric_limits<Real>::max();
   Real maxRhom = std::numeric_limits<Real>::min();
   Bz_S = perb_SW[fsgrids::bfield::PERBZ]+bgb_SW[fsgrids::bgbfield::BGBZ];
   Bx_W = perb_SW[fsgrids::bfield::PERBX]+bgb_SW[fsgrids::bgbfield::BGBX];
   Bx_E = perb_SE[fsgrids::bfield::PERBX]+bgb_SE[fsgrids::bgbfield::BGBX];
   Bz_N = perb_NW[fsgrids::bfield::PERBZ]+bgb_NW[fsgrids::bgbfield::BGBZ];
   perBz_S = perb_SW[fsgrids::bfield::PERBZ];
   perBx_W = perb_SW[fsgrids::bfield::PERBX];
   perBx_E = perb_SE[fsgrids::bfield::PERBX];
   perBz_N = perb_NW[fsgrids::bfield::PERBZ];
   Vx0  = moments_SW[fsgrids::moments::VX];
   Vz0  = moments_SW[fsgrids::moments::VZ];
   minRhom = min(minRhom,
       min(moments_SW[fsgrids::moments::RHOM],
         min(moments_SE[fsgrids::moments::RHOM],
           min(moments_NW[fsgrids::moments::RHOM],
             moments_NE[fsgrids::moments::RHOM])
           )
         )
       );
   maxRhom = max(maxRhom,
       max(moments_SW[fsgrids::moments::RHOM],
         max(moments_SE[fsgrids::moments::RHOM],
           max(moments_NW[fsgrids::moments::RHOM],
             moments_NE[fsgrids::moments::RHOM])
           )
         )
       );
   
   creal dBxdy_W = dperb_SW[fsgrids::dperb::dPERBxdy] + bgb_SW[fsgrids::bgbfield::dBGBxdy];
   creal dBxdz_W = dperb_SW[fsgrids::dperb::dPERBxdz] + bgb_SW[fsgrids::bgbfield::dBGBxdz];
   creal dBzdx_S = dperb_SW[fsgrids::dperb::dPERBzdx] + bgb_SW[fsgrids::bgbfield::dBGBzdx];
   creal dBzdy_S = dperb_SW[fsgrids::dperb::dPERBzdy] + bgb_SW[fsgrids::bgbfield::dBGBzdy];
   creal dBxdy_E = dperb_SE[fsgrids::dperb::dPERBxdy] + bgb_SE[fsgrids::bgbfield::dBGBxdy];
   creal dBxdz_E = dperb_SE[fsgrids::dperb::dPERBxdz] + bgb_SE[fsgrids::bgbfield::dBGBxdz];
   creal dBzdx_N = dperb_NW[fsgrids::dperb::dPERBzdx] + bgb_NW[fsgrids::bgbfield::dBGBzdx];
   creal dBzdy_N = dperb_NW[fsgrids::dperb::dPERBzdy] + bgb_NW[fsgrids::bgbfield::dBGBzdy];
   creal dperBzdx_S = dperb_SW[fsgrids::dperb::dPERBzdx];
   creal dperBzdx_N = dperb_NW[fsgrids::dperb::dPERBzdx];
   creal dperBxdz_W = dperb_SW[fsgrids::dperb::dPERBxdz];
   creal dperBxdz_E = dperb_SE[fsgrids::dperb::dPERBxdz];
   
   // Ey and characteristic speeds on this cell:
   // 1st order terms:
   Real Ey_SW  = Bz_S*Vx0 - Bx_W*Vz0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
      Ey_SW += meshParams.resistivity *
        sqrt((bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX])*
             (bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX]) +
             (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY])*
             (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY]) +
             (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])*
             (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])
            ) /
        moments_SW[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_SW[fsgrids::dperb::dPERBxdz]/technicalGrid.grid()->DZ - dperb_SW[fsgrids::dperb::dPERBzdx]/technicalGrid.grid()->DX);
   }

   // Hall term
   if (meshParams.ohmHallTerm > 0) {
      Ey_SW += EHallGrid.get(i,j,k)[fsgrids::ehall::EYHALL_000_010];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ey_SW += EGradPeGrid.get(i,j,k)[fsgrids::egradpe::EYGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms
      Ey_SW += +HALF*((Bz_S - HALF*dBzdx_S)*(-dmoments_SW[fsgrids::dmoments::dVxdx] - dmoments_SW[fsgrids::dmoments::dVxdz]) - dBzdx_S*Vx0 + SIXTH*dBzdy_S*dmoments_SW[fsgrids::dmoments::dVxdy]);
      Ey_SW += -HALF*((Bx_W - HALF*dBxdz_W)*(-dmoments_SW[fsgrids::dmoments::dVzdx] - dmoments_SW[fsgrids::dmoments::dVzdz]) - dBxdz_W*Vz0 + SIXTH*dBxdy_W*dmoments_SW[fsgrids::dmoments::dVzdy]);
   #endif
   
   calculateWaveSpeedXZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i, j, k,
      i, j+1, k,
      Bx_W, Bz_S, dBxdy_W, dBxdz_W, dBzdx_S, dBzdy_S, MINUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_z = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_x = c_z;
   az_neg   = max(ZERO,-Vz0 + c_z);
   az_pos   = max(ZERO,+Vz0 + c_z);
   ax_neg   = max(ZERO,-Vx0 + c_x);
   ax_pos   = max(ZERO,+Vx0 + c_x);
   maxV = max(maxV, calculateCflSpeed(Vz0, Vx0, vA, vS, vW));

   // Ey and characteristic speeds on k-1 neighbour:
   Vx0  = moments_SE[fsgrids::moments::VX];
   Vz0  = moments_SE[fsgrids::moments::VZ];

   // 1st order terms:
   Real Ey_SE    = Bz_S*Vx0 - Bx_E*Vz0;

   // Resistive term
   if (meshParams.resistivity > 0) {
      Ey_SE += meshParams.resistivity *
        sqrt((bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX])*
             (bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX]) +
             (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY])*
             (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY]) +
             (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])*
             (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])
            ) /
        moments_SE[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_SE[fsgrids::dperb::dPERBxdz]/technicalGrid.grid()->DZ - dperb_SE[fsgrids::dperb::dPERBzdx]/technicalGrid.grid()->DX);
   }

   // Hall term
   if (meshParams.ohmHallTerm > 0) {
      Ey_SE += EHallGrid.get(i,j,k-1)[fsgrids::ehall::EYHALL_001_011];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ey_SE += EGradPeGrid.get(i,j,k-1)[fsgrids::egradpe::EYGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ey_SE += +HALF*((Bz_S - HALF*dBzdx_S)*(-dmoments_SE[fsgrids::dmoments::dVxdx] + dmoments_SE[fsgrids::dmoments::dVxdz]) - dBzdx_S*Vx0 + SIXTH*dBzdy_S*dmoments_SE[fsgrids::dmoments::dVxdy]);
      Ey_SE += -HALF*((Bx_E + HALF*dBxdz_E)*(-dmoments_SE[fsgrids::dmoments::dVzdx] + dmoments_SE[fsgrids::dmoments::dVzdz]) + dBxdz_E*Vz0 + SIXTH*dBxdy_E*dmoments_SE[fsgrids::dmoments::dVzdy]);
   #endif
   
   calculateWaveSpeedXZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i, j  , k-1,
      i, j+1, k-1,
      Bx_E, Bz_S, dBxdy_E, dBxdz_E, dBzdx_S, dBzdy_S, MINUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_z = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_x = c_z;
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   ax_neg   = max(ax_neg,-Vx0 + c_x);
   ax_pos   = max(ax_pos,+Vx0 + c_x);
   maxV = max(maxV, calculateCflSpeed(Vz0, Vx0, vA, vS, vW));
   
   // Ey and characteristic speeds on i-1 neighbour:
   Vz0  = moments_NW[fsgrids::moments::VZ];
   Vx0  = moments_NW[fsgrids::moments::VX];
   
   // 1st order terms:
   Real Ey_NW    = Bz_N*Vx0 - Bx_W*Vz0;

   // Resistive term
   if (meshParams.resistivity > 0) {
      Ey_NW += meshParams.resistivity *
        sqrt((bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX])*
             (bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX]) +
             (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY])*
             (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY]) +
             (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])*
             (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])
            ) /
        moments_NW[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_NW[fsgrids::dperb::dPERBxdz]/technicalGrid.grid()->DZ - dperb_NW[fsgrids::dperb::dPERBzdx]/technicalGrid.grid()->DX);
   }

   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ey_NW += EHallGrid.get(i-1,j,k)[fsgrids::ehall::EYHALL_100_110];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ey_NW += EGradPeGrid.get(i-1,j,k)[fsgrids::egradpe::EYGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ey_NW += +HALF*((Bz_N + HALF*dBzdx_N)*(+dmoments_NW[fsgrids::dmoments::dVxdx] - dmoments_NW[fsgrids::dmoments::dVxdz]) + dBzdx_N*Vx0 + SIXTH*dBzdy_N*dmoments_NW[fsgrids::dmoments::dVxdy]);
      Ey_NW += -HALF*((Bx_W - HALF*dBxdz_W)*(+dmoments_NW[fsgrids::dmoments::dVzdx] - dmoments_NW[fsgrids::dmoments::dVzdz]) - dBxdz_W*Vz0 + SIXTH*dBxdy_W*dmoments_NW[fsgrids::dmoments::dVzdy]);
   #endif
   
   calculateWaveSpeedXZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i-1,j  ,k,
      i-1,j+1,k,
      Bx_W, Bz_N, dBxdy_W, dBxdz_W, dBzdx_N, dBzdy_N, PLUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_z = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_x = c_z;
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   ax_neg   = max(ax_neg,-Vx0 + c_x);
   ax_pos   = max(ax_pos,+Vx0 + c_x);
   maxV = max(maxV, calculateCflSpeed(Vz0, Vx0, vA, vS, vW));

   // Ey and characteristic speeds on i-1,k-1 neighbour:
   Vz0 = moments_NE[fsgrids::moments::VZ];
   Vx0 = moments_NE[fsgrids::moments::VX];
   
   // 1st order terms:
   Real Ey_NE    = Bz_N*Vx0 - Bx_E*Vz0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
      Ey_NE += meshParams.resistivity *
        sqrt((bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX])*
             (bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX]) +
             (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY])*
             (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY]) +
             (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])*
             (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])
            ) /
        moments_NE[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_NE[fsgrids::dperb::dPERBxdz]/technicalGrid.grid()->DZ - dperb_NE[fsgrids::dperb::dPERBzdx]/technicalGrid.grid()->DX);
   }

   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ey_NE += EHallGrid.get(i-1,j,k-1)[fsgrids::ehall::EYHALL_101_111];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ey_NE += EGradPeGrid.get(i-1,j,k-1)[fsgrids::egradpe::EYGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ey_NE += +HALF*((Bz_N + HALF*dBzdx_N)*(+dmoments_NE[fsgrids::dmoments::dVxdx] + dmoments_NE[fsgrids::dmoments::dVxdz]) + dBzdx_N*Vx0 + SIXTH*dBzdy_N*dmoments_NE[fsgrids::dmoments::dVxdy]);
      Ey_NE += -HALF*((Bx_E + HALF*dBxdz_E)*(+dmoments_NE[fsgrids::dmoments::dVzdx] + dmoments_NE[fsgrids::dmoments::dVzdz]) + dBxdz_E*Vz0 + SIXTH*dBxdy_E*dmoments_NE[fsgrids::dmoments::dVzdy]);
   #endif
   
   calculateWaveSpeedXZ(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i-1,j  ,k-1,
      i-1,j+1,k-1,
      Bx_E, Bz_N, dBxdy_E, dBxdz_E, dBzdx_N, dBzdy_N, PLUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_z = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_x = c_z;
   az_neg   = max(az_neg,-Vz0 + c_z);
   az_pos   = max(az_pos,+Vz0 + c_z);
   ax_neg   = max(ax_neg,-Vx0 + c_x);
   ax_pos   = max(ax_pos,+Vx0 + c_x);
   maxV = max(maxV, calculateCflSpeed(Vz0, Vx0, vA, vS, vW));
   // Calculate properly upwinded edge-averaged Ey:
   efield_SW[fsgrids::efield::EY]  = az_pos*ax_pos*Ey_NE + az_pos*ax_neg*Ey_SE + az_neg*ax_pos*Ey_NW + az_neg*ax_neg*Ey_SW;
   efield_SW[fsgrids::efield::EY] /= ((az_pos+az_neg)*(ax_pos+ax_neg)+EPS);

   if (meshParams.fieldSolverDiffusiveEterms) {
#ifdef FS_1ST_ORDER_SPACE
      efield_SW[fsgrids::efield::EY] -= ax_pos*ax_neg/(ax_pos+ax_neg+EPS)*(perBz_S-perBz_N);
      efield_SW[fsgrids::efield::EY] += az_pos*az_neg/(az_pos+az_neg+EPS)*(perBx_W-perBx_E);
#else
      efield_SW[fsgrids::efield::EY] -= ax_pos*ax_neg/(ax_pos+ax_neg+EPS)*((perBz_S-HALF*dperBzdx_S) - (perBz_N+HALF*dperBzdx_N));
      efield_SW[fsgrids::efield::EY] += az_pos*az_neg/(az_pos+az_neg+EPS)*((perBx_W-HALF*dperBxdz_W) - (perBx_E+HALF*dperBxdz_E));
#endif
   }
   
   if ((RKCase == RK_ORDER1) || (RKCase == RK_ORDER2_STEP2)) {
      //compute maximum timestep for fieldsolver in this cell (CFL=1)      
      Real min_dx=std::numeric_limits<Real>::max();;
      min_dx=min(min_dx,technicalGrid.grid()->DX);
      min_dx=min(min_dx,technicalGrid.grid()->DZ);
      //update max allowed timestep for field propagation in this cell, which is the minimum of CFL=1 timesteps
      if (maxV!=ZERO) technicalGrid.get(i,j,k)->maxFsDt=min(technicalGrid.get(i,j,k)->maxFsDt,min_dx/maxV);
   }
}

/*! \brief Low-level electric field propagation function.
 *
 * Computes the upwinded electric field Z component along the cell's corresponding edge as the cross product of B and V in the XY plane. Also includes the calculation of the maximally allowed time step.
 * 
 * Expects that the correct RHO and B fields are being passed, depending on the stage of the Runge-Kutta time stepping method.
 * 
 * Note that the background B field is excluded from the diffusive term calculations because they are equivalent to a current term and the background field is curl-free.
 * 
 * \param RKCase Element in the enum defining the Runge-Kutta method steps
 */
ARCH_HOSTDEV void calculateEdgeElectricFieldZ(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
   const arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
   const arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   cint i,
   cint j,
   cint k,
   cint& RKCase
) {
   #ifdef DEBUG_FSOLVER
   bool ok = true;
   if (technicalGrid.get(i,j,k) == NULL) ok = false;
   if (technicalGrid.get(i-1,j,k) == NULL) ok = false;
   if (technicalGrid.get(i-1,j-1,k) == NULL) ok = false;
   if (technicalGrid.get(i,j-1,k) == NULL) ok = false;
   if (ok == false) {
      cerr << "NULL pointer in " << __FILE__ << ":" << __LINE__ << std::endl;
      exit(1);
   }
   #endif

   // An edge has four neighbouring spatial cells. Calculate 
   // electric field in each of the four cells per edge.
   Real ax_pos,ax_neg;              // Max. characteristic velocities to x-direction
   Real ay_pos,ay_neg;              // Max. characteristic velocities to y-direction
   Real Vx0,Vy0;                    // Reconstructed V
   Real vA, vS, vW;                         // Alfven, sound, whistler speed
   Real maxV = 0.0;                 // Max velocity for CFL purposes
   Real c_x,c_y;                    // Characteristic speeds to xy-directions
   
   // Get read-only pointers to NE,NW,SE,SW states (SW is rw, result is written there):
   Real* perb_SW = perBGrid.get(i  ,j  ,k  );
   Real* perb_SE = perBGrid.get(i-1,j  ,k  );
   Real* perb_NE = perBGrid.get(i-1,j-1,k  );
   Real* perb_NW = perBGrid.get(i  ,j-1,k  );
   Real* bgb_SW = BgBGrid.get(i  ,j  ,k  );
   Real* bgb_SE = BgBGrid.get(i-1,j  ,k  );
   Real* bgb_NE = BgBGrid.get(i-1,j-1,k  );
   Real* bgb_NW = BgBGrid.get(i  ,j-1,k  );
   Real* moments_SW = momentsGrid.get(i  ,j  ,k  );
   Real* moments_SE = momentsGrid.get(i-1,j  ,k  );
   Real* moments_NE = momentsGrid.get(i-1,j-1,k  );
   Real* moments_NW = momentsGrid.get(i  ,j-1,k  );
   Real* dmoments_SW = dMomentsGrid.get(i  ,j  ,k  );
   Real* dmoments_SE = dMomentsGrid.get(i-1,j  ,k  );
   Real* dmoments_NE = dMomentsGrid.get(i-1,j-1,k  );
   Real* dmoments_NW = dMomentsGrid.get(i  ,j-1,k  );
   Real* dperb_SW = dPerBGrid.get(i  ,j  ,k  );
   Real* dperb_SE = dPerBGrid.get(i-1,j  ,k  );
   Real* dperb_NE = dPerBGrid.get(i-1,j-1,k  );
   Real* dperb_NW = dPerBGrid.get(i  ,j-1,k  );
   
   Real* efield_SW = EGrid.get(i,j,k);
   
   // Fetch needed plasma parameters/derivatives from the four cells:
   Real Bx_S, By_W, By_E, Bx_N, perBx_S, perBy_W, perBy_E, perBx_N;
   Real minRhom = std::numeric_limits<Real>::max();
   Real maxRhom = std::numeric_limits<Real>::min();

   Bx_S    = perb_SW[fsgrids::bfield::PERBX] + bgb_SW[fsgrids::bgbfield::BGBX];
   By_W    = perb_SW[fsgrids::bfield::PERBY] + bgb_SW[fsgrids::bgbfield::BGBY];
   By_E    = perb_SE[fsgrids::bfield::PERBY] + bgb_SE[fsgrids::bgbfield::BGBY];
   Bx_N    = perb_NW[fsgrids::bfield::PERBX] + bgb_NW[fsgrids::bgbfield::BGBX];
   perBx_S    = perb_SW[fsgrids::bfield::PERBX];
   perBy_W    = perb_SW[fsgrids::bfield::PERBY];
   perBy_E    = perb_SE[fsgrids::bfield::PERBY];
   perBx_N    = perb_NW[fsgrids::bfield::PERBX];
   Vx0  = moments_SW[fsgrids::moments::VX];
   Vy0  = moments_SW[fsgrids::moments::VY];
   minRhom = min(minRhom,
         min(moments_SW[fsgrids::moments::RHOM],
            min(moments_SE[fsgrids::moments::RHOM],
               min(moments_NW[fsgrids::moments::RHOM],
                  moments_NE[fsgrids::moments::RHOM])
               )
            )
         );
   maxRhom = max(maxRhom,
         max(moments_SW[fsgrids::moments::RHOM],
            max(moments_SE[fsgrids::moments::RHOM],
               max(moments_NW[fsgrids::moments::RHOM],
                  moments_NE[fsgrids::moments::RHOM])
               )
            )
         );
   
   creal dBxdy_S = dperb_SW[fsgrids::dperb::dPERBxdy] + bgb_SW[fsgrids::bgbfield::dBGBxdy];
   creal dBxdz_S = dperb_SW[fsgrids::dperb::dPERBxdz] + bgb_SW[fsgrids::bgbfield::dBGBxdz];
   creal dBydx_W = dperb_SW[fsgrids::dperb::dPERBydx] + bgb_SW[fsgrids::bgbfield::dBGBydx];
   creal dBydz_W = dperb_SW[fsgrids::dperb::dPERBydz] + bgb_SW[fsgrids::bgbfield::dBGBydz];
   creal dBydx_E = dperb_SE[fsgrids::dperb::dPERBydx] + bgb_SE[fsgrids::bgbfield::dBGBydx];
   creal dBydz_E = dperb_SE[fsgrids::dperb::dPERBydz] + bgb_SE[fsgrids::bgbfield::dBGBydz];
   creal dBxdy_N = dperb_NW[fsgrids::dperb::dPERBxdy] + bgb_NW[fsgrids::bgbfield::dBGBxdy];
   creal dBxdz_N = dperb_NW[fsgrids::dperb::dPERBxdz] + bgb_NW[fsgrids::bgbfield::dBGBxdz];
   creal dperBxdy_S = dperb_SW[fsgrids::dperb::dPERBxdy];
   creal dperBxdy_N = dperb_NW[fsgrids::dperb::dPERBxdy];
   creal dperBydx_W = dperb_SW[fsgrids::dperb::dPERBydx];
   creal dperBydx_E = dperb_SE[fsgrids::dperb::dPERBydx];
   
   // Ez and characteristic speeds on SW cell:
   // 1st order terms:
   Real Ez_SW = Bx_S*Vy0 - By_W*Vx0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
     Ez_SW += meshParams.resistivity *
       sqrt((bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX])*
            (bgb_SW[fsgrids::bgbfield::BGBX]+perb_SW[fsgrids::bfield::PERBX]) +
            (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY])*
            (bgb_SW[fsgrids::bgbfield::BGBY]+perb_SW[fsgrids::bfield::PERBY]) +
            (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])*
            (bgb_SW[fsgrids::bgbfield::BGBZ]+perb_SW[fsgrids::bfield::PERBZ])
           ) /
       moments_SW[fsgrids::moments::RHOQ] /
       physicalconstants::MU_0 *
       (dperb_SW[fsgrids::dperb::dPERBydx]/technicalGrid.grid()->DX - dperb_SW[fsgrids::dperb::dPERBxdy]/technicalGrid.grid()->DY);
   }
   
   // Hall term
   if (meshParams.ohmHallTerm > 0) {
      Ez_SW += EHallGrid.get(i,j,k)[fsgrids::ehall::EZHALL_000_001];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ez_SW += EGradPeGrid.get(i,j,k)[fsgrids::egradpe::EZGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ez_SW  += +HALF*((Bx_S - HALF*dBxdy_S)*(-dmoments_SW[fsgrids::dmoments::dVydx] - dmoments_SW[fsgrids::dmoments::dVydy]) - dBxdy_S*Vy0 + SIXTH*dBxdz_S*dmoments_SW[fsgrids::dmoments::dVydz]);
      Ez_SW  += -HALF*((By_W - HALF*dBydx_W)*(-dmoments_SW[fsgrids::dmoments::dVxdx] - dmoments_SW[fsgrids::dmoments::dVxdy]) - dBydx_W*Vx0 + SIXTH*dBydz_W*dmoments_SW[fsgrids::dmoments::dVxdz]);
   #endif
   
   // Calculate maximum wave speed (fast magnetosonic speed) on SW cell. In order 
   // to get Alfven speed we need to calculate some reconstruction coeff. for Bz:
   calculateWaveSpeedXY(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i, j, k,
      i, j, k+1,
      Bx_S, By_W, dBxdy_S, dBxdz_S, dBydx_W, dBydz_W, MINUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_x = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_y = c_x;
   ax_neg   = max(ZERO,-Vx0 + c_x);
   ax_pos   = max(ZERO,+Vx0 + c_x);
   ay_neg   = max(ZERO,-Vy0 + c_y);
   ay_pos   = max(ZERO,+Vy0 + c_y);
   maxV = max(maxV, calculateCflSpeed(Vx0, Vy0, vA, vS, vW));

   // Ez and characteristic speeds on SE (i-1) cell:
   Vx0  = moments_SE[fsgrids::moments::VX];
   Vy0  = moments_SE[fsgrids::moments::VY];
   
   // 1st order terms:
   Real Ez_SE = Bx_S*Vy0 - By_E*Vx0;

   // Resistive term
   if (meshParams.resistivity > 0) {
      Ez_SE += meshParams.resistivity *
        sqrt((bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX])*
             (bgb_SE[fsgrids::bgbfield::BGBX]+perb_SE[fsgrids::bfield::PERBX]) +
             (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY])*
             (bgb_SE[fsgrids::bgbfield::BGBY]+perb_SE[fsgrids::bfield::PERBY]) +
             (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])*
             (bgb_SE[fsgrids::bgbfield::BGBZ]+perb_SE[fsgrids::bfield::PERBZ])
            ) /
        moments_SE[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_SE[fsgrids::dperb::dPERBydx]/technicalGrid.grid()->DX - dperb_SE[fsgrids::dperb::dPERBxdy]/technicalGrid.grid()->DY);
   }
   
   // Hall term
   if (meshParams.ohmHallTerm > 0) {
      Ez_SE += EHallGrid.get(i-1,j,k)[fsgrids::ehall::EZHALL_100_101];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ez_SE += EGradPeGrid.get(i-1,j,k)[fsgrids::egradpe::EZGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ez_SE  += +HALF*((Bx_S - HALF*dBxdy_S)*(+dmoments_SE[fsgrids::dmoments::dVydx] - dmoments_SE[fsgrids::dmoments::dVydy]) - dBxdy_S*Vy0 + SIXTH*dBxdz_S*dmoments_SE[fsgrids::dmoments::dVydz]);
      Ez_SE  += -HALF*((By_E + HALF*dBydx_E)*(+dmoments_SE[fsgrids::dmoments::dVxdx] - dmoments_SE[fsgrids::dmoments::dVxdy]) + dBydx_E*Vx0 + SIXTH*dBydz_E*dmoments_SE[fsgrids::dmoments::dVxdz]);
   #endif
   
   calculateWaveSpeedXY(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i-1,j  ,k,
      i-1,j  ,k+1,
      Bx_S, By_E, dBxdy_S, dBxdz_S, dBydx_E, dBydz_E, PLUS, MINUS, minRhom, maxRhom, vA, vS, vW
   );
   c_x = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_y = c_x;
   ax_neg = max(ax_neg,-Vx0 + c_x);
   ax_pos = max(ax_pos,+Vx0 + c_x);
   ay_neg = max(ay_neg,-Vy0 + c_y);
   ay_pos = max(ay_pos,+Vy0 + c_y);
   maxV = max(maxV, calculateCflSpeed(Vx0, Vy0, vA, vS, vW));

   // Ez and characteristic speeds on NW (j-1) cell:
   Vx0  = moments_NW[fsgrids::moments::VX];
   Vy0  = moments_NW[fsgrids::moments::VY];
   
   // 1st order terms:
   Real Ez_NW = Bx_N*Vy0 - By_W*Vx0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
      Ez_NW += meshParams.resistivity *
        sqrt((bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX])*
             (bgb_NW[fsgrids::bgbfield::BGBX]+perb_NW[fsgrids::bfield::PERBX]) +
             (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY])*
             (bgb_NW[fsgrids::bgbfield::BGBY]+perb_NW[fsgrids::bfield::PERBY]) +
             (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])*
             (bgb_NW[fsgrids::bgbfield::BGBZ]+perb_NW[fsgrids::bfield::PERBZ])
            ) /
        moments_NW[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_NW[fsgrids::dperb::dPERBydx]/technicalGrid.grid()->DX - dperb_NW[fsgrids::dperb::dPERBxdy]/technicalGrid.grid()->DY);
   }
   
   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ez_NW += EHallGrid.get(i,j-1,k)[fsgrids::ehall::EZHALL_010_011];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ez_NW += EGradPeGrid.get(i,j-1,k)[fsgrids::egradpe::EZGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ez_NW  += +HALF*((Bx_N + HALF*dBxdy_N)*(-dmoments_NW[fsgrids::dmoments::dVydx] + dmoments_NW[fsgrids::dmoments::dVydy]) + dBxdy_N*Vy0 + SIXTH*dBxdz_N*dmoments_NW[fsgrids::dmoments::dVydz]);
      Ez_NW  += -HALF*((By_W - HALF*dBydx_W)*(-dmoments_NW[fsgrids::dmoments::dVxdx] + dmoments_NW[fsgrids::dmoments::dVxdy]) - dBydx_W*Vx0 + SIXTH*dBydz_W*dmoments_NW[fsgrids::dmoments::dVxdz]);
   #endif
   
   calculateWaveSpeedXY(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i, j-1, k,
      i, j-1, k+1,
      Bx_N, By_W, dBxdy_N, dBxdz_N, dBydx_W, dBydz_W, MINUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_x = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_y = c_x;
   ax_neg = max(ax_neg,-Vx0 + c_x); 
   ax_pos = max(ax_pos,+Vx0 + c_x);
   ay_neg = max(ay_neg,-Vy0 + c_y);
   ay_pos = max(ay_pos,+Vy0 + c_y);
   maxV = max(maxV, calculateCflSpeed(Vx0, Vy0, vA, vS, vW));
   
   // Ez and characteristic speeds on NE (i-1,j-1) cell:
   Vx0  = moments_NE[fsgrids::moments::VX];
   Vy0  = moments_NE[fsgrids::moments::VY];
   
   // 1st order terms:
   Real Ez_NE = Bx_N*Vy0 - By_E*Vx0;
   
   // Resistive term
   if (meshParams.resistivity > 0) {
      Ez_NE += meshParams.resistivity *
        sqrt((bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX])*
             (bgb_NE[fsgrids::bgbfield::BGBX]+perb_NE[fsgrids::bfield::PERBX]) +
             (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY])*
             (bgb_NE[fsgrids::bgbfield::BGBY]+perb_NE[fsgrids::bfield::PERBY]) +
             (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])*
             (bgb_NE[fsgrids::bgbfield::BGBZ]+perb_NE[fsgrids::bfield::PERBZ])
            ) /
        moments_NE[fsgrids::moments::RHOQ] /
        physicalconstants::MU_0 *
        (dperb_NE[fsgrids::dperb::dPERBydx]/technicalGrid.grid()->DX - dperb_NE[fsgrids::dperb::dPERBxdy]/technicalGrid.grid()->DY);
   }
   
   // Hall term
   if(meshParams.ohmHallTerm > 0) {
      Ez_NE += EHallGrid.get(i-1,j-1,k)[fsgrids::ehall::EZHALL_110_111];
   }
   
   // Electron pressure gradient term
   if(meshParams.ohmGradPeTerm > 0) {
      Ez_NE += EGradPeGrid.get(i-1,j-1,k)[fsgrids::egradpe::EZGRADPE];
   }
   
   #ifndef FS_1ST_ORDER_SPACE
      // 2nd order terms:
      Ez_NE  += +HALF*((Bx_N + HALF*dBxdy_N)*(+dmoments_NE[fsgrids::dmoments::dVydx] + dmoments_NE[fsgrids::dmoments::dVydy]) + dBxdy_N*Vy0 + SIXTH*dBxdz_N*dmoments_NE[fsgrids::dmoments::dVydz]);
      Ez_NE  += -HALF*((By_E + HALF*dBydx_E)*(+dmoments_NE[fsgrids::dmoments::dVxdx] + dmoments_NE[fsgrids::dmoments::dVxdy]) + dBydx_E*Vx0 + SIXTH*dBydz_E*dmoments_NE[fsgrids::dmoments::dVxdz]);
   #endif
   
   calculateWaveSpeedXY(
      perBGrid,
      momentsGrid,
      dPerBGrid,
      dMomentsGrid,
      BgBGrid,
      i-1,j-1,k,
      i-1,j-1,k+1,
      Bx_N, By_E, dBxdy_N, dBxdz_N, dBydx_E, dBydz_E, PLUS, PLUS, minRhom, maxRhom, vA, vS, vW
   );
   c_x = min(meshParams.maxWaveVelocity,sqrt(vA*vA + vS*vS) + vW);
   c_y = c_x;
   ax_neg = max(ax_neg,-Vx0 + c_x);
   ax_pos = max(ax_pos,+Vx0 + c_x);
   ay_neg = max(ay_neg,-Vy0 + c_y);
   ay_pos = max(ay_pos,+Vy0 + c_y);
   maxV = max(maxV, calculateCflSpeed(Vx0, Vy0, vA, vS, vW));

   // Calculate properly upwinded edge-averaged Ez:
   efield_SW[fsgrids::efield::EZ] = ax_pos*ay_pos*Ez_NE + ax_pos*ay_neg*Ez_SE + ax_neg*ay_pos*Ez_NW + ax_neg*ay_neg*Ez_SW;
   efield_SW[fsgrids::efield::EZ] /= ((ax_pos+ax_neg)*(ay_pos+ay_neg)+EPS);

   if (meshParams.fieldSolverDiffusiveEterms) {
#ifdef FS_1ST_ORDER_SPACE
      efield_SW[fsgrids::efield::EZ] -= ay_pos*ay_neg/(ay_pos+ay_neg+EPS)*(perBx_S-perBx_N);
      efield_SW[fsgrids::efield::EZ] += ax_pos*ax_neg/(ax_pos+ax_neg+EPS)*(perBy_W-perBy_E);
#else
      efield_SW[fsgrids::efield::EZ] -= ay_pos*ay_neg/(ay_pos+ay_neg+EPS)*((perBx_S-HALF*dperBxdy_S) - (perBx_N+HALF*dperBxdy_N));
      efield_SW[fsgrids::efield::EZ] += ax_pos*ax_neg/(ax_pos+ax_neg+EPS)*((perBy_W-HALF*dperBydx_W) - (perBy_E+HALF*dperBydx_E));
#endif
   }
   
   if ((RKCase == RK_ORDER1) || (RKCase == RK_ORDER2_STEP2)) {
      //compute maximum timestep for fieldsolver in this cell (CFL=1)
      Real min_dx=std::numeric_limits<Real>::max();;
      min_dx=min(min_dx,technicalGrid.grid()->DX);
      min_dx=min(min_dx,technicalGrid.grid()->DY);
      //update max allowed timestep for field propagation in this cell, which is the minimum of CFL=1 timesteps
      if(maxV!=ZERO) technicalGrid.get(i,j,k)->maxFsDt=min(technicalGrid.get(i,j,k)->maxFsDt,min_dx/maxV);
   }
}

/*! \brief Electric field propagation function.
 * 
 * Calls the general or the system boundary electric field propagation functions.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities
 * \param EGrid fsGrid holding the electric field
 * \param EHallGrid fsGrid holding the Hall contributions to the electric field
 * \param EGradPeGrid fsGrid holding the electron pressure gradient E field
 * \param momentsGrid fsGrid holding the moment quantities
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param technicalGrid fsGrid holding technical information (such as boundary types)
 * \param i,j,k fsGrid cell coordinates for the current cell
 * \param sysBoundaries System boundary conditions existing
 * \param RKCase Element in the enum defining the Runge-Kutta method steps
 * 
 * \sa calculateUpwindedElectricFieldSimple calculateEdgeElectricFieldX calculateEdgeElectricFieldY calculateEdgeElectricFieldZ
 * 
 */
ARCH_HOSTDEV void calculateElectricField(
   const arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   const arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
   const arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
   const arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
   const arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   const arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   const arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   const arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   cint i,
   cint j,
   cint k,
   const arch::buf<SysBoundary>& sysBoundaries,
   cint& RKCase
) {
   cuint cellSysBoundaryFlag = technicalGrid.get(i,j,k)->sysBoundaryFlag;
   
   if (cellSysBoundaryFlag == sysboundarytype::DO_NOT_COMPUTE) return;
   
   cuint bitfield = technicalGrid.get(i,j,k)->SOLVE;
   
   if ((bitfield & compute::EX) == compute::EX) {
      calculateEdgeElectricFieldX(
         perBGrid,
         EGrid,
         EHallGrid,
         EGradPeGrid,
         momentsGrid,
         dPerBGrid,
         dMomentsGrid,
         BgBGrid,
         technicalGrid,
         i,
         j,
         k,
         RKCase
      );
   } else {
      sysBoundaries.getSysBoundary(cellSysBoundaryFlag).fieldSolverBoundaryCondElectricField(EGrid, i, j, k, 0);
   }
   
   if ((bitfield & compute::EY) == compute::EY) {
      calculateEdgeElectricFieldY(
         perBGrid,
         EGrid,
         EHallGrid,
         EGradPeGrid,
         momentsGrid,
         dPerBGrid,
         dMomentsGrid,
         BgBGrid,
         technicalGrid,
         i,
         j,
         k,
         RKCase
      );
   } else {
      sysBoundaries.getSysBoundary(cellSysBoundaryFlag).fieldSolverBoundaryCondElectricField(EGrid, i, j, k, 1);
   }
   
   if ((bitfield & compute::EZ) == compute::EZ) {
      calculateEdgeElectricFieldZ(
         perBGrid,
         EGrid,
         EHallGrid,
         EGradPeGrid,
         momentsGrid,
         dPerBGrid,
         dMomentsGrid,
         BgBGrid,
         technicalGrid,
         i,
         j,
         k,
         RKCase
      );
   } else {
      sysBoundaries.getSysBoundary(cellSysBoundaryFlag).fieldSolverBoundaryCondElectricField(EGrid, i, j, k, 2);
   }
}

/*! \brief High-level electric field computation function.
 * 
 * Transfers the derivatives, calculates the edge electric fields and transfers the new electric fields.
 * 
 * \param perBGrid fsGrid holding the perturbed B quantities at runge-kutta t=0
 * \param perBDt2Grid fsGrid holding the perturbed B quantities at runge-kutta t=0.5
 * \param EGrid fsGrid holding the Electric field quantities at runge-kutta t=0
 * \param EDt2Grid fsGrid holding the Electric field quantities at runge-kutta t=0.5
 * \param EHallGrid fsGrid holding the Hall contributions to the electric field
 * \param EGradPeGrid fsGrid holding the electron pressure gradient E field
 * \param momentsGrid fsGrid holding the moment quantities at runge-kutta t=0
 * \param momentsDt2Grid fsGrid holding the moment quantities at runge-kutta t=0.5
 * \param dPerBGrid fsGrid holding the derivatives of perturbed B
 * \param dMomentsGrid fsGrid holding the derviatives of moments
 * \param BgBGrid fsGrid holding the background B quantities
 * \param technicalGrid fsGrid holding technical information (such as boundary types)
 * \param sysBoundaries System boundary conditions existing
 * \param RKCase Element in the enum defining the Runge-Kutta method steps
 * 
 * \sa calculateElectricField calculateEdgeElectricFieldX calculateEdgeElectricFieldY calculateEdgeElectricFieldZ
 */
void calculateUpwindedElectricFieldSimple(
   arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBGrid,
   arch::buf<FsGrid<Real, fsgrids::bfield::N_BFIELD, FS_STENCIL_WIDTH>> & perBDt2Grid,
   arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EGrid,
   arch::buf<FsGrid<Real, fsgrids::efield::N_EFIELD, FS_STENCIL_WIDTH>> & EDt2Grid,
   arch::buf<FsGrid<Real, fsgrids::ehall::N_EHALL, FS_STENCIL_WIDTH>> & EHallGrid,
   arch::buf<FsGrid<Real, fsgrids::egradpe::N_EGRADPE, FS_STENCIL_WIDTH>> & EGradPeGrid,
   arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsGrid,
   arch::buf<FsGrid<Real, fsgrids::moments::N_MOMENTS, FS_STENCIL_WIDTH>> & momentsDt2Grid,
   arch::buf<FsGrid<Real, fsgrids::dperb::N_DPERB, FS_STENCIL_WIDTH>> & dPerBGrid,
   arch::buf<FsGrid<Real, fsgrids::dmoments::N_DMOMENTS, FS_STENCIL_WIDTH>> & dMomentsGrid,
   arch::buf<FsGrid<Real, fsgrids::bgbfield::N_BGB, FS_STENCIL_WIDTH>> & BgBGrid,
   arch::buf<FsGrid< fsgrids::technical, 1, FS_STENCIL_WIDTH>> & technicalGrid,
   arch::buf<SysBoundary>& sysBoundaries,
   cint& RKCase
) {
   int timer;
   //const std::array<int, 3> gridDims = technicalGrid.getLocalSize();
   const int* gridDims = &technicalGrid.grid()->getLocalSize()[0];
   const size_t N_cells = gridDims[0]*gridDims[1]*gridDims[2];
   phiprof::start("Calculate upwinded electric field");
   
   timer=phiprof::initializeTimer("MPI","MPI");
   phiprof::start(timer);
   if(meshParams.ohmHallTerm > 0) {
      EHallGrid.syncHostData();
      EHallGrid.grid()->updateGhostCells();
      EHallGrid.syncDeviceData();
   }
   if(meshParams.ohmGradPeTerm > 0) {
      EGradPeGrid.syncHostData();
      EGradPeGrid.grid()->updateGhostCells();
      EGradPeGrid.syncDeviceData();
   }
   if(meshParams.ohmHallTerm == 0 && meshParams.ohmGradPeTerm == 0) {
      dPerBGrid.syncHostData();
      dPerBGrid.grid()->updateGhostCells();
      dPerBGrid.syncDeviceData();
      dMomentsGrid.syncHostData();
      dMomentsGrid.grid()->updateGhostCells();
      dMomentsGrid.syncDeviceData();
   }
   phiprof::stop(timer);
   
   // Calculate upwinded electric field on inner cells
   timer=phiprof::initializeTimer("Compute cells");
   phiprof::start(timer);
   arch::parallel_for({(uint)gridDims[0], (uint)gridDims[1], (uint)gridDims[2]}, ARCH_LOOP_LAMBDA(int i, int j, int k) { 
      if (RKCase == RK_ORDER1 || RKCase == RK_ORDER2_STEP2) {
         calculateElectricField(
            perBGrid,
            EGrid,
            EHallGrid,
            EGradPeGrid,
            momentsGrid,
            dPerBGrid,
            dMomentsGrid,
            BgBGrid,
            technicalGrid,
            i,
            j,
            k,
            sysBoundaries,
            RKCase
         );
      } else { // RKCase == RK_ORDER2_STEP1
         calculateElectricField(
            perBDt2Grid,
            EDt2Grid,
            EHallGrid,
            EGradPeGrid,
            momentsDt2Grid,
            dPerBGrid,
            dMomentsGrid,
            BgBGrid,
            technicalGrid,
            i,
            j,
            k,
            sysBoundaries,
            RKCase
         );
      }
   });
   phiprof::stop(timer,N_cells,"Spatial Cells");
   
   timer=phiprof::initializeTimer("MPI","MPI");
   phiprof::start(timer);
   // Exchange electric field with neighbouring processes
   if (RKCase == RK_ORDER1 || RKCase == RK_ORDER2_STEP2) {
      EGrid.syncHostData();
      EGrid.grid()->updateGhostCells();
      EGrid.syncDeviceData();
   } else { 
      EDt2Grid.syncHostData();
      EDt2Grid.grid()->updateGhostCells();
      EDt2Grid.syncDeviceData();
   }
   phiprof::stop(timer);
   
   phiprof::stop("Calculate upwinded electric field",N_cells,"Spatial Cells");
}
