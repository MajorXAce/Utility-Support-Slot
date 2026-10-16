"""Assemble shared factories into readable executables in dependency order.

Edit a factory's named Source/*.lua file, not an embedded USS-SOURCE block.
The readable main file owns everything outside those blocks. No code is
minified, executed, or loaded dynamically by this build step.
"""
from pathlib import Path
import json,re
ROOT=Path(__file__).resolve().parents[1]

def assemble():
    entries=json.loads((ROOT/'tools/fixtures/assembly.json').read_text(encoding='utf-8'))
    changed=[]
    for module,target,mode in entries:
        source=(ROOT/'Source'/module).read_text(encoding='utf-8').strip()
        if mode=='function':
            source=source[source.index('local function'):]
        begin='-- USS-SOURCE-BEGIN '+module
        end='-- USS-SOURCE-END '+module
        path=ROOT/'Source'/target
        text=path.read_text(encoding='utf-8')
        # Unique boundaries fail closed if accidentally deleted/duplicated.
        assert text.count(begin)==1 and text.count(end)==1,(module,target)
        pattern=re.escape(begin)+r'\n.*?\n'+re.escape(end)
        new,count=re.subn(pattern,lambda _:begin+'\n'+source+'\n'+end,text,flags=re.S)
        assert count==1,(module,target,count)
        if new!=text:
            path.write_text(new,encoding='utf-8');changed.append(target)
    return changed

if __name__=='__main__':
    changed=assemble()
    print('Assembled shared factories; changed:',', '.join(dict.fromkeys(changed)) or 'none')
