// QuickLoot IE LE - offline tests of the real Settings.cpp and Input.cpp.
//
// The plugin's own source files are compiled in unchanged; only what they call on the
// game (Loot, Engine, Guard) is replaced by the recording stubs below. CHECK is always
// on, in every configuration (F16). QL_TEST_NEGATIVE=1 flips one expectation on purpose:
// the run must then fail - that is the proof the checks are live.
//
// Copyright (C) 2026 Mhacks79. GPL-3.0-or-later WITH the Modding and Linking exceptions.

#include "Settings.h"
#include "Input.h"
#include "Loot.h"
#include "Engine.h"
#include "Guard.h"
#include "Log.h"

#include "skse/GameInput.h"

#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0, g_checks = 0;
#define CHECK(cond) do { ++g_checks; if(!(cond)) { ++g_failures; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while(0)

// ---------------------------------------------------------------------- stubs
static bool g_showing = true;
static int g_posts[16], g_postCount = 0;
static int g_disabled = -1;
InputEventDispatcher ** g_inputEventDispatcher = 0;

namespace QL
{
	namespace Loot
	{
		bool IsShowing() { return g_showing; }
		void Post(Action a, UInt32, UInt32, UInt32) { if(g_postCount < 16) g_posts[g_postCount++] = a; }
		void ToggleDisabledByKey(bool d) { g_disabled = d ? 1 : 0; }
	}
	namespace Engine
	{
		float ZKeyDelay() { return 0.4f; }
		UInt32 MappedKey(const char * control, UInt32 device) { return (!strcmp(control, "Activate") && device == 0) ? 18 : 0xFFFFFFFF; }
		static UInt32 s_flags[4] = { 0x4, 0x801, 0xFFFFFFFF, 0x8 };
		static const char * s_names[4] = { "Activate", "Forward", "Console", "Jump" };
		static const UInt32 s_buttons[4] = { 18, 17, 41, 57 };
		UInt32 GameplayMappings(UInt32 device, MappingView * out, UInt32 max)
		{
			if(device != 0) return 0;
			UInt32 n = 0;
			for(UInt32 i = 0; i < 4 && n < max; ++i) { out[n].name = s_names[i]; out[n].button = s_buttons[i]; out[n].flags = &s_flags[i]; ++n; }
			return n;
		}
	}
	namespace Guard
	{
		bool Run(const char *, Body body, Domain) { body(); return true; }
		bool Disabled() { return false; }
	}
}

// A button event laid out as the engine's (F-: InputEvent vtbl/device/type/next, id, mask, value, timer).
struct FakeButton { void * vtbl; UInt32 device, type; FakeButton * next; void * id; UInt32 mask; float value, timer; };

static void Press(UInt32 device, UInt32 mask, float value, float timer)
{
	FakeButton b = { 0, device, 0, 0, 0, mask, value, timer };
	QL::Input::HandleEvents((InputEvent *)&b);
}

static int PopAll(int * out, int max)
{
	int n = 0, a;
	while(QL::Input::PopUIAction(a) && n < max) out[n++] = a;
	return n;
}

// ---------------------------------------------------------------------- tests
static void TestNumbers()
{
	float f = 0;
	CHECK(QL::Config::ParseFloat("0.95", f) && f > 0.9499f && f < 0.9501f);
	CHECK(QL::Config::ParseFloat("0,95", f) && f > 0.9499f && f < 0.9501f);		// F5: a comma reads the same
	CHECK(QL::Config::ParseFloat(" -1.5 ", f) && f == -1.5f);
	CHECK(!QL::Config::ParseFloat("abc", f));
	CHECK(!QL::Config::ParseFloat("", f));
	char buf[32];
	setlocale(LC_ALL, "French");
	QL::Config::FormatFloat(0.3f, 2, buf, sizeof(buf));
	CHECK(!strcmp(buf, "0.30"));
	QL::Config::FormatFloat(-2.5f, 2, buf, sizeof(buf));
	CHECK(!strcmp(buf, "-2.50"));
	setlocale(LC_ALL, "C");
}

