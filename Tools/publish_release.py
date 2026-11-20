"""Publish a validated player ZIP after explicit review; default is read-only preflight."""
import argparse,json,os,re,subprocess,urllib.error,urllib.parse,urllib.request
from pathlib import Path
from check_package import validate
ROOT=Path(__file__).resolve().parents[1]
MISTAKEN_REPOSITORY='MajorXAce/Utility-Support-Slot'
MISTAKEN_TAG='v0.8.3'
MISTAKEN_SHA='fdff826b49b4b0efe462fcad81077d2ce24f2343'
class GithubRead:
    def __init__(self,token,opener=urllib.request.urlopen):self.token=token;self.opener=opener
    def get(self,path,missing=False):
        request=urllib.request.Request('https://api.github.com/'+path,headers={'Authorization':'Bearer '+self.token,'Accept':'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28','User-Agent':'USS-release'})
        try:
            with self.opener(request,timeout=30) as response:return json.load(response)
        except urllib.error.HTTPError as error:
            if missing and error.code==404:return None
            raise RuntimeError('GitHub lookup failed: HTTP '+str(error.code)+' for '+path) from None
        except urllib.error.URLError as error:raise RuntimeError('GitHub lookup failed: '+str(error.reason)) from None

def publication_plan(api,repo,tag,target,replace=False):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+',repo):raise ValueError('Invalid repository')
    if not re.fullmatch(r'v[0-9]+\.[0-9]+\.[0-9]+',tag) or not re.fullmatch(r'[0-9a-f]{40}',target):raise ValueError('Invalid release identity')
    identity=api.get('repos/'+repo)
    if not isinstance(identity,dict) or identity.get('full_name','').lower()!=repo.lower():raise RuntimeError('Repository identity not verified')
    if api.get('repos/'+repo+'/releases/tags/'+tag,missing=True) is not None:raise RuntimeError('Release already exists; preserve its files')
    ref=api.get('repos/'+repo+'/git/ref/tags/'+tag,missing=True)
    action='create'
    if replace:
        if repo!=MISTAKEN_REPOSITORY or tag!=MISTAKEN_TAG:raise RuntimeError('Replacement is limited to the reviewed mistaken tag')
        obj=ref.get('object',{}) if isinstance(ref,dict) else {}
        if not isinstance(ref,dict) or ref.get('ref')!='refs/tags/'+tag or obj.get('sha')!=MISTAKEN_SHA or obj.get('type')!='commit':raise RuntimeError('Mistaken tag changed; refuse replacement')
        if target==MISTAKEN_SHA:raise RuntimeError('Refuse publishing the unchanged mistaken target')
        action='replace-mistaken-v083'
    elif ref is not None:raise RuntimeError('Tag already exists; preserve it')
    return {'repository':repo,'tag':tag,'target':target,'action':action,'expected_old_sha':MISTAKEN_SHA if replace else None}

def publish(plan,package,notes,runner=subprocess.run):
    repo=plan['repository'];tag=plan['tag'];target=plan['target']
    if plan['action']=='replace-mistaken-v083':
        if repo!=MISTAKEN_REPOSITORY or tag!=MISTAKEN_TAG or plan.get('expected_old_sha')!=MISTAKEN_SHA:raise RuntimeError('Invalid replacement plan')
        remote=runner(['git','remote','get-url','origin'],cwd=ROOT,check=True,capture_output=True,text=True,timeout=30).stdout.strip()
        if remote not in ['https://github.com/'+repo,'https://github.com/'+repo+'.git']:raise RuntimeError('Wrong push remote')
        # Compare-and-swap guard: a concurrent tag change must refuse, never be overwritten.
        runner(['git','push','--force-with-lease=refs/tags/'+tag+':'+MISTAKEN_SHA,'origin',target+':refs/tags/'+tag],cwd=ROOT,check=True,timeout=120)
    runner(['gh','release','create',tag,str(package),'--repo',repo,'--target',target,'--title','Utility Support Slot '+tag[1:],'--notes-file',str(notes)],cwd=ROOT,check=True,timeout=120)

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--repo',default=os.environ.get('GITHUB_REPOSITORY'));parser.add_argument('--target',default=os.environ.get('GITHUB_SHA'));parser.add_argument('--publish',action='store_true');parser.add_argument('--replace-mistaken-v083',action='store_true');args=parser.parse_args()
    if not args.repo or not args.target:parser.error('Repository and target are required')
    version=(ROOT/'VERSION').read_text().strip();package=ROOT/'dist'/('Utility-Support-Slot-v'+version+'.zip');notes=ROOT/'release_notes.md'
    validate(package.read_bytes());body=notes.read_text(encoding='utf-8');assert body.strip(),'Release notes are empty'
    token=os.environ.get('GH_TOKEN') or os.environ.get('GITHUB_TOKEN')
    if not token:parser.error('A GitHub token is required; it is never printed')
    plan=publication_plan(GithubRead(token),args.repo,'v'+version,args.target,args.replace_mistaken_v083);print(json.dumps(plan,indent=2))
    if args.publish:publish(plan,package,notes)
    else:print('Read-only preflight; no tag, release or asset changed')
    return 0
if __name__=='__main__':raise SystemExit(main())
