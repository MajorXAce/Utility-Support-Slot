"""Preview/apply player text and version changes; never install or publish.

PLAYER_FACING_CONFIG.json is editable. The applied snapshot records the last
accepted values, so a manual source edit is detected rather than overwritten.
Saved identifiers, paths, option order and behavioral defaults are protected.
"""
from __future__ import annotations
import argparse
import copy
import difflib
import hashlib
import json
from pathlib import Path
import re
import runpy
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SNAPSHOT = 'tools/fixtures/player_config_applied.json'
INVENTORY = 'PLAYER_FACING_CONFIG.json'


def read(path):
    return path.read_text(encoding='utf-8')


def encoded(value):
    return (json.dumps(value, ensure_ascii=False, indent=2)+'\n').encode('utf-8')


def release_dir(root):
    return root/'release' if (root/'release').is_dir() else root


def mutable_paths():
    paths = [('version',), ('arsenal','Name'), ('arsenal','Description')]
    for i in range(2):
        paths += [('arsenal','Options',i,k) for k in ('Name','Description')]
        paths += [('arsenal','Options',1,'SubOptions',i,k) for k in ('Name','Description')]
    paths += [('mod_options',k) for k in ('label','mod','description')]
    paths += [('mod_options','choices',i) for i in range(2)]
    paths += [('mod_bindings',k) for k in ('label','category')]
    return paths


def get(data, path):
    for key in path:
        data = data[key]
    return data


def set_value(data, path, value):
    for key in path[:-1]:
        data = data[key]
    data[path[-1]] = value


def validate(base, desired):
    masked_base, masked_desired = copy.deepcopy(base), copy.deepcopy(desired)
    try:
        for path in mutable_paths():
            value = get(desired,path)
            if not isinstance(value,str) or not value.strip() or len(value)>2048 or any(ord(c)<32 for c in value):
                raise ValueError('Expected a nonempty single-line string: '+'.'.join(map(str,path)))
            set_value(masked_base,path,None)
            set_value(masked_desired,path,None)
    except (KeyError,IndexError,TypeError) as error:
        raise ValueError('Configuration shape changed') from error
    if masked_base != masked_desired:
        raise ValueError('Saved IDs, default settings, resource paths and schema are protected; edit display fields/version only')
    if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',desired['version']):
        raise ValueError('Version must be three numbers, such as 0.7.0')
    if not desired['arsenal']['Name']==desired['mod_options']['mod']==desired['mod_bindings']['category']:
        raise ValueError('Arsenal name, option mod name and binding category must agree')
    choices=desired['arsenal']['Options'][1]['SubOptions']
    if [x['Name'] for x in choices]!=desired['mod_options']['choices']:
        raise ValueError('Arsenal binding choices and in-game choices must agree')


def literal(value):
    # Lua strings use their own escapes; JSON's Unicode escapes are not Lua.
    return "'"+value.replace('\\','\\\\').replace("'","\\'")+"'"


def replace_once(text, before, after, owner):
    if text.count(before)!=1:
        raise ValueError('Owned text changed or ambiguous; reconcile before applying: '+owner)
    return text.replace(before,after,1)


def lua_changes(texts, base, desired):
    name='binding_mode_options.lua';text=texts[name]
    for key in ('label','mod','description'):
        text=replace_once(text,key+'='+literal(base['mod_options'][key]),
                          key+'='+literal(desired['mod_options'][key]),name+':'+key)
    def choices(data):return 'choices={'+','.join(literal(v) for v in data['mod_options']['choices'])+'}'
    texts[name]=replace_once(text,choices(base),choices(desired),name+':choices')
    name='utility_binding_registration.lua';text=texts[name]
    text=replace_once(text,'menu.register_binding,id,'+literal(base['mod_bindings']['label']),
                      'menu.register_binding,id,'+literal(desired['mod_bindings']['label']),name+':label')
    texts[name]=replace_once(text,'category='+literal(base['mod_bindings']['category']),
                            'category='+literal(desired['mod_bindings']['category']),name+':category')
    old=base['arsenal']['Name'];new=desired['arsenal']['Name']
    needle='; '+literal(old)[1:-1]+'\\n; Restart'
    texts['utility_configuration.lua']=replace_once(texts['utility_configuration.lua'],needle,
                                                    '; '+literal(new)[1:-1]+'\\n; Restart','INI template heading')
    old='VERSION='+base['version'];new='VERSION='+desired['version']
    if not any(old in text for text in texts.values()):raise ValueError('Current source version not found')
    for name in texts:texts[name]=texts[name].replace(old,new)


def replacements(base, desired):
    paths=[('arsenal','Name'),('mod_options','label'),('mod_bindings','label')]
    paths += [('arsenal','Options',i,'Name') for i in range(2)]
    paths += [('mod_options','choices',i) for i in range(2)]
    result={}
    for path in paths:
        a,b=get(base,path),get(desired,path)
        if a in result and result[a]!=b:raise ValueError('Ambiguous display-name replacement: '+a)
        result[a]=b
    return {a:b for a,b in result.items() if a!=b}


