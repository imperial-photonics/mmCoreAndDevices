#include "RGBW.h" 

#include "ModuleInterface.h"


///////////////////////////////////////////////////////////////////////////////
// Constants
/////////////////////////////////////////////////////////////////////////////// 

const char* g_RGBW_Hub_DeviceName = "RGBW Hub";
const char* g_RGBW_Hub_DeviceDescription = "Hub for a strip of Neopixel RGBW LEDs";

const char* g_RGBW_Pixel_DeviceName = "RGBW";
const char* g_RGBW_Pixel_DeviceDescription = "single Neopixel RGBW LED from the strip Hub";   

const char* g_RGBW_Hub_PropName_NumPixels = "Pixel number";
const char* g_RGBW_Hub_PropName_Brightness = "Brightness level"; 

const char* g_RGBW_Pixel_PropName_PixelInd = "Pixel index"; 


///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
/////////////////////////////////////////////////////////////////////////////// 

MODULE_API void InitializeModuleData()
{
	RegisterDevice(g_RGBW_Hub_DeviceName, MM::HubDevice, g_RGBW_Hub_DeviceDescription);  
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
    if (deviceName == 0)
        return 0;

    if (strcmp(deviceName, g_RGBW_Hub_DeviceName) == 0)
    {
        return new RGBW_Hub();
    }
    else if (strncmp(deviceName, g_RGBW_Pixel_DeviceName, strlen(g_RGBW_Pixel_DeviceName)) == 0)
    {
        std::string s = deviceName;
        s = s.substr(strlen(g_RGBW_Pixel_DeviceName));
        uint16_t pixelInd = std::stoul(s, nullptr, 10);
        
        return new RGBW_Pixel(pixelInd);
    } 

    return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
    delete pDevice;
}


///////////////////////////////////////////////////////////////////////////////
// Hub Constructor & destructor
/////////////////////////////////////////////////////////////////////////////// 

RGBW_Hub::RGBW_Hub() :
    initialized_(false),
    busy_(false),
    port_("Undefined"),
    numPixels_(0),
    brightness_level_(0)
{
    // setup error messages
    // -------------------
    InitializeDefaultErrorMessages();

    // init flags
    // -------------------
    int ret;
    CPropertyAction* pAct;

    // pre-init properties
    // -------------------
    pAct = new CPropertyAction(this, &RGBW_Hub::OnPort);
    ret = CreateStringProperty(MM::g_Keyword_Port, port_.c_str(), false, pAct, true);
    assert(ret == DEVICE_OK);
}

RGBW_Hub::~RGBW_Hub()
{
    Shutdown();
}


///////////////////////////////////////////////////////////////////////////////
// Hub Device API
///////////////////////////////////////////////////////////////////////////////

void RGBW_Hub::GetName(char* name) const
{
    CDeviceUtils::CopyLimitedString(name, g_RGBW_Hub_DeviceName);
}

bool RGBW_Hub::Busy()
{
    return busy_;
}

int RGBW_Hub::Initialize() {
	if (initialized_) {
		return DEVICE_OK;
	}

    // The first second or so after opening the serial port, the Arduino is waiting for firmwareupgrades.  Simply sleep 1 second.
    CDeviceUtils::SleepMs(2000);

	// init flags
	// -------------------
	int ret;
	CPropertyAction* pAct;
	std::string propStr; 

    // Initialize properties
    pAct = new CPropertyAction(this, &RGBW_Hub::OnNumPixels);
    ret = CreateIntegerProperty(g_RGBW_Hub_PropName_NumPixels, numPixels_, true, pAct, false);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = SetPropertyLimits(g_RGBW_Hub_PropName_NumPixels, 0, UINT16_MAX);
    if (ret != DEVICE_OK) {
        return ret;
    }

    pAct = new CPropertyAction(this, &RGBW_Hub::OnBrightness);
    ret = CreateIntegerProperty(g_RGBW_Hub_PropName_Brightness, brightness_level_, false, pAct, false);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = SetPropertyLimits(g_RGBW_Hub_PropName_Brightness, 0, UINT8_MAX);
    if (ret != DEVICE_OK) {
        return ret;
    }


    // detect children
    ret = DetectInstalledDevices();
    if (ret != DEVICE_OK) {
        return ret;
    }

	 
	// update flag
	initialized_ = true;

	return DEVICE_OK;
}

