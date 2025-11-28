///////////////////////////////////////////////////////////////////////////////
// FILE:          Bartels_QuadKey.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controller for Bartels QuadKey with mp6-pp pumps
// COPYRIGHT:     Imperial College London, 2018
// LICENSE:       GPL v3.0
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//
// AUTHOR:        Sunil Kumar, sunil.kumar@imperial.ac.uk
//
//				  Usually do LabVIEW, so there's probably a fair bit of
//				  rather terrible stuff in here. Sorry. Lots of learning
//				  done from the Thorlabs SC10 and VTI device adapters.


#include <windows.h>
#include "Bartels_QuadKey.h"
/// JUST IN CASE
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"
#include <sstream>
using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_Bartels_QuadKeyDeviceName, MM::GenericDevice, "Arduino pump controller");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_Bartels_QuadKeyDeviceName) == 0)
   {
      return new Bartels_QuadKey();
   }

   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

/////////////////////////////////////////////////////////
//For the pump
/////////////////////////////////////////////////////////

Bartels_QuadKey::Bartels_QuadKey() : 
CGenericBase<Bartels_QuadKey> (),
	numpumps_(6),
	initialized_(false),
	port_("Undefined"),
	active_(false),
	current_pump(0),
	V_val_1_(0.0),
	V_val_2_(0.0),
	V_val_3_(0.0),
	V_val_4_(0.0),
	T_val_1_(0.0),
	T_val_2_(0.0),
	T_val_3_(0.0),
	T_val_4_(0.0),
	Continuous_on_(false),
	answerTimeoutMs_(1000),
	which_pump_now_(1),
	whichpumps_(0),
	changedTime_(0)
{
	// From some uM base functions included?
	InitializeDefaultErrorMessages();

	//These property actions should turn up in the Hardware Configuration Wizard after adding
	//Need to define one exemplar	first?
	CPropertyAction * pAct = new CPropertyAction (this, &Bartels_QuadKey::OnPort);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);

}

Bartels_QuadKey::~Bartels_QuadKey(void)
{
}

void Bartels_QuadKey::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_Bartels_QuadKeyDeviceName);
}

int Bartels_QuadKey::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

    // Name
    int ret = CreateProperty(MM::g_Keyword_Name, g_Bartels_QuadKeyDeviceName, MM::String, true);
    if (DEVICE_OK != ret)
        return ret;
    
    // Description
    ret = CreateProperty(MM::g_Keyword_Description, "Arduino Bartels pump controller", MM::String, true);
    if (DEVICE_OK != ret)
        return ret;

//Adding stuff to the MM GUI here 
    ret = CreateIntegerProperty(MM::g_Keyword_State, 0, false, new CPropertyAction(this, &Bartels_QuadKey::OnState));
    if (DEVICE_OK != ret)
        return ret;
//   ret = SetPropertyLimits(MM::g_Keyword_State, 0, 1);
//   if (DEVICE_OK != ret)
//        return ret;
    AddAllowedValue(MM::g_Keyword_State, "0"); // Stopped
    AddAllowedValue(MM::g_Keyword_State, "1"); // Active

	ret = CreateFloatProperty(g_PropName_Voltage_1, 0.0, false, new CPropertyAction(this, &Bartels_QuadKey::OnVoltage1));
    if (DEVICE_OK != ret)
        return ret;
	SetPropertyLimits(g_PropName_Voltage_1, 0.0, 250.0);

	ret = CreateFloatProperty(g_PropName_Voltage_2, 0.0, false, new CPropertyAction(this, &Bartels_QuadKey::OnVoltage2));
    if (DEVICE_OK != ret)
        return ret;
	SetPropertyLimits(g_PropName_Voltage_2, 0.0, 250.0);

	ret = CreateFloatProperty(g_PropName_Voltage_3, 0.0, false, new CPropertyAction(this, &Bartels_QuadKey::OnVoltage3));
    if (DEVICE_OK != ret)
        return ret;
	SetPropertyLimits(g_PropName_Voltage_3, 0.0, 250.0);

	ret = CreateFloatProperty(g_PropName_Voltage_4, 0.0, false, new CPropertyAction(this, &Bartels_QuadKey::OnVoltage4));
    if (DEVICE_OK != ret)
        return ret;
	SetPropertyLimits(g_PropName_Voltage_4, 0.0, 250.0);

	changedTime_ = GetCurrentMMTime();
 	initialized_ = true;
	return DEVICE_OK;
}

int Bartels_QuadKey::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return DEVICE_OK;
}

bool Bartels_QuadKey::Busy()
{
	//HA HA Blatant lies for now!
    return false;
}


//////////////////////////////////////////////////////////
// Action handlers for pump
//////////////////////////////////////////////////////////

//Set serial port to be used - call before initialisation
int Bartels_QuadKey::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(port_.c_str());
	}
	else if (eAct == MM::AfterSet)
	{
		if (initialized_)
		{
			// revert
			pProp->Set(port_.c_str());
			return ERR_PORT_CHANGE_FORBIDDEN; 
		}

		pProp->Get(port_);
	}

	return DEVICE_OK;
}

