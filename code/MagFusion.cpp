/// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
/*
  Fusion of Position & Velocity Measurements for a 24 State Extended Kalman Navigation Filter
  Based on https://github.com/priseborough/InertialNav
  Converted to C++ by Paul Riseborough

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Kalman24.h"

// some macros to make the conversion from MATLab easier
#define single(x) float(x)
#define pi M_PI

/*
  run one state and covariance update step using a combination of NED Vel and Pos measurements
 */
void Kalman24::FuseMagnetometer(float accNavMag,
									bool FuseGPSData,
									Vector3f MagData,
									Vector3f PosNE,
									float StatesAtMeasTime[24],
									bool useCompass)
{

static float q0 = 1.0;
static float q1 = 0.0;
static float q2 = 0.0;
static float q3 = 0.0;
static float magN = 0.0;
static float magE = 0.0;
static float magD = 0.0;
static float magXbias = 0.0;
static float magYbias = 0.0;
static float magZbias = 0.0;
static uint8_t obsIndex = 1;
static float SH_MAG[9] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
static float Tnb[3][[3] = {
{1.0,0.0,0.0} ,
{0.0,1.0,0.0} ,
{0.0,0.0,1.0}
};
static float MagPred[3] = {0.0,0.0,0.0};
float K_MAG[24][1];

// define magnetometer measurement error variance (milligauss).
const float R_MAG = 2500.0;
// innovation variances
float varInnov[3];
// innovation vector
float innovation[3];
// intermediate variable for covariance update
float KH[24][24];
float KHP[24][24];

// Perform sequential fusion of Magnetometer measurements.
// This assumes that the errors in the different components are
// uncorrelated which is not true, however in the absence of covariance
// data fit is the only assumption we can make
// so we might as well take advantage of the computational efficiencies
// associated with sequential fusion
if (useCompass && FuseData) || (useCompass && (obsIndex == 1 || obsIndex == 2))
{
    
    // Sequential fusion of XYZ components to spread processing load across
    // three prediction time steps. 
    
    // Calculate observation jacobians and Kalman gains
    if (FuseData)
	{
	
        // Copy required states to local variable names
        q0       = StatesAtMeasTime[0];
        q1       = StatesAtMeasTime[1];
        q2       = StatesAtMeasTime[2];
        q3       = StatesAtMeasTime[3];
        magN     = StatesAtMeasTime[18];
        magE     = StatesAtMeasTime[19];
        magD     = StatesAtMeasTime[20];
        magXbias = StatesAtMeasTime[21];
        magYbias = StatesAtMeasTime[22];
        magZbias = StatesAtMeasTime[23];
		
        // rotate predicted earth components into body axes and calculate
        // predicted measurments
        Tnb = single([q0*q0 + q1*q1 - q2*q2 - q3*q3, 2*(q1*q2 + q0*q3) , 2*(q1*q3-q0*q2) ;...
            2*(q1*q2 - q0*q3), q0*q0 - q1*q1 + q2*q2 - q3*q3, 2*(q2*q3 + q0*q1);...
            2*(q1*q3 + q0*q2) , 2*(q2*q3 - q0*q1) , q0*q0 - q1*q1 - q2*q2 + q3*q3]);
		MagPred[0] = Tnb[0][0]*MagN + Tnb[0][1]*MagE  + Tnb[0][2]*MagD + MagXbias;
		MagPred[1] = Tnb[1][0]*MagN + Tnb[1][1]*MagE  + Tnb[1][2]*MagD + MagYbias;
		MagPred[2] = Tnb[2][0]*MagN + Tnb[2][1]*MagE  + Tnb[0][2]*MagD + MagZbias;
		
        // Calculate observation jacobian
		float SH_MAG[9][1];
		SH_MAG[0] = 2*magD*q3 + 2*magE*q2 + 2*magN*q1;
		SH_MAG[1] = 2*magD*q0 - 2*magE*q1 + 2*magN*q2;
		SH_MAG[2] = 2*magD*q1 + 2*magE*q0 - 2*magN*q3;
		SH_MAG[3] = sq(q3);
		SH_MAG[4] = sq(q2);
		SH_MAG[5] = sq(q1);
		SH_MAG[6] = sq(q0);
		SH_MAG[7] = 2*magN*q0;
		SH_MAG[8] = 2*magE*q3;
		H_MAG[0][0] = SH_MAG[7] + SH_MAG[8] - 2*magD*q2;
		H_MAG[0][1] = SH_MAG[0];
		H_MAG[0][2] = 2*magE*q1 - 2*magD*q0 - 2*magN*q2;
		H_MAG[0][3] = SH_MAG[2];
		H_MAG[0][18] = SH_MAG[5] - SH_MAG[4] - SH_MAG[3] + SH_MAG[6];
		H_MAG[0][19] = 2*q0*q3 + 2*q1*q2;
		H_MAG[0][20] = 2*q1*q3 - 2*q0*q2;
		H_MAG[0][21] = 1;
		H_MAG[1][0] = SH_MAG[2];
		H_MAG[1][1] = SH_MAG[1];
		H_MAG[1][2] = SH_MAG[0];
		H_MAG[1][3] = 2*magD*q2 - SH_MAG[8] - SH_MAG[7];
		H_MAG[1][18] = 2*q1*q2 - 2*q0*q3;
		H_MAG[1][19] = SH_MAG[4] - SH_MAG[3] - SH_MAG[5] + SH_MAG[6];
		H_MAG[1][20] = 2*q0*q1 + 2*q2*q3;
		H_MAG[1][22] = 1;
		H_MAG[2][0] = SH_MAG[1];
		H_MAG[2][1] = 2*magN*q3 - 2*magE*q0 - 2*magD*q1;
		H_MAG[2][2] = SH_MAG[7] + SH_MAG[8] - 2*magD*q2;
		H_MAG[2][3] = SH_MAG[0];
		H_MAG[2][18] = 2*q0*q2 + 2*q1*q3;
		H_MAG[2][19] = 2*q2*q3 - 2*q0*q1;
		H_MAG[2][20] = SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6];
		H_MAG[2][23] = 1;
		
        // calculate Kalman gain
		float SK_MX[6];
		SK_MX[0] = 1/(P[21][21] + R_MAG + P[1][21]*SH_MAG[0] + P[3][21]*SH_MAG[2] - P[18][21]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) - (2*magD*q0 - 2*magE*q1 + 2*magN*q2)*(P[21][2] + P[1][2]*SH_MAG[0] + P[3][2]*SH_MAG[2] - P[18][2]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][2]*(2*q0*q3 + 2*q1*q2) - P[20][2]*(2*q0*q2 - 2*q1*q3) - P[2][2]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][2]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + (SH_MAG[7] + SH_MAG[8] - 2*magD*q2)*(P[21][0] + P[1][0]*SH_MAG[0] + P[3][0]*SH_MAG[2] - P[18][0]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][0]*(2*q0*q3 + 2*q1*q2) - P[20][0]*(2*q0*q2 - 2*q1*q3) - P[2][0]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][0]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[0]*(P[21][1] + P[1][1]*SH_MAG[0] + P[3][1]*SH_MAG[2] - P[18][1]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][1]*(2*q0*q3 + 2*q1*q2) - P[20][1]*(2*q0*q2 - 2*q1*q3) - P[2][1]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][1]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[2]*(P[21][3] + P[1][3]*SH_MAG[0] + P[3][3]*SH_MAG[2] - P[18][3]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][3]*(2*q0*q3 + 2*q1*q2) - P[20][3]*(2*q0*q2 - 2*q1*q3) - P[2][3]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][3]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - (SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6])*(P[21][18] + P[1][18]*SH_MAG[0] + P[3][18]*SH_MAG[2] - P[18][18]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][18]*(2*q0*q3 + 2*q1*q2) - P[20][18]*(2*q0*q2 - 2*q1*q3) - P[2][18]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][18]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + P[19][21]*(2*q0*q3 + 2*q1*q2) - P[20][21]*(2*q0*q2 - 2*q1*q3) - P[2][21]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + (2*q0*q3 + 2*q1*q2)*(P[21][19] + P[1][19]*SH_MAG[0] + P[3][19]*SH_MAG[2] - P[18][19]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][19]*(2*q0*q3 + 2*q1*q2) - P[20][19]*(2*q0*q2 - 2*q1*q3) - P[2][19]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][19]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - (2*q0*q2 - 2*q1*q3)*(P[21][20] + P[1][20]*SH_MAG[0] + P[3][20]*SH_MAG[2] - P[18][20]*(SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6]) + P[19][20]*(2*q0*q3 + 2*q1*q2) - P[20][20]*(2*q0*q2 - 2*q1*q3) - P[2][20]*(2*magD*q0 - 2*magE*q1 + 2*magN*q2) + P[0][20]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + P[0][21]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2));
		SK_MX[1] = SH_MAG[3] + SH_MAG[4] - SH_MAG[5] - SH_MAG[6];
		SK_MX[2] = 2*magD*q0 - 2*magE*q1 + 2*magN*q2;
		SK_MX[3] = SH_MAG[7] + SH_MAG[8] - 2*magD*q2;
		SK_MX[4] = 2*q0*q2 - 2*q1*q3;
		SK_MX[5] = 2*q0*q3 + 2*q1*q2;
		K_MAG[0][0] = SK_MX[0]*(P[0][21] + P[0][1]*SH_MAG[0] + P[0][3]*SH_MAG[2] + P[0][0]*SK_MX[3] - P[0][2]*SK_MX[2] - P[0][18]*SK_MX[1] + P[0][19]*SK_MX[5] - P[0][20]*SK_MX[4]);
		K_MAG[1][0] = SK_MX[0]*(P[1][21] + P[1][1]*SH_MAG[0] + P[1][3]*SH_MAG[2] + P[1][0]*SK_MX[3] - P[1][2]*SK_MX[2] - P[1][18]*SK_MX[1] + P[1][19]*SK_MX[5] - P[1][20]*SK_MX[4]);
		K_MAG[2][0] = SK_MX[0]*(P[2][21] + P[2][1]*SH_MAG[0] + P[2][3]*SH_MAG[2] + P[2][0]*SK_MX[3] - P[2][2]*SK_MX[2] - P[2][18]*SK_MX[1] + P[2][19]*SK_MX[5] - P[2][20]*SK_MX[4]);
		K_MAG[3][0] = SK_MX[0]*(P[3][21] + P[3][1]*SH_MAG[0] + P[3][3]*SH_MAG[2] + P[3][0]*SK_MX[3] - P[3][2]*SK_MX[2] - P[3][18]*SK_MX[1] + P[3][19]*SK_MX[5] - P[3][20]*SK_MX[4]);
		K_MAG[4][0] = SK_MX[0]*(P[4][21] + P[4][1]*SH_MAG[0] + P[4][3]*SH_MAG[2] + P[4][0]*SK_MX[3] - P[4][2]*SK_MX[2] - P[4][18]*SK_MX[1] + P[4][19]*SK_MX[5] - P[4][20]*SK_MX[4]);
		K_MAG[5][0] = SK_MX[0]*(P[5][21] + P[5][1]*SH_MAG[0] + P[5][3]*SH_MAG[2] + P[5][0]*SK_MX[3] - P[5][2]*SK_MX[2] - P[5][18]*SK_MX[1] + P[5][19]*SK_MX[5] - P[5][20]*SK_MX[4]);
		K_MAG[6][0] = SK_MX[0]*(P[6][21] + P[6][1]*SH_MAG[0] + P[6][3]*SH_MAG[2] + P[6][0]*SK_MX[3] - P[6][2]*SK_MX[2] - P[6][18]*SK_MX[1] + P[6][19]*SK_MX[5] - P[6][20]*SK_MX[4]);
		K_MAG[7][0] = SK_MX[0]*(P[7][21] + P[7][1]*SH_MAG[0] + P[7][3]*SH_MAG[2] + P[7][0]*SK_MX[3] - P[7][2]*SK_MX[2] - P[7][18]*SK_MX[1] + P[7][19]*SK_MX[5] - P[7][20]*SK_MX[4]);
		K_MAG[8][0] = SK_MX[0]*(P[8][21] + P[8][1]*SH_MAG[0] + P[8][3]*SH_MAG[2] + P[8][0]*SK_MX[3] - P[8][2]*SK_MX[2] - P[8][18]*SK_MX[1] + P[8][19]*SK_MX[5] - P[8][20]*SK_MX[4]);
		K_MAG[9][0] = SK_MX[0]*(P[9][21] + P[9][1]*SH_MAG[0] + P[9][3]*SH_MAG[2] + P[9][0]*SK_MX[3] - P[9][2]*SK_MX[2] - P[9][18]*SK_MX[1] + P[9][19]*SK_MX[5] - P[9][20]*SK_MX[4]);
		K_MAG[10][0] = SK_MX[0]*(P[10][21] + P[10][1]*SH_MAG[0] + P[10][3]*SH_MAG[2] + P[10][0]*SK_MX[3] - P[10][2]*SK_MX[2] - P[10][18]*SK_MX[1] + P[10][19]*SK_MX[5] - P[10][20]*SK_MX[4]);
		K_MAG[11][0] = SK_MX[0]*(P[11][21] + P[11][1]*SH_MAG[0] + P[11][3]*SH_MAG[2] + P[11][0]*SK_MX[3] - P[11][2]*SK_MX[2] - P[11][18]*SK_MX[1] + P[11][19]*SK_MX[5] - P[11][20]*SK_MX[4]);
		K_MAG[12][0] = SK_MX[0]*(P[12][21] + P[12][1]*SH_MAG[0] + P[12][3]*SH_MAG[2] + P[12][0]*SK_MX[3] - P[12][2]*SK_MX[2] - P[12][18]*SK_MX[1] + P[12][19]*SK_MX[5] - P[12][20]*SK_MX[4]);
		K_MAG[13][0] = SK_MX[0]*(P[13][21] + P[13][1]*SH_MAG[0] + P[13][3]*SH_MAG[2] + P[13][0]*SK_MX[3] - P[13][2]*SK_MX[2] - P[13][18]*SK_MX[1] + P[13][19]*SK_MX[5] - P[13][20]*SK_MX[4]);
		K_MAG[14][0] = SK_MX[0]*(P[14][21] + P[14][1]*SH_MAG[0] + P[14][3]*SH_MAG[2] + P[14][0]*SK_MX[3] - P[14][2]*SK_MX[2] - P[14][18]*SK_MX[1] + P[14][19]*SK_MX[5] - P[14][20]*SK_MX[4]);
		K_MAG[15][0] = SK_MX[0]*(P[15][21] + P[15][1]*SH_MAG[0] + P[15][3]*SH_MAG[2] + P[15][0]*SK_MX[3] - P[15][2]*SK_MX[2] - P[15][18]*SK_MX[1] + P[15][19]*SK_MX[5] - P[15][20]*SK_MX[4]);
		K_MAG[16][0] = SK_MX[0]*(P[16][21] + P[16][1]*SH_MAG[0] + P[16][3]*SH_MAG[2] + P[16][0]*SK_MX[3] - P[16][2]*SK_MX[2] - P[16][18]*SK_MX[1] + P[16][19]*SK_MX[5] - P[16][20]*SK_MX[4]);
		K_MAG[17][0] = SK_MX[0]*(P[17][21] + P[17][1]*SH_MAG[0] + P[17][3]*SH_MAG[2] + P[17][0]*SK_MX[3] - P[17][2]*SK_MX[2] - P[17][18]*SK_MX[1] + P[17][19]*SK_MX[5] - P[17][20]*SK_MX[4]);
		K_MAG[18][0] = SK_MX[0]*(P[18][21] + P[18][1]*SH_MAG[0] + P[18][3]*SH_MAG[2] + P[18][0]*SK_MX[3] - P[18][2]*SK_MX[2] - P[18][18]*SK_MX[1] + P[18][19]*SK_MX[5] - P[18][20]*SK_MX[4]);
		K_MAG[19][0] = SK_MX[0]*(P[19][21] + P[19][1]*SH_MAG[0] + P[19][3]*SH_MAG[2] + P[19][0]*SK_MX[3] - P[19][2]*SK_MX[2] - P[19][18]*SK_MX[1] + P[19][19]*SK_MX[5] - P[19][20]*SK_MX[4]);
		K_MAG[20][0] = SK_MX[0]*(P[20][21] + P[20][1]*SH_MAG[0] + P[20][3]*SH_MAG[2] + P[20][0]*SK_MX[3] - P[20][2]*SK_MX[2] - P[20][18]*SK_MX[1] + P[20][19]*SK_MX[5] - P[20][20]*SK_MX[4]);
		K_MAG[21][0] = SK_MX[0]*(P[21][21] + P[21][1]*SH_MAG[0] + P[21][3]*SH_MAG[2] + P[21][0]*SK_MX[3] - P[21][2]*SK_MX[2] - P[21][18]*SK_MX[1] + P[21][19]*SK_MX[5] - P[21][20]*SK_MX[4]);
		K_MAG[22][0] = SK_MX[0]*(P[22][21] + P[22][1]*SH_MAG[0] + P[22][3]*SH_MAG[2] + P[22][0]*SK_MX[3] - P[22][2]*SK_MX[2] - P[22][18]*SK_MX[1] + P[22][19]*SK_MX[5] - P[22][20]*SK_MX[4]);
		K_MAG[23][0] = SK_MX[0]*(P[23][21] + P[23][1]*SH_MAG[0] + P[23][3]*SH_MAG[2] + P[23][0]*SK_MX[3] - P[23][2]*SK_MX[2] - P[23][18]*SK_MX[1] + P[23][19]*SK_MX[5] - P[23][20]*SK_MX[4]);
        varInnov[0] = 1.0/SK_MX[0];
        
        // reset the observation index to 0 (we start by fusing the X
        // measurement)
        obsIndex = 0;
		}
	else if (obsIndex == 1) // we are now fusing the Y measurement
		{
		
		float SK_MY[5];
		SK_MY[0] = 1/(P[22][22] + R_MAG + P[0][22]*SH_MAG[2] + P[1][22]*SH_MAG[1] + P[2][22]*SH_MAG[0] - P[19][22]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - (2*q0*q3 - 2*q1*q2)*(P[22][18] + P[0][18]*SH_MAG[2] + P[1][18]*SH_MAG[1] + P[2][18]*SH_MAG[0] - P[19][18]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][18]*(2*q0*q3 - 2*q1*q2) + P[20][18]*(2*q0*q1 + 2*q2*q3) - P[3][18]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + (2*q0*q1 + 2*q2*q3)*(P[22][20] + P[0][20]*SH_MAG[2] + P[1][20]*SH_MAG[1] + P[2][20]*SH_MAG[0] - P[19][20]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][20]*(2*q0*q3 - 2*q1*q2) + P[20][20]*(2*q0*q1 + 2*q2*q3) - P[3][20]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - (SH_MAG[7] + SH_MAG[8] - 2*magD*q2)*(P[22][3] + P[0][3]*SH_MAG[2] + P[1][3]*SH_MAG[1] + P[2][3]*SH_MAG[0] - P[19][3]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][3]*(2*q0*q3 - 2*q1*q2) + P[20][3]*(2*q0*q1 + 2*q2*q3) - P[3][3]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - P[18][22]*(2*q0*q3 - 2*q1*q2) + P[20][22]*(2*q0*q1 + 2*q2*q3) + SH_MAG[2]*(P[22][0] + P[0][0]*SH_MAG[2] + P[1][0]*SH_MAG[1] + P[2][0]*SH_MAG[0] - P[19][0]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][0]*(2*q0*q3 - 2*q1*q2) + P[20][0]*(2*q0*q1 + 2*q2*q3) - P[3][0]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[1]*(P[22][1] + P[0][1]*SH_MAG[2] + P[1][1]*SH_MAG[1] + P[2][1]*SH_MAG[0] - P[19][1]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][1]*(2*q0*q3 - 2*q1*q2) + P[20][1]*(2*q0*q1 + 2*q2*q3) - P[3][1]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[0]*(P[22][2] + P[0][2]*SH_MAG[2] + P[1][2]*SH_MAG[1] + P[2][2]*SH_MAG[0] - P[19][2]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][2]*(2*q0*q3 - 2*q1*q2) + P[20][2]*(2*q0*q1 + 2*q2*q3) - P[3][2]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - (SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6])*(P[22][19] + P[0][19]*SH_MAG[2] + P[1][19]*SH_MAG[1] + P[2][19]*SH_MAG[0] - P[19][19]*(SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6]) - P[18][19]*(2*q0*q3 - 2*q1*q2) + P[20][19]*(2*q0*q1 + 2*q2*q3) - P[3][19]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - P[3][22]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2));
		SK_MY[1] = SH_MAG[3] - SH_MAG[4] + SH_MAG[5] - SH_MAG[6];
		SK_MY[2] = SH_MAG[7] + SH_MAG[8] - 2*magD*q2;
		SK_MY[3] = 2*q0*q3 - 2*q1*q2;
		SK_MY[4] = 2*q0*q1 + 2*q2*q3;
		K_MAG[0][0] = SK_MY[0]*(P[0][22] + P[0][0]*SH_MAG[2] + P[0][1]*SH_MAG[1] + P[0][2]*SH_MAG[0] - P[0][3]*SK_MY[2] - P[0][19]*SK_MY[1] - P[0][18]*SK_MY[3] + P[0][20]*SK_MY[4]);
		K_MAG[1][0] = SK_MY[0]*(P[1][22] + P[1][0]*SH_MAG[2] + P[1][1]*SH_MAG[1] + P[1][2]*SH_MAG[0] - P[1][3]*SK_MY[2] - P[1][19]*SK_MY[1] - P[1][18]*SK_MY[3] + P[1][20]*SK_MY[4]);
		K_MAG[2][0] = SK_MY[0]*(P[2][22] + P[2][0]*SH_MAG[2] + P[2][1]*SH_MAG[1] + P[2][2]*SH_MAG[0] - P[2][3]*SK_MY[2] - P[2][19]*SK_MY[1] - P[2][18]*SK_MY[3] + P[2][20]*SK_MY[4]);
		K_MAG[3][0] = SK_MY[0]*(P[3][22] + P[3][0]*SH_MAG[2] + P[3][1]*SH_MAG[1] + P[3][2]*SH_MAG[0] - P[3][3]*SK_MY[2] - P[3][19]*SK_MY[1] - P[3][18]*SK_MY[3] + P[3][20]*SK_MY[4]);
		K_MAG[4][0] = SK_MY[0]*(P[4][22] + P[4][0]*SH_MAG[2] + P[4][1]*SH_MAG[1] + P[4][2]*SH_MAG[0] - P[4][3]*SK_MY[2] - P[4][19]*SK_MY[1] - P[4][18]*SK_MY[3] + P[4][20]*SK_MY[4]);
		K_MAG[5][0] = SK_MY[0]*(P[5][22] + P[5][0]*SH_MAG[2] + P[5][1]*SH_MAG[1] + P[5][2]*SH_MAG[0] - P[5][3]*SK_MY[2] - P[5][19]*SK_MY[1] - P[5][18]*SK_MY[3] + P[5][20]*SK_MY[4]);
		K_MAG[6][0] = SK_MY[0]*(P[6][22] + P[6][0]*SH_MAG[2] + P[6][1]*SH_MAG[1] + P[6][2]*SH_MAG[0] - P[6][3]*SK_MY[2] - P[6][19]*SK_MY[1] - P[6][18]*SK_MY[3] + P[6][20]*SK_MY[4]);
		K_MAG[7][0] = SK_MY[0]*(P[7][22] + P[7][0]*SH_MAG[2] + P[7][1]*SH_MAG[1] + P[7][2]*SH_MAG[0] - P[7][3]*SK_MY[2] - P[7][19]*SK_MY[1] - P[7][18]*SK_MY[3] + P[7][20]*SK_MY[4]);
		K_MAG[8][0] = SK_MY[0]*(P[8][22] + P[8][0]*SH_MAG[2] + P[8][1]*SH_MAG[1] + P[8][2]*SH_MAG[0] - P[8][3]*SK_MY[2] - P[8][19]*SK_MY[1] - P[8][18]*SK_MY[3] + P[8][20]*SK_MY[4]);
		K_MAG[9][0] = SK_MY[0]*(P[9][22] + P[9][0]*SH_MAG[2] + P[9][1]*SH_MAG[1] + P[9][2]*SH_MAG[0] - P[9][3]*SK_MY[2] - P[9][19]*SK_MY[1] - P[9][18]*SK_MY[3] + P[9][20]*SK_MY[4]);
		K_MAG[10][0] = SK_MY[0]*(P[10][22] + P[10][0]*SH_MAG[2] + P[10][1]*SH_MAG[1] + P[10][2]*SH_MAG[0] - P[10][3]*SK_MY[2] - P[10][19]*SK_MY[1] - P[10][18]*SK_MY[3] + P[10][20]*SK_MY[4]);
		K_MAG[11][0] = SK_MY[0]*(P[11][22] + P[11][0]*SH_MAG[2] + P[11][1]*SH_MAG[1] + P[11][2]*SH_MAG[0] - P[11][3]*SK_MY[2] - P[11][19]*SK_MY[1] - P[11][18]*SK_MY[3] + P[11][20]*SK_MY[4]);
		K_MAG[12][0] = SK_MY[0]*(P[12][22] + P[12][0]*SH_MAG[2] + P[12][1]*SH_MAG[1] + P[12][2]*SH_MAG[0] - P[12][3]*SK_MY[2] - P[12][19]*SK_MY[1] - P[12][18]*SK_MY[3] + P[12][20]*SK_MY[4]);
		K_MAG[13][0] = SK_MY[0]*(P[13][22] + P[13][0]*SH_MAG[2] + P[13][1]*SH_MAG[1] + P[13][2]*SH_MAG[0] - P[13][3]*SK_MY[2] - P[13][19]*SK_MY[1] - P[13][18]*SK_MY[3] + P[13][20]*SK_MY[4]);
		K_MAG[14][0] = SK_MY[0]*(P[14][22] + P[14][0]*SH_MAG[2] + P[14][1]*SH_MAG[1] + P[14][2]*SH_MAG[0] - P[14][3]*SK_MY[2] - P[14][19]*SK_MY[1] - P[14][18]*SK_MY[3] + P[14][20]*SK_MY[4]);
		K_MAG[15][0] = SK_MY[0]*(P[15][22] + P[15][0]*SH_MAG[2] + P[15][1]*SH_MAG[1] + P[15][2]*SH_MAG[0] - P[15][3]*SK_MY[2] - P[15][19]*SK_MY[1] - P[15][18]*SK_MY[3] + P[15][20]*SK_MY[4]);
		K_MAG[16][0] = SK_MY[0]*(P[16][22] + P[16][0]*SH_MAG[2] + P[16][1]*SH_MAG[1] + P[16][2]*SH_MAG[0] - P[16][3]*SK_MY[2] - P[16][19]*SK_MY[1] - P[16][18]*SK_MY[3] + P[16][20]*SK_MY[4]);
		K_MAG[17][0] = SK_MY[0]*(P[17][22] + P[17][0]*SH_MAG[2] + P[17][1]*SH_MAG[1] + P[17][2]*SH_MAG[0] - P[17][3]*SK_MY[2] - P[17][19]*SK_MY[1] - P[17][18]*SK_MY[3] + P[17][20]*SK_MY[4]);
		K_MAG[18][0] = SK_MY[0]*(P[18][22] + P[18][0]*SH_MAG[2] + P[18][1]*SH_MAG[1] + P[18][2]*SH_MAG[0] - P[18][3]*SK_MY[2] - P[18][19]*SK_MY[1] - P[18][18]*SK_MY[3] + P[18][20]*SK_MY[4]);
		K_MAG[19][0] = SK_MY[0]*(P[19][22] + P[19][0]*SH_MAG[2] + P[19][1]*SH_MAG[1] + P[19][2]*SH_MAG[0] - P[19][3]*SK_MY[2] - P[19][19]*SK_MY[1] - P[19][18]*SK_MY[3] + P[19][20]*SK_MY[4]);
		K_MAG[20][0] = SK_MY[0]*(P[20][22] + P[20][0]*SH_MAG[2] + P[20][1]*SH_MAG[1] + P[20][2]*SH_MAG[0] - P[20][3]*SK_MY[2] - P[20][19]*SK_MY[1] - P[20][18]*SK_MY[3] + P[20][20]*SK_MY[4]);
		K_MAG[21][0] = SK_MY[0]*(P[21][22] + P[21][0]*SH_MAG[2] + P[21][1]*SH_MAG[1] + P[21][2]*SH_MAG[0] - P[21][3]*SK_MY[2] - P[21][19]*SK_MY[1] - P[21][18]*SK_MY[3] + P[21][20]*SK_MY[4]);
		K_MAG[22][0] = SK_MY[0]*(P[22][22] + P[22][0]*SH_MAG[2] + P[22][1]*SH_MAG[1] + P[22][2]*SH_MAG[0] - P[22][3]*SK_MY[2] - P[22][19]*SK_MY[1] - P[22][18]*SK_MY[3] + P[22][20]*SK_MY[4]);
		K_MAG[23][0] = SK_MY[0]*(P[23][22] + P[23][0]*SH_MAG[2] + P[23][1]*SH_MAG[1] + P[23][2]*SH_MAG[0] - P[23][3]*SK_MY[2] - P[23][19]*SK_MY[1] - P[23][18]*SK_MY[3] + P[23][20]*SK_MY[4]);
		varInnov[1] = 1.0/SK_MY[0];
		
		}
    else if (obsIndex == 2) // we are now fusing the Z measurement
		{
		
        // calculate the observation jacobian
		float SK_MZ[6][1];
		SK_MZ[0] = 1/(P[23][23] + R_MAG + P[0][23]*SH_MAG[1] + P[3][23]*SH_MAG[0] + P[20][23]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) - (2*magD*q1 + 2*magE*q0 - 2*magN*q3)*(P[23][1] + P[0][1]*SH_MAG[1] + P[3][1]*SH_MAG[0] + P[20][1]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][1]*(2*q0*q2 + 2*q1*q3) - P[19][1]*(2*q0*q1 - 2*q2*q3) - P[1][1]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][1]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + (SH_MAG[7] + SH_MAG[8] - 2*magD*q2)*(P[23][2] + P[0][2]*SH_MAG[1] + P[3][2]*SH_MAG[0] + P[20][2]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][2]*(2*q0*q2 + 2*q1*q3) - P[19][2]*(2*q0*q1 - 2*q2*q3) - P[1][2]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][2]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[1]*(P[23][0] + P[0][0]*SH_MAG[1] + P[3][0]*SH_MAG[0] + P[20][0]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][0]*(2*q0*q2 + 2*q1*q3) - P[19][0]*(2*q0*q1 - 2*q2*q3) - P[1][0]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][0]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + SH_MAG[0]*(P[23][3] + P[0][3]*SH_MAG[1] + P[3][3]*SH_MAG[0] + P[20][3]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][3]*(2*q0*q2 + 2*q1*q3) - P[19][3]*(2*q0*q1 - 2*q2*q3) - P[1][3]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][3]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + (SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6])*(P[23][20] + P[0][20]*SH_MAG[1] + P[3][20]*SH_MAG[0] + P[20][20]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][20]*(2*q0*q2 + 2*q1*q3) - P[19][20]*(2*q0*q1 - 2*q2*q3) - P[1][20]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][20]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + P[18][23]*(2*q0*q2 + 2*q1*q3) - P[19][23]*(2*q0*q1 - 2*q2*q3) - P[1][23]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + (2*q0*q2 + 2*q1*q3)*(P[23][18] + P[0][18]*SH_MAG[1] + P[3][18]*SH_MAG[0] + P[20][18]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][18]*(2*q0*q2 + 2*q1*q3) - P[19][18]*(2*q0*q1 - 2*q2*q3) - P[1][18]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][18]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) - (2*q0*q1 - 2*q2*q3)*(P[23][19] + P[0][19]*SH_MAG[1] + P[3][19]*SH_MAG[0] + P[20][19]*(SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6]) + P[18][19]*(2*q0*q2 + 2*q1*q3) - P[19][19]*(2*q0*q1 - 2*q2*q3) - P[1][19]*(2*magD*q1 + 2*magE*q0 - 2*magN*q3) + P[2][19]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2)) + P[2][23]*(SH_MAG[7] + SH_MAG[8] - 2*magD*q2));
		SK_MZ[1] = SH_MAG[3] - SH_MAG[4] - SH_MAG[5] + SH_MAG[6];
		SK_MZ[2] = 2*magD*q1 + 2*magE*q0 - 2*magN*q3;
		SK_MZ[3] = SH_MAG[7] + SH_MAG[8] - 2*magD*q2;
		SK_MZ[4] = 2*q0*q1 - 2*q2*q3;
		SK_MZ[5] = 2*q0*q2 + 2*q1*q3;
		K_MAG[0][0] = SK_MZ[0]*(P[0][23] + P[0][0]*SH_MAG[1] + P[0][3]*SH_MAG[0] - P[0][1]*SK_MZ[2] + P[0][2]*SK_MZ[3] + P[0][20]*SK_MZ[1] + P[0][18]*SK_MZ[5] - P[0][19]*SK_MZ[4]);
		K_MAG[1][0] = SK_MZ[0]*(P[1][23] + P[1][0]*SH_MAG[1] + P[1][3]*SH_MAG[0] - P[1][1]*SK_MZ[2] + P[1][2]*SK_MZ[3] + P[1][20]*SK_MZ[1] + P[1][18]*SK_MZ[5] - P[1][19]*SK_MZ[4]);
		K_MAG[2][0] = SK_MZ[0]*(P[2][23] + P[2][0]*SH_MAG[1] + P[2][3]*SH_MAG[0] - P[2][1]*SK_MZ[2] + P[2][2]*SK_MZ[3] + P[2][20]*SK_MZ[1] + P[2][18]*SK_MZ[5] - P[2][19]*SK_MZ[4]);
		K_MAG[3][0] = SK_MZ[0]*(P[3][23] + P[3][0]*SH_MAG[1] + P[3][3]*SH_MAG[0] - P[3][1]*SK_MZ[2] + P[3][2]*SK_MZ[3] + P[3][20]*SK_MZ[1] + P[3][18]*SK_MZ[5] - P[3][19]*SK_MZ[4]);
		K_MAG[4][0] = SK_MZ[0]*(P[4][23] + P[4][0]*SH_MAG[1] + P[4][3]*SH_MAG[0] - P[4][1]*SK_MZ[2] + P[4][2]*SK_MZ[3] + P[4][20]*SK_MZ[1] + P[4][18]*SK_MZ[5] - P[4][19]*SK_MZ[4]);
		K_MAG[5][0] = SK_MZ[0]*(P[5][23] + P[5][0]*SH_MAG[1] + P[5][3]*SH_MAG[0] - P[5][1]*SK_MZ[2] + P[5][2]*SK_MZ[3] + P[5][20]*SK_MZ[1] + P[5][18]*SK_MZ[5] - P[5][19]*SK_MZ[4]);
		K_MAG[6][0] = SK_MZ[0]*(P[6][23] + P[6][0]*SH_MAG[1] + P[6][3]*SH_MAG[0] - P[6][1]*SK_MZ[2] + P[6][2]*SK_MZ[3] + P[6][20]*SK_MZ[1] + P[6][18]*SK_MZ[5] - P[6][19]*SK_MZ[4]);
		K_MAG[7][0] = SK_MZ[0]*(P[7][23] + P[7][0]*SH_MAG[1] + P[7][3]*SH_MAG[0] - P[7][1]*SK_MZ[2] + P[7][2]*SK_MZ[3] + P[7][20]*SK_MZ[1] + P[7][18]*SK_MZ[5] - P[7][19]*SK_MZ[4]);
		K_MAG[8][0] = SK_MZ[0]*(P[8][23] + P[8][0]*SH_MAG[1] + P[8][3]*SH_MAG[0] - P[8][1]*SK_MZ[2] + P[8][2]*SK_MZ[3] + P[8][20]*SK_MZ[1] + P[8][18]*SK_MZ[5] - P[8][19]*SK_MZ[4]);
		K_MAG[9][0] = SK_MZ[0]*(P[9][23] + P[9][0]*SH_MAG[1] + P[9][3]*SH_MAG[0] - P[9][1]*SK_MZ[2] + P[9][2]*SK_MZ[3] + P[9][20]*SK_MZ[1] + P[9][18]*SK_MZ[5] - P[9][19]*SK_MZ[4]);
		K_MAG[10][0] = SK_MZ[0]*(P[10][23] + P[10][0]*SH_MAG[1] + P[10][3]*SH_MAG[0] - P[10][1]*SK_MZ[2] + P[10][2]*SK_MZ[3] + P[10][20]*SK_MZ[1] + P[10][18]*SK_MZ[5] - P[10][19]*SK_MZ[4]);
		K_MAG[11][0] = SK_MZ[0]*(P[11][23] + P[11][0]*SH_MAG[1] + P[11][3]*SH_MAG[0] - P[11][1]*SK_MZ[2] + P[11][2]*SK_MZ[3] + P[11][20]*SK_MZ[1] + P[11][18]*SK_MZ[5] - P[11][19]*SK_MZ[4]);
		K_MAG[12][0] = SK_MZ[0]*(P[12][23] + P[12][0]*SH_MAG[1] + P[12][3]*SH_MAG[0] - P[12][1]*SK_MZ[2] + P[12][2]*SK_MZ[3] + P[12][20]*SK_MZ[1] + P[12][18]*SK_MZ[5] - P[12][19]*SK_MZ[4]);
		K_MAG[13][0] = SK_MZ[0]*(P[13][23] + P[13][0]*SH_MAG[1] + P[13][3]*SH_MAG[0] - P[13][1]*SK_MZ[2] + P[13][2]*SK_MZ[3] + P[13][20]*SK_MZ[1] + P[13][18]*SK_MZ[5] - P[13][19]*SK_MZ[4]);
		K_MAG[14][0] = SK_MZ[0]*(P[14][23] + P[14][0]*SH_MAG[1] + P[14][3]*SH_MAG[0] - P[14][1]*SK_MZ[2] + P[14][2]*SK_MZ[3] + P[14][20]*SK_MZ[1] + P[14][18]*SK_MZ[5] - P[14][19]*SK_MZ[4]);
		K_MAG[15][0] = SK_MZ[0]*(P[15][23] + P[15][0]*SH_MAG[1] + P[15][3]*SH_MAG[0] - P[15][1]*SK_MZ[2] + P[15][2]*SK_MZ[3] + P[15][20]*SK_MZ[1] + P[15][18]*SK_MZ[5] - P[15][19]*SK_MZ[4]);
		K_MAG[16][0] = SK_MZ[0]*(P[16][23] + P[16][0]*SH_MAG[1] + P[16][3]*SH_MAG[0] - P[16][1]*SK_MZ[2] + P[16][2]*SK_MZ[3] + P[16][20]*SK_MZ[1] + P[16][18]*SK_MZ[5] - P[16][19]*SK_MZ[4]);
		K_MAG[17][0] = SK_MZ[0]*(P[17][23] + P[17][0]*SH_MAG[1] + P[17][3]*SH_MAG[0] - P[17][1]*SK_MZ[2] + P[17][2]*SK_MZ[3] + P[17][20]*SK_MZ[1] + P[17][18]*SK_MZ[5] - P[17][19]*SK_MZ[4]);
		K_MAG[18][0] = SK_MZ[0]*(P[18][23] + P[18][0]*SH_MAG[1] + P[18][3]*SH_MAG[0] - P[18][1]*SK_MZ[2] + P[18][2]*SK_MZ[3] + P[18][20]*SK_MZ[1] + P[18][18]*SK_MZ[5] - P[18][19]*SK_MZ[4]);
		K_MAG[19][0] = SK_MZ[0]*(P[19][23] + P[19][0]*SH_MAG[1] + P[19][3]*SH_MAG[0] - P[19][1]*SK_MZ[2] + P[19][2]*SK_MZ[3] + P[19][20]*SK_MZ[1] + P[19][18]*SK_MZ[5] - P[19][19]*SK_MZ[4]);
		K_MAG[20][0] = SK_MZ[0]*(P[20][23] + P[20][0]*SH_MAG[1] + P[20][3]*SH_MAG[0] - P[20][1]*SK_MZ[2] + P[20][2]*SK_MZ[3] + P[20][20]*SK_MZ[1] + P[20][18]*SK_MZ[5] - P[20][19]*SK_MZ[4]);
		K_MAG[21][0] = SK_MZ[0]*(P[21][23] + P[21][0]*SH_MAG[1] + P[21][3]*SH_MAG[0] - P[21][1]*SK_MZ[2] + P[21][2]*SK_MZ[3] + P[21][20]*SK_MZ[1] + P[21][18]*SK_MZ[5] - P[21][19]*SK_MZ[4]);
		K_MAG[22][0] = SK_MZ[0]*(P[22][23] + P[22][0]*SH_MAG[1] + P[22][3]*SH_MAG[0] - P[22][1]*SK_MZ[2] + P[22][2]*SK_MZ[3] + P[22][20]*SK_MZ[1] + P[22][18]*SK_MZ[5] - P[22][19]*SK_MZ[4]);
		K_MAG[23][0] = SK_MZ[0]*(P[23][23] + P[23][0]*SH_MAG[1] + P[23][3]*SH_MAG[0] - P[23][1]*SK_MZ[2] + P[23][2]*SK_MZ[3] + P[23][20]*SK_MZ[1] + P[23][18]*SK_MZ[5] - P[23][19]*SK_MZ[4]);
        varInnov[2] = 1.0/SK_MZ[0];
		
    }
	
    // Calculate the measurement innovation
    innovation[obsIndex] = MagPred[obsIndex] - MagData[obsIndex];
    // Check the innovation for consistency and don't fuse if > 5Sigma
    if ((innovation[obsIndex]*innovation[obsIndex]/varInnov[obsIndex]) < 25.0)
	{
		// correct the state vector
		for (uint8_t index= 0; i<=23; i++)
		{
			xk[index] = K_MAG[index] * innovation(obsIndex);
			states[index] = states[index] - xk[index];
		}
        // normalise the quaternion states
        float quatMag = sqrt(states(0)*states(0) + states(1)*states(1) + states(2)*states(2) + states(3)*states(3));
        if (quatMag > 1e-12)
		{
			for (uint8_t index= 0; i<=3; i++)
			{
				float quatMagInv = 1.0/quatMag;
				states[index] = states[index] * quatMagInv;
			}
        }
        // correct the covariance P = (I - K*H)*P
        // take advantage of the empty columns in KH to reduce the
        // number of operations
		for (uint8_t rowIndex = 0; i<=23; i++)
		{
			for (uint8_t colIndex = 0; i<=3; i++)
			{
				KH[rowIndex][colIndex] = K_MAG[rowIndex] * H_MAG[colIndex];
			}
			for (uint8_t colIndex = 18; i<=23; i++)
			{
				KH[rowIndex][colIndex] = K_MAG[rowIndex] * H_MAG[colIndex];
			}
		}
        for (uint8_t rowIndex = 0; i<=23; i++)
		{
            for (uint8_t colIndex = 0; i<=23; i++)
			{
                for (uint8_t index = 0; i<=3; i++)
				{
                    KHP[rowIndex][colIndex] = KHP[rowIndex][colIndex] + KH[rowIndex][colIndex] * _P[index][colIndex];
                }
                for (uint8_t index = 18; i<=23; i++)
				{
                    KHP[rowIndex][colIndex] = KHP[rowIndex][colIndex] + KH[rowIndex][colIndex] * _P[index][colIndex];
                }
            }
        }
		for (uint8_t rowIndex = 0; i<=23; i++)
		{
			for (uint8_t colIndex = 0; i<=23; i++)
			{
				_P[rowIndex][colIndex] = _P[rowIndex][colIndex] - KHP[rowIndex][colIndex];
			}
		}
		// Force symmetry on the covariance matrix to prevent ill-conditioning
		// of the matrix which would cause the filter to blow-up
		for (uint8_t rowIndex = 1; i<=23; i++)
		{
			for (uint8_t colIndex = rowIndex-1; i<=22; i++)
			{
				float temp = 0.5*(_P[rowIndex][colIndex] + _P[colIndex][rowIndex]);
				_P[rowIndex][colIndex] = temp;
				_P[colindex][rowIndex] = temp;
			}
	}
	obsIndex = obsIndex + 1;
}
}