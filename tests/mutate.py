"""Puts known bugs back into a copy of the plugin source, one at a time, and checks
that the offline tests catch every one (a test that does not fail when its bug
returns is not a test). The real source is never touched.

    py -3.11 -I mutate.py
"""
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')
WORK = os.path.join(HERE, 'build', 'mutants')

MUTANTS = [
    ('Settings.cpp', "if(*text == '.' || *text == ',')", "if(*text == '.')", 'a comma decimal no longer parses (F5)'),
    ('Settings.cpp', 's.minLines		= Bound(s.minLines, 0, s.maxLines);', 's.minLines		= Bound(s.minLines, 0, 25);', 'min lines may exceed max lines'),
    ('Input.cpp', 's_bindings[i].flags |= kFlagOnRelease;', '(void)0;', 'keys shared with a hold action fire on press'),
    ('Input.cpp', 'QL::Loot::Post(QL::Loot::kGrab, 0, 0, 0);', '(void)0;', 'holding Activate no longer grabs'),
    ('Input.cpp', '*views[s.index].flags = s.flags;', '(void)0;', 'mapping flags are not given back on hide'),
    ('Input.cpp', 'if(sat && suppressed < 32)', 'if(false)', 'a satisfied modifier no longer hides the plain binding'),
]


def main():
    killed = 0
    for i, (fn, old, new, what) in enumerate(MUTANTS):
        target = os.path.join(WORK, 'm%d' % i)
        if os.path.isdir(target):
            shutil.rmtree(target)
        shutil.copytree(SRC, os.path.join(target, 'src'))
        path = os.path.join(target, 'src', fn)
        text = open(path, encoding='utf-8-sig').read()
        if old not in text:
            print('MUTANT %d NOT APPLIED (%s): pattern missing' % (i, what))
            return 2
        open(path, 'w', encoding='utf-8').write(text.replace(old, new, 1))
        env = dict(os.environ, QL_SRC=os.path.join(target, 'src'), QL_OUT=os.path.join(target, 'out'))
        b = subprocess.run(['cmd', '/c', os.path.join(HERE, 'build.bat')], env=env, capture_output=True, text=True)
        if b.returncode != 0:
            print('MUTANT %d did not build (%s) - not counted' % (i, what))
            return 2
        r = subprocess.run([os.path.join(target, 'out', 'QuickLootTests.exe')], capture_output=True, text=True, cwd=target)
        dead = r.returncode != 0
        killed += dead
        print('mutant %d %-8s %s' % (i, 'KILLED' if dead else 'SURVIVED', what))
    print('%d of %d mutants killed' % (killed, len(MUTANTS)))
    return 0 if killed == len(MUTANTS) else 1


if __name__ == '__main__':
    sys.exit(main())
