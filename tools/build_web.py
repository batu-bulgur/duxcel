#!/usr/bin/env python3
"""Compile the shared C99 App into a versioned, isolated browser artifact."""
import argparse,hashlib,json,os,shutil,subprocess,tempfile,time,shlex
from pathlib import Path
import web_toolchain as tools
import package_web_vendor as vendor
import web_raylib_adapter as adapter
from web_object_cache import ObjectCacheError,ensure_object
from test import FileLock,source_metadata
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser(description=__doc__);mode=parser.add_mutually_exclusive_group();mode.add_argument('--worker-driver',action='store_true');mode.add_argument('--codec-read-driver',action='store_true');mode.add_argument('--review',action='store_true');options=parser.parse_args()
    no_window=options.worker_driver or options.codec_read_driver
    lock_path=ROOT/'web/toolchain.lock.json'; lock=tools.load_lock(lock_path)
    sdk=tools.local_root(ROOT); state=tools.status(sdk,lock,tools.sha256(lock_path))
    tools.require(state['ready'],'Run tools/web_toolchain.py bootstrap first')
    library=ROOT/'build/web/raylib/libraylib.web.a'; lib_manifest=library.with_name('build.json')
    tools.require(library.is_file() and lib_manifest.is_file(),'Run tools/build_web_raylib.py first')
    lib=json.loads(lib_manifest.read_text());tools.require(tools.sha256(library)==lib['library_sha256'],'raylib web archive changed')
    tools.require(lib.get('adapter')=={str(p.relative_to(ROOT)):tools.sha256(p) for p in adapter.INPUTS},'Rebuild the current browser platform adapter with tools/build_web_raylib.py')
    sources=tools.command(['make','--no-print-directory','web-codec-read-sources' if options.codec_read_driver else 'web-worker-sources' if options.worker_driver else 'web-review-sources' if options.review else 'web-sources']).splitlines()
    tools.require(sources and all((ROOT/p).is_file() for p in sources),'Invalid shared web source inventory')
    headers=sorted((ROOT/'src').rglob('*.h'))+sorted((ROOT/'src').rglob('*.inc'))
    if options.codec_read_driver:
        headers+=[ROOT/'tests'/name for name in ('test_browser_codec_sorts.inc','test_browser_codec_preferences.inc','test_browser_codec_objects.inc','test_browser_codec_queries.inc','test_browser_codec_options.inc','test_browser_codec_tables.inc','test_browser_codec_pivot_marks.inc','test_browser_codec_filters.inc','test_browser_codec_grid.inc','test_browser_codec_counters.inc','test_browser_codec_source_counters.inc','test_browser_codec_source_versions.inc','test_browser_codec_source_bindings.inc','test_browser_codec_format_rows.inc') if (ROOT/'tests'/name).is_file()]
    host=[ROOT/'web/index.html',ROOT/'web/host.mjs',ROOT/'web/canvas_input.mjs',ROOT/'web/canvas_surface.mjs',ROOT/'web/canvas_wheel.mjs',ROOT/'web/browser_platform.mjs',ROOT/'web/clipboard.mjs',ROOT/'web/download.mjs',ROOT/'web/official_worker.mjs',ROOT/'web/engine_host.mjs',ROOT/'web/page_wire.mjs',ROOT/'assets/icons/png/duxcel-32.png']
    if options.worker_driver:host=[p for p in host if p.name not in ('index.html','host.mjs','canvas_input.mjs')]+[ROOT/'web/worker_test_host.mjs']
    if options.codec_read_driver:host=[ROOT/'web'/name for name in ('official_worker.mjs','engine_host.mjs','page_wire.mjs','codec_test_host.mjs','codec_stream_transport.mjs')]
    if options.review:host=[p for p in host if p.name!='host.mjs']+[ROOT/'web/host_review.mjs',ROOT/'web/review_host.mjs',ROOT/'web/review_queue.mjs']
    assets=sorted(p for base in ('assets/fonts','assets/ui') for p in (ROOT/base).rglob('*') if p.is_file())
    duckdb_flags=shlex.split(tools.command(['make','--no-print-directory','web-duckdb-flags']))
    external_header=next((Path(flag[2:])/'duckdb.h' for flag in duckdb_flags if flag.startswith('-I') and (Path(flag[2:])/'duckdb.h').is_file()),None)
    tools.require(external_header is not None,'Cannot locate the existing DuckDB type header from DUCKDB_CFLAGS')
    flags=['-O2' ,'-g','-std=c99','-Wall','-Wextra','-Wpedantic','-DDUXCEL_BROWSER_HOST','-DPLATFORM_WEB','-DGRAPHICS_API_OPENGL_ES2','-Isrc','-I'+str(sdk/'raylib/src'),*duckdb_flags]
    env=tools.environment(sdk,state);env['BINARYEN_CORES']='1'
    compiler_environment={name:env.get(name) for name in ('CPATH','C_INCLUDE_PATH','CPLUS_INCLUDE_PATH','OBJC_INCLUDE_PATH','LIBRARY_PATH','COMPILER_PATH','GCC_EXEC_PREFIX','SDKROOT','EM_CONFIG','EM_CACHE','EMSDK','EMSDK_PYTHON','EMSDK_NODE','BINARYEN_CORES')}
    environment_key=hashlib.sha256(json.dumps(compiler_environment,sort_keys=True).encode()).hexdigest()
    mode='codec-read' if options.codec_read_driver else 'worker' if options.worker_driver else 'review' if options.review else 'app'
    folder=ROOT/('build/web/codec-read-test' if options.codec_read_driver else 'build/web/worker-test' if options.worker_driver else 'build/web/review' if options.review else 'build/web/app'); folder.mkdir(parents=True,exist_ok=True)
    with FileLock(folder/'.build.lock','shared web App build'):
        review_header=None
        if options.review:
            flags+=['-DDUXCEL_DEV_REVIEW','-include','src/dev/review_input.h','-Ibuild/web/review-info']
            review_header=ROOT/'build/web/review-info/review_build_info.h'
            tools.command(['python3','tools/review_build_info.py','--header',str(review_header),'--flags',' '.join(flags)])
        link=['-sUSE_GLFW=3','-sALLOW_MEMORY_GROWTH=1','-sINITIAL_MEMORY=67108864','-sMAXIMUM_MEMORY=1073741824','-sSTACK_SIZE=2097152','-sASSERTIONS=1','-sMODULARIZE=1','-sEXPORT_ES6=1','-sEXPORT_NAME=createDuxcel','-sENVIRONMENT=web','-sEXIT_RUNTIME=0','-sMAX_WEBGL_VERSION=1',"-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dx_browser_query_result','_dx_browser_export_result','_dx_browser_file_result']",'--preload-file','assets/fonts@/assets/fonts','--preload-file','assets/ui@/assets/ui']
        if no_window:
            link=[value for value in link if value not in ('-sUSE_GLFW=3','-sMAX_WEBGL_VERSION=1','--preload-file','assets/fonts@/assets/fonts','assets/ui@/assets/ui') and not value.startswith('-sEXPORTED_FUNCTIONS=')]
            link.append("-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dx_browser_codec_fixture_result','_dx_browser_codec_result','_dx_browser_codec_stream_chunk','_dx_browser_codec_stream_done','_dx_browser_codec_stream_held','_dx_browser_codec_stream_cancelled']" if options.codec_read_driver else "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free','_dx_browser_query_result','_dx_browser_export_result']")
            assets=[]
        if options.review:
            link=[value.replace("'_dx_browser_file_result']","'_dx_browser_file_result','_dx_browser_review_result']") for value in link]
        # Only DuckDB's type declarations are read from its existing native header;
        # no native engine, thread, Objective-C or web runtime is linked into C UI.
        inputs={str(p.relative_to(ROOT)):tools.sha256(p) for p in [*(ROOT/p for p in sources),*headers,*host,*assets,ROOT/'Makefile',Path(__file__),ROOT/'tools/web_object_cache.py']}
        if options.review:
            inputs[str(review_header.relative_to(ROOT))]=tools.sha256(review_header)
            inputs['tools/review_build_info.py']=tools.sha256(ROOT/'tools/review_build_info.py')
        signature={'inputs':inputs,'flags':flags,'link':link,'toolchain':tools.sha256(lock_path),'raylib':lib['library_sha256'],'duckdb_header_sha256':tools.sha256(external_header),'compiler_environment_sha256':environment_key}
        vendor_files,_,_=vendor.inventory(ROOT)
        vendor_inputs={name:vendor.digest(path) for name,path in vendor_files.items()}
        signature['vendor']=vendor_inputs
        signature['packages']={str(p.relative_to(ROOT)):tools.sha256(p) for p in (ROOT/'web/package.json',ROOT/'web/package-lock.json',ROOT/'tools/package_web_vendor.py')}
        key=hashlib.sha256(json.dumps(signature,sort_keys=True).encode()).hexdigest()
        destination=folder/key[:20]
        if (destination/'build.json').is_file():
            record=json.loads((destination/'build.json').read_text())
            tools.require(all(tools.sha256(destination/p)==h for p,h in record['artifacts'].items()),'Versioned browser artifact changed')
            tools.write_json(folder/'latest.json',{'directory':str(destination),'signature':key})
            print(json.dumps(dict(status='ready',cached=True,directory=str(destination),manifest=record),indent=2));return 0
        stage=Path(tempfile.mkdtemp(prefix='.app-',dir=folder))
        before=source_metadata(ROOT); begin=time.monotonic()
        compiler=[state['python']['path'],str(sdk/'emsdk/upstream/emscripten/emcc.py')]
        # Each mode holds its own lock through linking and owns its object tree.
        # The helper scans real compiler dependencies, including generated review
        # input, rather than invalidating every object for an unused header edit.
        object_config={'cache_abi':1,'mode':mode,'flags':flags,'toolchain':signature['toolchain'],'duckdb_header':signature['duckdb_header_sha256'],'compiler_environment_sha256':environment_key}
        object_key=hashlib.sha256(json.dumps(object_config,sort_keys=True).encode()).hexdigest()
        cache=ROOT/'build/web/objects'/mode/object_key[:20];cache.mkdir(parents=True,exist_ok=True)
        objects=[]; rebuilt=[];validation_seconds=0.0;object_begin=time.monotonic()
        def validate_inputs():
            nonlocal validation_seconds
            started=time.monotonic()
            tools.require(all(tools.sha256(ROOT/name)==digest for name,digest in inputs.items()),'Browser build inputs changed during object validation')
            validation_seconds+=time.monotonic()-started
        log=stage/'build.txt'
        with log.open('w') as output:
            for source in sources:
                obj=(cache/source).with_suffix('.o');objects.append(obj)
                try:
                    changed=ensure_object(compiler=compiler,flags=flags,source=source,target=obj,cwd=ROOT,env=env,log=output,
                                          configuration=object_key,validate_source=validate_inputs)
                except ObjectCacheError as error:
                    raise ValueError(str(error)+'; see '+str(log)) from error
                if changed:
                    rebuilt.append(source)
            object_cache={'mode':mode,'configuration':object_key,'directory':str(cache),'scanned_sources':len(sources),'reused_sources':len(sources)-len(rebuilt),'elapsed_seconds':round(time.monotonic()-object_begin,3),'input_validation_seconds':round(validation_seconds,3)}
            command=[*compiler,*flags,*(str(p) for p in objects),*([str(library)] if not no_window else []),*link,'-lm','-o',str(stage/'duxcel.mjs')]
            result=subprocess.run(command,cwd=ROOT,env=env,stdout=output,stderr=subprocess.STDOUT)
            tools.require(result.returncode==0,'Browser link failed; see '+str(log))
        tools.require(source_metadata(ROOT)==before,'Source changed during browser build')
        for path in host:shutil.copyfile(path,stage/('host.mjs' if path.name in ('worker_test_host.mjs','codec_test_host.mjs','host_review.mjs') else path.name))
        if no_window:(stage/'index.html').write_text('<!doctype html><title>Duxcel '+('header codec' if options.codec_read_driver else 'Worker')+' acceptance</title><script type="module" src="./host.mjs"></script>')
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
        scope+=' Official asynchronous DuckDB-Wasm Worker backend for bounded source pages/count/SQL validation, paged available values/date hierarchy, asynchronous CSV/Parquet source/worksheet-Table import, bounded Rows/Columns/Values PIVOT matrices with source-derived totals and direct or mixed/manual selection statistics. Bounded direct/Pivot/mixed copy and typed Parquet export are integrated with acknowledged clipboard/requested-download adapters. Browser workbook Save/Open and native source-content verification are unavailable. Runtime acceptance is source-scoped and separate.'
        if options.review:scope+=' Review-only C registry/capture/composer and durable browser outbox/shared review queue transport; service/agent acceptance is separate.'
        if options.codec_read_driver:scope='No-window C meta/sheets Header decoder with separate opt-in primary secondary-sort, preferences, object-descriptor, primary/Table-query, source-options, coherent Table-source/filter/sort, Pivot/Marks, full primary FILTERS, ACKed column/cell streams, exact object/source-version counters, owned RAW source-ledger current-owner source-binding and streamed base format-row workflows through the official asynchronous DuckDB-Wasm Worker and owned DXP1 Page bridge. Immutable native v12/v13/v14 fixtures; no complete Workbook, App publication, browser Save/Open or remaining-family decoding.'
        record={'status':'built','signature':key,'source':before,'compiler':state['emcc'],'elapsed_seconds':round(time.monotonic()-begin,3),'sources':sources,'rebuilt_sources':rebuilt,'object_cache':object_cache,'flags':flags,'link':link,'artifacts':artifacts,'bytes':{p:(stage/p).stat().st_size for p in artifacts},'kind':'codec-read-driver' if options.codec_read_driver else 'worker-driver' if options.worker_driver else 'review' if options.review else 'application','scope':scope}
        for path in stage.glob('*.o'):path.unlink()
        tools.write_json(stage/'build.json',record);stage.rename(destination)
        tools.write_json(folder/'latest.json',{'directory':str(destination),'signature':key})
        print(json.dumps(dict(status='built',directory=str(destination),manifest=record),indent=2));return 0
if __name__=='__main__':
    try:
        with FileLock(ROOT/'build/.web-build.lock','Web builds in this worktree'):
            raise SystemExit(main())
    except (OSError,ValueError,subprocess.SubprocessError) as error:print(str(error));raise SystemExit(1)
