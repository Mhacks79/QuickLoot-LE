# QuickLoot IE LE - engine findings (TESV 1.9.32.0.8), Ghidra + capstone

F-01 PlayerCharacter crosshair update 0x00739FD0 (Ghidra name kHook_Crosshair_LookupREFRByHandle_Base).
     fastcall(PlayerCharacter* ecx). Builds a HUDData from pool (0x00A5C300 on [0x012E35E0]+8) and sends it
     with UIManager::AddMessage 0x00431B00 at call site 0x0073A1EF (E8 rel32):
       mov edx,[0x12E35E0]; mov ecx,[0x12E35E4]; add edx,0x50; push esi(data); push 0; push edx; call.
     HUDData fields written here: +0x08 type = 2 (target) or 3 (text only, the "bVar2" path),
     +0x0C BSString text via thiscall 0x00402C00(const char*, 0), +0x14 ref handle, +0x20 show byte.
     The "nothing to show" branch (0x0073A1B5) writes text 0x010D1068, show 0, handle 0.
     SE equivalent: ID 39535 +0x289 (QuickLoot ActivationPrompt AddMessageHook).
F-02 TESObjectREFR::ActivateRef 0x004E4230: thiscall(ref ecx, TESObjectREFR* activator, UInt8, TESBoundObject*, SInt32 count, bool defaultOnly).
     Papyrus ObjectReference.Activate native 0x00902460 calls it (push defaultOnly,1,0,0,activator). ret 0x14 (see below).
F-03 Lock: TESObjectREFR::GetLock 0x004EB5B0 thiscall(ref) -> REFR_LOCK* (null if none). Papyrus IsLocked native 0x009036F0:
     locked = lock && (byte[lock+8] & 1). lock+4 = key form, lock+0x10 = times-unlocked counter (LockpickingMenu).
F-04 Papyrus IsActivationBlocked native 0x009036E0: ExtraDataList at ref+0x48, thiscall 0x0040E850(list, 1) -> bool.
F-05 Lockpick re-activation call sites (SE QuickLoot LockpickActivation 17485+0x164 / 51088+0x1BE):
     0x004A4745 in TESObjectCONT activation 0x004A4620 (ret 0x14): unlock with key, then ActivateRef(player,0,0,1,0).
     0x008722E4 in LockpickingMenu success 0x00872140: ActivateRef(player,0,0,1,0) on [0x01B3FB88], then
     UIManager::AddMessage(LockpickingMenu name, 3 close). Both E8 rel32 to 0x004E4230, result ignored by both callers.
     0x004A4775 (no key) calls 0x00871010(ref) = open LockpickingMenu.
F-06 ContainerMenu::TransferItem 0x0084A9B0 thiscall(menu, InventoryEntryData*, SInt32 count, bool toPlayer) (ItemTransfer
     callback 0x0084B270 registered at 0x0084C22D). Take branch (0x0084AAF0..0x0084AC78), mode global [0x01B3E6FC]
     (1 steal-from-container, 2 pickpocket, 3 teammate, else normal):
     - PlayerCharacter::AddPlayerAddItemEvent 0x0073B020 thiscall(player, TESForm* base, TESForm* owner, REFR* container,
       UInt32 type) 4 stack args: 6 when container actor IsDead(slot 0x99, +0x264), else 5; 3 for pickpocket.
       owner = actor base [ref+0x24] for actors, else 0x004EC0C0 (ref GetOwner).
     - extraList = 0x00868950 cdecl(InventoryEntryData*, SInt32 count, bool viewingContainer=1), caller pops 0xC
       (SE 50948 GetInventoryEntryExtraListForRemoval).
     - container vtable +0x158 (slot 0x56) RemoveItem thiscall(ref, UInt32* outHandle, TESForm* item, SInt32 count,
       UInt32 reason, BaseExtraList* extra, TESObjectREFR* moveTo=player, NiPoint3* dropLoc=0, NiPoint3* rot=0).
       reason 4 normal, 1 steal (mode 1/2), 5 teammate.
     - item vtable +0x13C (slot 0x4F) HandleRemoveItemFromContainer thiscall(base, REFR* container).
     - stolen value: mode 1 and 0x004755A0(entry owner) != player base: value 0x00476900 cdecl(container, entry,
       count, bool) (min 1) is added to menu+0x68 (crime is raised later by the menu).
     - actor container with process: 0x00664B90(actor) cdecl, then 0x007031A0 thiscall(process=[actor+0x88], actor).
F-07 Actor::StealAlarm 0x006CC0D0 thiscall(actor, REFR* ref, TESForm* item, SInt32 count, SInt32 value, TESForm* owner,
     bool allowGetBack), ret 0x18. PlayerCharacter::PickUpObject 0x007437E0 calls it for an owned world item
     (ref, item, count, 0, owner, 1) then AddPlayerAddItemEvent(item, owner, 0, 1).
