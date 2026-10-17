#!/usr/bin/env python3
"""Build the pinned raylib WebGL library in an isolated ignored directory."""
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import time

import web_toolchain as tools
import web_raylib_adapter as adapter
from test import FileLock

ROOT=Path(__file__).resolve().parents[1]
def main():
    lock_path=ROOT/'web/toolchain.lock.json'
    lock=tools.load_lock(lock_path); lock_hash=tools.sha256(lock_path)
    sdk=tools.local_root(ROOT); state=tools.status(sdk,lock,lock_hash)
    tools.require(state['ready'],'Run tools/web_toolchain.py bootstrap first')
    folder=ROOT/'build/web/raylib'; folder.mkdir(parents=True,exist_ok=True)
    tools.require(not folder.is_symlink(),'Web build directory must be owned by this checkout')
    stamp={'source':lock['raylib']['commit'],'toolchain':lock_hash,'flags':'PLATFORM_WEB GRAPHICS_API_OPENGL_ES2 RELEASE STATIC upstream-gnu99','schema':2,
           'adapter':{str(p.relative_to(ROOT)):tools.sha256(p) for p in adapter.INPUTS}}
    with FileLock(folder/'.build.lock','raylib web build'):
        source=folder/'src'; source_stamp=source/'.duxcel-build.json'
        if source.exists():
            tools.require(source_stamp.is_file() and not source.is_symlink(),'Unrecognized raylib web build directory')
            if json.loads(source_stamp.read_text())!=stamp:
                shutil.rmtree(source)
        if not source.exists():
            shutil.copytree(sdk/'raylib/src',source)
            adapter.apply(source)
            source_stamp.write_text(json.dumps(stamp,indent=2)+'\n')
        output=folder/'staging'; output.mkdir(exist_ok=True)
        env=tools.environment(sdk,state)
        compiler=shlex.join([state['python']['path'],str(sdk/'emsdk/upstream/emscripten/emcc.py')])
        archiver=shlex.join([state['python']['path'],str(sdk/'emsdk/upstream/emscripten/emar.py')])
        command=['make','-C',str(source),'-j1','PLATFORM=PLATFORM_WEB','GRAPHICS=GRAPHICS_API_OPENGL_ES2','RAYLIB_LIBTYPE=STATIC','RAYLIB_BUILD_MODE=RELEASE','CC='+compiler,'AR='+archiver,'RAYLIB_RELEASE_PATH='+str(output)]
        begin=time.monotonic()
        with (folder/'build.txt').open('w') as log:
            result=subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT)
        tools.require(result.returncode==0,'raylib web build failed; see '+str(folder/'build.txt'))
        library=output/'libraylib.web.a'
        # Validate object format rather than accepting a native Homebrew archive.
        inspection=tools.command([sdk/'emsdk/upstream/bin/llvm-readobj','--file-headers',library])
        formats=[line.strip() for line in inspection.splitlines() if line.startswith('Format:')]
        tools.require(formats and all(line=='Format: WASM' for line in formats),'raylib archive contains non-Wasm objects')
        with tempfile.NamedTemporaryFile(prefix='.library-',dir=folder,delete=False) as temporary:
            staged=Path(temporary.name)
        try:
            shutil.copyfile(library,staged); staged.replace(folder/library.name)
        finally:
            staged.unlink(missing_ok=True)
        record=dict(stamp,command=command,compiler=state['emcc'],elapsed_seconds=round(time.monotonic()-begin,3),objects=len(formats),library_sha256=tools.sha256(folder/library.name),library_bytes=(folder/library.name).stat().st_size,scope='pinned raylib WebGL dependency (upstream GNU99) only; no shared App renderer/browser/Worker acceptance')
        tools.write_json(folder/'build.json',record)
        print(json.dumps(record,indent=2))
    return 0
if __name__=='__main__':
    try: raise SystemExit(main())
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print(str(error)); raise SystemExit(1)
