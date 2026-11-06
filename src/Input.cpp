// QuickLoot IE LE - keybindings and input. See Input.h.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE's Input/InputManager.cpp,
// Input/InputObserver.cpp, Input/ButtonArt.cpp and Config/UserSettings.cpp
// (Miss Corruption and contributors). GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Input.h"
#include "Loot.h"
#include "Engine.h"
#include "Guard.h"
#include "Log.h"

#include "skse/GameInput.h"
#include "skse/GameEvents.h"
#include "skse/InputMap.h"

#include <windows.h>

namespace
{
	enum Device { kKeyboard = 0, kMouse = 1, kGamepad = 2 };
	enum Group { kGroupNone = 0, kGroupButtonBar = 1, kGroupMouseWheel = 2, kGroupDpad = 3, kGroupEnable = 5,
				 kGroupArrows = 11 | 0x80, kGroupNumPad = 12 | 0x80 };
	enum Flags { kFlagGlobal = 1, kFlagRetrigger = 2, kFlagOnRelease = 4, kFlagOnHold = 8 };
	enum Action { kActNone, kActDisable, kActEnable, kActUse, kActTake, kActTakeAll, kActTransfer, kActScrollUp, kActScrollDown };

	const float kHoldThreshold = 0.4f;
	const float kInitialRetrigger = 0.5f;
	const float kRetriggerStep = 0.05f;

	struct Binding
	{
		int		group;
		UInt32	device, code;
		bool	hasModifier;
		UInt32	modDevice, modCode;
		int		action;
		int		flags;
		float	nextRetrigger;
		bool	modifierSatisfied;
	};

	CRITICAL_SECTION	s_lock;
	volatile LONG		s_lockReady = 0;
	Binding				s_bindings[32];
	int					s_count = 0;
	bool				s_pressed[3][512];		// device x code (gamepad masks folded below)
	UInt32				s_activateKey[3] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
	float				s_zKeyDelay = 0.4f;
	bool				s_triggerOnActivateRelease = false;
	bool				s_grabPosted = false;
	volatile LONG		s_buttonBarGeneration = 1;

	int					s_ui[32];
	int					s_uiCount = 0;

	struct Saved { UInt32 device, index; const char * name; UInt32 flags; };
	Saved				s_saved[128];
	int					s_savedCount = 0;

	void Lock()
	{
		if(InterlockedCompareExchange(&s_lockReady, 1, 0) == 0)
		{
			InitializeCriticalSection(&s_lock);
			InterlockedExchange(&s_lockReady, 2);
		}
		while(s_lockReady != 2) Sleep(0);
		EnterCriticalSection(&s_lock);
	}
	void Unlock() { LeaveCriticalSection(&s_lock); }

	UInt32 Slot(UInt32 device, UInt32 code)
	{
		if(device == kGamepad)
		{
			// XInput masks: fold to a small index
			for(UInt32 i = 0; i < 16; ++i) if(code == (1u << i)) return 256 + i;
			if(code == 0x9) return 300;
			if(code == 0xA) return 301;
			return 511;
		}
		return code < 511 ? code : 511;
	}

	bool Pressed(UInt32 device, UInt32 code) { return device < 3 && s_pressed[device][Slot(device, code)]; }

	void AddBinding(int group, UInt32 device, UInt32 code, bool hasMod, UInt32 modDevice, UInt32 modCode, int action, int flags)
	{
		if(s_count >= 32) return;
		if(code == 0 || code >= 65535) return;		// unmapped
		Binding & b = s_bindings[s_count++];
		b.group = group; b.device = device; b.code = code; b.hasModifier = hasMod; b.modDevice = modDevice; b.modCode = modCode;
		b.action = action; b.flags = flags; b.nextRetrigger = 0.0f; b.modifierSatisfied = false;
	}

