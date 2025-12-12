#pragma once

///////////////////////////////////////////////////////////////////////////////
// FILE:          openIncubate.h
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"
#include "../../MMDevice/ModuleInterface.h"

static const char* g_openIncubateDeviceName = "openIncubate";
static const char* g_PropName_Setpoint = "Setpoint [C]";
static const char* g_PropName_Debug = "Debug indicator";
static const char* g_PropName_Reset = "Reset warnings";

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

#define MAX_SETTABLE_T					50.0
#define MIN_SETTABLE_T					10.0
#define ERROR_MIN_T						0.0

// Buffer for sending commands to the delay box - better to be too big than too small...
#define bufSize							20
#define command_wait_time				100

class openIncubate : public CGenericBase<openIncubate>
{
public:
	openIncubate();
	~openIncubate();

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
	int On_Setpoint(MM::PropertyBase* pProp, MM::ActionType eAct);
	int On_Reset_Warnings(MM::PropertyBase* pProp, MM::ActionType eAct);
//	int On_Debug_Toggle(MM::PropertyBase* pProp, MM::ActionType eAct);

	//internal utilities - don't work in private? can't leave undeclared either
	int Set_Setpoint(double value_to_set);
	int Set_Generic_Property(int value, std::string propertyname);
	int Reset_Warnings();

private:
	bool initialized_;
	bool Enabled_;
	bool Debug_indicator_;
	double setpoint_;

	MM::MMTime changedTime_;

	int ExecuteCommand(const std::string& cmd, std::string& answer);
	std::string deviceInfo_;
	// Command exchange with MMCore
	std::string command_;
	// Last command sent to the controller
	std::string lastCommand_;
};