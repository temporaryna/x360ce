/*  x360ce - XBOX360 Controler Emulator
 *  Copyright (C) 2002-2010 ToCA Edit
 *
 *  x360ce is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  x360ce is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with x360ce.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "globals.h"
#include "utils.h"
#include "config.h"
#include "directinput.h"

extern HWND hWnd;
BOOL bEnabled = FALSE;
BOOL bUseEnabled= FALSE;

DWORD dwlastUserIndex = (DWORD) -1;
extern WORD wNativeMode;

VOID LoadOriginalDll(VOID)
{

	TCHAR buffer[MAX_PATH];

	// Getting path to system dir and to xinput1_3.dll
	GetSystemDirectory(buffer,MAX_PATH);

	// Append dll name
	_tcscat_s(buffer,sizeof(buffer),_T("\\xinput1_3.dll"));

	// try to load the system's dinput.dll, if pointer empty
	if (!hNativeInstance) hNativeInstance = ::LoadLibrary(buffer);

	// Debug
	if (!hNativeInstance)
	{
		ExitProcess(0); // exit the hard way
	}
}

HRESULT XInit(DWORD dwUserIndex){

	HRESULT hr=ERROR_DEVICE_NOT_CONNECTED;
	if( !Gamepad[dwUserIndex].vid ||  !Gamepad[dwUserIndex].pid ) return hr;

	if(
		Gamepad[dwUserIndex].g_pGamepad == NULL	&&	//and already gamepad is not enumerated
		dwUserIndex != dwlastUserIndex				//only on-change
		)
	{ 

		if(bInitBeep)
		{
			MessageBeep(MB_OK);
		}

		WriteLog(_T("Initializing Gamepad %d"),dwUserIndex+1);
		WriteLog(_T("User ID: %d, Last User ID: %d"),dwUserIndex,dwlastUserIndex);

		hr = Enumerate(dwUserIndex); //enumerate when is more configured devices than created
		if(SUCCEEDED(hr))
		{
				if(bInitBeep)
	{
		MessageBeep(MB_OK);
	}
			WriteLog(_T("[PAD%d] Enumeration finished"),dwUserIndex+1);
		}
		if(FAILED(hr)) return ERROR_DEVICE_NOT_CONNECTED;

		hr = InitDirectInput(hWnd,dwUserIndex);
		if(FAILED(hr))
		{
			WriteLog(_T("InitDirectInput fail (1) with %s"),DXErrStr(hr));
		}
	}
	else return ERROR_DEVICE_NOT_CONNECTED;

	dwlastUserIndex = dwUserIndex;

	return S_OK;
}

extern "C" DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	//WriteLog(_T("XInputGetState"));
	if(Gamepad[dwUserIndex].native) 
	{
		if(!hNativeInstance) LoadOriginalDll();
		typedef DWORD (WINAPI* XInputGetState_Type)(DWORD dwUserIndex, XINPUT_STATE* pState);
		XInputGetState_Type nativeXInputGetState = (XInputGetState_Type) GetProcAddress( hNativeInstance, "XInputGetState");
		return nativeXInputGetState(dwUserIndex,pState);
	}

	if (!pState || dwUserIndex >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS; 

	HRESULT hr=ERROR_DEVICE_NOT_CONNECTED;

	/*
	Nasty trick to support XInputEnable states, because not every game calls it so:
	- must support games that use it, and do enable/disable as needed by game
		if bEnabled is FALSE and bUseEnabled is TRUE = gamepad is disabled -> return fake S_OK
		if bEnabled is TRUE and bUseEnabled is TRUE = gamepad is enabled -> continue 
	- must support games that not call it
		if bUseEnabled is FALSE ie. XInputEnable was not call -> do not care about XInputEnable states 
	*/

	if(!bEnabled && bUseEnabled) return S_OK;

	hr = XInit(dwUserIndex);

	if(Gamepad[dwUserIndex].g_pGamepad == NULL) return ERROR_DEVICE_NOT_CONNECTED;

	GamepadMap PadMap = GamepadMapping[dwUserIndex];

	XINPUT_GAMEPAD &xGamepad = pState->Gamepad;

	xGamepad.wButtons = 0;
	xGamepad.bLeftTrigger = 0;
	xGamepad.bRightTrigger = 0;
	xGamepad.sThumbLX = 0;
	xGamepad.sThumbLY = 0;
	xGamepad.sThumbRX = 0;
	xGamepad.sThumbRY = 0;

	// poll data from device
	hr = UpdateState(dwUserIndex);

	// timestamp packet
	pState->dwPacketNumber=GetTickCount();

	// --- Map buttons ---
	for (INT i = 0; i < 10; ++i) {
		if ((PadMap.Button[i] >= 0)
			&&	ButtonPressed(PadMap.Button[i],dwUserIndex)
			) {
				xGamepad.wButtons |= buttonIDs[i];
		}
	}

	// --- Map POV to the D-pad ---
	if (PadMap.DpadPOV >= 0 ) {

		//INT pov = POVState(PadMap.DpadPOV,dwUserIndex,Gamepad[dwUserIndex].povrotation);

		DWORD pov = Gamepad[dwUserIndex].state.rgdwPOV[PadMap.DpadPOV];
		DWORD povdeg = pov/100;

		if (IN_RANGE(povdeg,270,360) || IN_RANGE(povdeg,0,90) || povdeg == 0 ) // Up-left, up, up-right, up (at 360 degrees)
		{
			xGamepad.wButtons |= PadMap.pov[0];
		}

		if (IN_RANGE(povdeg,0,180)) // Up-right, right, down-right
		{	
			xGamepad.wButtons |= PadMap.pov[3];	
		}

		if (IN_RANGE(povdeg,90,270)) // Down-right, down, down-left
		{	
			xGamepad.wButtons |= PadMap.pov[1];
		}

		if (IN_RANGE(povdeg,180,360)) // Down-left, left, up-left
		{	
			xGamepad.wButtons |= PadMap.pov[2];
		}

	}

	// Created so we can refer to each axis with an ID
	LONG axis[7] = {
		Gamepad[dwUserIndex].state.lX,
		Gamepad[dwUserIndex].state.lY,
		Gamepad[dwUserIndex].state.lZ,
		Gamepad[dwUserIndex].state.lRx,
		Gamepad[dwUserIndex].state.lRy,
		Gamepad[dwUserIndex].state.lRz,
		0
	};
	LONG slider[2] = {
		Gamepad[dwUserIndex].state.rglSlider[0],
		Gamepad[dwUserIndex].state.rglSlider[1]
	};

	// --- Map triggers ---
	BYTE *targetTrigger[2] = {
		&xGamepad.bLeftTrigger,
		&xGamepad.bRightTrigger
	};

	for (INT i = 0; i < 2; ++i) {

		MappingType triggerType = PadMap.Trigger[i].type;

		if (triggerType == DIGITAL) {
			if (ButtonPressed(PadMap.Trigger[i].id,dwUserIndex))
			{	*(targetTrigger[i]) = 255;		}
		} else {
			LONG *values;
			switch (triggerType) {
				case AXIS:
				case HAXIS:


					values = axis;
					break;


				case SLIDER:
				case HSLIDER:

					values = slider;
					break;
				default:
					values = axis;
					break;
			}

			LONG v = (PadMap.Trigger[i].id > 0 ?
				values[PadMap.Trigger[i].id -1] :
			-values[-PadMap.Trigger[i].id -1] - 1);

			/*
				--- v is the full range (-32768 .. +32767) that should be projected to 0...255

				--- Full ranges
				AXIS:	(	0 to 255 from -32768 to 32767) using axis
				SLIDER:	(	0 to 255 from -32768 to 32767) using slider
				------------------------------------------------------------------------------
				--- Half ranges
				HAXIS:	(	0 to 255 from 0 to 32767) using axis
				HSLIDER:	(	0 to 255 from 0 to 32767) using slider
			*/

			LONG v2=0;
			LONG offset=0;
			LONG scaling=1;


			switch (triggerType) {
				// Full range
				case AXIS:
				case SLIDER:
					scaling = 256; offset = 32768;
					break;
					// Half range
				case HAXIS:
				case HSLIDER:
					scaling = 128; offset = 0;
					break;
				default:
					scaling = 1; offset = 0;
					break;
			}

			v2 = (v + offset) / scaling;

			// Add deadzones
			*(targetTrigger[i]) = (BYTE) deadzone(v2, 0, 255, Gamepad[dwUserIndex].tdeadzone, 255);

		}
	}

	// --- Map thumbsticks ---

	// Created so we can refer to each axis with an ID
	SHORT *targetAxis[4] = {
		&xGamepad.sThumbLX,
		&xGamepad.sThumbLY,
		&xGamepad.sThumbRX,
		&xGamepad.sThumbRY
	};

	// NOTE: Could add symbolic constants as indexers, such as 
	// THUMB_LX_AXIS, THUMB_LX_POSITIVE, THUMB_LX_NEGATIVE
	if(Gamepad[dwUserIndex].axistodpad==0)
	{


		for (INT i = 0; i < 4; ++i) {
			LONG *values = axis;
			// Analog input
			if (PadMap.Axis[i].analogType == AXIS) values = axis;
			if (PadMap.Axis[i].analogType == SLIDER) values = slider;
			if (PadMap.Axis[i].analogType != NONE)
			{

				if(PadMap.Axis[i].id > 0 )
				{
					*(targetAxis[i]) = (SHORT) values[PadMap.Axis[i].id - 1];
					*(targetAxis[i])= (SHORT) clamp(*(targetAxis[i]),-32768,32767);
				}
				else if(PadMap.Axis[i].id < 0 )
				{
					LONG val = -values[-PadMap.Axis[i].id - 1];

					if(val > 0) val = 32767 * val / 32768;
					else val = 32768 * val / 32767; 
					*(targetAxis[i]) = (SHORT) val;
				}


			}

			// Digital input, positive direction
			if (PadMap.Axis[i].hasDigital && PadMap.Axis[i].positiveButtonID >= 0) {

				if (ButtonPressed(PadMap.Axis[i].positiveButtonID,dwUserIndex))
					*(targetAxis[i]) = 32767;	
			}	
			// Digital input, negative direction
			if (PadMap.Axis[i].hasDigital && PadMap.Axis[i].negativeButtonID >= 0) {

				if (ButtonPressed(PadMap.Axis[i].negativeButtonID,dwUserIndex))
					*(targetAxis[i]) = -32768;
			}	
		}
	}

		//WILDS - Axis to D-Pad
		if(Gamepad[dwUserIndex].axistodpad==1)
		{
			//WriteLog("x: %d, y: %d, z: %d",Gamepad[dwUserIndex].state.lX,Gamepad[dwUserIndex].state.lY,Gamepad[dwUserIndex].state.lZ);

			if(Gamepad[dwUserIndex].state.lX - Gamepad[dwUserIndex].axistodpadoffset > Gamepad[dwUserIndex].axistodpaddeadzone)
				xGamepad.wButtons |= XINPUT_GAMEPAD_LEFT_THUMB;
			if(Gamepad[dwUserIndex].state.lX - Gamepad[dwUserIndex].axistodpadoffset < -Gamepad[dwUserIndex].axistodpaddeadzone)
				xGamepad.wButtons |= XINPUT_GAMEPAD_RIGHT_THUMB;

			if(Gamepad[dwUserIndex].state.lY - Gamepad[dwUserIndex].axistodpadoffset < -Gamepad[dwUserIndex].axistodpaddeadzone)
				xGamepad.wButtons |= XINPUT_GAMEPAD_DPAD_UP;
			if(Gamepad[dwUserIndex].state.lY - Gamepad[dwUserIndex].axistodpadoffset > Gamepad[dwUserIndex].axistodpaddeadzone)
				xGamepad.wButtons |= XINPUT_GAMEPAD_DPAD_DOWN;
		}
		//WILDS END

		// --- Do Linears ---
		// TODO

		return hr;
}