int RGBW_Hub::Shutdown() {
    if (initialized_) {
        // update flag
        initialized_ = false;
    }

    return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Hub API
///////////////////////////////////////////////////////////////////////////////

int RGBW_Hub::DetectInstalledDevices()
{
    std::ostringstream device_name;
    std::ostringstream device_label;

    for (size_t i = 0; i < numPixels_; i++)
    {  
        device_name.str("");
        device_name.clear();
        device_label.str("");
        device_label.clear();

        device_name << g_RGBW_Pixel_DeviceName << std::to_string(i);
        device_label << std::to_string(i) << "-th" << g_RGBW_Pixel_DeviceDescription;

        // register children device 
        RegisterDevice(device_name.str().c_str(), MM::GenericDevice, device_label.str().c_str());

        // create children device
        MM::Device* pDev = CreateDevice(device_name.str().c_str());

        // install children device
        if (pDev)
        {
            AddInstalledDevice(pDev);
        }
    }   

    return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Hub Actions
///////////////////////////////////////////////////////////////////////////////

int RGBW_Hub::OnPort(MM::PropertyBase* pProp, MM::ActionType eAct) {
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
            return DEVICE_CAN_NOT_SET_PROPERTY;
        }
        pProp->Get(port_);
    }

    return DEVICE_OK;
}

int RGBW_Hub::OnNumPixels(MM::PropertyBase* pProp, MM::ActionType eAct) {
    if (eAct == MM::BeforeGet)
    {
        int ret = DEVICE_OK; 

        ret = GetNumPixels(numPixels_);
        if (ret != DEVICE_OK) {
            return ret;
        }

        pProp->Set(static_cast<long>(numPixels_));
    }
    else if (eAct == MM::AfterSet) {
        /*long value = static_cast<long>(numPixels_);
        pProp->Get(value);
        if (value != numPixels_) {
            numPixels_ = value;
        }*/

        // read-only property
    }

    return DEVICE_OK;
}

