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
#include <cstdlib>

#include <opendavinci/odcore/data/TimeStamp.h>
#include <opendavinci/odcore/strings/StringToolbox.h>
#include <opendavinci/odcore/wrapper/Eigen.h>


#include "slipestimate.hpp"

namespace opendlv {
namespace logic {
namespace cfsd18 {
namespace action {

using namespace std;
using namespace odcore::base;

Slipestimate::Slipestimate(int32_t const &a_argc, char **a_argv) :
  DataTriggeredConferenceClientModule(a_argc, a_argv, "logic-cfsd18-action-slipestimate"),
  m_pwmId(),
  m_stateID()
{
}

Slipestimate::~Slipestimate()
{
}




void Slipestimate::nextContainer(odcore::data::Container &a_container)
{
  if (a_container.getDataType() == opendlv::proxy::GroundDecelerationRequest::ID()) {
     auto deceleration = a_container.getData<opendlv::proxy::GroundDecelerationRequest>();
     float pwm = 3.5f * deceleration.getGroundDeceleration();
     uint32_t pwmrequest = static_cast<uint32_t>(pwmrequest);


     opendlv::proxy::PulseWidthModulationRequest pr(pwmrequest);
     odcore::data::Container c1(pr);
     c1.setSenderStamp(m_pwmId);
     getConference().send(c1);

      opendlv::proxy::SwitchStateRequest state;
      if (pwm < 0) {
        state.setState(1);
       } else {
        state.setState(0);
      }
      odcore::data::Container c2(state);
      c2.setSenderStamp(m_stateID);
      getConference().send(c2);
  }
}

void Slipestimate::setUp()
{
  std::cout << "Slipestimate setup" << std::endl;
  auto kv = getKeyValueConfiguration();
  m_pwmId = kv.getValue<uint32_t>("global.sender-stamp.brake");
  m_stateID = kv.getValue<uint32_t>("global.sender-stamp.state");
}

void Slipestimate::tearDown()
{
}

}
}
}
}