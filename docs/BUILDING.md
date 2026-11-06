# Building QuickLoot IE LE

## What you need

- Visual Studio 2022 or the Build Tools 2022 with the MSVC v143 x86 toolset and a Windows SDK.
- The SKSE 1.7.3 source (from the SKSE 1.7.3 download at https://skse.silverlock.org, folder
  `src`), with its `common` project.

## Layout

The project file uses paths relative to the SKSE source tree. Put this repository at:

```
<SKSE source>\skse_plugins\QuickLootIELE\
```

next to `<SKSE source>\skse`, `<SKSE source>\common` and `<SKSE source>\config`.

## Build

From a Developer Command Prompt:

```
msbuild QuickLootIELE.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:SdkTrimPrefix=<SKSE source>
```

The output is `Builds\Release\QuickLootIELE.dll` (Win32, static C runtime). `SdkTrimPrefix`
keeps the build machine's paths out of the binary.

## Install for testing

Copy `Builds\Release\QuickLootIELE.dll` and the contents of `data\` into the game's `Data`
folder (or into a mod manager folder): `SKSE\Plugins\QuickLootIELE.dll`,
`SKSE\Plugins\QuickLootIELE.ini`, `Interface\LootMenuIE.swf`, `Interface\translations\*`.

## Tests

```
tests\build.bat
tests\build\QuickLootTests.exe
py -3 tests\mutate.py
```

The tests compile the plugin's own `Settings.cpp` and `Input.cpp` against stand-ins for the
game. `mutate.py` puts known bugs back into a copy of the source and checks that the tests
catch each of them.

## Startup self-check table

`src\Signatures.h` is generated from TESV.exe 1.9.32.0.8:

```
set TESV_EXE=<path to TESV.exe>
py -3 tools\gen_signatures.py
```

## The loot window movie

`data\Interface\LootMenuIE.swf` is produced from the original movie with:

```
py -3 tools\prepare_swf.py original\QuickLootIE-4.1.3\res\swf\Interface\LootMenuIE.swf data\Interface\LootMenuIE.swf
```

Only the SWF version byte changes (15 to 10); the tool checks that every tag is identical.