int RGBW_Hub::OnBrightness(MM::PropertyBase* pProp, MM::ActionType eAct) {
    int ret = DEVICE_OK;
    if (eAct == MM::BeforeGet)
    {
        ret = GetBrightness(brightness_level_);
        if (ret != DEVICE_OK) {
            return ret;
        }

        pProp->Set(static_cast<long>(brightness_level_));

    }  
    else if (eAct == MM::AfterSet) {
        long value = static_cast<long>(brightness_level_);
        pProp->Get(value);

        if (value != brightness_level_) {
            ret = SetBrightness(value, brightness_level_);
            if (ret != DEVICE_OK) {
                return ret;
            }

            pProp->Set(static_cast<long>(brightness_level_));
        } 
    }
    return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Hub Public Utilities - LED related
///////////////////////////////////////////////////////////////////////////////

const char g_NumPixels = 'N';
const char g_Brightness = 'L';
const char* g_GetFlag = "?";

int RGBW_Hub::GetNumPixels(uint16_t& numPixels) {
    int ret = DEVICE_OK;

    std::ostringstream cmd;
    cmd << 0 << g_NumPixels << g_GetFlag;

    std::string answer;
    ret = SendRecv(port_.c_str(), cmd.str().c_str(), answer);
    if (ret != DEVICE_OK) {
        return ret;
    }

    numPixels = std::stoul(answer, nullptr, 10);

    return ret;
}

int RGBW_Hub::GetBrightness(uint8_t& brightness_level) {
    int ret = DEVICE_OK;

    std::ostringstream cmd;
    cmd << 0 << g_Brightness << g_GetFlag;

    std::string answer;
    ret = SendRecv(port_.c_str(), cmd.str().c_str(), answer);
    if (ret != DEVICE_OK) {
        return ret;
    }

    brightness_level = std::stoul(answer, nullptr, 10);

    return ret;
}

int RGBW_Hub::SetBrightness(uint8_t brightness_level, uint8_t& actual_brightness_level) {
    int ret = DEVICE_OK;

    std::ostringstream cmd;
    cmd << 0 << g_Brightness << std::to_string(brightness_level);

    std::string answer;
    ret = SendRecv(port_.c_str(), cmd.str().c_str(), answer);
    if (ret != DEVICE_OK) {
        return ret;
    }

    actual_brightness_level = std::stoul(answer, nullptr, 10);

    return ret;
}

int RGBW_Hub::GetColor(uint16_t pixelInd, RGBW_channel channel, uint8_t& value) {
    int ret = DEVICE_OK;

    std::ostringstream cmd;
    cmd << pixelInd << g_RGBW_channels[channel] << g_GetFlag;

    std::string answer;
    ret = SendRecv(port_.c_str(), cmd.str().c_str(), answer);
    if (ret != DEVICE_OK) {
        return ret;
    }

    value = std::stoul(answer, nullptr, 10);

    return ret;
}

int RGBW_Hub::SetColor(uint16_t pixelInd, RGBW_channel channel, uint8_t value, uint8_t& actual_value) {
    int ret = DEVICE_OK;

    std::ostringstream cmd;
    cmd << pixelInd << g_RGBW_channels[channel] << std::to_string(value); 

    std::string answer;
    ret = SendRecv(port_.c_str(), cmd.str().c_str(), answer);
    if (ret != DEVICE_OK) {
        return ret;
    } 

    actual_value = std::stoul(answer, nullptr, 10); 

    return ret;
}


///////////////////////////////////////////////////////////////////////////////
// Hub Private Utilities - LED related
///////////////////////////////////////////////////////////////////////////////

const char* Term_Flag = "\n";
const char* Err_Sign = "Err:";

int RGBW_Hub::SendRecv(const char* port, const char* msg, std::string& answer) {
    int ret = DEVICE_OK;

    std::ostringstream logErr;
    logErr << "Sending message '" << msg << "' to " << port << ". ";

    ret = SendSerialCommand(port, msg, Term_Flag);
    if (ret != DEVICE_OK) {
        logErr << "Sending failed. ";
        LogMessage(logErr.str(), false);
        answer = "";
        return ret;
    }

    ret = GetSerialAnswer(port, Term_Flag, answer);
    if (ret != DEVICE_OK) {
        logErr << "Sending succeeded but receiving answer failed. ";
        LogMessage(logErr.str(), false);
        answer = "";
        return ret;
    }

    if (answer.rfind(Err_Sign, 0) == 0) { 
        logErr << "Received error: " << answer.substr(strlen(Err_Sign));
        LogMessage(logErr.str(), false);
        answer = "";
        return DEVICE_UNSUPPORTED_COMMAND;
    } 

    return ret;
}


///////////////////////////////////////////////////////////////////////////////
// Pixel Constructor & destructor
///////////////////////////////////////////////////////////////////////////////
RGBW_Pixel::RGBW_Pixel(uint16_t pixelInd) : 
    initialized_(false),
    busy_(false),
    R_(0),
    G_(0),
    B_(0),
    W_(0)
{
    // set pixelInd value
    pixelInd_ = pixelInd;

    // setup error messages
    // -------------------
    InitializeDefaultErrorMessages();

    // parent ID display
    CreateHubIDProperty(); 
}

RGBW_Pixel::~RGBW_Pixel() {
    Shutdown();
}

///////////////////////////////////////////////////////////////////////////////
// Pixel Device API
///////////////////////////////////////////////////////////////////////////////

void RGBW_Pixel::GetName(char* name) const
{
    std::ostringstream device_name;
    device_name << g_RGBW_Pixel_DeviceName << std::to_string(pixelInd_);

    CDeviceUtils::CopyLimitedString(name, device_name.str().c_str());  
}

bool RGBW_Pixel::Busy()
{
    return busy_;
}

int RGBW_Pixel::Initialize() {
    if (initialized_) {
        return DEVICE_OK;
    } 

    // parent related
    // ------------------- 
    RGBW_Hub* hub = static_cast<RGBW_Hub*>(GetParentHub());
    if (!hub) {
        return ERROR_DEVICE_NOT_AVAILABLE;
    }
    char hubLabel[MM::MaxStrLength];
    hub->GetLabel(hubLabel);
    SetParentID(hubLabel); // for backward comp.

    // init flags
    // -------------------
    int ret;
    CPropertyAction* pAct;
    std::string propStr;

    // init props
    // -------------------
    pAct = new CPropertyAction(this, &RGBW_Pixel::OnPixelInd);
    ret = CreateIntegerProperty(g_RGBW_Pixel_PropName_PixelInd, pixelInd_, true, pAct, false);
    if (ret != DEVICE_OK) {
        return ret;
    } 
    long numPixels;
    ret = hub->GetProperty(g_RGBW_Hub_PropName_NumPixels, numPixels);
    if (ret != DEVICE_OK) {
        return ret;
    }
    ret = SetPropertyLimits(g_RGBW_Pixel_PropName_PixelInd, 0, numPixels);
    if (ret != DEVICE_OK) {
        return ret;
    } 

    std::string s = "";
    uint8_t value = 0;
    for (int channel = RGBW_channel::R; channel <= RGBW_channel::W; channel++) {
        switch (static_cast<RGBW_channel>(channel)) {
            case RGBW_channel::R:
                pAct = new CPropertyAction(this, &RGBW_Pixel::OnR);
                value = R_;
                break;
            case RGBW_channel::G:
                pAct = new CPropertyAction(this, &RGBW_Pixel::OnG);
                value = G_;
                break;
            case RGBW_channel::B:
                pAct = new CPropertyAction(this, &RGBW_Pixel::OnB);
                value = B_;
                break;
            case RGBW_channel::W:
                pAct = new CPropertyAction(this, &RGBW_Pixel::OnW);
                value = W_;
                break; 
        }
        s = g_RGBW_channels[channel];
        ret = CreateIntegerProperty(s.c_str(), value, false, pAct, false); 
        if (ret != DEVICE_OK) {
            return ret;
        }
        ret = SetPropertyLimits(s.c_str(), 0, UINT8_MAX);
        if (ret != DEVICE_OK) {
            return ret;
        }
    } 

    // update flag
    // -------------------
    initialized_ = true;

    return DEVICE_OK;
}

int RGBW_Pixel::Shutdown() {
    if (initialized_) {
        // update flag
        initialized_ = false;
    }

    return DEVICE_OK;
}


///////////////////////////////////////////////////////////////////////////////
// Pixel Actions
///////////////////////////////////////////////////////////////////////////////

int RGBW_Pixel::OnPixelInd(MM::PropertyBase* pProp, MM::ActionType eAct) { 
    if (eAct == MM::BeforeGet)
    {  
        pProp->Set(static_cast<long>(pixelInd_));
    }
    else if (eAct == MM::AfterSet) { 
        // read-only property
    }

    return DEVICE_OK;
}

int RGBW_Pixel::OnR(MM::PropertyBase* pProp, MM::ActionType eAct) {
    return OnChannel(RGBW_channel::R, pProp, eAct);
}

int RGBW_Pixel::OnG(MM::PropertyBase* pProp, MM::ActionType eAct) {
    return OnChannel(RGBW_channel::G, pProp, eAct);
}

int RGBW_Pixel::OnB(MM::PropertyBase* pProp, MM::ActionType eAct) {
    return OnChannel(RGBW_channel::B, pProp, eAct);
} 

int RGBW_Pixel::OnW(MM::PropertyBase* pProp, MM::ActionType eAct) {
    return OnChannel(RGBW_channel::W, pProp, eAct);
}

int RGBW_Pixel::OnChannel(RGBW_channel channel, MM::PropertyBase* pProp, MM::ActionType eAct) {
    int ret = DEVICE_OK;

    RGBW_Hub* hub = static_cast<RGBW_Hub*>(GetParentHub());
    if (!hub) {
        return ERROR_DEVICE_NOT_AVAILABLE;
    }

    uint8_t value_ = 0; 

    if (eAct == MM::BeforeGet)
    {
        ret = hub->GetColor(pixelInd_, channel, value_);
        if (ret != DEVICE_OK) {
            return ret;
        } 

        switch (channel) {
            case RGBW_channel::R:
                R_ = value_;
                break;
            case RGBW_channel::G:
                G_ = value_;
                break;
            case RGBW_channel::B:
                B_ = value_;
                break;
            case RGBW_channel::W:
                W_ = value_;
                break;
        } 
        pProp->Set(static_cast<long>(value_));
    }
    else if (eAct == MM::AfterSet)
    {
        switch (channel) {
            case RGBW_channel::R:
                value_ = R_;
                break;
            case RGBW_channel::G:
                value_ = G_;
                break;
            case RGBW_channel::B:
                value_ = B_;
                break;
            case RGBW_channel::W:
                value_ = W_;
                break;
        }

        long value = static_cast<long>(value_);
        pProp->Get(value);
         
        if (value != value_) {   
            ret = hub->SetColor(pixelInd_, channel, value, value_);
            if (ret != DEVICE_OK) {
                return ret;
            } 

            switch (channel) {
                case RGBW_channel::R:
                    R_ = value_;
                    break;
                case RGBW_channel::G:
                    G_ = value_;
                    break;
                case RGBW_channel::B:
                    B_ = value_;
                    break;
                case RGBW_channel::W:
                    W_ = value_;
                    break;
            }
            pProp->Set(static_cast<long>(value_));
        }
    }

    return DEVICE_OK;
}