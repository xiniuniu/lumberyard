/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/
// Original file Copyright Crytek GMBH or its affiliates, used under license.

#pragma once


#ifdef CRYINPUT_EXPORTS
    #define CRYINPUT_API DLL_EXPORT
#else
    #define CRYINPUT_API DLL_IMPORT
#endif

#include <platform.h>
#include <CryName.h>
#include <AzCore/EBus/EBus.h>

#if defined(AZ_RESTRICTED_PLATFORM) || defined(AZ_TOOLS_EXPAND_FOR_RESTRICTED_PLATFORMS)
#undef AZ_RESTRICTED_SECTION
#define IINPUT_H_SECTION_ENUM 1
#define IINPUT_H_SECTION_2 2
#endif

struct ISystem;

namespace AzFramework
{
    class InputDeviceId;
}

enum EModifierMask
{
    eMM_None                = 0,
    eMM_LCtrl               = (1 << 0),
    eMM_LShift          = (1 << 1),
    eMM_LAlt                = (1 << 2),
    eMM_LWin                = (1 << 3),
    eMM_RCtrl               = (1 << 4),
    eMM_RShift          = (1 << 5),
    eMM_RAlt                = (1 << 6),
    eMM_RWin                = (1 << 7),
    eMM_NumLock         = (1 << 8),
    eMM_CapsLock        = (1 << 9),
    eMM_ScrollLock  = (1 << 10),

    eMM_Ctrl                = (eMM_LCtrl | eMM_RCtrl),
    eMM_Shift               = (eMM_LShift | eMM_RShift),
    eMM_Alt                 = (eMM_LAlt | eMM_RAlt),
    eMM_Win                 = (eMM_LWin | eMM_RWin),
    eMM_Modifiers       = (eMM_Ctrl | eMM_Shift | eMM_Alt | eMM_Win),
    eMM_LockKeys        = (eMM_CapsLock | eMM_NumLock | eMM_ScrollLock)
};

const static int32 EFF_INVALID_DEVICE_INDEX = -1;

enum EFFEffectId
{
    eFF_Rumble_Basic = 0,
    eFF_Rumble_Frame
};

//Summary:
//   Input Event types.
enum EInputState
{
    eIS_Unknown     = 0,
    eIS_Pressed     = (1 << 0),
    eIS_Released    = (1 << 1),
    eIS_Down            = (1 << 2),
    eIS_Changed     = (1 << 3),
    eIS_UI              = (1 << 4),
};

enum EInputDeviceType
{
    eIDT_Keyboard,
    eIDT_Mouse,
    eIDT_Joystick,
    eIDT_Gamepad,
    eIDT_TouchScreen,
    eIDT_MotionSensor,
    eIDT_MotionController,

    eIDT_Count,
    eIDT_Unknown    = 0xff,
};

enum EInputPlatformFlags
{
    eIPF_NONE                                                   = 0,
    eIPF_SwapFrontEndForwardAndBack     = (1 << 0),
};

#define USE_CRY_NAME_FOR_KEY_NAMES 0

#if USE_CRY_NAME_FOR_KEY_NAMES



typedef CCryName TKeyName;

#else

struct TKeyName
{
    const char* key;

    TKeyName() { key = ""; }
    TKeyName(const char* _key) { key = _key; };
    operator const char*() const {
        return key;
    };
    bool operator<(const TKeyName& n) const { return _stricmp(key, n.key) < 0; }
    bool operator>(const TKeyName& n) const { return _stricmp(key, n.key) > 0; }
    bool operator==(const TKeyName& n) const { return _stricmp(key, n.key) == 0; }
    bool operator!=(const TKeyName& n) const { return _stricmp(key, n.key) != 0; }
    bool operator<(const char* str) const { return _stricmp(key, str) < 0; }
    bool operator>(const char* str) const { return _stricmp(key, str) > 0; }
    bool operator==(const char* str) const { return _stricmp(key, str) == 0; }
    bool operator!=(const char* str) const { return _stricmp(key, str) != 0; }
    const char* c_str() const { return key; }
};
inline bool operator==(const char* str, const TKeyName& n) { return n == str; }
inline bool operator!=(const char* str, const TKeyName& n) { return n != str; }
inline bool operator<(const char* str, const TKeyName& n) { return n < str; }
inline bool operator>(const char* str, const TKeyName& n) { return n > str; }

#endif //USE_CRY_NAME_FOR_KEY_NAMES

#define KI_KEYBOARD_BASE  0
#define KI_MOUSE_BASE     256
#define KI_XINPUT_BASE    512
#define KI_PROVO_BASE     1024
#define KI_XENIA_BASE     1536
#define KI_MOTION_BASE    2048
#define KI_SYS_BASE       4096