extern "C" DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{

	if(Gamepad[dwUserIndex].native) 
	{
		if(!hNativeInstance) LoadOriginalDll();
		typedef DWORD (WINAPI* XInputSetState_Type)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
		XInputSetState_Type nativeXInputSetState = (XInputSetState_Type) GetProcAddress( hNativeInstance, "XInputSetState");
		return nativeXInputSetState(dwUserIndex,pVibration);
	}

	if (!pVibration || dwUserIndex >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS; 

	if(!bEnabled && bUseEnabled) return S_OK;

	HRESULT hr=ERROR_SUCCESS;
	if(!bEnabled && bUseEnabled) return S_OK;

	hr = XInit(dwUserIndex);

	HRESULT hrLeftForce = S_FALSE;
	HRESULT hrRightForce = S_FALSE;

	if(Gamepad[dwUserIndex].g_pGamepad == NULL) return ERROR_DEVICE_NOT_CONNECTED;
	if(!Gamepad[dwUserIndex].useforce) return ERROR_SUCCESS;

	WORD wLeftMotorSpeed = 0;
	WORD wRightMotorSpeed = 0;

	if(NULL == Gamepad[dwUserIndex].g_pEffect[0]) hrLeftForce = PrepareForce(dwUserIndex,Gamepad[dwUserIndex].wLMotorDirection);
	if(NULL == Gamepad[dwUserIndex].g_pEffect[1]) hrRightForce = PrepareForce(dwUserIndex,Gamepad[dwUserIndex].wRMotorDirection);


	if(FAILED(hrLeftForce))WriteLog(_T("PrepareForce for pad %d failed with code hrLeftForce = %s"), dwUserIndex, DXErrStr(hr));
	if(FAILED(hrRightForce))WriteLog(_T("PrepareForce for pad %d failed with code hrRightForce = %s"), dwUserIndex, DXErrStr(hr));


	//Modified by Racer_S 9/20/2009
	if(Gamepad[dwUserIndex].swapmotor)
	{
		wRightMotorSpeed = (WORD)((FLOAT)pVibration->wLeftMotorSpeed * Gamepad[dwUserIndex].forcepercent);
		wLeftMotorSpeed =  (WORD)((FLOAT)pVibration->wRightMotorSpeed * Gamepad[dwUserIndex].forcepercent);
	}
	else
	{

		wLeftMotorSpeed =  (WORD)((FLOAT)pVibration->wLeftMotorSpeed * Gamepad[dwUserIndex].forcepercent);
		wRightMotorSpeed = (WORD)((FLOAT)pVibration->wRightMotorSpeed * Gamepad[dwUserIndex].forcepercent);
	}

	// wLeftMotorSpeed

	if(SUCCEEDED(hrLeftForce))hr = SetDeviceForces(dwUserIndex,wLeftMotorSpeed,0);
	if(FAILED(hr))WriteLog(_T("SetDeviceForces for pad %d failed with code HR = %s"), dwUserIndex, DXErrStr(hr));


	// wRightMotorSpeed

	if(SUCCEEDED(hrRightForce))hr = SetDeviceForces(dwUserIndex,wRightMotorSpeed,1);
	if(FAILED(hr))WriteLog(_T("SetDeviceForces for pad %d failed with code HR = %s"), dwUserIndex, DXErrStr(hr));

	return ERROR_SUCCESS;
}

extern "C" DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities)
{
	if(Gamepad[dwUserIndex].native) 
	{
		if(!hNativeInstance) LoadOriginalDll();
		typedef DWORD (WINAPI* XInputGetCapabilities_Type)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
		XInputGetCapabilities_Type nativeXInputGetCapabilities = (XInputGetCapabilities_Type) GetProcAddress( hNativeInstance, "XInputGetCapabilities");
		return nativeXInputGetCapabilities(dwUserIndex,dwFlags,pCapabilities);
	}

	if (!pCapabilities || dwUserIndex >= XUSER_MAX_COUNT) return ERROR_BAD_ARGUMENTS; 

	if(!Gamepad[dwUserIndex].connected) return ERROR_DEVICE_NOT_CONNECTED;

	// Dump from original x360 controller
	XINPUT_GAMEPAD xGamepad;
	xGamepad.bLeftTrigger = (BYTE)0xFF;
	xGamepad.bRightTrigger = (BYTE)0xFF;
	xGamepad.sThumbLX = (SHORT) 4294967232;
	xGamepad.sThumbLY = (SHORT) 4294967232;
	xGamepad.sThumbRX = (SHORT) 4294967232;
	xGamepad.sThumbRY = (SHORT) 4294967232;
	xGamepad.wButtons = (WORD)  62463;

	XINPUT_VIBRATION Vibration = {(BYTE)0xFF,(BYTE)0xFF};					//this should be max WORD val, but in dump is max BYTE val

	pCapabilities->Flags = (WORD) 4;
	pCapabilities->SubType=(BYTE)Gamepad[dwUserIndex].gamepadtype;
	pCapabilities->Gamepad = xGamepad;
	pCapabilities->Vibration = Vibration;
	pCapabilities->Type = (BYTE) 0;											//strange because spec says 1, but in dump this is 0

	WriteLog(_T("XInputGetCapabilities send type %i"),(BYTE)Gamepad[dwUserIndex].gamepadtype);

	return ERROR_SUCCESS;
}