def prose(text, mapping):
    # Leave commands, folder names and link destinations intact. Apply name
    # replacements simultaneously so one new name cannot be replaced twice.
    if not mapping:return text
    pattern=re.compile(r'(?<![\w])(?:'+'|'.join(re.escape(k) for k in sorted(mapping,key=len,reverse=True))+r')(?![\w])')
    parts=re.split(r'(```.*?```|`[^`]*`|(?<=\])\([^)]*\))',text,flags=re.S)
    return ''.join(part if i%2 else pattern.sub(lambda m:mapping[m[0]],part) for i,part in enumerate(parts))


def reference_values(data):
    a=data['arsenal'];options=a['Options'];choices=options[1]['SubOptions']
    values={('Arsenal','Mod title'):a['Name'],('Arsenal','Mod description'):a['Description']}
    for label,item in [('Core',options[0]),('Bindings',options[1]),('Default Bindings',choices[0]),('Dedicated Key',choices[1])]:
        values[('Arsenal',label+' label')]=item['Name']
        values[('Arsenal',label+' description')]=item['Description']
    values.update({('In-game Mod Options Menu','Mod name'):data['mod_options']['mod'],
                   ('In-game Mod Options Menu','Option label'):data['mod_options']['label'],
                   ('In-game Mod Options Menu','Choices'):'; '.join(data['mod_options']['choices']),
                   ('In-game Mod Options Menu','Description'):data['mod_options']['description'],
                   ('In-game Mod Bindings Menu','Category'):data['mod_bindings']['category'],
                   ('In-game Mod Bindings Menu','Action label'):data['mod_bindings']['label']})
    return values


def reference(text, base, desired):
    old,new=reference_values(base),reference_values(desired)
    mapping=replacements(base,desired);section='';seen=set();lines=[]
    escape=lambda v:v.replace('|',r'\|')
    for line in text.splitlines(keepends=True):
        if line.startswith('## '):section=line[3:].strip()
        row=re.match(r'^\| ([^|]+) \| (.*) \|\s*$',line)
        key=(section,row[1]) if row else None
        if key in old:
            if key in seen or row[2]!=escape(old[key]):raise ValueError('Reference table drift: '+str(key))
            seen.add(key);line='| '+row[1]+' | '+escape(new[key])+' |\n'
        else:line=prose(line,mapping)
        lines.append(line)
    if seen!=set(old):raise ValueError('Reference table row missing')
    return ''.join(lines).replace('Current v'+base['version']+' text','Current v'+desired['version']+' text')



def normalize(base, desired):
    """Fill unchanged duplicate labels/references from the edited display fields.

    Explicit custom descriptions are preserved. Preview shows derived JSON edits
    as well, so renaming a binding is one edit rather than several inventories.
    """
    desired=copy.deepcopy(desired)
    for section,key in [('mod_options','mod'),('mod_bindings','category')]:
        if desired[section][key]==base[section][key]:desired[section][key]=desired['arsenal']['Name']
    if desired['mod_options']['choices']==base['mod_options']['choices']:
        desired['mod_options']['choices']=[x['Name'] for x in desired['arsenal']['Options'][1]['SubOptions']]
    mapping=replacements(base,desired)
    paths=[('arsenal','Description'),('mod_options','description')]
    paths += [('arsenal','Options',i,'Description') for i in range(2)]
    paths += [('arsenal','Options',1,'SubOptions',i,'Description') for i in range(2)]
    for path in paths:
        if get(desired,path)==get(base,path):set_value(desired,path,prose(get(base,path),mapping))
    return desired


