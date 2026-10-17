import sys
from pathlib import Path
sys.path.insert(0,'tools')
from test import Runner,parser,test_binary,configurations,ensure_unchanged,sha256
args=parser().parse_args(['records','--config','both'])
r=Runner(args);r.summary['scope']='core/workbook adjacency, native Page allocator, browser wire ownership; sequential isolated engine integration'
r.summary['driver_sha256']=sha256(__file__)
def check_integration():
 runs=[]
 for config in configurations(args.config):
  for module in ('core','workbook','page-engine'):runs.append((module+'-'+config,test_binary(module,config)))
  runs.append(('wire-'+config,'build/web-tests/test-wire'+('-sanitize' if config=='sanitize' else '')))
 r.build([binary for _,binary in runs],'build-engine-integration');r.retain_binaries([binary for _,binary in runs])
 for name,binary in runs:
  ensure_unchanged(r.summary['binaries']);r.record(r.command(name,['./'+binary],'PASS:'));ensure_unchanged(r.summary['binaries'])
r.records=check_integration
raise SystemExit(r.run())