int Bartels_QuadKey::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Getting selected pump info\n");
      pProp->Set(whichpumps_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();

      long selpumps;
      pProp->Get(selpumps);
      printf("Setting active pumps: %ld\n", whichpumps_);
      if (selpumps >= 4 || selpumps < 0)
      {
         pProp->Set(whichpumps_); // revert
         return ERR_THIS_PUMP_DOESNT_EXIST;
      }
	  //const int bufSize = 20;
      //char buf[bufSize];
      //snprintf(buf, bufSize, "pos=%ld", pos + 1);
	   
	  //SendSerialCommand(port_.c_str(),buf,"\r");
      whichpumps_ = selpumps;
   }

   return DEVICE_OK;
}

int Bartels_QuadKey::OnVoltage1(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Setting voltages\n");
	  pProp->Set(V_val_1_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();
      double V_toset;
      pProp->Get(V_toset);

	  int V_toset_int = (int) V_toset;
	  std::string answer;
	  //Stupid C++ and its stupid string handling
	  //See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	  int ret = ExecuteCommand("1 !P", answer);
      if (ret != DEVICE_OK){
         return ret;
	  }
      char buf2[bufSize];
      snprintf(buf2, bufSize, "%ld !V", V_toset_int);

      ret = ExecuteCommand(buf2, answer);
      if (ret != DEVICE_OK){
         return ret;
	  }

	  V_val_1_ = V_toset;
   }

   return DEVICE_OK;
}

int Bartels_QuadKey::OnVoltage2(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Setting voltages\n");
	  pProp->Set(V_val_2_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();
      double V_toset;
      pProp->Get(V_toset);

	  int V_toset_int = (int) V_toset;
	  std::string answer;
	  //Stupid C++ and its stupid string handling
	  //See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	  int ret = ExecuteCommand("2 !P", answer);
      if (ret != DEVICE_OK){
         return ret;
	  }
      char buf2[bufSize];
      snprintf(buf2, bufSize, "%ld !V", V_toset_int);

      ret = ExecuteCommand(buf2, answer);
      if (ret != DEVICE_OK){
         return ret;
	  }

	  V_val_2_ = V_toset;
   }

   return DEVICE_OK;
}

int Bartels_QuadKey::OnVoltage3(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Setting voltages\n");
	  pProp->Set(V_val_3_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();
      double V_toset;
      pProp->Get(V_toset);

	  int V_toset_int = (int) V_toset;
	  std::string answer;
	  //Stupid C++ and its stupid string handling
	  //See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	  int ret = ExecuteCommand("3 !P", answer);
      if (ret != DEVICE_OK){
         return ret;
	  }
      char buf2[bufSize];
      snprintf(buf2, bufSize, "%ld !V", V_toset_int);

      ret = ExecuteCommand(buf2, answer);
      if (ret != DEVICE_OK){
         return ret;
	  }

	  V_val_3_ = V_toset;
   }

   return DEVICE_OK;
}

int Bartels_QuadKey::OnVoltage4(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      printf("Setting voltages\n");
	  pProp->Set(V_val_4_);
      // nothing to do, let the caller to use cached property
   }
   else if (eAct == MM::AfterSet)
   {
      // Set timer for the Busy signal
      changedTime_ = GetCurrentMMTime();
      double V_toset;
      pProp->Get(V_toset);

	  int V_toset_int = (int) V_toset;
	  std::string answer;
	  //Stupid C++ and its stupid string handling
	  //See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	  int ret = ExecuteCommand("4 !P", answer);
      if (ret != DEVICE_OK){
         return ret;
	  }
      //char buf2[bufSize];
      //snprintf(buf2, bufSize, "%ld !V", V_toset_int);
	  //char buf2 = numspace_concat(V_toset_int, "%ld !V")
      ret = ExecuteCommand(buf2, answer);
      if (ret != DEVICE_OK){
         return ret;
	  }

	  V_val_4_ = V_toset;
   }

   return DEVICE_OK;
}

////////////////////////////////
char numspace_concat(int num, std::string str){
      char buf2[bufSize];
	  std::string command = "%ld ";
	  // Again, stupid C string stuff - need .c_str() here to convert to const char *...
	  snprintf(buf2, bufSize, command.c_str(), num);
	  return buf2;
}

////////////////////////////////

int Bartels_QuadKey::ExecuteCommand(const std::string& cmd, std::string& answer)
{
	//THIS BIT ALL TAKEN FROM THE SC10 one, including most comments
	//Send command, add return onto end
	PurgeComPort(port_.c_str());
	if (DEVICE_OK != SendSerialCommand(port_.c_str(), cmd.c_str(), "\r"))
		return DEVICE_SERIAL_COMMAND_FAILED;
	return DEVICE_OK;
	// Get answer from the device.  It will always end with a new prompt - REMOVED THE NEW PROMPT PART (WAS ">") - now suitable for returning 'ok\r\n'
	int ret = GetSerialAnswer(port_.c_str(), "ok\r\n", answer);
	if (ret != DEVICE_OK)
		return ret;
	// Check if the command was echoed and strip it out
	if (answer.compare(0, cmd.length(), cmd) == 0) {
		answer = answer.substr(cmd.length() + 1);
	// I really don't understand this, but on the Mac I get a space as the first character
	// This could be a problem in the serial adapter!
	} else if (answer.compare(1, cmd.length(), cmd) == 0) {
		answer = answer.substr(cmd.length() + 2);
	} else
		return DEVICE_SERIAL_COMMAND_FAILED;
	//If it all went well...
	return DEVICE_OK;
}