F-08 TESObjectREFR::GetOwner 0x004EC0C0 fastcall(ref) -> TESForm* (ref ExtraOwnership, else persistent/ cell owner).
F-09 TESObjectREFR::IsAnOwner 0x004EC3B0 thiscall(ref, Actor*, bool useFaction, bool requireOwner), ret 0xC.
F-10 PlayerCharacter::IsPlayerAllowedToTakeItemWithValue 0x00745060 thiscall(player, TESForm* owner, SInt32 value)
     (SE 39584): owner == player base -> 1; faction -> favor 0x00744FE0 >= value (and >1) or IsInFaction slot +0x3E0;
     NPC -> relationship 0x005557E0 >= value (and >1).
F-11 InventoryEntryData owner 0x004755A0 fastcall(entry): first extra list's ExtraOwnership (0x0040C0B0) or 0.
     Stolen value helper 0x00476900 cdecl(REFR*, entry, count, bool) uses InventoryEntryData::GetValue.
F-12 ChestsLooted story event: 0x0084A580 (no arguments, sends once, ContainerMenu first take).
F-13 Actor::IsInCombat = actor vtable +0x388 (slot 0xE2) thiscall -> bool (Papyrus IsInCombat native 0x008DB020 jmps there).
F-14 PlayerCharacter grabbed object handle at player+0x568 (Papyrus Game.GetPlayerGrabbedRef native 0x008F46D0 reads
     it and looks it up with 0x004951F0). SE equivalent IsGrabbing = handle at +0x8C8 non-null.
F-15 PlayerCharacter actor-doing-command handle at player+0x54C: getter 0x0073D4B0 thiscall(player) -> Actor* (looked up
     with 0x004A9180, reference released), setter 0x0073D500 thiscall(player, Actor*). Used by Actor::SetDoingFavor
     0x006C2870 (Papyrus SetDoingFavor native 0x008DB110). SE equivalent HasActorDoingCommand = handle at +0x894.
F-16 MenuControls beast form flag = byte [MenuControls+0x41] (singleton [0x01B10D5C]); written by setter 0x00877C00
     thiscall(MenuControls, bool) ret 4, which Papyrus Game.SetBeastForm native 0x008F33B0 calls. SE InBeastForm.
F-17 ControlMap (SDK InputManager, [0x012E7458]) enabled-controls flags at +0x90 (Papyrus Is*ControlsEnabled natives
     0x008F3380/0x008F3390 test bits of it).
F-18 ControlMap::ToggleControls 0x00A67A30 thiscall(map, UInt32 flags, bool enable, bool alsoSaved) ret 0xC: edits
     +0x90 (and +0x94 unless it holds 0x80000000), then sends the controls-changed event.
F-19 ControlMap mapping (SDK InputContext::Mapping, 0x10 bytes) +0x0C = user event group flags. Mapping-enabled test
     0x00A66A00 thiscall(map, Mapping*, UInt32 device) ret 8: flags == -1 or flags subset of {0x10} -> enabled;
     else enabled iff ([map+0x90] & flags) == flags (plus text-input gates +0x98..+0x9A). Same semantics as SE
     userEventGroupFlag (kInvalid = -1), so QuickLoot's private group bit 1<<12 works the same way.
F-20 IMenu ctor 0x00A64B70: +0x0C depth 3, +0x10 flags 0, +0x14 input context 0x12 (none), +0x18 0.
     LE menu flag bits match SE's low bits: HUDMenu ctor 0x008652E0 sets 0x18902 (SE HUD: alwaysOpen|requiresUpdate|
     allowSaving|customRendering|assignCursorToRenderer), FaderMenu 0x8802, TutorialMenu 0x411 (pause|modal|
     updateUsesCursor, context 1, depth 10). QuickLoot LE uses 0x800 (allowSaving), depth 3, context 0x12, as SE.
F-21 MenuManager::Advance 0x00A5CD90 copies the menu stack (+0x94) and calls every menu's NextFrame (slot 5, +0x14)
     with ([mgr+0xFC], [mgr+0x100]) - no flag gating, so an open non-pausing menu is ticked every frame.
F-22 UI sound by descriptor EditorID: 0x00899620 cdecl(const char*) (caller pops 4; ItemList focus 0x0084DF08 plays
     "UIMenuFocus" with it; the FxDelegate PlaySound callback 0x00899940 forwards to it). SE RE::PlaySound.