enum EKeyId
{
    eKI_Escape = KI_KEYBOARD_BASE,
    eKI_1,
    eKI_2,
    eKI_3,
    eKI_4,
    eKI_5,
    eKI_6,
    eKI_7,
    eKI_8,
    eKI_9,
    eKI_0,
    eKI_Minus,
    eKI_Equals,
    eKI_Backspace,
    eKI_Tab,
    eKI_Q,
    eKI_W,
    eKI_E,
    eKI_R,
    eKI_T,
    eKI_Y,
    eKI_U,
    eKI_I,
    eKI_O,
    eKI_P,
    eKI_LBracket,
    eKI_RBracket,
    eKI_Enter,
    eKI_LCtrl,
    eKI_A,
    eKI_S,
    eKI_D,
    eKI_F,
    eKI_G,
    eKI_H,
    eKI_J,
    eKI_K,
    eKI_L,
    eKI_Semicolon,
    eKI_Apostrophe,
    eKI_Tilde,
    eKI_LShift,
    eKI_Backslash,
    eKI_Z,
    eKI_X,
    eKI_C,
    eKI_V,
    eKI_B,
    eKI_N,
    eKI_M,
    eKI_Comma,
    eKI_Period,
    eKI_Slash,
    eKI_RShift,
    eKI_NP_Multiply,
    eKI_LAlt,
    eKI_Space,
    eKI_CapsLock,
    eKI_F1,
    eKI_F2,
    eKI_F3,
    eKI_F4,
    eKI_F5,
    eKI_F6,
    eKI_F7,
    eKI_F8,
    eKI_F9,
    eKI_F10,
    eKI_NumLock,
    eKI_ScrollLock,
    eKI_NP_7,
    eKI_NP_8,
    eKI_NP_9,
    eKI_NP_Substract,
    eKI_NP_4,
    eKI_NP_5,
    eKI_NP_6,
    eKI_NP_Add,
    eKI_NP_1,
    eKI_NP_2,
    eKI_NP_3,
    eKI_NP_0,
    eKI_F11,
    eKI_F12,
    eKI_F13,
    eKI_F14,
    eKI_F15,
    eKI_Colon,
    eKI_Underline,
    eKI_NP_Enter,
    eKI_RCtrl,
    eKI_NP_Period,
    eKI_NP_Divide,
    eKI_Print,
    eKI_RAlt,
    eKI_Pause,
    eKI_Home,
    eKI_Up,
    eKI_PgUp,
    eKI_Left,
    eKI_Right,
    eKI_End,
    eKI_Down,
    eKI_PgDn,
    eKI_Insert,
    eKI_Delete,
    eKI_LWin,
    eKI_RWin,
    eKI_Apps,
    eKI_OEM_102,

    // android specific "keyboard" keys
    eKI_Android_Back, // physical back button

    // Mouse.
    eKI_Mouse1 = KI_MOUSE_BASE,
    eKI_Mouse2,
    eKI_Mouse3,
    eKI_Mouse4,
    eKI_Mouse5,
    eKI_Mouse6,
    eKI_Mouse7,
    eKI_Mouse8,
    eKI_MouseWheelUp,
    eKI_MouseWheelDown,
    eKI_MouseX,
    eKI_MouseY,
    eKI_MouseZ,
    eKI_MousePosition,
    eKI_MouseLast,

    // Touch
    eKI_Touch0,
    eKI_Touch1,
    eKI_Touch2,
    eKI_Touch3,
    eKI_Touch4,
    eKI_Touch5,
    eKI_Touch6,
    eKI_Touch7,
    eKI_Touch8,
    eKI_Touch9,
    eKI_TouchLast,

    eKI_XI_DPadUp = KI_XINPUT_BASE,
    eKI_XI_DPadDown,
    eKI_XI_DPadLeft,
    eKI_XI_DPadRight,
    eKI_XI_Start,
    eKI_XI_Back,
    eKI_XI_ThumbL,
    eKI_XI_ThumbR,
    eKI_XI_ShoulderL,
    eKI_XI_ShoulderR,
    eKI_XI_A,
    eKI_XI_B,
    eKI_XI_X,
    eKI_XI_Y,
    eKI_XI_TriggerL,
    eKI_XI_TriggerR,
    eKI_XI_ThumbLX,
    eKI_XI_ThumbLY,
    eKI_XI_ThumbLUp,
    eKI_XI_ThumbLDown,
    eKI_XI_ThumbLLeft,
    eKI_XI_ThumbLRight,
    eKI_XI_ThumbRX,
    eKI_XI_ThumbRY,
    eKI_XI_ThumbRUp,
    eKI_XI_ThumbRDown,
    eKI_XI_ThumbRLeft,
    eKI_XI_ThumbRRight,
    eKI_XI_TriggerLBtn,
    eKI_XI_TriggerRBtn,
    eKI_XI_Connect,     // should be deprecated because all devices can be connected, use eKI_SYS_ConnectDevice instead
    eKI_XI_Disconnect,  // should be deprecated because all devices can be disconnected, use eKI_SYS_DisconnectDevice instead

#if defined(AZ_PLATFORM_XENIA) || defined(TOOLS_SUPPORT_XENIA)
    #define AZ_RESTRICTED_SECTION IINPUT_H_SECTION_ENUM
    #include "Xenia/IInput_h_xenia.inl"
#endif

#if defined(AZ_PLATFORM_PROVO) || defined(TOOLS_SUPPORT_PROVO)
    #define AZ_RESTRICTED_SECTION IINPUT_H_SECTION_ENUM
    #include "Provo/IInput_h_provo.inl"
#endif

#if defined(AZ_PLATFORM_SALEM) || defined(TOOLS_SUPPORT_SALEM)
    #define AZ_RESTRICTED_SECTION IINPUT_H_SECTION_ENUM
    #include "Salem/IInput_h_salem.inl"
#endif

