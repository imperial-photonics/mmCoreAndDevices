#pragma once

#ifndef _NEOPIXEL_LED_RGBW_H_
#define _NEOPIXEL_LED_RGBW_H_

#include "MMDevice.h"
#include "DeviceBase.h"  

enum RGBW_channel { 
	R,
	G,
	B,
	W
};
const char g_RGBW_channels[] = "RGBW";


class RGBW_Hub : public HubBase<RGBW_Hub>
{
public:
	// ----------
	// Constructor & Destructor
	// ----------

	RGBW_Hub();
	~RGBW_Hub();


	// ----------
	// Device API
	// ----------

	int Initialize();
	int Shutdown();
	void GetName(char* Name) const;
	bool Busy();

	// ----------
	// Hub API
	// ----------

	int DetectInstalledDevices();

	// ----------
	// Actions  
	// ---------- 

	int OnPort(MM::PropertyBase* pProp, MM::ActionType eAct); 
	int OnNumPixels(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBrightness(MM::PropertyBase* pProp, MM::ActionType eAct);

	// -----------
	// Utilities -- LED related
	// -----------

	int GetNumPixels(uint16_t& numPixels);
	
	int GetBrightness(uint8_t& brightness_level);
	int SetBrightness(uint8_t brightness_level, uint8_t& actual_brightness_level);

	int GetColor(uint16_t pixelInd, RGBW_channel channel, uint8_t& value);
	int SetColor(uint16_t pixelInd, RGBW_channel channel, uint8_t value, uint8_t& actual_value);


private:
	// -----------
	// Attributes -- MMDevice related
	// -----------

	bool initialized_;
	bool busy_;
	std::string port_;  

	// -----------
	// Attributes -- LED related
	// -----------

	uint16_t numPixels_;
	uint8_t brightness_level_;

	// -----------
	// Utilities -- LED related
	// -----------

	int SendRecv(const char* port, const char* msg, std::string& answer);
};


class RGBW_Pixel : public CGenericBase<RGBW_Pixel>
{
public:
	// ----------
	// Constructor & Destructor
	// ----------

	RGBW_Pixel(uint16_t pixelInd);
	~RGBW_Pixel(); 

	// ----------
	// Device API
	// ----------

	int Initialize();
	int Shutdown();
	void GetName(char* Name) const;
	bool Busy();

	// ----------
	// Actions  
	// ---------- 
	int OnPixelInd(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnR(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnG(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnB(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnW(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnChannel(RGBW_channel channel, MM::PropertyBase* pProp, MM::ActionType eAct);

private:

	// -----------
	// Attributes -- MMDevice related
	// -----------

	bool initialized_;
	bool busy_;

	// -----------
	// Attributes -- LED related
	// -----------

	uint16_t pixelInd_;
	uint8_t R_;
	uint8_t G_;
	uint8_t B_;
	uint8_t W_;
};

#endif // _NEOPIXEL_LED_RGBW_H_