F-23 PlayerControls input handlers (alloc 0x00774010): +0x128 movement, +0x12C look, +0x130 sprint, +0x134 readyWeapon,
     +0x138 autoMove, +0x13C toggleRun, +0x140 ACTIVATE (vtable 0x010D453C, 0x10 bytes), +0x144 jump, +0x148 shout,
     +0x14C attackBlock, +0x150 run, +0x154 sneak, +0x158 togglePOV (same order as SE).
F-24 ActivateHandler::ProcessButton 0x007724C0 (slot 4): handler +0x0C pressed, +0x0D held action done
     (SE SetHeldButtonActionSuccess), +0x0E grab/command started, +0x0F disabled. Held >= fZKeyDelay: commandable
     actor -> Actor::SetDoingFavor(1) 0x006C2870, else if player grab type [player+0x6D4]==0 -> StartGrabObject
     0x0074FE50 thiscall(player). Release: grab type 1 -> release 0x0074C580, else if pressed -> ActivatePickRef
     0x00743530 thiscall(player) (calls ActivateRef 0x004E4230 at 0x00743609).
F-25 LootMenuIE.swf imports gfxfontlib.swf ($EverywhereMediumFont) and skyui/buttonart.swf (ButtonArt). SkyUI LE 5.1
     (SkyUI 5.1 LE SkyUI.bsa, read-only extract): buttonart.swf v10, ButtonArt sprite 323 frames (QuickLoot
     uses frames 0..281 for keyboard/mouse/gamepad); icons_item_psychosteve.swf has 107 frame labels and contains
     every icon label QuickLoot's icon selection can emit (0 missing). So SkyUI LE is a requirement, as on SE.
F-26 Player encumbrance as the item menus show it: 0x00733110 thiscall(player, float* outWeight, float* outMax) ret 8
     (called by the shared bottom-bar builder 0x0087CE40 for "encumbrance"/"maxEncumbrance"):
     outWeight = ActorValueOwner(player+0x60) slot 1 GetCurrent(0x1F InventoryWeight),
     outMax = Actor::GetTotalCarryWeight 0x006A8600 thiscall(actor) -> float (AV 0x20 CarryWeight through perk entry 0x0A).
F-27 ActiveEffect::Dispel 0x00657160 thiscall(effect, bool force) (Papyrus ActiveMagicEffect.Dispel native 0x008D8520,
     registered at 0x008D9977, pushes 0).
F-28 Summoned creatures: SE reads MiddleHighProcessData::summonedCreature. Not located in LE within budget; the LE port
     uses the actor base's ACBS flag 0x4000 (Summonable) instead - every vanilla conjured creature base carries it.
F-29 BGSOpenCloseForm: GetOpenState 0x0044BE80 cdecl(REFR*) -> 0 none, 1 open, 2 opening, 3 closed, 4 closing (same as SE).
     0x0044BF30 cdecl(REFR*, activator) toggles the Open/Close sequences; with activator == player it also arms the
     player's container-open timer (player+0x725 |= 0x20, +0x574 = 5.0) and calls the form's open handler, i.e. it is the
     vanilla "open this chest" path (TESObjectCONT activate 0x004A4620 calls it when state != 1). Not safe to reuse for
     a cosmetic lid animation, so QuickLoot LE does NOT animate container lids (SE ContainerAnimator not ported).
F-30 Item use (vanilla item-menu use handler 0x00849EC0): EquipManager [0x012E5FAC] EquipEntry 0x006EF820 thiscall(mgr,
     Actor*, InventoryEntryData*, BGSEquipSlot*, bool=1) for every type; books additionally:
     teaches spell (byte [book+0x94] & 4) -> TESObjectBOOK::Read 0x004A3AD0 thiscall(book, Actor*) -> bool, then RemoveItem 1;
     otherwise TESDescription::Get 0x0044FB70 thiscall(book+0x5C, BSString* out, TESForm* 0, 'DESC') and
     OpenBookMenu 0x00845A80 cdecl(BSString*, BaseExtraList*, REFR*, TESObjectBOOK*, NiPoint3 pos, NiMatrix3 rot,
     float scale, bool useDefaultPos) all by value, caller pops 0x48 (default pos (-23,-501,60)); it posts the Book Menu
     open message and calls Read itself. Same signature as SE BookMenu::OpenBookMenu.
F-31 Main::Update 0x0069CBD0 runs UIManager event processing 0x00A5D370 (which reaches ProcessEventQueue 0x00A5C270,
     where SKSE runs AddUITask delegates) at 0x0069D0A3 and then MenuManager::Advance 0x00A5CD90 at 0x0069D0EF, one
     after the other on the same thread; the alternative path 0x006F4040/0x006F40EF calls the same pair. So a UI
     task and a menu's NextFrame never run at the same time.
