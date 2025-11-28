#pragma once

///////////////////////////////////////////////////////////////////////////////
// FILE:          Bartels_QuadKey.h
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


#include "../../MMDevice/MMDevice.h"
#include "../../MMDevice/DeviceBase.h"
#include "../../MMDevice/DeviceUtils.h"
#include "../../MMDevice/ModuleInterface.h"

static const char* g_Bartels_QuadKeyDeviceName = "Bartels_QuadKey";
static const char* g_PropName_Voltage_1 = "Pump 1 voltage (V)";
static const char* g_PropName_Voltage_2 = "Pump 2 voltage (V)";
static const char* g_PropName_Voltage_3 = "Pump 3 voltage (V)";
static const char* g_PropName_Voltage_4 = "Pump 4 voltage (V)";
static const char* g_PropName_Sel_pumps = "Selected pumps";

/////////////////////////////////////////////
// Error codes
/////////////////////////////////////////////

#define ERR_COMMAND_NOT_RECOGNISED 		101
#define ERR_UNEXPECTED_RESPONSE 		102
#define ERR_PORT_CHANGE_FORBIDDEN		111
#define ERR_THIS_PUMP_DOESNT_EXIST		121
#define ERR_SERIAL_COMMS_ERROR			199
#define bufSize							20

/////////////////////////////////////////////
// Predefined constants
/////////////////////////////////////////////

#define MAX_CHANNELS	4
#define MAX_DURATION_S	10

class Bartels_QuadKey : public CGenericBase<Bartels_QuadKey>
{
public:
	Bartels_QuadKey(void);
	~Bartels_QuadKey(void);

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
	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnVoltage1(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnVoltage2(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnVoltage3(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnVoltage4(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnState(MM::PropertyBase* pProp, MM::ActionType eAct);

private:
	long numpumps_;
	bool initialized_;
	bool active_;
	double current_pump;
	double V_val_1_;
	double V_val_2_;
	double V_val_3_;
	double V_val_4_;
	double T_val_1_;
	double T_val_2_;
	double T_val_3_;
	double T_val_4_;
	bool Continuous_on_;
	long answerTimeoutMs_;
	long which_pump_now_;
	long whichpumps_;
	MM::MMTime changedTime_;

	//Taken from SC10

    int ExecuteCommand(const std::string& cmd, std::string& answer);
    std::string deviceInfo_;
    // Command exchange with MMCore
    std::string command_;
    // Last command sent to the controller
    std::string lastCommand_;
};

