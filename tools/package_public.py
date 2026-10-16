"""Build only the installable public ZIP. Never upload, tag or push."""
from pathlib import Path
import argparse
import hashlib
import zipfile
from verify_public import public_files,verify

ROOT=Path(__file__).resolve().parents[1]


def package(root=ROOT,output_dir=None):
    root=Path(root);verify(root)
    release,files=public_files(root)
    output=Path(output_dir) if output_dir is not None else root.parent
    output.mkdir(parents=True,exist_ok=True)
    archive=output/('Utility-Support-Slot-v'+(root/'VERSION').read_text(encoding='utf-8').strip()+'.zip')
    expected={p.relative_to(release).as_posix():p.read_bytes() for p in files}
    if archive.exists():
        with zipfile.ZipFile(archive) as z:
            if len(z.namelist())!=len(expected) or set(z.namelist())!=set(expected) or any(z.read(name)!=data for name,data in expected.items()):
                raise ValueError('Existing version ZIP has different content; choose a separate draft folder or an approved new version')
    else:
        # Exclusive creation protects a prepared/published version from replacement.
        with zipfile.ZipFile(archive,'x',zipfile.ZIP_DEFLATED) as z:
            for name,data in sorted(expected.items()):
                info=zipfile.ZipInfo(name,date_time=(1980,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
                info.external_attr=0o100644<<16;z.writestr(info,data)
    with zipfile.ZipFile(archive) as z:
        if z.testzip() is not None:raise ValueError('ZIP integrity failure')
        if {name:z.read(name) for name in z.namelist()}!=expected:raise ValueError('ZIP content drift')
    return archive,hashlib.sha256(archive.read_bytes()).hexdigest()


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir',type=Path)
    args=parser.parse_args()
    try:path,sha=package(output_dir=args.output_dir)
    except ValueError as error:parser.exit(1,str(error)+'\n')
    print(path);print('SHA256',sha)
