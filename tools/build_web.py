#!/usr/bin/env python3
"""Compile the shared C99 App into a versioned, isolated browser artifact."""
import hashlib,json,os,shutil,subprocess,tempfile,time,shlex
from pathlib import Path
import web_toolchain as tools
from test import FileLock,source_metadata
ROOT=Path(__file__).resolve().parents[1]
def main():
    lock_path=ROOT/'web/toolchain.lock.json'; lock=tools.load_lock(lock_path)
    sdk=tools.local_root(ROOT); state=tools.status(sdk,lock,tools.sha256(lock_path))
    tools.require(state['ready'],'Run tools/web_toolchain.py bootstrap first')
    library=ROOT/'build/web/raylib/libraylib.web.a'; lib_manifest=library.with_name('build.json')
    tools.require(library.is_file() and lib_manifest.is_file(),'Run tools/build_web_raylib.py first')
    lib=json.loads(lib_manifest.read_text());tools.require(tools.sha256(library)==lib['library_sha256'],'raylib web archive changed')
    sources=tools.command(['make','--no-print-directory','web-sources']).splitlines()
    tools.require(sources and all((ROOT/p).is_file() for p in sources),'Invalid shared web source inventory')
    headers=sorted((ROOT/'src').rglob('*.h'))+sorted((ROOT/'src').rglob('*.inc'))
    host=[ROOT/'web/index.html',ROOT/'web/host.mjs',ROOT/'assets/icons/png/duxcel-32.png']
    assets=sorted(p for base in ('assets/fonts','assets/ui') for p in (ROOT/base).rglob('*') if p.is_file())
    duckdb_flags=shlex.split(tools.command(['make','--no-print-directory','web-duckdb-flags']))
    external_header=next((Path(flag[2:])/'duckdb.h' for flag in duckdb_flags if flag.startswith('-I') and (Path(flag[2:])/'duckdb.h').is_file()),None)
    tools.require(external_header is not None,'Cannot locate the existing DuckDB type header from DUCKDB_CFLAGS')
    flags=['-O2' ,'-g','-std=c99','-Wall','-Wextra','-Wpedantic','-DDUXCEL_BROWSER_HOST','-DPLATFORM_WEB','-DGRAPHICS_API_OPENGL_ES2','-Isrc','-I'+str(sdk/'raylib/src'),*duckdb_flags]
    link=['-sUSE_GLFW=3','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=67108864','-sMAXIMUM_MEMORY=1073741824','-sSTACK_SIZE=2097152','-sASSERTIONS=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=createDuxcel','-sENVIRONMENT=web','-sEXIT_RUNTIME=0','-sMAX_WEBGL_VERSION=1','--preload-file','assets/fonts@/assets/fonts','--preload-file','assets/ui@/assets/ui']
    # Only DuckDB's type declarations are read from its existing native header;
    # no native engine, thread, Objective-C or web runtime is linked into C UI.
    inputs={str(p.relative_to(ROOT)):tools.sha256(p) for p in [*(ROOT/p for p in sources),*headers,*host,*assets,ROOT/'Makefile',Path(__file__)]}
    signature={'inputs':inputs,'flags':flags,'link':link,'toolchain':tools.sha256(lock_path),'raylib':lib['library_sha256'],'duckdb_header_sha256':tools.sha256(external_header)}
    key=hashlib.sha256(json.dumps(signature,sort_keys=True).encode()).hexdigest()
    folder=ROOT/'build/web/app'; folder.mkdir(parents=True,exist_ok=True)
    destination=folder/key[:20]
    with FileLock(folder/'.build.lock','shared web App build'):
        if (destination/'build.json').is_file():
            record=json.loads((destination/'build.json').read_text())
            tools.require(all(tools.sha256(destination/p)==h for p,h in record['artifacts'].items()),'Versioned browser artifact changed')
            tools.write_json(folder/'latest.json',{'directory':str(destination),'signature':key})
            print(json.dumps(dict(status='ready',cached=True,directory=str(destination),manifest=record),indent=2));return 0
        stage=Path(tempfile.mkdtemp(prefix='.app-',dir=folder))
        before=source_metadata(ROOT); begin=time.monotonic()
        env=tools.environment(sdk,state);env['BINARYEN_CORES']='1'
        compiler=[state['python']['path'],str(sdk/'emsdk/upstream/emscripten/emcc.py')]
        # Per-file objects share only this exact compiler/flag/header setup.
        # A host/asset edit relinks/packages without recompiling the C UI.
        object_config={'flags':flags,'headers':{str(p.relative_to(ROOT)):tools.sha256(p) for p in headers},'toolchain':signature['toolchain'],'duckdb_header':signature['duckdb_header_sha256']}
        object_key=hashlib.sha256(json.dumps(object_config,sort_keys=True).encode()).hexdigest()[:20]
        cache=ROOT/'build/web/objects'/object_key;cache.mkdir(parents=True,exist_ok=True)
        objects=[]; rebuilt=[]
        log=stage/'build.txt'
        with log.open('w') as output:
            for source in sources:
                obj=(cache/source).with_suffix('.o');stamp=obj.with_suffix('.sha256')
                digest=tools.sha256(ROOT/source);objects.append(obj)
                if obj.is_file() and stamp.is_file() and stamp.read_text()==digest:continue
                obj.parent.mkdir(parents=True,exist_ok=True);temporary=obj.with_suffix('.o.new')
                command=[*compiler,*flags,'-c',source,'-o',str(temporary)]
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT)
                tools.require(result.returncode==0,'C99 browser compilation failed; see '+str(log))
                temporary.replace(obj);stamp.write_text(digest);rebuilt.append(source)
            command=[*compiler,*flags,*(str(p) for p in objects),str(library),*link,'-lm','-o',str(stage/'duxcel.mjs')]
            result=subprocess.run(command,cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT)
            tools.require(result.returncode==0,'Browser link failed; see '+str(log))
        tools.require(source_metadata(ROOT)==before,'Source changed during browser build')
        for path in host:shutil.copyfile(path,stage/path.name)
        artifacts={p.name:tools.sha256(p) for p in stage.iterdir() if p.suffix in ('.html','.mjs','.wasm','.data','.png')}
        record={'status':'built','signature':key,'source':before,'compiler':state['emcc'],'elapsed_seconds':round(time.monotonic()-begin,3),'sources':sources,'rebuilt_sources':rebuilt,'flags':flags,'link':link,'artifacts':artifacts,'bytes':{p:(stage/p).stat().st_size for p in artifacts},'scope':'Shared C99 renderer with browser-scheduled App frames. Browser engine/files/workbooks are explicitly unavailable pending official Worker integration. No browser runtime acceptance yet.'}
        for path in stage.glob('*.o'):path.unlink()
        tools.write_json(stage/'build.json',record);stage.rename(destination)
        tools.write_json(folder/'latest.json',{'directory':str(destination),'signature':key})
        print(json.dumps(dict(status='built',directory=str(destination),manifest=record),indent=2));return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,subprocess.SubprocessError) as error:print(str(error));raise SystemExit(1)