static void TestClamp()
{
	QL::Settings s;
	QL::Config::Defaults(s);
	s.windowScale = 10.0f; s.minLines = 30; s.maxLines = 4; s.windowAnchor = 42; s.opacityEmpty = -1.0f;
	s.infoColumns.clear(); s.infoColumns.push_back("WEIGHT"); s.infoColumns.push_back("bogus"); s.infoColumns.push_back("weight");
	s.sortRules.clear(); s.sortRules.push_back("byname"); s.sortRules.push_back("Nope"); s.sortRules.push_back("Gold"); s.sortRules.push_back("ByName");
	s.keyboard[0].key = 999;
	QL::Config::Clamp(s);
	CHECK(s.windowScale == 3.0f);
	CHECK(s.maxLines == 4 && s.minLines == 4);
	CHECK(s.windowAnchor == QL::kAnchorCount - 1);
	CHECK(s.opacityEmpty == 0.0f);
	CHECK(s.infoColumns.size() == 1 && s.infoColumns[0] == "weight");
	CHECK(s.sortRules.size() == 2 && s.sortRules[0] == "ByName" && s.sortRules[1] == "Gold");
	CHECK(s.keyboard[0].key == -1);
}

static void TestRoundTrip()
{
	setlocale(LC_ALL, "Russian");
	QL::Config::Edit([](QL::Settings & s) {
		QL::Config::Defaults(s);
		s.windowScale = 1.25f; s.opacityEmpty = 0.45f; s.windowX = -321;
		s.sortRules.clear(); s.sortRules.push_back("ByV/W"); s.sortRules.push_back("Gems");
		s.containerBlacklist.push_back("Dawnguard.esm|0x001234");
	});
	CHECK(QL::Config::Save());
	QL::Config::Edit([](QL::Settings & s) { QL::Config::Defaults(s); });
	QL::Config::Load();
	QL::Settings r;
	QL::Config::Copy(r);
	CHECK(r.windowScale > 1.249f && r.windowScale < 1.251f);
	CHECK(r.opacityEmpty > 0.449f && r.opacityEmpty < 0.451f);
	CHECK(r.windowX == -321);
	CHECK(r.sortRules.size() == 2 && r.sortRules[0] == "ByV/W" && r.sortRules[1] == "Gems");
	CHECK(r.containerBlacklist.size() == 2 && r.containerBlacklist[1] == "Dawnguard.esm|0x001234");
	setlocale(LC_ALL, "C");
	DeleteFileA(QL::Config::IniPath());
}

