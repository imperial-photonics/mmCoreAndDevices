#include "openIncubate.h"

///////////////////////////////////////////////////////////////////////////////
// FILE:          openIncubate.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controller for openIncubate
// COPYRIGHT:     Imperial College London, 2025
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

#include <windows.h>
#include <iostream>
#include <vector>
#include "openIncubate.h"

/// JUST IN CASE
#include <cstdio>
#include <string>
#include <math.h>
#include "../../MMDevice/ModuleInterface.h"

using namespace std;

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_openIncubateDeviceName, MM::GenericDevice, "openIncubate temperature controller");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_openIncubateDeviceName) == 0)
	{
		return new openIncubate();
	}

	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

/////////////////////////////////////////////////////////
//For the incubator
/////////////////////////////////////////////////////////

openIncubate::openIncubate() : CGenericBase<openIncubate>(),
port_("Undefined")
{
	// From some uM base functions included?
	InitializeDefaultErrorMessages();
	//This property action should turn up in the Hardware Configuration Wizard after adding
	//Need to define port on initialisation?
	CPropertyAction* pAct = new CPropertyAction(this, &openIncubate::On_Port);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}


openIncubate::~openIncubate()
{
}

void openIncubate::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_openIncubateDeviceName);
}

int openIncubate::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_openIncubateDeviceName, MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// Description
	ret = CreateProperty(MM::g_Keyword_Description, "openIncubate temperature controller", MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	//Adding options exposed to the MM GUI here:
	ret = CreateFloatProperty(g_PropName_Setpoint, 37.0, false, new CPropertyAction(this, &openIncubate::On_Setpoint), false);
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_Setpoint, 10.0, 50.0);

	ret = CreateStringProperty(g_PropName_Debug, "Idle", false, new CPropertyAction(this, &openIncubate::On_Reset_Warnings),false);
	if (DEVICE_OK != ret)
		return ret;
	AddAllowedValue(g_PropName_Debug,"Reset warnings");

	//ret = CreateStringProperty(g_PropName_Debug, "Off", false, new CPropertyAction(this, &openIncubate::On_Debug_Toggle),false);
	//if (DEVICE_OK != ret)
	//	return ret;
	//AddAllowedValue(g_PropName_Debug,"On");

	changedTime_ = GetCurrentMMTime();
	initialized_ = true;
	return DEVICE_OK;
}

int openIncubate::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return DEVICE_OK;
}

bool openIncubate::Busy()
{
	if ((GetCurrentMMTime() - changedTime_).getMsec() < command_wait_time) {
		return true;
	}
	else {
		return false;
	}
}

//////////////////////////////////////////////////////////
// Action handlers for QKPP
//////////////////////////////////////////////////////////

//Set serial port to be used - call before initialisation
int openIncubate::On_Port(MM::PropertyBase* pProp, MM::ActionType eAct)
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
		LogMessage("Serial port set for openIncubate", true);
	}
	return DEVICE_OK;
}

int openIncubate::On_Setpoint(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		LogMessage("openIncubate setpoint changed", false);
		pProp->Set(setpoint_);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		double val_toset;
		pProp->Get(val_toset);	
		Sleep(10);
		int ret = Set_Setpoint(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		setpoint_ = val_toset;
	}
	return DEVICE_OK;
}

int openIncubate::On_Reset_Warnings(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		LogMessage("openIncubate Reset", false);
		pProp->Set("Idle");
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		string val_toset;
		pProp->Get(val_toset);
		Sleep(10);
		int ret = Reset_Warnings();
		if (ret != DEVICE_OK) {
			return ret;
		}
	}
	return DEVICE_OK;
}

//int openIncubate::On_Debug_Toggle(MM::PropertyBase* pProp, MM::ActionType eAct)
//{
//	if (eAct == MM::BeforeGet) {
//		LogMessage("openIncubate debug indicator toggled", true);
//		 = "On";
//		if (Debug_indicator_ == 0) {
//			val = "Off";
//		}
//		pProp->Set(val);
//		// nothing to do, let the caller use cached property
//	}
//	else if (eAct == MM::AfterSet)
//	{
//		// Set timer for the Busy signal
//		changedTime_ = GetCurrentMMTime();
//		//No ints, just longs
//		string val_toset;
//		pProp->Get(val_toset);
//		Sleep(10);
//		Debug_indicator_ = val_toset;
//	}
//	return DEVICE_OK;
//}

/////////////////////////////////////////////////////////////////////
// Internal functions for the driver
/////////////////////////////////////////////////////////////////////
int openIncubate::Set_Setpoint(double new_setpoint) {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	snprintf(buf2, bufSize, "%f !S", new_setpoint);
	int ret = ExecuteCommand(buf2, answer);

	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

int openIncubate::Reset_Warnings() {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	snprintf(buf2, bufSize, "%d !R",0);
	int ret = ExecuteCommand(buf2, answer);

	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////////
int openIncubate::ExecuteCommand(const std::string& cmd, std::string& answer)
{
	//THIS BIT ALL TAKEN FROM THE SC10 one, including most comments
	//Send command, add return onto end
	//SOMETHING HERE IS NOT WORKING QUITE RIGHT - MORE DIAGNOSTICS NEEDED!
	int ret = 0;
	PurgeComPort(port_.c_str());
	if (DEVICE_OK != SendSerialCommand(port_.c_str(), cmd.c_str(), "\r\n")) {
		LogMessage("Purge command failed for openIncubate", false);
		return DEVICE_SERIAL_COMMAND_FAILED;
	}
	// Get answer from the device.  It will always end with a 'ok\r\n' if it worked
	ret = GetSerialAnswer(port_.c_str(), "ok\r\n", answer);
	if (ret != DEVICE_OK)
		LogMessage("Send command failed for openIncubate", false);
	return ret;
	// Check if the command was echoed and strip it out - think this should still apply for character-by-character echo?
	if (answer.compare(0, cmd.length(), cmd) == 0) {
		answer = answer.substr(cmd.length() + 1);
		// These 2 comments below are copied from the Thorlabs SC10 adapter. Only have a PC to test on here.
		// I really don't understand this, but on the Mac I get a space as the first character
		// This could be a problem in the serial adapter!
	}
	else if (answer.compare(1, cmd.length(), cmd) == 0) {
		answer = answer.substr(cmd.length() + 2);
	}
	else {
		return DEVICE_SERIAL_COMMAND_FAILED;
		LogMessage("Unexpected return string from openIncubate", true);
	}
	//If it all went well...
	return DEVICE_OK;
}