	void UpdateModifierStates()
	{
		bool barChanged = false;
		UInt32 suppressedDev[32], suppressedCode[32];
		int suppressed = 0;
		for(int i = 0; i < s_count; ++i)
		{
			Binding & b = s_bindings[i];
			if(!b.hasModifier) continue;
			bool blocked = false;
			for(int j = 0; j < suppressed; ++j) if(suppressedDev[j] == b.device && suppressedCode[j] == b.code) blocked = true;
			const bool sat = !blocked && Pressed(b.modDevice, b.modCode);
			if(sat != b.modifierSatisfied && b.group == kGroupButtonBar) barChanged = true;
			b.modifierSatisfied = sat;
			if(sat && suppressed < 32) { suppressedDev[suppressed] = b.device; suppressedCode[suppressed] = b.code; ++suppressed; }
		}
		for(int i = 0; i < s_count; ++i)
		{
			Binding & b = s_bindings[i];
			if(b.hasModifier) continue;
			bool blocked = false;
			for(int j = 0; j < suppressed; ++j) if(suppressedDev[j] == b.device && suppressedCode[j] == b.code) blocked = true;
			const bool sat = !blocked;
			if(sat != b.modifierSatisfied && b.group == kGroupButtonBar) barChanged = true;
			b.modifierSatisfied = sat;
		}
		if(barChanged) InterlockedIncrement(&s_buttonBarGeneration);
	}

	Binding * FindSatisfied(UInt32 device, UInt32 code, float holdTime)
	{
		Binding * result = 0;
		for(int i = 0; i < s_count; ++i)
		{
			Binding & b = s_bindings[i];
			if(result && !(b.flags & kFlagOnHold)) continue;		// hold bindings take precedence
			if(!b.modifierSatisfied) continue;
			if(b.device != device || b.code != code) continue;
			if((b.flags & kFlagOnHold) && holdTime < kHoldThreshold) continue;
			result = &b;
		}
		return result;
	}

	void PushUI(int action)
	{
		if(s_uiCount < 32) s_ui[s_uiCount++] = action;
		// full: the press is dropped; nothing is taken (fails closed)
	}

	void Trigger(const Binding * b)
	{
		if(!(b->flags & kFlagGlobal) && !QL::Loot::IsShowing()) return;
		switch(b->action)
		{
		case kActDisable:		QL::Loot::ToggleDisabledByKey(true); break;
		case kActEnable:		QL::Loot::ToggleDisabledByKey(false); break;
		case kActUse:			PushUI(QL::Input::kUse); break;
		case kActTake:			PushUI(QL::Input::kTake); break;
		case kActTakeAll:		PushUI(QL::Input::kTakeAll); break;
		case kActTransfer:		PushUI(QL::Input::kTransfer); break;
		case kActScrollUp:		PushUI(QL::Input::kScrollUp); break;
		case kActScrollDown:	PushUI(QL::Input::kScrollDown); break;
		default: break;
		}
	}

	void HandleHold(bool up, float held, Binding * b)
	{
		if(up) { b->nextRetrigger = 0.0f; return; }
		const bool retrigger = (b->flags & kFlagRetrigger) != 0, hold = (b->flags & kFlagOnHold) != 0;
		if(!retrigger && !hold) return;
		const float threshold = hold ? kHoldThreshold : kInitialRetrigger;
		if(b->nextRetrigger < threshold) b->nextRetrigger = threshold;
		if(held >= b->nextRetrigger)
		{
			Trigger(b);
			b->nextRetrigger = retrigger ? b->nextRetrigger + kRetriggerStep : 3.0e38f;
		}
	}

	// InputManager::HandleGrab: the Activate key takes on release, grabs when held.
	bool HandleGrab(UInt32 device, UInt32 code, bool down, bool pressed, float held, Binding * b)
	{
		if(device > 2 || code != s_activateKey[device]) return false;
		if(down)
		{
			s_triggerOnActivateRelease = QL::Loot::IsShowing();
			s_grabPosted = false;
			return true;
		}
		if(!pressed && s_triggerOnActivateRelease && b)
		{
			Trigger(b);
			s_triggerOnActivateRelease = false;
			return true;
		}
		if(!pressed || held < s_zKeyDelay) return true;
		if(QL::Loot::IsShowing() && !s_grabPosted)
		{
			s_grabPosted = true;
			s_triggerOnActivateRelease = false;
			QL::Loot::Post(QL::Loot::kGrab, 0, 0, 0);
		}
		return true;
	}

