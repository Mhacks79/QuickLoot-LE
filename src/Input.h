// QuickLoot IE LE - keybindings, input handling and the control map group.
//
// Port of QuickLoot IE's Input/ (InputManager, InputObserver, ButtonArt). Button
// events arrive on the engine's input thread: this module only decides which
// action a key means and queues it - for the menu (scrolling, takes) or for the
// game task (grab, enable/disable). It never calls into the engine from that thread.
//
// The conflicting-input suppression is the original's: every gameplay mapping that
// collides with a QuickLoot key (and "Activate") is put in a private user-event group
// (bit 1<<12, F-19) while the menu shows, and that group is switched off with
// ControlMap::ToggleControls. The LE port also remembers each mapping's original
// flags and puts them back when the menu hides.
//
// Copyright (C) 2026 Mhacks79. Derived from QuickLoot IE (Miss Corruption and
// contributors). GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

#include "Settings.h"

#include <vector>

class InputEvent;

namespace QL
{
	namespace Input
	{
		const UInt32 kGroupFlag = 1u << 12;

		enum UIAction { kNone, kUse, kTake, kTakeAll, kTransfer, kScrollUp, kScrollDown };

		void Reload(const Settings & s);			// any thread (takes the lock)
		void Install();								// registers the input sink (kDataLoaded)
		void HandleEvents(InputEvent * first);		// input thread

		// The menu's side (UI thread).
		bool PopUIAction(int & action);
		void ClearUIActions();

		struct Button { int action; int artIndex; bool hold; };
		unsigned ButtonBarGeneration();
		void ButtonBar(bool gamepad, std::vector<Button> & out);

		// Game thread only.
		void ApplyControlGroups();
		void RestoreControlGroups();

		// Shared with the offline tests and the KHA-OS key names.
		int ArtIndexForSkseKey(int skseKey);
		void SkseKeyToDevice(int skseKey, UInt32 & device, UInt32 & code);
	}
}
