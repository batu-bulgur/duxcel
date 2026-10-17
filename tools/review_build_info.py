#!/usr/bin/env python3
"""Atomic review-only build provenance; no native runtime dependency."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
def replace_if_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.read_bytes() == data:
        return
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(path)
def digest(paths):
    result = hashlib.sha256()
    for path in sorted(paths):
        result.update(str(path.relative_to(ROOT)).encode()+b'\0')
        result.update(path.read_bytes())
    return result.hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--header', type=Path)
    parser.add_argument('--binary', type=Path)
    parser.add_argument('--flags', default='')
    args=parser.parse_args()
    if args.binary:
        replace_if_changed(Path(str(args.binary)+'.sha256'), (hashlib.sha256(args.binary.read_bytes()).hexdigest()+'\n').encode())
        return
    if not args.header:
        parser.error('--header or --binary required')
    common=subprocess.check_output(['git','rev-parse','--git-common-dir'],cwd=ROOT,text=True).strip()
    common=str((ROOT/common).resolve())
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    source=[ROOT/'Makefile']+[p for p in (ROOT/'src').rglob('*') if p.suffix in ('.c','.h','.m','.inc')]
    assets=[p for p in (ROOT/'assets').rglob('*') if p.is_file() and p.suffix in ('.otf','.ttf','.png')]
    fields={'SOURCE':digest(source),'REVISION':revision,'ASSETS':digest(assets),'FLAGS':args.flags,
            'PROJECT':'duxcel-'+hashlib.sha256(common.encode()).hexdigest()[:16]}
    text='#ifndef DUXCEL_REVIEW_BUILD_INFO_H\n#define DUXCEL_REVIEW_BUILD_INFO_H\n'
    text+=''.join('#define DX_REVIEW_BUILD_'+k+' '+json.dumps(v)+'\n' for k,v in fields.items())
    text+='#endif\n'
    replace_if_changed(args.header,text.encode())
if __name__=='__main__':
    main()