	void HandleButton(UInt32 device, UInt32 mask, float value, float timer)
	{
		UInt32 code = mask;
		if(device > 2) return;
		const bool pressed = value != 0.0f;
		const bool down = pressed && timer == 0.0f;
		const bool up = !pressed && timer != 0.0f;
		const UInt32 slot = Slot(device, code);
		const bool was = s_pressed[device][slot];
		s_pressed[device][slot] = pressed;

		bool isModifier = false, isInput = false;
		for(int i = 0; i < s_count; ++i)
		{
			if(s_bindings[i].hasModifier && s_bindings[i].modDevice == device && s_bindings[i].modCode == code) isModifier = true;
			if(s_bindings[i].device == device && s_bindings[i].code == code) isInput = true;
		}
		if(isModifier && was != pressed) UpdateModifierStates();
		if(!isInput && !(device <= 2 && code == s_activateKey[device])) return;

		Binding * b = FindSatisfied(device, code, timer);
		if(HandleGrab(device, code, down, pressed, timer, b)) return;
		if(!b) return;
		if((down && !(b->flags & (kFlagOnRelease | kFlagOnHold))) || (up && (b->flags & kFlagOnRelease)))
		{
			b->nextRetrigger = 0.0f;
			Trigger(b);
			return;
		}
		HandleHold(up, timer, b);
	}

	class InputSink : public BSTEventSink<InputEvent>
	{
	public:
		virtual EventResult ReceiveEvent(InputEvent ** evns, InputEventDispatcher *)
		{
			if(evns && *evns) QL::Input::HandleEvents(*evns);
			return kEvent_Continue;
		}
	};
	InputSink s_sink;

	InputEvent * s_events = 0;	// guarded body argument (single input thread)
	void HandleEventsBody()
	{
		QL::Log::Thread("the input sink");
		Lock();
		UInt32 guard = 0;
		for(InputEvent * e = s_events; e && guard++ < 256; e = e->next)
		{
			if(!QL::Engine::Ptr(e)) break;
			if(e->eventType != InputEvent::kEventType_Button) continue;
			const UInt8 * raw = (const UInt8 *)e;
			HandleButton(e->deviceType, *(const UInt32 *)(raw + 0x14), *(const float *)(raw + 0x18), *(const float *)(raw + 0x1C));
		}
		Unlock();
	}
}

namespace QL
{
	namespace Input
	{
		void SkseKeyToDevice(int skseKey, UInt32 & device, UInt32 & code)
		{
			if(skseKey >= 266)		{ device = kGamepad; code = InputMap::GamepadKeycodeToMask((UInt32)skseKey); }
			else if(skseKey >= 256)	{ device = kMouse; code = (UInt32)(skseKey - 256); }
			else					{ device = kKeyboard; code = (UInt32)(skseKey > 0 ? skseKey : 0); }
		}

