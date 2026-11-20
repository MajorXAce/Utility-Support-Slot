"""Owned mocked API/CLI checks; no network, Git writes or publication."""
import io,subprocess,unittest,urllib.error
from publish_release import GithubRead,publication_plan,publish,MISTAKEN_REPOSITORY as REPO,MISTAKEN_TAG as TAG,MISTAKEN_SHA as OLD
TARGET='a'*40
class Api:
    def __init__(self,ref=None,release=None):self.ref=ref;self.release=release
    def get(self,path,missing=False):
        if path=='repos/'+REPO:return {'full_name':REPO}
        if '/releases/tags/' in path:return self.release
        return self.ref

def ref(sha=OLD,kind='commit'):return {'ref':'refs/tags/'+TAG,'object':{'sha':sha,'type':kind}}
class Checks(unittest.TestCase):
    def test_new_release(self):self.assertEqual(publication_plan(Api(),REPO,'v0.8.4',TARGET)['action'],'create')
    def test_existing_release(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref(),{}),REPO,TAG,TARGET,True)
    def test_existing_tag(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref()),REPO,TAG,TARGET)
    def test_reviewed_replacement(self):self.assertEqual(publication_plan(Api(ref()),REPO,TAG,TARGET,True)['expected_old_sha'],OLD)
    def test_changed_tag(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref('b'*40)),REPO,TAG,TARGET,True)
    def test_missing_old_tag(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(),REPO,TAG,TARGET,True)
    def test_annotated_tag_refused(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref(kind='tag')),REPO,TAG,TARGET,True)
    def test_other_version_refused(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref()),REPO,'v0.8.4',TARGET,True)
    def test_unchanged_mistake_refused(self):
        with self.assertRaises(RuntimeError):publication_plan(Api(ref()),REPO,TAG,OLD,True)
    def test_bad_target(self):
        with self.assertRaises(ValueError):publication_plan(Api(),REPO,TAG,'not-a-sha')
    def test_identity_failure(self):
        class Bad(Api):
            def get(self,path,missing=False):return {'full_name':'other/repo'}
        with self.assertRaises(RuntimeError):publication_plan(Bad(),REPO,TAG,TARGET,True)
    def test_http_errors(self):
        for status in [401,403,429,500]:
            def opener(request,timeout):raise urllib.error.HTTPError(request.full_url,status,'fixture',{},None)
            with self.subTest(status=status),self.assertRaises(RuntimeError):GithubRead('fixture',opener).get('repos/'+REPO+'/releases/tags/'+TAG,missing=True)
    def test_only_explicit_404_missing(self):
        def opener(request,timeout):raise urllib.error.HTTPError(request.full_url,404,'fixture',{},None)
        client=GithubRead('fixture',opener);self.assertIsNone(client.get('fixture',missing=True))
        with self.assertRaises(RuntimeError):client.get('fixture')
    def test_network_failure(self):
        def opener(request,timeout):raise urllib.error.URLError('fixture timeout')
        with self.assertRaises(RuntimeError):GithubRead('fixture',opener).get('fixture',missing=True)
    def test_successful_json(self):
        def opener(request,timeout):return io.BytesIO(b'{"full_name":"MajorXAce/Utility-Support-Slot"}')
        self.assertEqual(GithubRead('fixture',opener).get('fixture')['full_name'],REPO)
    def test_lease_and_exact_notes(self):
        calls=[]
        def run(args,**kwargs):calls.append(args);return subprocess.CompletedProcess(args,0,stdout='https://github.com/'+REPO+'.git')
        publish(publication_plan(Api(ref()),REPO,TAG,TARGET,True),'player.zip','release_notes.md',run)
        self.assertIn('--force-with-lease=refs/tags/'+TAG+':'+OLD,calls[1]);self.assertEqual(calls[2][-2:],['--notes-file','release_notes.md']);self.assertNotIn('--notes',calls[2])
    def test_failed_lease_stops_release(self):
        calls=[]
        def run(args,**kwargs):
            calls.append(args)
            if args[:2]==['git','push']:raise subprocess.CalledProcessError(1,args)
            return subprocess.CompletedProcess(args,0,stdout='https://github.com/'+REPO+'.git')
        with self.assertRaises(subprocess.CalledProcessError):publish(publication_plan(Api(ref()),REPO,TAG,TARGET,True),'player.zip','notes',run)
        self.assertFalse(any(args[0]=='gh' for args in calls))
    def test_wrong_remote(self):
        def run(args,**kwargs):return subprocess.CompletedProcess(args,0,stdout='https://github.com/other/repo.git')
        with self.assertRaises(RuntimeError):publish(publication_plan(Api(ref()),REPO,TAG,TARGET,True),'player.zip','notes',run)
    def test_no_git_write_for_new_version(self):
        calls=[]
        def run(args,**kwargs):calls.append(args);return subprocess.CompletedProcess(args,0)
        publish(publication_plan(Api(),REPO,'v0.8.4',TARGET),'player.zip','notes',run);self.assertEqual([x[0] for x in calls],['gh'])
if __name__=='__main__':unittest.main()