def plan(root, desired=None):
    root=Path(root).resolve();base=json.loads(read(root/SNAPSHOT))
    desired=json.loads(read(root/INVENTORY)) if desired is None else desired
    desired=normalize(base,desired)
    validate(base,desired)
    release=release_dir(root)
    if json.loads(read(release/'manifest.json'))!=base['arsenal']:raise ValueError('Manifest drift from applied configuration')
    if read(root/'VERSION').strip()!=base['version']:raise ValueError('VERSION drift from applied configuration')
    originals={p.name:read(p) for p in sorted((root/'Source').glob('*.lua'))}
    source=dict(originals)
    compact=runpy.run_path(str(root/'tools/lua_tokens.py'))['compact']
    pins=json.loads(read(root/'tools/fixtures/runtime_gameplay_tokens.json'))
    for name,pin in pins.items():
        if hashlib.sha256(compact(originals[name]).encode()).hexdigest()!=pin:raise ValueError('Unreviewed source fingerprint: '+name)
    # Assemble in isolation. This plan must not mutate the checkout or packages.
    with tempfile.TemporaryDirectory(prefix='uss-config-') as directory:
        trial=Path(directory);(trial/'Source').mkdir();(trial/'tools/fixtures').mkdir(parents=True)
        (trial/'tools/fixtures/assembly.json').write_bytes((root/'tools/fixtures/assembly.json').read_bytes())
        for name,text in source.items():(trial/'Source'/name).write_text(text,encoding='utf-8')
        assemble=runpy.run_path(str(root/'tools/assemble_sources.py'))['assemble'];assemble.__globals__['ROOT']=trial
        if assemble():raise ValueError('Source assembly drift; assemble/review existing edits first')
        lua_changes(source,base,desired)
        for name,text in source.items():(trial/'Source'/name).write_text(text,encoding='utf-8')
        assemble()
        source={name:read(trial/'Source'/name) for name in source}
    changes={}
    def add(path,data):
        path=Path(path);before=path.read_bytes()
        if before!=data:changes[path.relative_to(root).as_posix()]={'before':before,'after':data}
    if desired['arsenal']!=base['arsenal']:add(release/'manifest.json',encoded(desired['arsenal']))
    if desired['version']!=base['version']:add(root/'VERSION',(desired['version']+'\n').encode())
    mapping=replacements(base,desired)
    for path in dict.fromkeys([root/'README.md',release/'README.md',release/'release_notes.md']):
        text=read(path);changed=prose(text,mapping)
        changed=changed.replace(' v'+base['version'],' v'+desired['version'])
        if changed!=text:add(path,changed.encode())
    text=read(root/'PLAYER_FACING_CONFIG.md');updated=reference(text,base,desired)
    if text!=updated:add(root/'PLAYER_FACING_CONFIG.md',updated.encode())
    for name,text in source.items():
        if text!=originals[name]:add(root/'Source'/name,text.encode())
    # Only declared strings and their generated copies were changed above.
    # Updating the tripwire here attests that restricted transformation, never
    # an arbitrary gameplay edit discovered elsewhere in the working tree.
    new_pins={name:hashlib.sha256(compact(source[name]).encode()).hexdigest() for name in pins}
    if new_pins!=pins:add(root/'tools/fixtures/runtime_gameplay_tokens.json',encoded(new_pins))
    policy=root/'tools/test_resupply_policy.py'
    if policy.is_file():
        prefix=lambda text:text[:text.index('-- Automatic bounded source classifier.')]
        old_hash=hashlib.sha256(compact(prefix(originals['utility_support_slot_resupply.lua'])).encode()).hexdigest()
        new_hash=hashlib.sha256(compact(prefix(source['utility_support_slot_resupply.lua'])).encode()).hexdigest()
        text=read(policy)
        if text.count("=='"+old_hash+"'")!=1:raise ValueError('Reviewed refill fingerprint drift')
        if new_hash!=old_hash:add(policy,text.replace("=='"+old_hash+"'","=='"+new_hash+"'").encode())
    # Avoid newline-only changes on an unchanged inventory/snapshot.
    for name in (INVENTORY,SNAPSHOT):
        if json.loads(read(root/name))!=desired:add(root/name,encoded(desired))
    return changes


def apply(root, changes):
    root=Path(root).resolve()
    for name,item in changes.items():
        path=root/name
        current=path
        while current!=root:
            if current.is_symlink() or (hasattr(current,'is_junction') and current.is_junction()):raise ValueError('Linked configuration path: '+name)
            current=current.parent
        if path.read_bytes()!=item['before']:raise ValueError('File changed since preview: '+name)
    written=[]
    try:
        for name,item in changes.items():
            (root/name).write_bytes(item['after']);written.append(name)
    except OSError:
        for name in written:(root/name).write_bytes(changes[name]['before'])
        raise


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['check','preview','apply'])
    parser.add_argument('--config',type=Path,help='Desired JSON; default is PLAYER_FACING_CONFIG.json')
    args=parser.parse_args()
    desired=json.loads(read(args.config)) if args.config else None
    try:changes=plan(ROOT,desired)
    except (ValueError,KeyError) as error:parser.exit(1,str(error)+'\n')
    if args.command=='preview':
        for name,item in changes.items():
            print(''.join(difflib.unified_diff(item['before'].decode().splitlines(True),item['after'].decode().splitlines(True),fromfile=name,tofile=name)),end='')
        print('Planned files:',len(changes))
    elif args.command=='check':
        if changes:parser.exit(1,'Configuration differs: '+', '.join(changes)+'; run preview\n')
        print('PASS configuration/inventory/source/document agreement')
    else:
        apply(ROOT,changes)
        print('Applied files:',len(changes))
        if changes:print('Review the diff, build_release.py and run appropriate validation before packaging. No install or publication occurred.')


if __name__=='__main__':main()