		void Reload(const Settings & s)
		{
			Lock();
			s_count = 0;
			const int actions[kKeyActionCount] = { kActUse, kActTake, kActTakeAll, kActTransfer, kActDisable, kActEnable };
			for(int pass = 0; pass < 2; ++pass)
				for(int a = 0; a < kKeyActionCount; ++a)
				{
					const KeySetting & k = pass == 0 ? s.keyboard[a] : s.gamepad[a];
					if(k.key <= 0) continue;
					UInt32 d, c, md = 0, mc = 0;
					SkseKeyToDevice(k.key, d, c);
					const bool hasMod = k.modifier > 0;
					if(hasMod) SkseKeyToDevice(k.modifier, md, mc);
					const bool enable = a == kKeyDisable || a == kKeyEnable;
					AddBinding(enable ? kGroupEnable : kGroupButtonBar, d, c, hasMod, md, mc, actions[a], enable ? kFlagGlobal : 0);
				}
			// Hardcoded scrolling (InputManager::ReloadKeybindings)
			AddBinding(kGroupMouseWheel, kMouse, 8, false, 0, 0, kActScrollUp, 0);
			AddBinding(kGroupMouseWheel, kMouse, 9, false, 0, 0, kActScrollDown, 0);
			AddBinding(kGroupArrows, kKeyboard, 0xC8, false, 0, 0, kActScrollUp, kFlagRetrigger);
			AddBinding(kGroupArrows, kKeyboard, 0xD0, false, 0, 0, kActScrollDown, kFlagRetrigger);
			AddBinding(kGroupNumPad, kKeyboard, 0x48, false, 0, 0, kActScrollUp, kFlagRetrigger);
			AddBinding(kGroupNumPad, kKeyboard, 0x50, false, 0, 0, kActScrollDown, kFlagRetrigger);
			AddBinding(kGroupDpad, kGamepad, 0x0001, false, 0, 0, kActScrollUp, kFlagRetrigger);
			AddBinding(kGroupDpad, kGamepad, 0x0002, false, 0, 0, kActScrollDown, kFlagRetrigger);

			// A modifier equal to the key makes a hold binding; then every other binding on that key fires on release.
			for(int i = 0; i < s_count; ++i)
			{
				Binding & b = s_bindings[i];
				if(b.hasModifier && b.modDevice == b.device && b.modCode == b.code) { b.hasModifier = false; b.flags |= kFlagOnHold; }
			}
			for(int i = 0; i < s_count; ++i)
				for(int j = 0; j < s_count; ++j)
					if((s_bindings[j].flags & kFlagOnHold) && !(s_bindings[i].flags & kFlagOnHold) &&
					   s_bindings[i].device == s_bindings[j].device && s_bindings[i].code == s_bindings[j].code)
						s_bindings[i].flags |= kFlagOnRelease;
			UpdateModifierStates();
			InterlockedIncrement(&s_buttonBarGeneration);
			Unlock();
		}

		void Install()
		{
			InputEventDispatcher * d = *g_inputEventDispatcher;
			if(Engine::Ptr(d)) d->AddEventSink(&s_sink);
		}

		void HandleEvents(InputEvent * first)
		{
			s_events = first;
			Guard::Run("input", HandleEventsBody, Guard::kInput);
		}

		bool PopUIAction(int & action)
		{
			Lock();
			const bool any = s_uiCount > 0;
			if(any)
			{
				action = s_ui[0];
				for(int i = 1; i < s_uiCount; ++i) s_ui[i - 1] = s_ui[i];
				--s_uiCount;
			}
			Unlock();
			return any;
		}

		void ClearUIActions()
		{
			Lock();
			s_uiCount = 0;
			Unlock();
		}

		unsigned ButtonBarGeneration() { return (unsigned)s_buttonBarGeneration; }

		void ButtonBar(bool gamepad, std::vector<Button> & out)
		{
			out.clear();
			Lock();
			for(int i = 0; i < s_count; ++i)
			{
				const Binding & b = s_bindings[i];
				if(b.group != kGroupButtonBar || !b.modifierSatisfied) continue;
				if((b.device == kGamepad) != gamepad) continue;
				Button btn;
				btn.action	= b.action == kActUse ? kUse : b.action == kActTake ? kTake : b.action == kActTakeAll ? kTakeAll : kTransfer;
				btn.hold	= (b.flags & kFlagOnHold) != 0;
				int skse	= 0;
				if(b.device == kGamepad)	skse = (int)InputMap::GamepadMaskToKeycode(b.code);
				else if(b.device == kMouse)	skse = 256 + (int)b.code;
				else						skse = (int)b.code;
				btn.artIndex = ArtIndexForSkseKey(skse);
				out.push_back(btn);
			}
			Unlock();
		}

