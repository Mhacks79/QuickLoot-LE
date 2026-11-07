"""Game text must reach Scaleform the way the game's own menus send it (TESV 0x00920950):
an unmanaged char* GFxValue converted inside SetMember. CreateString / CreateWideString or a
code page picked from sLanguage break Chinese, Korean and every fan translation.
Usage: py -3 check_strings.py [path to LootMenu.cpp]"""
import re, sys, pathlib
p = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path(__file__).parent.parent / 'src' / 'LootMenu.cpp'
src = p.read_text(encoding='utf-8')
m = re.search(r'void MakeString\([^)]*\)\s*\{(.*?)\n\t\}', src, re.S)
fail = []
if not m: fail.append('MakeString not found')
else:
    body = m.group(1)
    if 'SetString(' not in body: fail.append('MakeString does not use an unmanaged GFxValue::SetString')
    for bad in ('CreateString', 'CreateWideString', 'MultiByteToWideChar'):
        if bad in body: fail.append('MakeString uses ' + bad)
for bad in ('GameCodePage', 'sLanguage:General'):
    if bad in src: fail.append('LootMenu.cpp still picks a code page (' + bad + ')')
for name in ('displayName', 'text', 'label', 'iconLabel'):
    if re.search(r'CreateString\([^;]*' + name, src): fail.append('game text "%s" sent through CreateString' % name)
print('\n'.join(fail) if fail else 'strings: game text is sent as the game sends it')
sys.exit(1 if fail else 0)
