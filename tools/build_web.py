#!/usr/bin/env python3
"""Compile the shared C99 App into a versioned, isolated browser artifact."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,time,shlex
from pathlib import Path
import web_toolchain as tools
import package_web_vendor as vendor
from test import FileLock,source_metadata
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__);mode=parser.add_mutually_exclusive_group();mode.add_argument('--worker-driver',action='store_true');mode.add_argument('--review',action='store_true');options=parser.parse_args()
    lock_path=ROOT/'web/toolchain.lock.json'; lock=tools.load_lock(lock_path)
    sdk=tools.local_root(ROOT); state=tools.status(sdk,lock,tools.sha256(lock_path))
    tools.require(state['ready'],'Run tools/web_toolchain.py bootstrap first')
    library=ROOT/'build/web/raylib/libraylib.web.a'; lib_manifest=library.with_name('build.json')
    tools.require(library.is_file() and lib_manifest.is_file(),'Run tools/build_web_raylib.py first')
    lib=json.loads(lib_manifest.read_text());tools.require(tools.sha256(library)==lib['library_sha256'],'raylib web archive changed')
    sources=tools.command(['make','--no-print-directory','web-worker-sources' if options.worker_driver else 'web-review-sources' if options.review else 'web-sources']).splitlines()
    tools.require(sources and all((ROOT/p).is_file() for p in sources),'Invalid shared web source inventory')
    headers=sorted((ROOT/'src').rglob('*.h'))+sorted((ROOT/'src').rglob('*.inc'))
    host=[ROOT/'web/index.html',ROOT/'web/host.mjs',ROOT/'web/official_worker.mjs',ROOT/'web/engine_host.mjs',ROOT/'web/page_wire.mjs',ROOT/'assets/icons/png/duxcel-32.png']
    if options.worker_driver:host=[p for p in host if p.name not in ('index.html','host.mjs')]+[ROOT/'web/worker_test_host.mjs']
    if options.review:host=[p for p in host if p.name!='host.mjs']+[ROOT/'web/host_review.mjs',ROOT/'web/review_host.mjs']
    assets=sorted(p for base in ('assets/fonts','assets/ui') for p in (ROOT/base).rglob('*') if p.is_file())
    duckdb_flags=shlex.split(tools.command(['make','--no-print-directory','web-duckdb-flags']))
    external_header=next((Path(flag[2:])/'duckdb.h' for flag in duckdb_flags if flag.startswith('-I') and (Path(flag[2:])/'duckdb.h').is_file()),None)
    tools.require(external_header is not None,'Cannot locate the existing DuckDB type header from DUCKDB_CFLAGS')
    flags=['-O2' ,'-g','-std=c99','-Wall','-Wextra','-Wpedantic','-DDUXCEL_BROWSER_HOST','-DPLATFORM_WEB','-DGRAPHICS_API_OPENGL_ES2','-Isrc','-I'+str(sdk/'raylib/src'),*duckdb_flags]
    review_header=None
    if options.review:
        flags+=['-DDUXCEL_DEV_REVIEW','-include','src/dev/review_input.h','-Ibuild/web/review-info']
        review_header=ROOT/'build/web/review-info/review_build_info.h'
        tools.command(['python3','tools/review_build_info.py','--header',str(review_header),'--flags',' '.join(flags)])
    link=['-sUSE_GLFW=3','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=67108864','-sMAXIMUM_MEMORY=1073741824','-sSTACK_SIZE=2097152','-sASSERTIONS=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=createDuxcel','-sENVIRONMENT=web','-sEXIT_RUNTIME=0','-sMAX_WEBGL_VERSION=1',"-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dx_browser_query_result','_dx_browser_file_result']",'--preload-file','assets/fonts@/assets/fonts','--preload-file','assets/ui@/assets/ui']
    if options.worker_driver:
        link=[value for value in link if value not in ('-sUSE_GLFW=3','-sMAX_WEBGL_VERSION=1','--preload-file','assets/fonts@/assets/fonts','assets/ui@/assets/ui') and not value.startswith('-sEXPORTED_FUNCTIONS=')]
        link.append("-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dx_browser_query_result']")
        assets=[]
    if options.review:
        link=[value.replace("'_dx_browser_file_result']","'_dx_browser_file_result','_dx_browser_review_result']") for value in link]
    # Only DuckDB's type declarations are read from its existing native header;
    # no native engine, thread, Objective-C or web runtime is linked into C UI.
    inputs={str(p.relative_to(ROOT)):tools.sha256(p) for p in [*(ROOT/p for p in sources),*headers,*host,*assets,ROOT/'Makefile',Path(__file__)]}
    if options.review:
        inputs[str(review_header.relative_to(ROOT))]=tools.sha256(review_header)
        inputs['tools/review_build_info.py']=tools.sha256(ROOT/'tools/review_build_info.py')
    signature={'inputs':inputs,'flags':flags,'link':link,'toolchain':tools.sha256(lock_path),'raylib':lib['library_sha256'],'duckdb_header_sha256':tools.sha256(external_header)}
    vendor_files,_,_=vendor.inventory(ROOT)
    vendor_inputs={name:vendor.digest(path) for name,path in vendor_files.items()}
    signature['vendor']=vendor_inputs
    signature['packages']={str(p.relative_to(ROOT)):tools.sha256(p) for p in (ROOT/'web/package.json',ROOT/'web/package-lock.json',ROOT/'tools/package_web_vendor.py')}
    key=hashlib.sha256(json.dumps(signature,sort_keys=True).encode()).hexdigest()
    folder=ROOT/('build/web/worker-test' if options.worker_driver else 'build/web/review' if options.review else 'build/web/app'); folder.mkdir(parents=True,exist_ok=True)
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
                if options.review and source=='src/dev/review_capture.c':digest+=':'+tools.sha256(review_header)
                if obj.is_file() and stamp.is_file() and stamp.read_text()==digest:continue
                obj.parent.mkdir(parents=True,exist_ok=True);temporary=obj.with_suffix('.o.new')
                command=[*compiler,*flags,'-c',source,'-o',str(temporary)]
                result=subprocess.run(command,cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT)
                tools.require(result.returncode==0,'C99 browser compilation failed; see '+str(log))
                temporary.replace(obj);stamp.write_text(digest);rebuilt.append(source)
            command=[*compiler,*flags,*(str(p) for p in objects),*([str(library)] if not options.worker_driver else []),*link,'-lm','-o',str(stage/'duxcel.mjs')]
            result=subprocess.run(command,cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT)
            tools.require(result.returncode==0,'Browser link failed; see '+str(log))
        tools.require(source_metadata(ROOT)==before,'Source changed during browser build')
        for path in host:shutil.copyfile(path,stage/('host.mjs' if path.name in ('worker_test_host.mjs','host_review.mjs') else path.name))
        if options.worker_driver:(stage/'index.html').write_text('<!doctype html><title>Duxcel Worker acceptance</title><script type="module" src="./host.mjs"></script>')
        packaged=vendor.package_vendor(ROOT,stage)
        tools.require(packaged['files']==vendor_inputs,'Vendor inputs changed during build')
        tools.require(all(tools.sha256(ROOT/name)==digest for name,digest in inputs.items()),'Browser build inputs changed during compilation/packaging')
        tools.write_json(stage/'vendor.json',packaged)
        html=(stage/'index.html').read_text()
        html=html.replace('<script type="module"', '<script type="importmap">'+json.dumps(packaged['import_map'])+'</script><script type="module"')
        (stage/'index.html').write_text(html)
        artifacts={p.name:tools.sha256(p) for p in stage.iterdir() if p.suffix in ('.html','.mjs','.wasm','.data','.png','.json')}
        artifacts.update({name:item['sha256'] for name,item in packaged['files'].items()})
        scope='No-window shared C/official Worker acceptance driver.' if options.worker_driver else 'Shared C99 renderer with browser-scheduled App frames.'
        scope+=' Official asynchronous DuckDB-Wasm Worker backend for bounded source pages/count/SQL validation, paged available values/date hierarchy and asynchronous CSV/Parquet import. Browser PIVOT/totals/statistics/copy/export/workbooks remain explicitly unavailable. Runtime acceptance is separate.'
        if options.review:scope+=' Review-only C registry/capture/composer and browser download adapter; not durable outbox storage.'
        record={'status':'built','signature':key,'source':before,'compiler':state['emcc'],'elapsed_seconds':round(time.monotonic()-begin,3),'sources':sources,'rebuilt_sources':rebuilt,'flags':flags,'link':link,'artifacts':artifacts,'bytes':{p:(stage/p).stat().st_size for p in artifacts},'kind':'worker-driver' if options.worker_driver else 'review' if options.review else 'application','scope':scope}
        for path in stage.glob('*.o'):path.unlink()
        tools.write_json(stage/'build.json',record);stage.rename(destination)
        tools.write_json(folder/'latest.json',{'directory':str(destination),'signature':key})
        print(json.dumps(dict(status='built',directory=str(destination),manifest=record),indent=2));return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,subprocess.SubprocessError) as error:print(str(error));raise SystemExit(1)
