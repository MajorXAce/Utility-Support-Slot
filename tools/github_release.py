"""Plan a GitHub release; only --execute creates it. Never replaces tags/assets."""
from pathlib import Path
import argparse
import json
import re
import subprocess
from package_public import package

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = 'MajorXAce/Utility-Support-Slot'


def command(root, args):
    return subprocess.run(args, cwd=root, text=True, capture_output=True, check=True).stdout.strip()


def plan(root=ROOT, publish=False):
    root = Path(root)
    version = (root / 'VERSION').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'0\.\d+\.\d+', version):
        raise ValueError('Expected a reviewed 0.x.y VERSION')
    target = command(root, ['git', 'rev-parse', 'HEAD'])
    if not re.fullmatch(r'[0-9a-f]{40}', target):
        raise ValueError('Expected an exact commit')
    notes = root / 'release_notes.md'
    if not notes.is_file():
        raise ValueError('Run from the curated public source checkout')
    if not notes.read_text(encoding='utf-8').startswith('# Utility Support Slot v' + version):
        raise ValueError('Release notes/version disagree')
    archive, sha = package(root, root / 'dist')
    return {'repository': REPOSITORY, 'tag': 'v' + version, 'target': target,
            'archive': str(archive), 'sha256': sha, 'notes': str(notes), 'draft': not publish}


def execute(root, release, run=command):
    # Read failures abort. A failed create is never retried or uploaded over.
    origin = run(root, ['git', 'remote', 'get-url', 'origin']).removesuffix('.git')
    if origin != 'https://github.com/' + REPOSITORY:
        raise ValueError('Expected the public repository origin')
    if run(root, ['git', 'status', '--porcelain']):
        raise ValueError('Release checkout must be clean')
    if run(root, ['git', 'rev-parse', 'HEAD']) != release['target']:
        raise ValueError('Release commit changed after preparation')
    if run(root, ['git', 'ls-remote', '--tags', '--refs', 'origin', 'refs/tags/' + release['tag']]):
        raise ValueError('Release tag already exists; never replace a published version')
    releases = json.loads(run(root, ['gh', 'release', 'list', '--repo', REPOSITORY,
                                    '--limit', '1000', '--json', 'tagName']))
    if any(item['tagName'] == release['tag'] for item in releases):
        raise ValueError('Release already exists, including drafts')
    args = ['gh', 'release', 'create', release['tag'], release['archive'],
            '--repo', REPOSITORY, '--target', release['target'],
            '--title', 'Utility Support Slot ' + release['tag'], '--notes-file', release['notes']]
    if release['draft']:
        args.append('--draft')
    return run(root, args)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--execute', action='store_true', help='Create release; default only prepares a plan')
    parser.add_argument('--publish', action='store_true', help='Publish immediately instead of creating a draft')
    args = parser.parse_args()
    try:
        release = plan(publish=args.publish)
        print(json.dumps(release, indent=2))
        if args.execute:
            print(execute(ROOT, release))
    except (ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
