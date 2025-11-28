#include "QKPP.h"

///////////////////////////////////////////////////////////////////////////////
// FILE:          QKPP.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Controller for Bartels QuadKey piezo pump
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
#include <iostream>
#include <vector>
#include "QKPP.h"

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
	RegisterDevice(g_QKPPDeviceName, MM::GenericDevice, "Bartels piezo pump controller - Model: QuadKey");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
	if (deviceName == 0)
		return 0;

	if (strcmp(deviceName, g_QKPPDeviceName) == 0)
	{
		return new QKPP();
	}

	return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
	delete pDevice;
}

/////////////////////////////////////////////////////////
//For the piezo pump
/////////////////////////////////////////////////////////


QKPP::QKPP() : CGenericBase<QKPP>(),
	port_("Undefined"),
	Dispense_state_ (0)
{
	// From some uM base functions included?
	InitializeDefaultErrorMessages();
	//This property action should turn up in the Hardware Configuration Wizard after adding
	//Need to define port on initialisation?
	CPropertyAction * pAct = new CPropertyAction(this, &QKPP::On_Port);
	CreateProperty(MM::g_Keyword_Port, "Undefined", MM::String, false, pAct, true);
}


QKPP::~QKPP()
{
}

void QKPP::GetName(char* name) const
{
	// Return the name used to refer to this device adapter
	CDeviceUtils::CopyLimitedString(name, g_QKPPDeviceName);
}

int QKPP::Initialize()
{
	if (initialized_)
		return DEVICE_OK;

	//Set the stored voltages and runtimes to defaults THAT SHOULD BE in the arrays in the device adapter...
	voltages.push_back(250);
	voltages.push_back(250);
	voltages.push_back(250);
	voltages.push_back(250);
	runtimes.push_back(1000);
	runtimes.push_back(1000);
	runtimes.push_back(1000);
	runtimes.push_back(1000);

	// Name
	int ret = CreateProperty(MM::g_Keyword_Name, g_QKPPDeviceName, MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

	// Description
	ret = CreateProperty(MM::g_Keyword_Description, "Bartels piezo pump - QuadKey", MM::String, true);
	if (DEVICE_OK != ret)
		return ret;

//Adding options exposed to the MM GUI here:
	ret = CreateIntegerProperty(g_PropName_T1, 0, false, new CPropertyAction(this, &QKPP::On_T1));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_T1, 0, MAX_PUMPTIME);

	ret = CreateIntegerProperty(g_PropName_T2, 0, false, new CPropertyAction(this, &QKPP::On_T2));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_T2, 0, MAX_PUMPTIME);

	ret = CreateIntegerProperty(g_PropName_T3, 0, false, new CPropertyAction(this, &QKPP::On_T3));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_T3, 0, MAX_PUMPTIME);

	ret = CreateIntegerProperty(g_PropName_T4, 0, false, new CPropertyAction(this, &QKPP::On_T4));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_T4, 0, MAX_PUMPTIME);

	ret = CreateIntegerProperty(g_PropName_V1, 0, false, new CPropertyAction(this, &QKPP::On_V1));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_V1, 0, MAX_VOLTAGE);

	ret = CreateIntegerProperty(g_PropName_V2, 0, false, new CPropertyAction(this, &QKPP::On_V2));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_V2, 0, MAX_VOLTAGE);

	ret = CreateIntegerProperty(g_PropName_V3, 0, false, new CPropertyAction(this, &QKPP::On_V3));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_V3, 0, MAX_VOLTAGE);

	ret = CreateIntegerProperty(g_PropName_V4, 0, false, new CPropertyAction(this, &QKPP::On_V4));
	if (DEVICE_OK != ret)
		return ret;
	ret = SetPropertyLimits(g_PropName_V4, 0, MAX_VOLTAGE);

	ret = CreateStringProperty(g_PropName_Dispensing, "Idle", false, new CPropertyAction(this, &QKPP::On_Dispense));
	if (DEVICE_OK != ret)
		return ret;
	AddAllowedValue(g_PropName_Dispensing, "Abort");
	AddAllowedValue(g_PropName_Dispensing, "Start Pump 1");
	AddAllowedValue(g_PropName_Dispensing, "Start Pump 2");
	AddAllowedValue(g_PropName_Dispensing, "Start Pump 3");
	AddAllowedValue(g_PropName_Dispensing, "Start Pump 4");
	AddAllowedValue(g_PropName_Dispensing, "Idle");

	ret = CreateStringProperty(g_PropName_Continuous, "Timed", false, new CPropertyAction(this, &QKPP::On_Cont));
	if (DEVICE_OK != ret)
		return ret;
	AddAllowedValue(g_PropName_Continuous, "Continuous");
	AddAllowedValue(g_PropName_Continuous, "Timed");

	changedTime_ = GetCurrentMMTime();
	initialized_ = true;
	return DEVICE_OK;
}