		// ButtonArt::GetFrameIndexForDeviceKey, in SKSE key codes: the button art movie
		// holds keyboard scan codes 1:1, the mouse from 256 and the gamepad from 266.
		int ArtIndexForSkseKey(int k)
		{
			const int kNoMapping = 282;
			if(k >= 266 && k < 282) return k;
			if(k >= 256 && k <= 265) return k;
			if(k <= 0 || k > 255) return kNoMapping;
			const bool ok = (k >= 0x01 && k <= 0x53) || (k >= 0x57 && k <= 0x58) || (k >= 0x9C && k <= 0x9D) ||
							k == 0xB5 || (k >= 0xB7 && k <= 0xB8) || k == 0xC5 || (k >= 0xC7 && k <= 0xC9) ||
							k == 0xCB || k == 0xCD || (k >= 0xCF && k <= 0xD3);
			return ok ? k : kNoMapping;
		}

		void ApplyControlGroups()
		{
			RestoreControlGroups();
			s_zKeyDelay = Engine::ZKeyDelay();
			for(UInt32 d = 0; d < 3; ++d) s_activateKey[d] = Engine::MappedKey("Activate", d);

			Lock();
			Engine::MappingView views[3][160];
			UInt32 counts[3];
			for(UInt32 d = 0; d < 3; ++d) counts[d] = Engine::GameplayMappings(d, views[d], 160);

			// optional groups that collide with any predefined mapping are switched off entirely
			int disabledGroups[8];
			int disabled = 0;
			for(UInt32 d = 0; d < 3; ++d)
				for(UInt32 i = 0; i < counts[d]; ++i)
					for(int k = 0; k < s_count; ++k)
					{
						const Binding & b = s_bindings[k];
						if(b.device != d || b.code != views[d][i].button || (b.flags & kFlagGlobal)) continue;
						if(!(b.group & 0x80)) continue;
						bool known = false;
						for(int g = 0; g < disabled; ++g) if(disabledGroups[g] == b.group) known = true;
						if(!known && disabled < 8) disabledGroups[disabled++] = b.group;
					}
			for(int k = 0; k < s_count; ++k)
				for(int g = 0; g < disabled; ++g)
					if(s_bindings[k].group == disabledGroups[g]) s_bindings[k].code = 0xFFFFFFFF;	// out of play until reload

			for(UInt32 d = 0; d < 3; ++d)
				for(UInt32 i = 0; i < counts[d]; ++i)
				{
					bool conflicts = !_stricmp(views[d][i].name, "Activate");
					for(int k = 0; k < s_count && !conflicts; ++k)
					{
						const Binding & b = s_bindings[k];
						if(b.device == d && b.code == views[d][i].button && !(b.flags & kFlagGlobal)) conflicts = true;
					}
					if(!conflicts || s_savedCount >= 128) continue;
					UInt32 * flags = views[d][i].flags;
					Saved & s = s_saved[s_savedCount++];
					s.device = d; s.index = i; s.name = views[d][i].name; s.flags = *flags;
					UInt32 f = *flags == 0xFFFFFFFF ? 0 : *flags;
					*flags = f | QL::Input::kGroupFlag;
				}
			Unlock();
			QL_LOG("control group: %d mappings moved into QuickLoot's group", s_savedCount);
		}

		void RestoreControlGroups()
		{
			if(!s_savedCount) return;
			Engine::MappingView views[160];
			for(int i = 0; i < s_savedCount; ++i)
			{
				const Saved & s = s_saved[i];
				const UInt32 n = Engine::GameplayMappings(s.device, views, 160);
				if(s.index < n && views[s.index].name == s.name)		// same mapping (interned name pointer)
					*views[s.index].flags = s.flags;
			}
			s_savedCount = 0;
		}
	}
}
