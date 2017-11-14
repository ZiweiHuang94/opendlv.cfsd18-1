/**
* Copyright (C) 2017 Chalmers Revere
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
* USA.
*/

#include <iostream>

#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/strings/StringToolbox.h>
#include <opendavinci/odcore/wrapper/Eigen.h>
#include <opendavinci/generated/odcore/data/CompactPointCloud.h>
#include <opendavinci/odcore/base/Lock.h>
#include "attention.hpp"


namespace opendlv {
namespace logic {
namespace cfsd18 {
namespace sensation {

using namespace std;
using namespace odcore::base;
using namespace odcore::data;
using namespace odcore::wrapper;

Attention::Attention(int32_t const &a_argc, char **a_argv) :
  DataTriggeredConferenceClientModule(a_argc, a_argv, "logic-cfsd18-sensation-attention")
  , m_12_startingSensorID_32(0)//The first HDL-32E CPC starts from Layer 0
  , m_11_startingSensorID_32(2)//The second HDL-32E CPC starts from Layer 2
  , m_9_startingSensorID_32(5)//The third HDL-32E CPC starts from Layer 5
  , m_12_verticalAngles()
  , m_11_verticalAngles()
  , m_9_verticalAngles()
  , m_12_cpcDistance_32("")
  , m_11_cpcDistance_32("")
  , m_9_cpcDistance_32("")
  , m_previousCPC32TimeStamp(0)
  , m_cpcMask_32(0)
  , m_cpc()
  , m_cpcMutex()
  , m_SPCReceived(false)
  , m_CPCReceived(false)
  , m_recordingYear()
{
  //#############################################################################
  // Following part are prepared to decode CPC of HDL32 3 parts
  //#############################################################################
  std::array<float, 32>  sensorIDs_32;
  bool use32IncrementA = true;
  sensorIDs_32[0] = START_V_ANGLE_32;
  //Derive the 32 vertical angles for HDL-32E based on the starting angle and the two increments
  for (uint8_t counter = 1; counter < 31; counter++) {
    sensorIDs_32[counter] += use32IncrementA ?  V_INCREMENT_32_A : V_INCREMENT_32_B;
    use32IncrementA = !use32IncrementA;
  }
  //Derive the 12 vertical angles associated with the first part of CPC for HDL-32E
  m_12_verticalAngles[0] = sensorIDs_32[m_12_startingSensorID_32];
  uint8_t currentSensorID = m_12_startingSensorID_32 + 1;
  m_12_verticalAngles[1] = sensorIDs_32[currentSensorID];
  for (uint8_t counter = 2; counter < 11; counter++) {
      m_12_verticalAngles[counter] = sensorIDs_32[currentSensorID + 3];    
  }
  //Derive the 11 vertical angles associated with the second part of CPC for HDL-32E
  m_11_verticalAngles[0] = sensorIDs_32[m_11_startingSensorID_32];
  currentSensorID = m_11_startingSensorID_32 + 1;
  m_11_verticalAngles[1] = sensorIDs_32[currentSensorID];
  for (uint8_t counter = 2; counter < 10; counter++) {
      m_11_verticalAngles[counter] = sensorIDs_32[currentSensorID + 3];    
  }
  //Derive the 9 vertical angles associated with the third part of CPC for HDL-32E
  m_9_verticalAngles[0] = sensorIDs_32[m_9_startingSensorID_32];
  currentSensorID = m_9_startingSensorID_32 + 1;
  m_9_verticalAngles[1] = sensorIDs_32[currentSensorID];
  for (uint8_t counter = 2; counter < 8; counter++) {
      m_9_verticalAngles[counter] = sensorIDs_32[currentSensorID + 3];    
  }
  //###########################################################################
}

Attention::~Attention()
{
}



void Attention::nextContainer(odcore::data::Container &a_container)
{
  if(a_container.getDataType() == odcore::data::SharedPointCloud::ID()){
    m_SPCReceived = true;
    cout << "Error: Don't use SharedPointCloud here!!!" << endl;
    /*
    m_velodyneFrame = c.getData<SharedPointCloud>();//Get shared point cloud
    if (!m_hasAttachedToSharedImageMemory) {
      m_velodyneSharedMemory=SharedMemoryFactory::attachToSharedMemory(m_velodyneFrame.getName()); // Attach the shared point cloud to the shared memory.
      m_hasAttachedToSharedImageMemory = true; 
    }  */
}
  if (a_container.getDataType() == odcore::data::CompactPointCloud::ID()) {
    m_CPCReceived = true;
    TimeStamp ts = a_container.getSampleTimeStamp();
    m_recordingYear = ts.getYear();

    // auto kinematicState = a_container.getData<opendlv::coord::KinematicState>();
    /*  
      opendlv::logic::sensation::Attention o1;
      odcore::data::Container c1(o1);
      getConference().send(c1);
    */
    if (m_CPCReceived && !m_SPCReceived) {
      Lock lockCPC(m_cpcMutex);
      m_cpc = a_container.getData<CompactPointCloud>(); 
      const float startAzimuth = m_cpc.getStartAzimuth();
      const float endAzimuth = m_cpc.getEndAzimuth();
      const uint8_t numberOfLayers = m_cpc.getEntriesPerAzimuth();  // Get number of layers

         
      const uint8_t numberOfBitsForIntensity = m_cpc.getNumberOfBitsForIntensity();
      
      /*const uint8_t intensityPlacement = m_cpc.getIntensityPlacement();

      uint16_t tmpMask = 0xFFFF;
      float intensityMaxValue = 0.0f;
      if (numberOfBitsForIntensity > 0) {
        if (intensityPlacement == 0) {
          tmpMask = tmpMask >> numberOfBitsForIntensity; //higher bits for intensity
        } 
        else {
          tmpMask = tmpMask << numberOfBitsForIntensity; //lower bits for intensity
        }
        intensityMaxValue = pow(2.0f, static_cast<float>(numberOfBitsForIntensity)) - 1.0f;
      }
      */
      const uint8_t distanceEncoding = m_cpc.getDistanceEncoding();  
      
      if (numberOfLayers == 16) {  // Deal with VLP-16
        cout << "Connecting to VLP16" <<  endl;
        cout << "Start Azimuth angle: " << startAzimuth << "End Azimuth angle: " << endAzimuth << endl;

      }
      else {  // Deal with HDL-32
        cout << "Connecting to HDL-32" <<  endl;

        const uint64_t currentTime = ts.toMicroseconds();
        //Check if this HDL-32E CPC comes from a new scan. The interval between two scans is roughly 100ms. It is safe to assume that a new CPC comes from a new scan if the interval is longer than 50ms
        const uint64_t deltaTime = (currentTime > m_previousCPC32TimeStamp) ? (currentTime - m_previousCPC32TimeStamp) : (m_previousCPC32TimeStamp - currentTime);
        if (deltaTime > 50000) {
          m_cpcMask_32 = 0;//Reset the mask that represents which HDL-32E CPC part has arrived
        }
        m_previousCPC32TimeStamp = currentTime;
        if (numberOfLayers == 12) {
          m_cpcMask_32 = m_cpcMask_32 | 0x04;
          m_12_cpcDistance_32 = m_cpc.getDistances();
        }
        if (numberOfLayers == 11) {
          m_cpcMask_32 = m_cpcMask_32 | 0x02;
          m_11_cpcDistance_32 = m_cpc.getDistances();
        }
        if (numberOfLayers == 9) {
          m_cpcMask_32 = m_cpcMask_32 | 0x01;
          m_9_cpcDistance_32 = m_cpc.getDistances();
        }
      

        if ((m_cpcMask_32 & 0x04) > 0) {//The first part, 12 layers
          if (numberOfBitsForIntensity == 0) {
            cout << "The first part, 12 layers, no Intensity" <<  endl;
            SaveCPC32NoIntensity(1, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding);
          } 
          else {
            cout << "The first part, 12 layers, with Intensity" <<  endl;
            //drawCPC32withIntensity(1, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding, numberOfBitsForIntensity, intensityPlacement, tmpMask, intensityMaxValue);
          }
        }
        if ((m_cpcMask_32 & 0x02) > 0) {//The second part, 11 layers
          if (numberOfBitsForIntensity == 0) {
            cout << "The second part, 11 layers, no Intensity" <<  endl;
            SaveCPC32NoIntensity(2, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding);
          } 
          else {
            cout << "The second part, 11 layers, with Intensity" <<  endl;
            //drawCPC32withIntensity(2, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding, numberOfBitsForIntensity, intensityPlacement, tmpMask, intensityMaxValue);
          }
        }
        if ((m_cpcMask_32 & 0x01) > 0) {//The third part, 9 layers
          if (numberOfBitsForIntensity == 0) {
            cout << "The third part, 9 layers, no Intensity" << endl;
            SaveCPC32NoIntensity(3, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding);
          } 
          else {
            cout << "The third part, 9 layers, with Intensity" << endl;
            //drawCPC32withIntensity(3, numberOfLayers, startAzimuth, endAzimuth, distanceEncoding, numberOfBitsForIntensity, intensityPlacement, tmpMask, intensityMaxValue);
          }
        }
      }
    }

  }
}

void Attention::setUp()
{
  // std::string const exampleConfig = 
  //   getKeyValueConfiguration().getValue<std::string>(
  //     "logic-cfsd18-sensation-attention.example-config");

  // if (isVerbose()) {
  //   std::cout << "Example config is " << exampleConfig << std::endl;
  // }
}

void Attention::tearDown()
{
}

void Attention::SaveCPC32NoIntensity(const uint8_t &part, const uint8_t &entriesPerAzimuth, const float &startAzimuth, const float &endAzimuth, const uint8_t &distanceEncoding)
{
  float azimuth = startAzimuth;
  uint32_t numberOfPoints;
  stringstream sstr;
  if (part == 1) {
      numberOfPoints = m_12_cpcDistance_32.size() / 2;
      sstr.str(m_12_cpcDistance_32);
  } else if (part == 2) {
      numberOfPoints = m_11_cpcDistance_32.size() / 2;
      sstr.str(m_11_cpcDistance_32);
  } else {
      numberOfPoints = m_9_cpcDistance_32.size() / 2;
      sstr.str(m_9_cpcDistance_32);
  }
  uint32_t numberOfAzimuths = numberOfPoints / entriesPerAzimuth;
  float azimuthIncrement = (endAzimuth - startAzimuth) / numberOfAzimuths;//Calculate the azimuth increment
  uint16_t distance = 0;
  
  opendlv::coord::Position pointPosition;
  for (uint32_t azimuthIndex = 0; azimuthIndex < numberOfAzimuths; azimuthIndex++) {
    for (uint8_t sensorIndex = 0; sensorIndex < entriesPerAzimuth; sensorIndex++) {
      sstr.read((char*)(&distance), 2); // Read distance value from the string in a CPC container point by point
      if (part == 1) {
          pointPosition = SaveOneCPCPointNoIntensity(distance, azimuth, m_12_verticalAngles[sensorIndex], distanceEncoding);
      } else if (part == 2) {
          pointPosition = SaveOneCPCPointNoIntensity(distance, azimuth, m_11_verticalAngles[sensorIndex], distanceEncoding);
      } else {
          pointPosition = SaveOneCPCPointNoIntensity(distance, azimuth, m_9_verticalAngles[sensorIndex], distanceEncoding);
      }
    }
    azimuth += azimuthIncrement;
  }

  //return pointPosition;
}
//void SaveCPC32WithIntensity(const uint8_t &part, const uint8_t &entriesPerAzimuth, const float &startAzimuth, const float &endAzimuth, const uint8_t &distanceEncoding, const uint8_t &numberOfBitsForIntensity, const uint8_t &intensityPlacement, const uint16_t &mask, const float &intensityMaxValue)
//{
  //opendlv::coord::Position pointPosition;
  //return pointPosition;
//}

opendlv::coord::Position Attention::SaveOneCPCPointNoIntensity(const uint16_t &, const float &, const float &, const uint8_t &)
{

  opendlv::coord::Position pointPosition;
  return pointPosition;

}

}
}
}
}