    // Normal inputs should be added above
    // eKI_SYS_Commit and below will be ignored by input blocking functionality
    eKI_SYS_Commit = KI_SYS_BASE,
    eKI_SYS_ConnectDevice,
    eKI_SYS_DisconnectDevice,

    // Terminator.
    eKI_Unknown     = 0xffffffff,
};

enum EMotionSensorFlags
{
    eMSF_None = 0,
    eMSF_AccelerationRaw = BIT(0),
    eMSF_AccelerationUser = BIT(1),
    eMSF_AccelerationGravity = BIT(2),
    eMSF_RotationRateRaw = BIT(3),
    eMSF_RotationRateUnbiased = BIT(4),
    eMSF_MagneticFieldRaw = BIT(5),
    eMSF_MagneticFieldUnbiased = BIT(6),
    eMSF_MagneticNorth = BIT(7),
    eMSF_Orientation = BIT(8),
    eMSF_OrientationDelta = BIT(9),
    eMSF_AllAvailable = 0xff
};

struct SMotionSensorData
{
    SMotionSensorData()
        : accelerationRaw(0.0f, 0.0f, 0.0f)
        , accelerationUser(0.0f, 0.0f, 0.0f)
        , accelerationGravity(0.0f, 0.0f, 0.0f)
        , rotationRateRaw(0.0f, 0.0f, 0.0f)
        , rotationRateUnbiased(0.0f, 0.0f, 0.0f)
        , magneticFieldRaw(0.0f, 0.0f, 0.0f)
        , magneticFieldUnbiased(0.0f, 0.0f, 0.0f)
        , magneticNorth(0.0f, 0.0f, 0.0f)
        , orientation(IDENTITY)
        , orientationDelta(IDENTITY) {}

    Vec3 accelerationRaw;
    Vec3 accelerationUser;
    Vec3 accelerationGravity;

    Vec3 rotationRateRaw;
    Vec3 rotationRateUnbiased;

    Vec3 magneticFieldRaw;
    Vec3 magneticFieldUnbiased;
    Vec3 magneticNorth;

    Quat orientation;
    Quat orientationDelta;
};

struct SMotionSensorEvent
{
    SMotionSensorEvent(const SMotionSensorData& a_sensorData,
        EMotionSensorFlags a_updatedFlags)
        : sensorData(a_sensorData)
        , updatedFlags(a_updatedFlags) {}

    const SMotionSensorData& sensorData;
    const EMotionSensorFlags updatedFlags;
};

struct IMotionSensorEventListener
{
    virtual ~IMotionSensorEventListener() {}
    virtual void OnMotionSensorEvent(const SMotionSensorEvent& event) = 0;
};

struct IMotionSensorFilter
{
    virtual ~IMotionSensorFilter() {}

    // Each implementation is responsible for storing any samples it needs to apply the filter to the next sample.
    virtual void Filter(SMotionSensorData& io_newSample, EMotionSensorFlags newSampleFlags) = 0;
};

struct SInputSymbol;
// Description:
//   InputEvents are generated by input system and dispatched to all event listeners.
// The stream of SInputEvent should not be used for text input, the SUnicodeEvent stream should be used instead
struct SInputEvent
{
    EInputDeviceType deviceType;     // Which device did the event originate from.
    EInputState      state;          // Type of input event.
    wchar_t          inputChar;      // If event is UI (Contains translated input char)
    TKeyName         keyName;        // Human readable name of the event.
    AZ::Crc32        crc;            // crc of keyName
    EKeyId           keyId;          // Device-specific id corresponding to the event.
    int              modifiers;      // Key modifiers enabled at the time of this event.
    float            value;          // Value associated with the event.
    SInputSymbol*   pSymbol;         // Input symbol the event originated from.
    uint8            deviceIndex;    // Controller index
    uint8            deviceUniqueID; // Process wide unique controller ID.
    Vec2             screenPosition; // Screen position of pointer (eg. mouse, touch) events

