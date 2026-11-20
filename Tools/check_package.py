"""Validate the C-only six-file player ZIP; never load the DLL."""
from pathlib import Path
import argparse,hashlib,io,json,struct,zipfile
from player_package import PLAYER_FILES
ROOT=Path(__file__).resolve().parents[1]
EXPORTS={'hellforge_mod_init','uss_lua_menu_default_mode_v1','uss_lua_menu_post_v2'}
def pe_exports(blob):
    assert blob[:2]==b'MZ';pe=struct.unpack_from('<I',blob,60)[0];assert blob[pe:pe+4]==b'PE\0\0'
    machine,count,_,_,_,optional,_=struct.unpack_from('<HHIIIHH',blob,pe+4);assert machine==0x8664
    opt=pe+24;assert struct.unpack_from('<H',blob,opt)[0]==0x20b
    sections=[struct.unpack_from('<8sIIII',blob,opt+optional+40*i) for i in range(count)]
    def offset(rva):
        for _,size,base,rawsize,raw in sections:
            if base<=rva<base+max(size,rawsize):return raw+rva-base
        raise AssertionError('RVA outside sections')
    directory=offset(struct.unpack_from('<I',blob,opt+112)[0]);names=struct.unpack_from('<I',blob,directory+24)[0];table=offset(struct.unpack_from('<I',blob,directory+32)[0]);result=set()
    for i in range(names):
        at=offset(struct.unpack_from('<I',blob,table+4*i)[0]);result.add(blob[at:blob.index(b'\0',at)].decode('ascii'))
    return sorted(result)
def validate(payload,root=ROOT):
    with zipfile.ZipFile(io.BytesIO(payload)) as package:
        assert len(package.namelist())==6 and set(package.namelist())==set(PLAYER_FILES),'Expected six C-only player files'
        manifest=json.loads(package.read('hellforge-mod.json'))
        assert manifest['id']=='majorxace.utility-support-slot' and manifest['name']=='Utility Support Slot'
        assert manifest['version']==(root/'VERSION').read_text().strip()
        assert manifest['code']['dll']=='utility_support_slot.dll' and manifest['code']['sdk']==11 and manifest['code']['game']=='25480438'
        assert manifest['gameplay'] is True and manifest['code']['prebuilt'] is True
        assert not any(k in manifest for k in ['data','lua','runtime','dependencies'])
        assert set(pe_exports(package.read('utility_support_slot.dll')))==EXPORTS
        for name in PLAYER_FILES:
            if name!='utility_support_slot.dll':assert package.read(name)==(root/name).read_bytes(),name
    return True

def negative_controls(payload):
    with zipfile.ZipFile(io.BytesIO(payload)) as package:files={name:package.read(name) for name in package.namelist()}
    cases=[]
    cases.append({**files,'data/old-lua.patch_0':b'forbidden extra resource'})
    cases.append({name:data for name,data in files.items() if name!='README.md'})
    cases.append({**files,'utility_support_slot.dll':b'not a PE'})
    bad=dict(files);m=json.loads(bad['hellforge-mod.json']);m['id']='other.mod';bad['hellforge-mod.json']=json.dumps(m).encode();cases.append(bad)
    for files in cases:
        out=io.BytesIO()
        with zipfile.ZipFile(out,'w') as package:
            for name,data in files.items():package.writestr(name,data)
        try:validate(out.getvalue())
        except (AssertionError,KeyError,ValueError,struct.error,zipfile.BadZipFile):pass
        else:raise AssertionError('Damaged player package passed')
    return len(cases)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('package',type=Path);args=parser.parse_args();payload=args.package.read_bytes()
    validate(payload);count=negative_controls(payload)
    print('C_ONLY_PACKAGE_PASS; FILES=6; DAMAGED_CONTROLS='+str(count)+'; SHA256='+hashlib.sha256(payload).hexdigest())
