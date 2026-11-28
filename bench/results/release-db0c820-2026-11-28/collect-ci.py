#!/usr/bin/env python3
"""Record completed hosted portability jobs for precisely the candidate SHA."""
import json
import sys
from pathlib import Path
from urllib.request import urlopen

out = Path(sys.argv[1]); sha = (out / 'source-head.txt').read_text().strip()
url = 'https://api.github.com/repos/red-9m/quadtree/actions'
def get(url):
    with urlopen(url, timeout=15) as response:
        return json.load(response)
runs = get(f'{url}/runs?head_sha={sha}')['workflow_runs']
run = next((r for r in runs if r['head_sha'] == sha and r['name'] == 'Portability'), None)
if not run or run['status'] != 'completed':
    sys.exit('Hosted run pending')
record = {k:run[k] for k in ('html_url', 'head_sha', 'status', 'conclusion', 'updated_at')}
record['jobs'] = [{k:j[k] for k in ('name','html_url','conclusion')} for j in get(run['jobs_url']+'?per_page=100')['jobs']]
(out / 'ci.json').write_text(json.dumps(record, indent=2)+'\n')
print(record['html_url'], record['conclusion'])