    SInputEvent()
    {
        deviceType = eIDT_Unknown;
        state = eIS_Unknown;
        keyId = eKI_Unknown;
        modifiers = eMM_None;
        value = 0;
        keyName = "";
        pSymbol = 0;
        deviceIndex = 0;
        deviceUniqueID = 0;
        screenPosition = Vec2(ZERO);
        crc = 0;
    }
    void GetMemoryUsage(ICrySizer* pSizer) const{}
};

// Description:
//   UnicodeEvents are generated by input system and dispatched to all event listeners
//   The stream of Unicode events should only be used for text input
struct SUnicodeEvent
{
    const uint32 inputChar; // The unicode code-point that was entered

    SUnicodeEvent(uint32 _inputChar = 0)
        : inputChar(_inputChar) {}
    void GetMemoryUsage(ICrySizer* pSizer) const {}
};

// Description:
// SFFTriggerOutputData are force feedback signals send to an input controller's triggers.
struct SFFTriggerOutputData
{
    struct Initial
    {
        enum Value
        {
            ZeroIt = 0,
            Default
        };
    };

    struct Flags
    {
        enum Value
        {
            LeftTouchToActivate     = 1 << 0,
            RightTouchToActivate    = 1 << 1,
        };
    };

    float  leftGain, rightGain;
    uint16 leftEnv, rightEnv;
    uint32 flags;

    // -------------------------------------------------------------------------
    SFFTriggerOutputData()  { Init(Initial::ZeroIt); }
    SFFTriggerOutputData(Initial::Value v)  { Init(v); }
    SFFTriggerOutputData(bool leftTouchToActivate, bool rightTouchToActivate, float lTrigger, float rTrigger, uint16 lTriggerEnv, uint16 rTriggerEnv)
        : leftGain(lTrigger)
        , rightGain(rTrigger)
        , leftEnv(lTriggerEnv)
        , rightEnv(rTriggerEnv)
    {
        SetFlag(Flags::LeftTouchToActivate, leftTouchToActivate);
        SetFlag(Flags::RightTouchToActivate, rightTouchToActivate);
    }

    // -------------------------------------------------------------------------
    void Init(Initial::Value v)
    {
        if (v == Initial::ZeroIt)
        {
            flags = 0;
            leftGain = rightGain = 0.f;
            leftEnv = rightEnv = 0;
        }
        else if (v == Initial::Default)
        {
            flags = 0;
            leftGain = rightGain = 1.f;
            leftEnv = rightEnv = 4;
        }
    }

    void operator += (const SFFTriggerOutputData& operand2)
    {
        leftGain += operand2.leftGain;
        rightGain += operand2.rightGain;
        // DARIO_TODO: check what to do with envelopes in this case
        leftEnv = max(leftEnv, operand2.leftEnv);
        rightEnv = max(rightEnv, operand2.rightEnv);
        // DARIO_TODO: check what to do with the touch required in this case
        SetFlag(Flags::LeftTouchToActivate, IsFlagEnabled(Flags::LeftTouchToActivate) || operand2.IsFlagEnabled(Flags::LeftTouchToActivate));
        SetFlag(Flags::RightTouchToActivate, IsFlagEnabled(Flags::RightTouchToActivate) || operand2.IsFlagEnabled(Flags::RightTouchToActivate));
    }

    ILINE float GetClampedLeftGain() const  { return clamp_tpl(leftGain, 0.0f, 1.0f); }
    ILINE float GetClampedRightGain() const { return clamp_tpl(rightGain, 0.0f, 1.0f); }

    bool IsFlagEnabled(Flags::Value f) const { return (flags & f) != 0; }

private:
    void EnableFlag(Flags::Value f) { flags |= f; }
    void DisableFlag(Flags::Value f) { flags &= ~f; }
    void SetFlag(Flags::Value f, bool b)
    {
        if (b)
        {
            EnableFlag(f);
        }
        else
        {
            DisableFlag(f);
        }
    }
};

// Description:
//   SFFOutputEvents are force feedback signals send to an input controller.
struct SFFOutputEvent
{
    EInputDeviceType        deviceType; // Which device will receive the event.
    EFFEffectId eventId;
    float               amplifierS, amplifierA;
    float               timeInSeconds;

    SFFTriggerOutputData triggerData;

    SFFOutputEvent()
        : triggerData(SFFTriggerOutputData::Initial::Default)
    {
        deviceType  = eIDT_Unknown;
        eventId = eFF_Rumble_Basic;
        amplifierS = 1.0f;
        amplifierA = 1.0f;
        timeInSeconds = 0.3f;
    }

    SFFOutputEvent(EInputDeviceType id, EFFEffectId event, SFFTriggerOutputData::Initial::Value triggerInitValue, float time = 1.0f, float ampA = 1.0f, float ampB = 1.0f)
        : deviceType(id)
        , eventId(event)
        , timeInSeconds(time)
        , amplifierS(ampA)
        , amplifierA(ampB)
        , triggerData(triggerInitValue)
    {}