static void TestInput()
{
	QL::Settings s;
	QL::Config::Defaults(s);
	QL::Input::Reload(s);
	QL::Input::ApplyControlGroups();			// caches the Activate key (18) and moves conflicting mappings
	CHECK(QL::Engine::s_flags[0] == (0x4u | QL::Input::kGroupFlag));		// Activate joins the group
	CHECK(QL::Engine::s_flags[2] == 0xFFFFFFFF);							// Console (41) is not touched
	CHECK(QL::Engine::s_flags[3] == 0x8);
	int a[16], n;

	// E tapped while the window shows: Take on release, nothing on press
	g_showing = true;
	Press(0, 18, 1.0f, 0.0f);
	CHECK(PopAll(a, 16) == 0);
	Press(0, 18, 0.0f, 0.1f);
	n = PopAll(a, 16);
	CHECK(n == 1 && a[0] == QL::Input::kTake);

	// Shift+E: Use
	Press(0, 42, 1.0f, 0.0f);
	Press(0, 18, 1.0f, 0.0f);
	Press(0, 18, 0.0f, 0.1f);
	Press(0, 42, 0.0f, 0.3f);
	n = PopAll(a, 16);
	CHECK(n == 1 && a[0] == QL::Input::kUse);

	// E held past fZKeyDelay: grab requested once, no take on release
	g_postCount = 0;
	Press(0, 18, 1.0f, 0.0f);
	Press(0, 18, 1.0f, 0.2f);
	Press(0, 18, 1.0f, 0.5f);
	Press(0, 18, 1.0f, 0.6f);
	Press(0, 18, 0.0f, 0.7f);
	CHECK(g_postCount == 1 && g_posts[0] == QL::Loot::kGrab);
	CHECK(PopAll(a, 16) == 0);

	// Not showing: E does nothing of ours
	g_showing = false;
	Press(0, 18, 1.0f, 0.0f);
	Press(0, 18, 0.0f, 0.1f);
	CHECK(PopAll(a, 16) == 0);
	g_showing = true;

	// R: take all on press; wheel: scroll on press
	Press(0, 19, 1.0f, 0.0f);
	Press(1, 9, 1.0f, 0.0f);
	n = PopAll(a, 16);
	CHECK(n == 2 && a[0] == QL::Input::kTakeAll && a[1] == QL::Input::kScrollDown);

	// arrow down held: once on press, again after 0.5 s, then every 0.05 s
	Press(0, 0xD0, 1.0f, 0.0f);
	Press(0, 0xD0, 1.0f, 0.3f);
	Press(0, 0xD0, 1.0f, 0.51f);
	Press(0, 0xD0, 1.0f, 0.56f);
	Press(0, 0xD0, 0.0f, 0.6f);
	n = PopAll(a, 16);
	CHECK(n == 3);

	// gamepad A (0x1000): take on press
	Press(2, 0x1000, 1.0f, 0.0f);
	n = PopAll(a, 16);
	CHECK(n == 1 && a[0] == QL::Input::kTake);

	// button bar: keyboard without shift shows Take, Take All, Search (Use hidden)
	std::vector<QL::Input::Button> bar;
	QL::Input::ButtonBar(false, bar);
	CHECK(bar.size() == 3);
	bool sawUse = false;
	for(size_t i = 0; i < bar.size(); ++i) sawUse |= bar[i].action == QL::Input::kUse;
	const char * neg = getenv("QL_TEST_NEGATIVE");
	CHECK(neg && *neg == '1' ? sawUse : !sawUse);		// the negative control flips this one
	Press(0, 42, 1.0f, 0.0f);
	QL::Input::ButtonBar(false, bar);
	bool sawTake = false;
	sawUse = false;
	for(size_t i = 0; i < bar.size(); ++i) { sawUse |= bar[i].action == QL::Input::kUse; sawTake |= bar[i].action == QL::Input::kTake; }
	CHECK(sawUse && !sawTake);
	Press(0, 42, 0.0f, 0.2f);
	QL::Input::ButtonBar(true, bar);
	CHECK(bar.size() == 4);		// gamepad: A take, Y use, X take all, Back search - no modifiers

	QL::Input::RestoreControlGroups();
	CHECK(QL::Engine::s_flags[0] == 0x4);			// original flags back after the window hides

	// A hold action on R (Transfer = R held, modifier == key) makes R's tap action fire on release.
	s.keyboard[QL::kKeyTransfer].key = 19;
	s.keyboard[QL::kKeyTransfer].modifier = 19;
	QL::Input::Reload(s);
	Press(0, 19, 1.0f, 0.0f);
	CHECK(PopAll(a, 16) == 0);				// nothing on press
	Press(0, 19, 0.0f, 0.1f);
	n = PopAll(a, 16);
	CHECK(n == 1 && a[0] == QL::Input::kTakeAll);
	Press(0, 19, 1.0f, 0.0f);
	Press(0, 19, 1.0f, 0.45f);
	Press(0, 19, 1.0f, 0.6f);
	Press(0, 19, 0.0f, 0.7f);
	n = PopAll(a, 16);
	CHECK(n == 1 && a[0] == QL::Input::kTransfer);
}

static void TestArt()
{
	CHECK(QL::Input::ArtIndexForSkseKey(18) == 18);
	CHECK(QL::Input::ArtIndexForSkseKey(0x54) == 282);
	CHECK(QL::Input::ArtIndexForSkseKey(276) == 276);
	CHECK(QL::Input::ArtIndexForSkseKey(264) == 264);
	CHECK(QL::Input::ArtIndexForSkseKey(-1) == 282);
}

int main()
{
	TestNumbers();
	TestClamp();
	TestRoundTrip();
	TestInput();
	TestArt();
	printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
