"""Check the public source/build boundary without a game or Lua installation.

These checks validate assembly, reviewed source tokens and exact packaged Lua;
they do not establish live native behavior or replace gameplay testing.
"""
from pathlib import Path
import hashlib
import json
import runpy
import struct

ROOT=Path(__file__).resolve().parents[1]
PUBLIC_FILES={'manifest.json','icon.png','README.md','release_notes.md','LICENSE'}
FOLDERS={'Core':'.patch_0','DefaultBindings':'.patch_1','DedicatedKey':'.patch_1'}
BASE='9ba626afa44a3aa3'


def public_files(root):
    root=Path(root)
    release=root/'release' if (root/'release').is_dir() else root
    names=set(PUBLIC_FILES)
    for folder,suffix in FOLDERS.items():
        names.update(folder+'/'+BASE+suffix+end for end in ('','.stream','.gpu_resources'))
    paths=[release/name for name in sorted(names)]
    for path in paths:
        current=path
        while current!=release:
            if current.is_symlink() or (hasattr(current,'is_junction') and current.is_junction()):
                raise ValueError('Linked public file: '+str(path.relative_to(release)))
            current=current.parent
        if not path.is_file():raise ValueError('Missing public file: '+str(path.relative_to(release)))
    return release,paths


def verify(root=ROOT):
    root=Path(root);release,paths=public_files(root)
    if (root/'LICENSE').read_bytes()!=(release/'LICENSE').read_bytes():raise ValueError('License drift')
    sources={p.name:p.read_text(encoding='utf-8') for p in (root/'Source').glob('*.lua')}
    for module,target,mode in json.loads((root/'tools/fixtures/assembly.json').read_text(encoding='utf-8')):
        expected=sources[module].strip()
        if mode=='function':expected=expected[expected.index('local function'):]
        begin='-- USS-SOURCE-BEGIN '+module+'\n';end='\n-- USS-SOURCE-END '+module
        if sources[target].count(begin)!=1 or sources[target].count(end)!=1:raise ValueError('Assembly boundary: '+module)
        if sources[target].split(begin,1)[1].split(end,1)[0]!=expected:raise ValueError('Stale assembled source: '+module)
    compact=runpy.run_path(str(root/'tools/lua_tokens.py'))['compact']
    for name,pin in json.loads((root/'tools/fixtures/runtime_gameplay_tokens.json').read_text(encoding='utf-8')).items():
        if hashlib.sha256(compact(sources[name]).encode()).hexdigest()!=pin:raise ValueError('Unreviewed source tokens: '+name)
    build=runpy.run_path(str(root/'tools/build_release.py'))
    core=(release/'Core'/ (BASE+'.patch_0')).read_bytes()
    if build['pack'](core)!=core:raise ValueError('Core differs from current source; build first')
    inert=json.loads((root/'tools/fixtures/inert_resources.json').read_text(encoding='utf-8'))
    for i in range(struct.unpack_from('<I',core,8)[0]):
        entry=104+i*80;identity,kind,offset=struct.unpack_from('<QQQ',core,entry)
        length,version=struct.unpack_from('<II',core,offset)
        data=core[offset+8:offset+8+length]
        marker=data.splitlines()[0].decode().removeprefix('-- HD2-Addon: ')
        if identity!=build['resource_hash'](marker) or version!=2:raise ValueError('Resource identity/version changed')
        if marker not in build['SOURCES'] and hashlib.sha256(data).hexdigest()!=inert[marker]:raise ValueError('Inert resource changed')
    for folder,mode in [('DefaultBindings','default'),('DedicatedKey','dedicated')]:
        if (release/folder/(BASE+'.patch_1')).read_bytes()!=build['mode_pack'](core,mode):raise ValueError('Binding patch differs from source')
    for path in paths:
        if path.name.endswith(('.stream','.gpu_resources')) and path.read_bytes():raise ValueError('Nonempty sidecar')
    import player_config
    if player_config.plan(root):raise ValueError('Configuration drift; run player_config.py preview')
    return {'source_files':len(sources),'public_files':len(paths),'core_sha256':hashlib.sha256(core).hexdigest()}


if __name__=='__main__':print('PASS',json.dumps(verify(),sort_keys=True))