    SFFOutputEvent(EInputDeviceType id, EFFEffectId event, const SFFTriggerOutputData& _triggerData, float time = 1.0f, float ampA = 1.0f, float ampB = 1.0f)
        : deviceType(id)
        , eventId(event)
        , timeInSeconds(time)
        , amplifierS(ampA)
        , amplifierA(ampB)
        , triggerData(_triggerData)
    {}
};

struct SInputSymbol
{
    // Summary:
    //   Input symbol types.
    enum EType
    {
        Button,     // state == press/hold/release -- value = 0, 1
        Toggle,     // transition state with a press.
        RawAxis,    // state == change -- value = movement of axis.
        Axis,       // state == change -- value = -1.0 to 1.0
        Trigger,    // state == change -- value = 0.0 to 1.0
    };

    SInputSymbol(uint32 devSpecId_, EKeyId keyId_, const TKeyName& name_, EType type_, uint32 user_ = 0)
        : devSpecId(devSpecId_)
        , keyId(keyId_)
        , name(name_)
        , state(eIS_Unknown)
        , type(type_)
        , value(0.0f)
        , user(user_)
        , deviceIndex(0)
        , screenPosition(ZERO)
        , crc(name)
    {
    }

    void PressEvent(bool pressed)
    {
        if (pressed)
        {
            state = eIS_Pressed;
            value = 1.0f;
        }
        else
        {
            state = eIS_Released;
            value = 0.0f;
        }
    }
    void ChangeEvent(float v)
    {
        state = eIS_Changed;
        value = v;
    }
    void AssignTo(SInputEvent& event, int modifiers = 0)
    {
        event.pSymbol    = this;
        event.deviceType = deviceType;
        event.modifiers  = modifiers;
        event.state      = state;
        event.value      = value;
        event.keyName    = name;
        event.keyId      = keyId;
        event.crc        = crc;
        //event.deviceIndex = deviceIndex; //symbol does not know device index, but needs to cache it for hold events
        event.screenPosition = screenPosition;
    }

    EKeyId              keyId;              // External id for fast comparison.
    TKeyName            name;               // Human readable name of the event.
    AZ::Crc32           crc;                // CRC of the name
    uint32              devSpecId;          // Device internal id of this symbol (we will use it to look it up).
    EInputState         state;              // Current state.
    EType               type;               // Type of this symbol.
    float               value;              // Current value.
    uint32              user;               // Type dependent value (toggle-mask for toggles).
    EInputDeviceType    deviceType;         // Which device does the symbol belong to.
    uint8               deviceIndex;        // Device index - controller 1/2 etc
    Vec2                screenPosition;     // Screen position of pointer (eg. mouse, touch) events
};

typedef std::vector<SInputSymbol*> TInputSymbols;
struct SInputBlockData
{
    // Summary:
    //   Data used to block input symbol from firing event if matched

    SInputBlockData(const EKeyId keyId_,
        const float fBlockDuration_,
        const bool bAllDeviceIndices_ = true,
        const uint8 deviceIndex_ = 0)
        : keyId(keyId_)
        , fBlockDuration(fBlockDuration_)
        , bAllDeviceIndices(bAllDeviceIndices_)
        , deviceIndex(deviceIndex_)
    {
    }

    float                       fBlockDuration;         // How long will still be blocked for
    const EKeyId        keyId;                          // External id for fast comparison.
    uint8                       deviceIndex;                // Device index - controller 1/2 etc
    bool                        bAllDeviceIndices;  // True to block all device indices of deviceID type, otherwise uses deviceIndex
};

// Description:
//   Input event listeners registered to input system and receive input events when they are generated.
struct IInputEventListener
{
    // <interfuscator:shuffle>
    virtual ~IInputEventListener(){}
    // Summary:
    //   Called every time input event is generated.
    // Return Value:
    //   If it returns True then the broadcasting of this event should be aborted and the rest of input
    //   listeners should not receive this event.
    virtual bool OnInputEvent(const SInputEvent& event) = 0;
    virtual bool OnInputEventUI(const SUnicodeEvent& event) { return false; }

    // Summary:
    //   Used to sort the listeners based on priority
    // Return Value:
    //   It returns priority associated with the listener (Higher the priority, the earlier
    //   it will be processed relative to other listeners, default = 0)
    virtual int GetPriority() const { return 0; }
    // </interfuscator:shuffle>
};

struct IFFParams
{
    EInputDeviceType deviceType;
    EFFEffectId effectId;
    float strengthA, strengthB;
    float timeInSeconds;
    SFFTriggerOutputData triggerData;

    IFFParams()
        : strengthA(0)
        , strengthB(0)
        , timeInSeconds(0)
        , triggerData(SFFTriggerOutputData::Initial::ZeroIt)
    {
        effectId = eFF_Rumble_Basic;
        deviceType = eIDT_Unknown;
    }
};

//////////////////////////////////////////////////////////////////////////
///Natural Point Device Interfaces

