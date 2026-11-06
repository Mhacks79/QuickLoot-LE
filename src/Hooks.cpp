// QuickLoot IE LE - engine patches. See Hooks.h; evidence in evidence\FINDINGS.md.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's Behaviors/ActivationPrompt.cpp and
// Behaviors/LockpickActivation.cpp (Miss Corruption and contributors).
// GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Hooks.h"
#include "Loot.h"
#include "Engine.h"
#include "Guard.h"
#include "Log.h"

#include "skse/SafeWrite.h"

#include <windows.h>

namespace
{
	// ------------------------------------------------------------ sites
	const UInt32 kCrosshairMessageCall	= 0x0073A1EF;	// call UIManager::AddMessage 0x00431B00
	const UInt32 kAddMessage			= 0x00431B00;
	const UInt32 kKeyUnlockActivate		= 0x004A4745;	// call TESObjectREFR::ActivateRef 0x004E4230
	const UInt32 kLockpickActivate		= 0x008722E4;	// call TESObjectREFR::ActivateRef 0x004E4230
	const UInt32 kActivateRef			= 0x004E4230;
	const UInt32 kHudNextFrameSlot		= 0x010E5390;	// HUDMenu vtable 0x010E537C + 5 * 4
	const UInt32 kHudNextFrame			= 0x0085EF60;
	const UInt32 kSetBSString			= 0x00402C00;	// thiscall(BSString*, const char*, UInt32)
	const char * const kEmptyString		= (const char *)0x010D1068;

	typedef void (__thiscall * _AddMessage)(void * ui, void * name, UInt32 message, void * data);
	typedef bool (__thiscall * _ActivateRef)(TESObjectREFR *, TESObjectREFR *, UInt8, TESForm *, SInt32, bool);
	typedef void (__thiscall * _NextFrame)(void * menu, UInt32, UInt32);
	typedef void (__thiscall * _SetString)(void * str, const char * text, UInt32 max);

	_AddMessage		s_addMessage		= 0;
	_ActivateRef	s_keyUnlockActivate	= 0;
	_ActivateRef	s_lockpickActivate	= 0;
	_NextFrame		s_hudNextFrame		= 0;

	UInt32 CallTarget(UInt32 site)
	{
		if(*(UInt8 *)site != 0xE8) return 0;
		return site + 5 + *(SInt32 *)(site + 1);
	}

	bool IsCode(UInt32 address)
	{
		MEMORY_BASIC_INFORMATION mbi;
		if(!VirtualQuery((void *)address, &mbi, sizeof(mbi))) return false;
		return mbi.State == MEM_COMMIT && (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
	}

	// ------------------------------------------------------------ crosshair prompt
	void StripPrompt(UInt8 * data)
	{
		__try
		{
			const UInt32 type = *(UInt32 *)(data + 0x08);
			if(type == 2 || type == 3)		// HUD kSetCrosshairTarget / kSetCrosshairTargetTextOnly
			{
				((_SetString)kSetBSString)(data + 0x0C, kEmptyString, 0);
				*(UInt32 *)(data + 0x14) = 0;
				data[0x20] = 0;			// show = false (what the "nothing in the crosshair" branch sends)
			}
		}
		__except(EXCEPTION_EXECUTE_HANDLER) {}
	}

	void __fastcall AddMessageHook(void * ui, void *, void * name, UInt32 message, void * data)
	{
		if(QL::Loot::PromptBlocked() && !QL::Guard::Disabled() && QL::Engine::Ptr(data)) StripPrompt((UInt8 *)data);
		s_addMessage(ui, name, message, data);
	}

	// ------------------------------------------------------------ lockpick re-activation
	bool Blocks(TESObjectREFR * ref)
	{
		bool block = false;
		__try
		{
			block = QL::Loot::LockpickActivationBlocked() && QL::Engine::TypeOf(QL::Engine::Base(ref)) == kFormType_Container;
		}
		__except(EXCEPTION_EXECUTE_HANDLER) { block = false; }
		return block;
	}

	bool __fastcall KeyUnlockHook(TESObjectREFR * ref, void *, TESObjectREFR * activator, UInt8 a, TESForm * object, SInt32 count, bool defaultOnly)
	{
		if(Blocks(ref)) { QL::Loot::RequestRefresh(); return false; }
		return s_keyUnlockActivate(ref, activator, a, object, count, defaultOnly);
	}

	bool __fastcall LockpickHook(TESObjectREFR * ref, void *, TESObjectREFR * activator, UInt8 a, TESForm * object, SInt32 count, bool defaultOnly)
	{
		if(Blocks(ref)) { QL::Loot::RequestRefresh(); return false; }
		return s_lockpickActivate(ref, activator, a, object, count, defaultOnly);
	}

	// ------------------------------------------------------------ HUD frame probe
	void PollBody() { QL::Log::Thread("the HUD frame probe"); QL::Loot::PollTick(); }

	void __fastcall HudNextFrameHook(void * menu, void *, UInt32 a, UInt32 b)
	{
		QL::Guard::Run("HUD frame probe", PollBody, QL::Guard::kHook);
		s_hudNextFrame(menu, a, b);
	}

	bool HookCall(UInt32 site, UInt32 expected, void * thunk, void ** original, const char * what)
	{
		const UInt32 target = CallTarget(site);
		if(!target || !IsCode(target))
		{
			QL_LOG("self-check: %s at %08X is not a call (bytes changed by another mod?) - left alone", what, site);
			return false;
		}
		if(target != expected) QL_LOG("self-check: %s at %08X already detoured to %08X - chaining to it", what, site, target);
		*original = (void *)target;
		WriteRelCall(site, (UInt32)thunk);
		return true;
	}
}

namespace QL
{
	namespace Hooks
	{
		int Install()
		{
			int n = 0;
			if(HookCall(kCrosshairMessageCall, kAddMessage, (void *)AddMessageHook, (void **)&s_addMessage, "crosshair HUD message")) ++n;
			if(HookCall(kKeyUnlockActivate, kActivateRef, (void *)KeyUnlockHook, (void **)&s_keyUnlockActivate, "key unlock activation")) ++n;
			if(HookCall(kLockpickActivate, kActivateRef, (void *)LockpickHook, (void **)&s_lockpickActivate, "lockpick activation")) ++n;

			const UInt32 current = *(UInt32 *)kHudNextFrameSlot;
			if(IsCode(current))
			{
				if(current != kHudNextFrame) QL_LOG("self-check: HUD NextFrame slot holds %08X (another mod) - chaining to it", current);
				s_hudNextFrame = (_NextFrame)current;
				SafeWrite32(kHudNextFrameSlot, (UInt32)HudNextFrameHook);
				++n;
			}
			else QL_LOG("self-check: HUD NextFrame slot holds %08X, not code - sneak changes will wait for the next crosshair change", current);

			QL_LOG("patches installed: %d of 4", n);
			return n;
		}
	}
}