extern "C" VOID WINAPI XInputEnable(BOOL enable)
{
	if(wNativeMode) 
	{
		if(!hNativeInstance) LoadOriginalDll();
		typedef VOID (WINAPI* XInputEnable_Type)(BOOL enable);
		XInputEnable_Type nativeXInputEnable = (XInputEnable_Type) GetProcAddress( hNativeInstance, "XInputSetState");
		return nativeXInputEnable(enable);
	}

	WriteLog(_T("XInputEnable called, state %d"),enable);

	bEnabled = enable;
	bUseEnabled = TRUE;

}

extern "C" DWORD WINAPI XInputGetDSoundAudioDeviceGuids
(
 DWORD dwUserIndex,          // [in] Index of the gamer associated with the device
 GUID* pDSoundRenderGuid,    // [out] DSound device ID for render
 GUID* pDSoundCaptureGuid    // [out] DSound device ID for capture
 )
{
	UNREFERENCED_PARAMETER(pDSoundRenderGuid);
	UNREFERENCED_PARAMETER(pDSoundCaptureGuid);

	if(Gamepad[dwUserIndex].g_pGamepad == NULL) return ERROR_DEVICE_NOT_CONNECTED;
	return ERROR_SUCCESS;
}

extern "C" DWORD WINAPI XInputGetBatteryInformation
(
 DWORD                       dwUserIndex,        // [in]  Index of the gamer associated with the device
 BYTE                        devType,            // [in]  Which device on this user index
 XINPUT_BATTERY_INFORMATION* pBatteryInformation // [out] Contains the level and types of batteries
 )

{
	UNREFERENCED_PARAMETER(devType);

	if(Gamepad[dwUserIndex].g_pGamepad == NULL) return ERROR_DEVICE_NOT_CONNECTED;

	// Report a wired controller
	pBatteryInformation->BatteryType = BATTERY_TYPE_WIRED;
	return ERROR_SUCCESS;

}

extern "C" DWORD WINAPI XInputGetKeystroke
(
 DWORD dwUserIndex,              // [in]  Index of the gamer associated with the device
 DWORD dwReserved,               // [in]  Reserved for future use
 PXINPUT_KEYSTROKE pKeystroke    // [out] Pointer to an XINPUT_KEYSTROKE structure that receives an input event.
 )
{

	if(Gamepad[dwUserIndex].g_pGamepad == NULL) return ERROR_DEVICE_NOT_CONNECTED;

	pKeystroke->Flags = NULL;
	pKeystroke->HidCode = NULL;
	pKeystroke->Unicode = NULL;
	pKeystroke->UserIndex = NULL;
	dwReserved=NULL;

	return ERROR_SUCCESS;
}