//////////////////////////////////////////////////////////////////////////
struct NP_RawData
{
    float fNPRoll;
    float fNPPitch;
    float fNPYaw;

    float fNPX;
    float fNPY;
    float fNPZ;
};

//////////////////////////////////////////////////////////////////////////
struct INaturalPointInput
{
    // <interfuscator:shuffle>
    virtual ~INaturalPointInput(){};

    virtual bool Init() = 0;
    virtual void Update() = 0;
    virtual bool IsEnabled() = 0;

    virtual void Recenter() = 0;

    // Summary;:
    //      Get raw skeleton data
    virtual bool GetNaturalPointData(NP_RawData& npRawData) const = 0;
    // </interfuscator:shuffle>
};

//////////////////////////////////////////////////////////////////////////
typedef uint64 TInputDeviceId;
#define InvalidInputDeviceId 0

struct IInputDevice
{
    // <interfuscator:shuffle>
    // Summary:
    //   Implements virtual destructor just for safety.
    virtual ~IInputDevice(){}

    virtual const char* GetDeviceCommonName() const { return GetDeviceName(); }
    virtual const char* GetDeviceName() const   = 0;
    virtual EInputDeviceType GetDeviceType() const  = 0;
    virtual TInputDeviceId GetDeviceId() const = 0;
    virtual int GetDeviceIndex() const = 0;
    // Initialization.
    virtual bool    Init() = 0;
    virtual void  PostInit() = 0;
    // Update.
    virtual void    Update(bool bFocus) = 0;
    // Summary:
    //   Sets force feedback .
    // Return Value:
    //   True if successful.
    virtual bool SetForceFeedback(IFFParams params) = 0;
    // Summary:
    //   Checks for key pressed and held.
    virtual bool    InputState(const TKeyName& key, EInputState state) = 0;
    // Summary:
    //   Sets/unsets DirectInput to exclusive mode.
    virtual bool    SetExclusiveMode(bool value) = 0;
    // Summary:
    //   Clears the key (pressed) state.
    virtual void    ClearKeyState() = 0;
    // Summary:
    //   Clears analog position state.
    virtual void    ClearAnalogKeyState(TInputSymbols& clearedSymbols) = 0;
    // Summary:
    //   Called upon creation to assign a process wide unique Id.
    virtual void SetUniqueId(uint8 const uniqueId) = 0;
    virtual const char* GetKeyName(const SInputEvent& event) const = 0;
    virtual const char* GetKeyName(const EKeyId keyId) const = 0;
    virtual char GetInputCharAscii(const SInputEvent& event) = 0;
    virtual const char* GetOSKeyName(const SInputEvent& event) = 0;
    virtual SInputSymbol* LookupSymbol(EKeyId id) const = 0;
    virtual const SInputSymbol* GetSymbolByName(const char* name) const = 0;
    virtual bool IsOfDeviceType(EInputDeviceType type) const = 0;
    virtual void Enable(bool enable) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void OnLanguageChange() = 0;
    //! Dead zone settings for input devices where this is relevant (i.e. controllers with analog sticks)
    virtual void SetDeadZone(float fThreshold) = 0; // between 0 and 1
    virtual void RestoreDefaultDeadZone() = 0;

    typedef std::map<uint32, SInputSymbol*> TDevSpecIdToSymbolMap;
    virtual const TDevSpecIdToSymbolMap& GetInputToSymbolMappings() const = 0;
    // </interfuscator:shuffle>
};

// Description:
//   Interface to the Input system.
//   The input system give access and initialize Keyboard,Mouse and Joystick SubSystems.
// Summary:
//   Main Input system interface.
struct IInput
{
    // <interfuscator:shuffle>
    virtual ~IInput(){}
    // Summary:
    //   Registers new input events listener.
    virtual void AddEventListener(IInputEventListener* pListener) = 0;
    virtual void RemoveEventListener(IInputEventListener* pListener) = 0;

    // Description:
    //   Registers new console input event listeners. console input listeners receive all events, no matter what.
    virtual void AddConsoleEventListener(IInputEventListener* pListener) = 0;
    virtual void RemoveConsoleEventListener(IInputEventListener* pListener) = 0;

    virtual bool IsMotionSensorDataAvailable(EMotionSensorFlags sensorFlags) const = 0;
    virtual bool AddMotionSensorEventListener(IMotionSensorEventListener* pListener, EMotionSensorFlags sensorFlags = eMSF_AllAvailable) = 0;
    virtual void RemoveMotionSensorEventListener(IMotionSensorEventListener* pListener, EMotionSensorFlags sensorFlags = eMSF_AllAvailable) = 0;

    virtual void ActivateMotionSensors(EMotionSensorFlags sensorFlags = eMSF_AllAvailable) = 0;
    virtual void DeactivateMotionSensors(EMotionSensorFlags sensorFlags = eMSF_AllAvailable) = 0;

