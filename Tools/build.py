"""Build/package USS from public source. No install, launch, tag or publication."""
import argparse,json,os,shutil,subprocess,sys,hashlib,zipfile
from pathlib import Path
from sdk_inputs import ROOT,validate_inputs,sha

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--compiler',choices=['msvc','zig'],default='msvc')
    parser.add_argument('--test',action='store_true');parser.add_argument('--package',action='store_true')
    args=parser.parse_args();baseline,sdk,zig=validate_inputs()
    if args.compiler=='msvc' and not shutil.which('cl.exe'):
        vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
        if not vswhere.is_file():raise RuntimeError('Install Visual Studio C++ Build Tools; see BUILDING.md')
        install=subprocess.check_output([str(vswhere),'-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip()
        vcvars=Path(install)/'VC/Auxiliary/Build/vcvars64.bat'
        if not install or not vcvars.is_file():raise RuntimeError('MSVC x64 environment not found')
        # Literal task script, no user text is interpolated into commands.
        out=ROOT/'build';out.mkdir(exist_ok=True);script=out/'msvc-build.cmd'
        command=subprocess.list2cmdline([sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
        script.write_text('@echo off\ncall "'+str(vcvars)+'"\nif errorlevel 1 exit /b 1\n'+command+'\nexit /b %errorlevel%\n')
        return subprocess.call(['cmd.exe','/d','/c',str(script)],cwd=ROOT)
    if args.test:
        command=[sys.executable,str(ROOT/'Tools/check_current.py')]
        if args.compiler=='msvc':command.append('--msvc')
        subprocess.run(command,cwd=ROOT,check=True)
    version=(ROOT/'VERSION').read_text().strip();manifest=json.loads((ROOT/'hellforge-mod.json').read_text())
    assert manifest['version']==version and manifest['name']=='Utility Support Slot'
    out=ROOT/'build';out.mkdir(exist_ok=True);dll=out/manifest['code']['dll'];source=ROOT/'src/normal_gameplay.c'
    if args.compiler=='msvc':
        command=['cl.exe','/nologo','/LD','/experimental:c11atomics','/std:c11','/W4','/WX','/TC','/O2','/I',str(sdk),'/DHF_USS_NORMAL_ENABLED=1',str(source),'/Fo'+str(out/'utility_support_slot.obj'),'/Fe'+str(dll),'/link','user32.lib','bcrypt.lib']
    else:
        if subprocess.check_output([str(zig),'version'],text=True).strip()!=baseline['compiler_version']:raise RuntimeError('Use reviewed Zig '+baseline['compiler_version'])
        command=[str(zig),'cc','-std=c11','-target','x86_64-windows-gnu','-shared','-O2','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-I',str(sdk),'-DHF_USS_NORMAL_ENABLED=1',str(source),'-o',str(dll),'-luser32','-lbcrypt']
    subprocess.run(command,cwd=ROOT,check=True)
    files={str(p.relative_to(ROOT)).replace('\\','/'):sha(p) for folder in ['src','ResearchSource'] for p in (ROOT/folder).iterdir() if p.suffix in ['.c','.h']}
    receipt={'version':version,'compiler':args.compiler,'dll_sha256':sha(dll),'source_sha256':files,'sdk_headers':baseline['headers'],'command':command,'installed':False,'live_tested_by_build':False}
    (out/'build-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    if args.package:
        dist=ROOT/'dist';dist.mkdir(exist_ok=True);package=dist/('Utility-Support-Slot-v'+version+'.zip')
        names=['hellforge-mod.json','icon.png','LICENSE','THIRD_PARTY_LICENSES.txt','README.md','release_notes.md',dll.name]
        import io
        data=io.BytesIO()
        with zipfile.ZipFile(data,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for name in names:
                path=dll if name==dll.name else ROOT/name
                info=zipfile.ZipInfo(name,(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16
                z.writestr(info,path.read_bytes())
        payload=data.getvalue()
        if package.exists() and package.read_bytes()!=payload:raise RuntimeError('Preserve immutable ZIP: '+str(package))
        package.write_bytes(payload);print(str(package)+' SHA256 '+hashlib.sha256(payload).hexdigest())
    return 0
if __name__=='__main__':sys.exit(main())
