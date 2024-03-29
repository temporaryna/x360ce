/*  x360ce - XBOX360 Controller Emulator
 *
 *  https://code.google.com/p/x360ce/
 *
 *  Copyright (C) 2002-2010 Racer_S
 *  Copyright (C) 2010-2013 Robert Krawczyk
 *
 *  x360ce is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Foundation,
 *  either version 3 of the License, or any later version.
 *
 *  x360ce is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with x360ce.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _InputHook_H_
#define _InputHook_H_

#include <CGuid.h>
#include <vector>
#include <MinHook.h>
#include "Logger.h"

#if _MSC_VER < 1700
#include "mutex.h"
#else
#include <mutex>
#endif

class iHookDevice
{
public:
	iHookDevice(DWORD userindex, const GUID& productid, const GUID& instanceid)
		:m_enabled(true)
		, m_productid(productid)
		, m_instanceid(instanceid)
		, m_userindex(userindex)
	{}
	virtual ~iHookDevice() {};

	inline void Enable()
	{
		m_enabled = true;
	}

	inline void Disable()
	{
		m_enabled = false;
	}

	inline bool GetHookState()
	{
		return m_enabled;
	}

	inline GUID GetProductGUID()
	{
		return m_productid;
	}

	inline GUID GetInstanceGUID()
	{
		return m_instanceid;
	}

	inline DWORD GetProductPIDVID()
	{
		return m_productid.Data1;
	}

	inline DWORD GetUserIndex()
	{
		return m_userindex;
	}

private:
	bool  m_enabled;
	GUID  m_productid;
	GUID  m_instanceid;
	DWORD m_userindex;
};

class iHook
{
private:
#if _MSC_VER < 1700
	recursive_mutex m_mutex;
#else
	std::mutex m_mutex;
#endif

public:
	iHook()
		:m_hookmask(0x80000000)
		, m_fakepidvid(MAKELONG(0x045E, 0x028E))
		, m_timeout(30)
	{
	}
	virtual ~iHook()
	{
#if _MSC_VER < 1700
		lock_guard lock(m_mutex);
#else
		std::lock_guard<std::mutex> lock(m_mutex);
#endif
		MH_Uninitialize();
		m_devices.clear();

		if (m_timeout_thread) CloseHandle(m_timeout_thread);
	};

	static const DWORD HOOK_NONE		= (DWORD)0;			// 0x00000000
	static const DWORD HOOK_LL			= (DWORD)1;			// 0x00000001
	static const DWORD HOOK_COM			= (DWORD)1 << 1;	// 0x00000002
	static const DWORD HOOK_DI			= (DWORD)1 << 2;	// 0x00000004
	static const DWORD HOOK_PIDVID		= (DWORD)1 << 3;	// 0x00000008
	static const DWORD HOOK_NAME		= (DWORD)1 << 4;	// 0x00000010
	static const DWORD HOOK_SA			= (DWORD)1 << 5;	// 0x00000020

	static const DWORD HOOK_WT			= (DWORD)1 << 24;	// 0x10000000
	static const DWORD HOOK_STOP		= (DWORD)1 << 25;	// 0x20000000
	static const DWORD HOOK_NOTIMEOUT	= (DWORD)1 << 26;	// 0x40000000
	static const DWORD HOOK_DISABLE		= (DWORD)1 << 31;	// 0x80000000

	typedef std::vector<iHookDevice>::iterator iterator;
	typedef std::vector<iHookDevice>::const_iterator const_iterator;

	iterator begin() { return m_devices.begin(); }
	const_iterator cbegin() const { return m_devices.cbegin(); }
	iterator end() { return m_devices.end(); }
	const_iterator cend() const { return m_devices.cend(); }

	inline void Enable()
	{
		m_hookmask &= ~HOOK_DISABLE;
	}

	inline void Disable()
	{
		m_hookmask |= HOOK_DISABLE;
	}

	inline void EnableHook(const DWORD& flag)
	{
		m_hookmask |= flag;
	}

	inline void DisableHook(const DWORD& flag)
	{
		m_hookmask &= ~flag;
	}

	inline const bool GetState(const DWORD& flag = HOOK_NONE) const
	{
		if (m_hookmask & HOOK_DISABLE || m_hookmask == HOOK_NONE) return false;
		return (m_hookmask & flag) == flag;
	}

	inline DWORD GetMask()
	{
		return m_hookmask;
	}

	inline void SetMask(const DWORD& mask)
	{
		m_hookmask = mask;
	}

	inline void SetFakePIDVID(const DWORD& pidvid)
	{
		m_fakepidvid = pidvid;
	}

	inline DWORD GetFakePIDVID()
	{
		return m_fakepidvid;
	}

	inline void SetTimeout(const DWORD& timeout)
	{
		m_timeout = timeout;
	}

	inline iHookDevice& GetPadConfig(const DWORD& dwUserIndex)
	{
		return m_devices.at(dwUserIndex);
	}

#if _MSC_VER < 1700
	inline void AddHook(DWORD userindex, const GUID& productid, const GUID& instanceid)
	{
		iHookDevice hdevice(userindex, productid, instanceid);
		m_devices.push_back(hdevice);
	}
#else
	inline void AddHook(DWORD userindex, const GUID& productid, const GUID& instanceid)
	{
		m_devices.emplace_back(userindex, productid, instanceid);
	}
#endif

	inline HMODULE GetEmulator()
	{
		return CURRENT_MODULE;
	}

	inline static DWORD WINAPI ThreadProc(_In_  LPVOID lpParameter)
	{
		DWORD* pTimeout = reinterpret_cast<DWORD*>(lpParameter);
		if (!pTimeout) return 0;

		PrintLog("Waiting for hooks...");
		Sleep(*pTimeout * 1000);

		PrintLog("Hook timeout");
		MH_Uninitialize();

		ExitThread(0);
	}

	inline void ExecuteHooks()
	{
		if (!GetState())
		{
			m_devices.clear();
			return;
		}

#if _MSC_VER < 1700
		lock_guard lock(m_mutex);
#else
		std::lock_guard<std::mutex> lock(m_mutex);
#endif

		PrintLog("InputHook starting with mask 0x%08X", m_hookmask);

		MH_Initialize();

		if (GetState(HOOK_LL))
			HookLL();

		if (GetState(HOOK_COM))
			HookCOM();

		if (GetState(HOOK_DI))
			HookDI();

		if (GetState(HOOK_SA))
			HookSA();

		if (GetState(HOOK_WT))
			HookWT();

		MH_EnableHook(MH_ALL_HOOKS);

		if (m_timeout > 0 && !GetState(HOOK_NOTIMEOUT)) m_timeout_thread = CreateThread(NULL, NULL, ThreadProc, (LPVOID*)&m_timeout, NULL, NULL);
	}

	void HookDICOM(REFIID riidltf, LPVOID *ppv);

private:
	DWORD m_hookmask;
	DWORD m_fakepidvid;
	DWORD m_timeout;
	HANDLE m_timeout_thread;

	std::vector<iHookDevice> m_devices;

	void HookLL();
	void HookCOM();
	void HookDI();
	void HookWT();
	void HookSA();
};

#endif