    virtual void PauseMotionSensorInput() = 0;
    virtual void UnpauseMotionSensorInput() = 0;

    virtual void SetMotionSensorUpdateInterval(float seconds, bool force = false) = 0;
    virtual void SetMotionSensorFilter(IMotionSensorFilter* filter) = 0;
    virtual const SMotionSensorData* GetMostRecentMotionSensorData() const = 0;

    // Description:
    //   Registers an exclusive listener which has the ability to filter out events before they arrive at the normal
    //   listeners.
    virtual void SetExclusiveListener(IInputEventListener* pListener) = 0;
    virtual IInputEventListener* GetExclusiveListener() = 0;

    virtual bool AddInputDevice(IInputDevice* pDevice) = 0;
    virtual bool AddAzToLyInputDevice(EInputDeviceType cryInputDeviceType,
                                      const char* cryInputDeviceDisplayName,
                                      const std::vector<SInputSymbol>& azToCryInputSymbols,
                                      const AzFramework::InputDeviceId& azFrameworkInputDeviceId) = 0;
    virtual void EnableEventPosting (bool bEnable) = 0;
    virtual bool IsEventPostingEnabled () const = 0;
    virtual void PostInputEvent(const SInputEvent& event, bool bForce = false) = 0;
    virtual void PostMotionSensorEvent(const SMotionSensorEvent& event, bool bForce = false) = 0;
    virtual void PostUnicodeEvent(const SUnicodeEvent& event, bool bForce = false) = 0;

    // Description:
    //   For direct key processing (e.g. win proc functions)
    //   currently only used by some consoles
    virtual void ProcessKey(uint32 key, bool pressed, wchar_t unicode, bool repeat) = 0;

    // Description:
    //   Posts a force feedback / rumble output event.
    virtual void ForceFeedbackEvent(const SFFOutputEvent& event) = 0;
    // Description:
    //   Sets a filter so that only one device can output force feedback
    virtual void ForceFeedbackSetDeviceIndex(int index) = 0;

    // Summary
    //   Initializes input system.
    // Note:
    //   Required params should be passed through constructor
    virtual bool    Init() = 0;
    // Summary
    //   Post Initialization called at end of initialization
    virtual void  PostInit() = 0;
    // Description:
    //   Updates Keyboard, Mouse and Joystick. Sets bFocus to true if window has focus and input is enabled.
    virtual void    Update(bool bFocus) = 0;
    // Summary:
    //   Clears all subsystems.
    virtual void    ShutDown() = 0;

    // See also:
    //   IInputDevice::SetExclusive
    virtual void    SetExclusiveMode(EInputDeviceType deviceType, bool exclusive, void* hwnd = 0) = 0;

    // See also:
    //   IInputDevice::InputState
    virtual bool    InputState(const TKeyName& key, EInputState state) = 0;

    // Description:
    //   Converts an input event to the key name. The function should internally dispatch to all managed
    //   input devices and return the first recognized event.
    // Arguments:
    //   event - Input event to translate into a name.
    virtual const char* GetKeyName(const SInputEvent& event) const = 0;

    // Description:
    //   Converts an input event to the key name. The function should internally dispatch to all managed
    //   input devices and return the first recognized keyId.
    // Arguments:
    //   keyId - Input keyId to translate into a name.
    // Return Value:
    //   Translated key name
    virtual const char* GetKeyName(EKeyId keyId) const = 0;

    // Description:
    //   Gets an input char translated to ascii from the event. The function should internally dispatch to all managed
    //   input devices and return the first recognized event.
    // Arguments:
    //   event - Input event to translate into a name.
    virtual char GetInputCharAscii(const SInputEvent& event) = 0;

    // Summary:
    //   Lookups a symbol for a given symbol and key ids.
    virtual SInputSymbol* LookupSymbol(EInputDeviceType deviceType, int deviceIndex, EKeyId keyId) = 0;

    // Summary:
    //   Looks up a symbol for a key name
    virtual const SInputSymbol* GetSymbolByName(const char* name) const = 0;

    // Summary:
    //   Gets OS Keyname.
    // Arguments:
    //   event - Input event to translate into a name.
    virtual const char* GetOSKeyName(const SInputEvent& event) = 0;

    // Summary:
    //   Clears key states of all devices.
    virtual void ClearKeyState() = 0;

    // Summary:
    //   Clears analog key states of all devices.
    virtual void ClearAnalogKeyState() = 0;

    // Summary:
    //   Re-triggers pressed keys.
    // Note:
    //   Used for transitioning action maps.
    virtual void RetriggerKeyState() = 0;

    // Summary:
    //   Gets if we are currently re-triggering.
    // Note:
    //   Needed to filter out actions.
    virtual bool Retriggering() = 0;

    // Description:
    //   Queries to see if this machine has some kind of input device connected.
    virtual bool HasInputDeviceOfType(EInputDeviceType type) = 0;

