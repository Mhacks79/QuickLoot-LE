// QuickLoot IE LE - the engine patches.
//
//   * crosshair prompt (SE ActivationPrompt::AddMessageHook): the HUD message sent by
//     PlayerCharacter::UpdateCrosshairs at 0x0073A1EF (F-01);
//   * lockpick re-activation (SE LockpickActivation): 0x004A4745 and 0x008722E4 (F-05);
//   * a per-frame probe on the HUD menu's NextFrame, vtable 0x010E537C slot 5 (F-21),
//     which replaces SE's SetMoveModeBits call-site patch (sneak changes).
//
// Every patch chains to whatever the site held (another plugin's detour included, F7),
// and a site whose bytes are not a CALL / a code pointer is left alone and reported.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.
#pragma once

namespace QL
{
	namespace Hooks
	{
		// Returns the number of patches installed; the self-check result is logged.
		int Install();
	}
}
