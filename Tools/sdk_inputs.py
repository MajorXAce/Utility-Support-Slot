"""Resolve external, hash-pinned SDK headers. Never vendor or execute SDK downloads."""
from pathlib import Path
import os,json,hashlib,shutil,sys,urllib.request
ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def validate_inputs(snapshot=None,compiler=None):
    baseline=json.loads((ROOT/'Tools/sdk_baseline.json').read_text())
    sdk=Path(snapshot or os.environ.get('USS_SDK',ROOT/'.sdk'))
    for name,wanted in baseline['headers'].items():
        if not (sdk/name).is_file() or sha(sdk/name)!=wanted:
            raise RuntimeError('Missing/changed SDK header: '+name+'; see BUILDING.md')
    zig=Path(compiler or os.environ.get('USS_ZIG') or shutil.which('zig') or 'zig')
    return baseline,sdk,zig
if __name__=='__main__':
    if '--fetch' in sys.argv:
        baseline=json.loads((ROOT/'Tools/sdk_baseline.json').read_text())
        sdk=Path(os.environ.get('USS_SDK',ROOT/'.sdk'));sdk.mkdir(parents=True,exist_ok=True)
        for name,wanted in baseline['headers'].items():
            p=sdk/name
            if p.exists():
                if sha(p)!=wanted:raise RuntimeError('Existing SDK differs: '+name)
                continue
            url='https://raw.githubusercontent.com/SkyeShade/HellForge/'+baseline['reference_commit']+'/native/mod-sdk/'+name
            with urllib.request.urlopen(url,timeout=30) as response:data=response.read()
            if hashlib.sha256(data).hexdigest()!=wanted:raise RuntimeError('SDK download hash mismatch: '+name)
            p.write_bytes(data)
    baseline,sdk,zig=validate_inputs();print('SDK'+str(baseline['sdk'])+' headers verified')
