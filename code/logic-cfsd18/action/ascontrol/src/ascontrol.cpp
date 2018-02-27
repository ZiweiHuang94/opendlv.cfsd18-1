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
#include <math.h>
#include <chrono>

#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/strings/StringToolbox.h>
#include <opendavinci/odcore/wrapper/Eigen.h>

#include "ascontrol.hpp"

namespace opendlv {
namespace logic {
namespace cfsd18 {
namespace action {


Ascontrol::Ascontrol(int32_t const &a_argc, char **a_argv) :
  TimeTriggeredConferenceClientModule(a_argc, a_argv, "logic-cfsd18-action-ascontrol"),
  m_pwmIdRed(),
  m_pwmIdGreen(),
  m_pwmIdBlue()
{
}

Ascontrol::~Ascontrol()
{
}

void Ascontrol::nextContainer(odcore::data::Container &a_container)
{
  if (a_container.getDataType() == opendlv::system::SignalStatusMessage::ID()) {
    // auto kinematicState = a_container.getData<opendlv::coord::KinematicState>();



  }
}

odcore::data::dmcp::ModuleExitCodeMessage::ModuleExitCode Ascontrol::body()
{
  while (getModuleStateAndWaitForRemainingTimeInTimeslice() ==
      odcore::data::dmcp::ModuleStateMessage::RUNNING) {
          std::cout << "Ascontrol update" << std::endl;

          opendlv::system::SystemOperationState ASstatusMessage(1,"OKAY");
          odcore::data::Container output(ASstatusMessage);

          getConference().send(output);
          opendlv::proxy::GroundDecelerationRequest test;
          odcore::data::Container outTest(test);
          getConference().send(outTest);

    /*      auto time = std::chrono::duration_cast< std::chrono::milliseconds >(
    std::chrono::system_clock::now().time_since_epoch());
*/
          float headingRequest =  0.1f; //*sin ((float)time);
          opendlv::logic::action::AimPoint aimpoint(headingRequest,0.0f,5.0f);
          odcore::data::Container c2(aimpoint);
          getConference().send(c2);

          float accelerationRequest = 2.0f;
          opendlv::proxy::GroundAccelerationRequest accReq(accelerationRequest);
          odcore::data::Container c3(accReq);
          getConference().send(c3);


  }


// Comment this out until PwmRequest has been added in our project

  opendlv::proxy::PulseWidthModulationRequest LED_R;
  opendlv::proxy::PulseWidthModulationRequest LED_G;
  opendlv::proxy::PulseWidthModulationRequest LED_B;


  //if (true) // Add logic to decide which colour to send
  //{
    int32_t R = 50; // Assign the percentage of each colour (0-100)
    int32_t G = 50; // These values should be set in a config file
    int32_t B = 50;
    int32_t tot = fmax(B,fmax(R,G));
  //}

  LED_R.setDutyCycleNs(R/tot*100);  // The fractions are normalized to make brightness more consistent
  LED_G.setDutyCycleNs(G/tot*100);
  LED_B.setDutyCycleNs(B/tot*100);

  odcore::data::Container red(LED_R);
  odcore::data::Container green(LED_G);
  odcore::data::Container blue(LED_B);

  red.setSenderStamp(m_pwmIdRed);
  green.setSenderStamp(m_pwmIdGreen);
  blue.setSenderStamp(m_pwmIdBlue);

  // It will be necessary to add some if statement controlling the frequency of the light to match rules

  getConference().send(red);
  getConference().send(green);
  getConference().send(blue);


  return odcore::data::dmcp::ModuleExitCodeMessage::OKAY;
}

void Ascontrol::setUp()
{
  auto kv = getKeyValueConfiguration();

  m_pwmIdRed = kv.getValue<uint32_t>("global.sender-stamp.red");
  m_pwmIdGreen = kv.getValue<uint32_t>("global.sender-stamp.green");
  m_pwmIdBlue = kv.getValue<uint32_t>("global.sender-stamp.blue");
}

void Ascontrol::tearDown()
{
}

}
}
}
}