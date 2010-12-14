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
#include "InputHook.h"

InputHook_CONIFG* InputHookConfig = NULL;
InputHook_GAMEPAD_CONIFG* GamepadConfig[4] = {NULL,NULL,NULL,NULL};

ULONG ACLEntries[1] = {0};

InputHook_CONIFG* InputHook_Config()
{
	return InputHookConfig;
}

InputHook_GAMEPAD_CONIFG* InputHook_GamepadConfig(DWORD dwUserIndex)
{
	return GamepadConfig[dwUserIndex];
}

BOOL InputHook_Enable(BOOL state)
{
	InputHook_Config()->bEnabled = state;
	return state;
}

BOOL InputHook_Enable()
{
	return InputHook_Config()->bEnabled;
}

BOOL InputHook_Init(InputHook_CONIFG* fconfig, InputHook_GAMEPAD_CONIFG* gconfig)
{

	if(!fconfig) return FALSE;

	InputHookConfig = fconfig;

	if(!fconfig->bEnabled) return FALSE;

	for(WORD i = 0; i < 4; i++) {

		GamepadConfig[i] = gconfig;
		gconfig++;

		if(!IsEqualGUID(InputHook_GamepadConfig(i)->productGUID, GUID_NULL) && !IsEqualGUID(InputHook_GamepadConfig(i)->instanceGUID, GUID_NULL))
		{
			if(!InputHook_GamepadConfig(i)->dwVID) InputHook_GamepadConfig(i)->dwVID = LOWORD(InputHook_GamepadConfig(i)->productGUID.Data1);
			if(!InputHook_GamepadConfig(i)->dwPID) InputHook_GamepadConfig(i)->dwPID = HIWORD(InputHook_GamepadConfig(i)->productGUID.Data1);
		}
		else InputHook_GamepadConfig(i)->bEnabled = FALSE;
	}
	
	if(InputHook_Config()->bEnabled) {
		HookWMI();
		if(InputHook_Config()->dwHookMode >= 2) HookDI();
		if(InputHook_Config()->dwHookWinTrust) HookWinTrust();
	}

	return TRUE;
}

BOOL InputHook_Clean()
{
	LhUninitializeLibrary();

	HookWMIClean();
	HookDIClean();
	HookWinTrustClean();

	return TRUE;
}
