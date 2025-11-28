#pragma once

///////////////////////////////////////////////////////////////////////////////
// FILE:          QKPP.h
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"
#include "../../MMDevice/ModuleInterface.h"

static const char* g_QKPPDeviceName = "Bartels QuadKey";
static const char* g_PropName_T1 = "Pump 1 runtime [ms]";
static const char* g_PropName_T2 = "Pump 2 runtime [ms]";
static const char* g_PropName_T3 = "Pump 3 runtime [ms]";
static const char* g_PropName_T4 = "Pump 4 runtime [ms]";
static const char* g_PropName_V1 = "Pump 1 voltage [V]";
static const char* g_PropName_V2 = "Pump 2 voltage [V]";
static const char* g_PropName_V3 = "Pump 3 voltage [V]";
static const char* g_PropName_V4 = "Pump 4 voltage [V]";
static const char* g_PropName_Dispensing = "Dispensing";
static const char* g_PropName_Continuous = "Continuous";

/////////////////////////////////////////////
// Error codes
/////////////////////////////////////////////

#define ERR_COMMAND_NOT_RECOGNISED 		101
#define ERR_UNEXPECTED_RESPONSE 		102
#define ERR_DELAY_OUT_OF_RANGE 			103
#define ERR_PORT_CHANGE_FORBIDDEN		111
#define ERR_SERIAL_COMMS_ERROR			199

/////////////////////////////////////////////
// Predefined constants
/////////////////////////////////////////////

#define MAX_PUMPTIME					10000
#define MAX_VOLTAGE						250

// Buffer for sending commands to the delay box - better to be too big than too small...
#define bufSize							20
#define command_wait_time				100


class QKPP : public CGenericBase<QKPP>
{
public:
	QKPP();
	~QKPP();

	// MMDevice API
	// ------------
	// Functions here are found in the .cpp file
	int Initialize();
	int Shutdown();
	void GetName(char* pszName) const;
	bool Busy();

	// MMCore name of serial port
	std::string port_;

	//Action interface
	//----------------
	int On_Port(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_T1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_T2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_T3(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_T4(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_V1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_V2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_V3(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_V4(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_Dispense(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_Cont(MM::PropertyBase* pProp, MM::ActionType eAct);

	//internal utilities - don't work in private? can't leave undeclared either
	int Set_Pump(int which_pump);
	int Set_Voltage(int voltage_to_set);
	int Set_Runtime(int voltage_to_set);
	int Set_Generic_Property(int value, std::string propertyname);

private:
	bool initialized_;
	int Dispense_state_;
	bool Continuous_;

	std::vector<long> runtimes;
	std::vector<long> voltages;
	long lastvolt;
	
	MM::MMTime changedTime_;

	//Taken from SC10
	int ExecuteCommand(const std::string& cmd, std::string& answer);
	std::string deviceInfo_;
	// Command exchange with MMCore
	std::string command_;
	// Last command sent to the controller
	std::string lastCommand_;
};