    // Summary:
    //   Gets the currently pressed modifiers.
    virtual int GetModifiers() const = 0;

    // Summary:
    //   Tells devices whether to report input or not.
    virtual void EnableDevice(EInputDeviceType deviceType, bool enable) = 0;

    //! Dead zone settings for input devices where this is relevant (i.e. controllers with analog sticks)
    virtual void SetDeadZone(float fThreshold) = 0; // between 0 and 1
    virtual void RestoreDefaultDeadZone() = 0;

    // Summary:
    //   Gets the device at an index for a specific Type
    // Return value:
    //   Pointer to device, or NULL if not found/valid
    virtual IInputDevice* GetDevice(uint16 id, EInputDeviceType deviceType) = 0;

    // Summary:
    //   Returns EInputPlatformFlags
    virtual uint32 GetPlatformFlags() const = 0;

    // Summary:
    //   Adds or updates blocking input if SInputBlockData::fBlockDuration exceeds previously blocking time
    // Return Value:
    //   True if successfully added inputBlocker or updated existed input blocker's remaining blocking time
    virtual bool SetBlockingInput(const SInputBlockData& inputBlockData) = 0;

    // Summary:
    //   Removes blocking input
    // Return Value:
    //   True if found and removed, false otherwise
    virtual bool RemoveBlockingInput(const SInputBlockData& inputBlockData) = 0;

    // Summary:
    //   Checks if the specified input is currently being blocked
    // Return Value:
    //   True if specified input is currently being blocked, false otherwise
    virtual bool HasBlockingInput(const SInputBlockData& inputBlockData) const = 0;

    // Summary:
    //   Gets the number of inputs currently being blocked
    // Return Value:
    //   Number of inputs currently being blocked
    virtual int GetNumBlockingInputs() const = 0;

    // Summary:
    //   Clears all the inputs being blocked
    // Return Value:
    //   None
    virtual void ClearBlockingInputs() = 0;

    // Summary:
    //   Checks if the input specified should be blocked
    // Return Value:
    //   True if input specified should be blocked, false otherwise
    virtual bool ShouldBlockInputEventPosting(const EKeyId keyId, const EInputDeviceType    deviceType, const uint8 deviceIndex) const = 0;

    // Summary:
    //   Informs the input system that text input is expected.
    //   Activates the virtual keyboard on mobile systems.
    //   Should be paired with a call to StopTextInput.
    // Return Value:
    //   None
    virtual void StartTextInput(const Vec2& inputRectTopLeft = ZERO, const Vec2& inputRectBottomRight = ZERO) = 0;

    // Summary:
    //   Informs the input system that text input is no longer expected.
    //   Deactivates the virtual keyboard on mobile systems.
    //   Should be paired with a call to StartTextInput.
    // Return Value:
    //   None
    virtual void StopTextInput() = 0;

    // Summary:
    //   Checks if the on-screen keyboard is being shown.
    // Return Value:
    //   True if the on-screen keyboard is being shown, false otherwise
    virtual bool IsScreenKeyboardShowing() const = 0;

    // Summary:
    //   Checks if the current system supports an on-screen keyboard.
    // Return Value:
    //   True if on-screen keyboard is supported, false otherwise
    virtual bool IsScreenKeyboardSupported() const = 0;

    //////////////////////////////////////////////////////////////////////////
    // TrackIR
    virtual INaturalPointInput* GetNaturalPointInput() = 0;

    //////////////////////////////////////////////////////////////////////////
    virtual bool GrabInput(bool bGrab) = 0;

    //////////////////////////////////////////////////////////////////////////
    /// Enumerate registered devices
    typedef std::vector<IInputDevice*> TInputDevices;
    virtual const TInputDevices& GetInputDevices() const = 0;
    // </interfuscator:shuffle>
};

class CryLegacyInputRequests
    : public AZ::EBusTraits
{
public:
    //////////////////////////////////////////////////////////////////////////
    // EBusTraits overrides
    static const AZ::EBusHandlerPolicy HandlerPolicy = AZ::EBusHandlerPolicy::Single;
    static const AZ::EBusAddressPolicy AddressPolicy = AZ::EBusAddressPolicy::Single;
    //////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////////////////////////////////////
    // Creates and initializes a legacy IInput instance
    virtual IInput* InitInput() = 0;

    //////////////////////////////////////////////////////////////////////////
    // Shuts down and destroys a legacy IInput instance
    virtual void ShutdownInput(IInput* input) = 0;
};
using CryLegacyInputRequestBus = AZ::EBus<CryLegacyInputRequests>;

#ifdef __cplusplus
extern "C" {
#endif

typedef IInput  (*  CRY_PTRCREATEINPUTFNC(ISystem * pSystem, void* hwnd));

CRYINPUT_API IInput* CreateInput(ISystem* pSystem, void* hwnd);

#ifdef __cplusplus
};
#endif