int QKPP::Shutdown()
{
	if (initialized_)
	{
		initialized_ = false;
	}
	return DEVICE_OK;
}

bool QKPP::Busy()
{
	if ((GetCurrentMMTime() - changedTime_).getMsec() < command_wait_time) {
		return true;
	}
	else {
		return false;
	}
}

//////////////////////////////////////////////////////////
// Action handlers for delay box
//////////////////////////////////////////////////////////

//Set serial port to be used - call before initialisation
int QKPP::On_Port(MM::PropertyBase* pProp, MM::ActionType eAct)
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
		LogMessage("Serial port set for Piezo pump", true);
	}
	return DEVICE_OK;
}

int QKPP::On_V1(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(voltages[0]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(1);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Voltage(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		voltages[0] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_V2(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(voltages[1]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(2);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Voltage(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		voltages[1] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_V3(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(voltages[2]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(3);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Voltage(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		voltages[2] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_V4(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(voltages[3]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(4);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Voltage(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		voltages[3] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_T1(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(runtimes[0]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(1);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Runtime(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		runtimes[0] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_T2(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(runtimes[1]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(2);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Runtime(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		runtimes[1] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_T3(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(runtimes[2]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(3);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Runtime(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		runtimes[2] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_T4(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet) {
		//LogMessage("QKPP voltage set", true);
		pProp->Set(runtimes[3]);
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//No ints, just longs
		long val_toset;
		pProp->Get(val_toset);
		int ret = Set_Pump(4);
		if (ret != DEVICE_OK) {
			return ret;
		}
		Sleep(10);
		ret = Set_Runtime(val_toset);
		if (ret != DEVICE_OK) {
			return ret;
		}
		runtimes[3] = val_toset;
		//LogMessage("HDG delay set", true);
	}
	return DEVICE_OK;
}

int QKPP::On_Dispense(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		if (Dispense_state_ == 0)
		{
			pProp->Set("Idle");
		}
		else if (Dispense_state_ == 1) {
			pProp->Set("Abort");
		}
		else if (Dispense_state_ == 2) {
			pProp->Set("Start Pump 1");
		}
		else if (Dispense_state_ == 3) {
			pProp->Set("Start Pump 2");
		}
		else if (Dispense_state_ == 4) {
			pProp->Set("Start Pump 3");
		}
		else if (Dispense_state_ == 5) {
			pProp->Set("Start Pump 4");
		}
		else {
			pProp->Set("Idle");//duplication for clarity
		}
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		std::string Dispense;
		//Returns a std::string
		pProp->Get(Dispense);
		bool dispensenow = false;
		//Stupid C++ and its stupid string handling
		//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
		char buf2[bufSize];
		if (Dispense.compare("Abort") == 0)// 0 means a string matches
		{
			snprintf(buf2, bufSize, "*");
			Dispense_state_ = 1;
		} else if (Dispense.compare("Start Pump 1") == 0) {
			snprintf(buf2, bufSize, "1 !P");
			dispensenow = true;
			Dispense_state_ = 2;
		} else if (Dispense.compare("Start Pump 2") == 0) {
			snprintf(buf2, bufSize, "2 !P");
			Dispense_state_ = 3;
			dispensenow = true;
		} else if (Dispense.compare("Start Pump 3") == 0) {
			snprintf(buf2, bufSize, "3 !P");
			Dispense_state_ = 4;
			dispensenow = true;
		} else if (Dispense.compare("Start Pump 4") == 0) {
			snprintf(buf2, bufSize, "4 !P");
			Dispense_state_ = 5;
			dispensenow = true;
		} else {//Idle
			snprintf(buf2, bufSize, "");
			Dispense_state_ = 0;
		}
		std::string answer;
		int ret = ExecuteCommand(buf2, answer);
		if (ret != DEVICE_OK) {
			return ret;
		}
		
		if (dispensenow == true){
			char buf3[bufSize];
			snprintf(buf3, bufSize, "/");
			int ret = ExecuteCommand(buf3, answer);
			if (ret != DEVICE_OK) {
				return ret;
			}
		}
		

		//Default position for diaplay should be idle
		pProp->Set("Idle");
		//LogMessage("Dispense command called", true);
		ret = OnPropertiesChanged();
		Dispense_state_ = 0;
		if (ret != DEVICE_OK)
		{
			return ret;
		}
	}
	return DEVICE_OK;
}

int QKPP::On_Cont(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		//LogMessage("Dispense command called", true);
		if (Continuous_ == true)
		{
			pProp->Set("Continuous");
		}
		else {
			pProp->Set("Timed");
		}
		// nothing to do, let the caller use cached property
	}
	else if (eAct == MM::AfterSet)
	{
		// Set timer for the Busy signal
		changedTime_ = GetCurrentMMTime();
		//Stupid C++ and its stupid string handling
		//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
		char buf2[bufSize];
		snprintf(buf2, bufSize, "!C");
		Continuous_ = !Continuous_;
		
		std::string answer;
		int ret = ExecuteCommand(buf2, answer);
		if (ret != DEVICE_OK) {
			return ret;
		}
		//LogMessage("Continuous mode toggled", true);
	}
	return DEVICE_OK;
}



/////////////////////////////////////////////////////////////////////
// Internal functions for the driver
/////////////////////////////////////////////////////////////////////
int QKPP::Set_Pump(int which_pump) {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	snprintf(buf2, bufSize, "%ld !P", which_pump);
	int ret = ExecuteCommand(buf2, answer);

	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

int QKPP::Set_Voltage(int voltage_to_set) {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	snprintf(buf2, bufSize, "%ld !V", voltage_to_set);
	int ret = ExecuteCommand(buf2, answer);

	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

int QKPP::Set_Runtime(int time_to_set) {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	snprintf(buf2, bufSize, "%ld !T", time_to_set);
	int ret = ExecuteCommand(buf2, answer);

	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

int QKPP::Set_Generic_Property(int value, std::string propertyname) {
	// Set timer for the Busy signal
	changedTime_ = GetCurrentMMTime();
	//No ints, just longs
	//Stupid C++ and its stupid string handling
	//See this: https://stackoverflow.com/questions/191757/how-to-concatenate-a-stdstring-and-an-int
	std::string answer;
	char buf2[bufSize];
	//maybe add a case for setting -ve values, where the value is ignored - could be easy to reuse as generic command send then?
	std::string cmdstring = "%ld !" + propertyname;
	snprintf(buf2, bufSize, cmdstring.c_str(), value);
	int ret = ExecuteCommand(buf2, answer);
	if (ret != DEVICE_OK) {
		return ret;
	}
	return DEVICE_OK;
}

/////////////////////////////////////////////////////////////////////
int QKPP::ExecuteCommand(const std::string& cmd, std::string& answer)
{
	//THIS BIT ALL TAKEN FROM THE SC10 one, including most comments
	//Send command, add return onto end
	//SOMETHING HERE IS NOT WORKING QUITE RIGHT - MORE DIAGNOSTICS NEEDED!
	int ret = 0;
	PurgeComPort(port_.c_str());
	if (DEVICE_OK != SendSerialCommand(port_.c_str(), cmd.c_str(), "\r")) {
		LogMessage("Purge command failed for QKPP", true);
		return DEVICE_SERIAL_COMMAND_FAILED;
	}
	// Get answer from the device.  It will always end with a 'ok\r\n' if it worked
	ret = GetSerialAnswer(port_.c_str(), "ok\r\n", answer);
	if (ret != DEVICE_OK)
		LogMessage("Send command failed for QKPP", true);
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
		LogMessage("Unexpected return string from HDG", true);
	}
	//If it all went well...
	return DEVICE_OK;
}