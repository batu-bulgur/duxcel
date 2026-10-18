#!/usr/bin/env python3
"""Run the actual shared C browser UI in an owned, hidden browser session."""
import argparse, datetime, functools, hashlib, http.server, json, os, re, shutil, signal, socketserver, stat, subprocess, threading, time
from pathlib import Path
from test import FileLock, native_lock_path, source_metadata
ROOT=Path(__file__).resolve().parents[1]
CODEC_FIXTURES={
    'v12.dxls':(12,25),'v12-reordered.dxls':(12,25),
    'v12-legacy24.dxls':(12,24),'v13.dxls':(13,25),
    'v13-reordered.dxls':(13,25),'v14.dxls':(14,25),
    'v14-reordered.dxls':(14,25)}
CODEC_SORTS_FIXTURES={
    'sorts-v12.dxls':(12,25),'sorts-v13.dxls':(13,25),
    'sorts-v14.dxls':(14,25),'sorts-v14-reordered.dxls':(14,25)}
CODEC_PREFERENCES_FIXTURES={
    'preferences-v12.dxls':(12,25),'preferences-v13.dxls':(13,25),
    'preferences-v14.dxls':(14,25),'preferences-v14-reordered.dxls':(14,25),
    'preferences-v14-unknown-null.dxls':(14,25),'preferences-v14-unknown-nul.dxls':(14,25)}
CODEC_OBJECTS_FIXTURES={
    'objects-v12.dxls':(12,25),'objects-v13.dxls':(13,25),
    'objects-v14.dxls':(14,25),'objects-v14-reordered.dxls':(14,25)}
CODEC_QUERIES_FIXTURES={
    'queries-v12.dxls':(12,25),'queries-v13.dxls':(13,25),
    'queries-v14.dxls':(14,25),'queries-v14-reordered.dxls':(14,25),
    'queries-v14-blank.dxls':(14,25),'queries-v14-nul.dxls':(14,25)}
CODEC_SOURCE_OPTIONS_FIXTURES={
    'options-v12.dxls':(12,25),'options-v13.dxls':(13,25),
    'options-v14.dxls':(14,25),'options-v14-reordered.dxls':(14,25)}

CODEC_TABLES_FIXTURES={
    'tables-v12.dxls':(12,25),'tables-v13.dxls':(13,25),
    'tables-v14.dxls':(14,25),'tables-v14-reordered.dxls':(14,25)}

CODEC_PIVOT_MARKS_FIXTURES={
    'pivot-marks-v12.dxls':(12,25),'pivot-marks-v13.dxls':(13,25),
    'pivot-marks-v14.dxls':(14,25),'pivot-marks-v14-reordered.dxls':(14,25)}

CODEC_FILTERS_FIXTURES={
    'filters-v12.dxls':(12,25),'filters-v13.dxls':(13,25),
    'filters-v14.dxls':(14,25),'filters-v14-reordered.dxls':(14,25)}

CODEC_GRID_FIXTURES={
    'grid-v12.dxls':(12,25),'grid-v13.dxls':(13,25),
    'grid-v14.dxls':(14,25),'grid-v14-reordered.dxls':(14,25)}

CODEC_COUNTERS_FIXTURES={
    'counters-v12.dxls':(12,25),'counters-v13.dxls':(13,25),
    'counters-v14.dxls':(14,25),'counters-v14-reordered.dxls':(14,25)}

CODEC_SOURCE_COUNTERS_FIXTURES={
    'source-counters-v12.dxls':(12,25),'source-counters-v13.dxls':(13,25),
    'source-counters-v14.dxls':(14,25),'source-counters-v14-reordered.dxls':(14,25)}

CODEC_SOURCE_VERSIONS_FIXTURES={
    'source-versions-v12.dxls':(12,25),'source-versions-v13.dxls':(13,25),
    'source-versions-v14.dxls':(14,25),'source-versions-v14-reordered.dxls':(14,25)}

CODEC_SOURCE_BINDINGS_FIXTURES={
    'source-bindings-v12.dxls':(12,25),'source-bindings-v13.dxls':(13,25),
    'source-bindings-v14.dxls':(14,25),'source-bindings-v14-reordered.dxls':(14,25)}
CODEC_FORMAT_ROWS_FIXTURES={
    'format-rows-v12.dxls':(12,25),'format-rows-v13.dxls':(13,25),
    'format-rows-v14.dxls':(14,25),'format-rows-v14-reordered.dxls':(14,25)}

def codec_fixture_manifest(directory, emitted, expected=CODEC_FIXTURES):
    """Validate only this invocation's closed, bounded immutable archives."""
    if emitted.get('schema')!='duxcel.codec-fixtures' or emitted.get('version')!=1:
        raise RuntimeError('Invalid native codec fixture manifest')
    files=emitted.get('files')
    if not isinstance(files,list) or len(files)!=len(expected):
        raise RuntimeError('Incomplete native codec fixture set')
    seen=set();entries=[];inodes=set();total=0
    for entry in files:
        name=entry.get('name')
        if name not in expected or name in seen or (entry.get('version'),entry.get('layout'))!=expected[name]:
            raise RuntimeError('Unexpected native codec fixture: '+str(name))
        seen.add(name);path=directory/name;info=path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_mode&0o222 or not 0<info.st_size<=32*1024*1024:
            raise RuntimeError('Codec fixture must be a closed read-only bounded file: '+name)
        inode=(info.st_dev,info.st_ino)
        if inode in inodes:raise RuntimeError('Codec fixtures share an inode')
        inodes.add(inode);total+=info.st_size
        if total>128*1024*1024:raise RuntimeError('Codec fixture set exceeds transport budget')
        entries.append({'name':name,'version':entry['version'],'layout':entry['layout'],'bytes':info.st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    if {path.name for path in directory.iterdir()}!=seen:
        raise RuntimeError('Unexpected files remain in codec fixture directory')
    return {'schema':'duxcel.codec-fixtures','version':1,'files':entries}
class LoopbackServer(http.server.ThreadingHTTPServer):
    def server_bind(self):
        # HTTPServer's getfqdn() spends 35 s in reverse DNS on this host.
        # This acceptance server always binds an explicit loopback address.
        socketserver.TCPServer.server_bind(self)
        self.server_name='127.0.0.1';self.server_port=self.server_address[1]
class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args): pass
    def do_GET(self):
        # Blank test-only document: no C App, graphics or second UI/engine.
        if self.path in ('/__metadata-stream-probe.html','/metadata_stream_probe.mjs'):
            imports=getattr(self.server,'metadata_stream_import_map',None)
            if imports is None:self.send_error(404);return
            if self.path.endswith('.html'):
                mapping=json.dumps(imports).replace('<','\\u003c')
                content=('<!doctype html><html><head><meta charset="utf-8"><title>Owned metadata stream probe</title></head><body><script type="importmap">'+mapping+'</script></body></html>').encode();mime='text/html'
            else:content=(ROOT/'web/metadata_stream_probe.mjs').read_bytes();mime='text/javascript'
            self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content);return
        # Test-only probe is served by this owned harness, never packaged into App.
        if self.path.startswith('/__codec-fixtures/'):
            manifest=getattr(self.server,'codec_manifest',None)
            if manifest and self.path=='/__codec-fixtures/manifest.json':
                content=json.dumps(manifest,sort_keys=True).encode();mime='application/json'
            elif manifest and self.path in self.server.codec_files:
                path,entry=self.server.codec_files[self.path];mime='application/octet-stream'
                try:
                    info=path.lstat()
                    if not stat.S_ISREG(info.st_mode) or info.st_mode&0o222 or info.st_size!=entry['bytes']:raise OSError('Fixture identity changed')
                    content=path.read_bytes()
                except OSError:self.send_error(409,'Codec fixture changed');return
                if len(content)!=entry['bytes'] or hashlib.sha256(content).hexdigest()!=entry['sha256']:
                    self.send_error(409,'Codec fixture changed');return
                self.server.codec_requests.append(dict(entry))
            else:self.send_error(404);return
            self.send_response(200);self.send_header('Content-Type',mime);self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content);return
        probe={'/official_worker_probe.mjs':ROOT/'web/official_worker_probe.mjs','/persistence_probe.mjs':ROOT/'web/persistence_probe.mjs'}
        if self.path in probe:
            content=probe[self.path].read_bytes()
            self.send_response(200);self.send_header('Content-Type','text/javascript');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
        elif self.path=='/native-fixture.dxls' and getattr(self.server,'persistence_fixture',None):
            content=self.server.persistence_fixture.read_bytes()
            self.send_response(200);self.send_header('Content-Type','application/octet-stream');self.send_header('Content-Length',str(len(content)));self.end_headers();self.wfile.write(content)
        else: super().do_GET()

def parse_options(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--no-build', action='store_true')
    parser.add_argument('--worker-probe', action='store_true')
    parser.add_argument('--metadata-stream-probe',action='store_true',help='Isolated official Worker streaming transport; cached matching assets, no C App or workbook Open/Save')
    parser.add_argument('--source',type=Path)
    parser.add_argument('--filters',action='store_true',help='Exercise the real transactions.csv value and date dropdowns')
    parser.add_argument('--busy',action='store_true',help='Require real UI input/rendering before the imported source job completes')
    parser.add_argument('--worker-driver',action='store_true')
    parser.add_argument('--codec-read-driver',action='store_true',help='Isolated C meta/sheets Header decoding over seven immutable native archives and the official Worker; no App Save/Open')
    parser.add_argument('--codec-sorts',action='store_true',help='Separate opt-in saved-sort family proof; requires --codec-read-driver')
    parser.add_argument('--codec-preferences',action='store_true',help='Separate opt-in saved preferences proof; requires --codec-read-driver')
    parser.add_argument('--codec-objects',action='store_true',help='Separate opt-in saved object descriptor proof; requires --codec-read-driver')
    parser.add_argument('--codec-queries',action='store_true',help='Separate opt-in primary/Table query definition proof; requires --codec-read-driver')
    parser.add_argument('--codec-filters',action='store_true',help='Separate full primary filter metadata proof; requires --codec-read-driver')
    parser.add_argument('--codec-format-rows',action='store_true',help='Separate streamed base format properties after owned ledger/bindings; requires --codec-read-driver')
    parser.add_argument('--codec-source-bindings',action='store_true',help='Separate complete current-owner source-binding metadata proof; requires --codec-read-driver')
    parser.add_argument('--codec-source-versions',action='store_true',help='Separate bounded RAW source-ledger metadata streaming proof; requires --codec-read-driver')
    parser.add_argument('--codec-source-counters',action='store_true',help='Separate exact source-version allocation-counter metadata proof; requires --codec-read-driver')
    parser.add_argument('--codec-counters',action='store_true',help='Separate exact object allocation-counter metadata proof; requires --codec-read-driver')
    parser.add_argument('--codec-grid',action='store_true',help='Separate ACKed column/sparse-cell metadata streaming proof; requires --codec-read-driver')
    parser.add_argument('--codec-pivot-marks',action='store_true',help='Separate coherent saved Pivot/Marks definition proof; requires --codec-read-driver')
    parser.add_argument('--codec-tables',action='store_true',help='Separate coherent saved Table source/filter/sort chain proof; requires --codec-read-driver')
    parser.add_argument('--codec-source-options',action='store_true',help='Separate opt-in primary/Table source options proof; requires --codec-read-driver')
    parser.add_argument('--review',action='store_true')
    parser.add_argument('--source-labels-only',action='store_true',help='Original source filenames across primary/Table imports, rename, filter history, query and preview; requires --review and transactions.csv')
    parser.add_argument('--chart-other-only',action='store_true',help='Real same-sheet chart weighted AVG/distinct Other, cached Plot/Marks/representation and history; requires --review, chart-other.csv and --dpr 2')
    parser.add_argument('--source-bindings-only',action='store_true',help='Real registration-bound primary/Table jobs and atomic mixed-copy revocation; requires --review and transactions.csv')
    parser.add_argument('--inspector-sql-only',action='store_true',help='Frozen SQL inspector anatomy, real cached text and fractional scroll ownership; requires --review, transactions.csv and --dpr 2')
    parser.add_argument('--inspector-shadow-only',action='store_true',help='Exact frozen body shadow pixels and actual cache reuse at100/150 percent; requires --review, transactions.csv and --dpr 2')
    parser.add_argument('--inspector-scroll-only',action='store_true',help='Real six-step nested inspector scrolling against frozen anatomy; requires --review, transactions.csv and --dpr 2')
    parser.add_argument('--pane-rail-only',action='store_true',help='Actual retained rail and measured expanded Collapse/Dismiss bounds, colors and narrow shadow absence at100/150 percent; requires --review and transactions.csv')
    parser.add_argument('--pane-shortcuts-only',action='store_true',help='Delivered pane Control/Meta history, refresh/new-sheet/pane toggles and local editing ownership; requires --review and transactions.csv')
    parser.add_argument('--steps-clip-only',action='store_true',help='Focused short Steps Edit/right-details input and retained SQL/Back widths at100/150 percent; requires --review and transactions.csv')
    parser.add_argument('--feedback-review-only',action='store_true',help='Real primary Steps/SQL, historical preview, distinct feedback and readouts; requires --review and transactions.csv')
    parser.add_argument('--workspace-review-only',action='store_true',help='Actual workspace Fields/compact shelves/Marks review with empty and populated same-sheet pivots; requires --review and transactions.csv')
    parser.add_argument('--source-cache-only',action='store_true',help='Actual Open consumer-cache reset, original-sheet republication and 64-sheet rejected-open retention; requires --review and null-text.csv')
    parser.add_argument('--selection-only',action='store_true',help='Real primary/Table/Pivot/manual containing-range selection with retained active cell/cache/history; requires --review and null-text.csv')
    parser.add_argument('--export-cancel-only',action='store_true',help='Real600000-row pending export Escape, retained owners, Undo and small download recovery; requires --review and null-text.csv')
    parser.add_argument('--export-only',action='store_true',help='Actual App Parquet download, honest dispatch failure/recovery and typed reopen in a private folder; requires --review and null-text.csv')
    parser.add_argument('--copy-only',action='store_true',help='Owned memory clipboard acceptance for cached/Worker copies and asynchronous results; requires --review and null-text.csv')
    parser.add_argument('--focus-only',action='store_true',help='Actual pane-focus Help and File outside-click ownership regression; requires --review and transactions.csv')
    parser.add_argument('--review-error-only',action='store_true',help='Real source execution Error dialog review/cancel and Undo recovery; requires --review and transactions.csv')
    parser.add_argument('--review-cell-only',action='store_true',help='Real F2 inline draft capture/cancel, UTF-8 selection and commit/Undo at100/150 percent; requires --review')
    parser.add_argument('--review-guide-only',action='store_true',help='Blank-workbook Guide text/keys/actions/scroll review cancellation at wide100 and short150; requires --review')
    parser.add_argument('--access-only',action='store_true',help='Focused blank-workbook access keys and File/document flow; requires --review')
    parser.add_argument('--zoom-only',action='store_true',help='Focused real C grid zoom/header geometry and pointer checks; requires --review, optional transactions.csv')
    parser.add_argument('--review-controls-only',action='store_true',help='Focused real C contextual targets, editable width/Move and Close guard; requires --review and transactions.csv')
    parser.add_argument('--analysis-only',action='store_true',help='Real C same-sheet Rows/Values matrix and grand totals; requires --review and transactions.csv')
    parser.add_argument('--analysis-columns-only',action='store_true',help='Real C Columns Approved pivot over Rows City/SUM Amount; requires --review and transactions.csv')
    parser.add_argument('--review-sql-only',action='store_true',help='Actual primary/cached Generated SQL origins, scrolling and frozen review cancellation; requires --review and transactions.csv')
    parser.add_argument('--review-query-only',action='store_true',help='Primary query draft/status/real validation-error review while a pivot is selected; requires --review and transactions.csv')
    parser.add_argument('--mixed-statistics-only',action='store_true',help='Real mixed source/Table/matrix/manual statistics and sparse NULL ranges; requires --review and transactions.csv')
    parser.add_argument('--table-statistics-only',action='store_true',help='Real File Import table, same-sheet Table statistics and retained source/matrix/manual cells; requires --review and transactions.csv')
    parser.add_argument('--review-values-only',action='store_true',help='Actual Statistics/Cell Inspector review ownership and NULL/empty/literal values; requires --review and null-text.csv')
    parser.add_argument('--analysis-statistics-only',action='store_true',help='Real C Rows/Columns matrix-cell statistics and retained analytical/source ownership; requires --review and transactions.csv')
    parser.add_argument('--statistics-only',action='store_true',help='Real C source selection statistics, cache retention and filtered-source invalidation; requires --review and transactions.csv')
    parser.add_argument('--query-apply',action='store_true',help='Additionally validate/apply the real edited source through the official Worker; requires --review-query-only')
    parser.add_argument('--review-outbox',type=Path,help='Private shared outbox for an integrated native/browser review gate')
    parser.add_argument('--review-offline',action='store_true',help='Commit a report offline, reload real IndexedDB, then automatically upload')
    parser.add_argument('--dpr',type=int,choices=(1,2),default=1)
    parser.add_argument('--canvas-transform',action='store_true',help='Exercise review picking through an inset canvas CSS transform')
    parser.add_argument('--alt-stress',action='store_true',help='Repeat trusted ribbon Alt shortcuts in the owned platform gate')
    parser.add_argument('--platform',action='store_true',help='Real C DPI/pixel pan/zoom gate (review source fixture)')
    parser.add_argument('--persistence',action='store_true',help='BP0 real native/Worker v12 file compatibility (test-only, no Save UI claim)')
    parser.add_argument('--persistence-owners',action='store_true',help='Separate BP0 v13 owner lifetime/format package rewrite and native readback; no Save UI claim')
    options=parser.parse_args(argv)
    if options.metadata_stream_probe:
        if not options.no_build or options.dpr!=1 or any(value for name,value in vars(options).items() if name not in ('metadata_stream_probe','no_build','dpr')):
            parser.error('--metadata-stream-probe requires --no-build and is isolated from App/other profiles')
    if options.codec_sorts and not options.codec_read_driver:parser.error('--codec-sorts requires --codec-read-driver')
    if options.codec_preferences and not options.codec_read_driver:parser.error('--codec-preferences requires --codec-read-driver')
    if options.codec_objects and not options.codec_read_driver:parser.error('--codec-objects requires --codec-read-driver')
    if options.codec_queries and not options.codec_read_driver:parser.error('--codec-queries requires --codec-read-driver')
    if options.codec_filters and not options.codec_read_driver:parser.error('--codec-filters requires --codec-read-driver')
    if options.codec_format_rows and not options.codec_read_driver:parser.error('--codec-format-rows requires --codec-read-driver')
    if options.codec_source_bindings and not options.codec_read_driver:parser.error('--codec-source-bindings requires --codec-read-driver')
    if options.codec_source_versions and not options.codec_read_driver:parser.error('--codec-source-versions requires --codec-read-driver')
    if options.codec_source_counters and not options.codec_read_driver:parser.error('--codec-source-counters requires --codec-read-driver')
    if options.codec_counters and not options.codec_read_driver:parser.error('--codec-counters requires --codec-read-driver')
    if options.codec_grid and not options.codec_read_driver:parser.error('--codec-grid requires --codec-read-driver')
    if options.codec_pivot_marks and not options.codec_read_driver:parser.error('--codec-pivot-marks requires --codec-read-driver')
    if options.codec_tables and not options.codec_read_driver:parser.error('--codec-tables requires --codec-read-driver')
    if options.codec_source_options and not options.codec_read_driver:parser.error('--codec-source-options requires --codec-read-driver')
    if options.codec_read_driver:
        if sum((options.codec_sorts,options.codec_preferences,options.codec_objects,options.codec_queries,options.codec_source_options,options.codec_tables,options.codec_pivot_marks,options.codec_filters,options.codec_grid,options.codec_counters,options.codec_source_counters,options.codec_source_versions,options.codec_source_bindings,options.codec_format_rows))>1:parser.error('Choose one independent codec family gate')
        if options.dpr!=1 or any(value for name,value in vars(options).items() if name not in ('codec_read_driver','codec_sorts','codec_preferences','codec_objects','codec_queries','codec_source_options','codec_tables','codec_pivot_marks','codec_filters','codec_grid','codec_counters','codec_source_counters','codec_source_versions','codec_source_bindings','codec_format_rows','no_build','dpr')):
            parser.error('--codec-read-driver is an isolated no-window gate; choose one optional codec family, --no-build and default --dpr 1')
        return options
    if options.chart_other_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='chart-other.csv' or options.dpr!=2 or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--chart-other-only requires --review/chart-other.csv/--dpr 2 and excludes other profiles/engine/report-upload options')
    if options.inspector_sql_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or options.dpr!=2 or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--inspector-sql-only requires --review/transactions.csv/--dpr 2 and excludes other profiles/engine/report-upload options')
    if options.inspector_shadow_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or options.dpr!=2 or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--inspector-shadow-only requires --review/transactions.csv/--dpr 2 and excludes other profiles/engine/report-upload options')
    if options.inspector_scroll_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or options.dpr!=2 or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--inspector-scroll-only requires --review/transactions.csv/--dpr 2 and excludes other profiles/engine/report-upload options')
    if options.source_bindings_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--source-bindings-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.source_labels_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--source-labels-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.selection_only or options.source_cache_only:
        profile='--source-cache-only' if options.source_cache_only else '--selection-only'
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='null-text.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error(profile+' requires --review/null-text.csv and excludes other profiles/engine/report-upload options')
    if options.export_only or options.export_cancel_only:
        profile='--export-cancel-only' if options.export_cancel_only else '--export-only'
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='null-text.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error(profile+' requires --review/null-text.csv and excludes other profiles/engine/report-upload options')
    if options.pane_shortcuts_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--pane-shortcuts-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.pane_rail_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--pane-rail-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.steps_clip_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--steps-clip-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.feedback_review_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--feedback-review-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.workspace_review_only:
        other_profiles=sum(bool(value) for name,value in vars(options).items() if name.endswith('_only'))>1
        if not options.review or not options.source or options.source.name!='transactions.csv' or other_profiles or any((options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--workspace-review-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.copy_only:
        if not options.review or not options.source or options.source.name!='null-text.csv' or any((options.focus_only,options.review_error_only,options.review_cell_only,options.review_guide_only,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--copy-only requires --review/null-text.csv and excludes other profiles/engine/report-upload options')
    if options.focus_only:
        if not options.review or not options.source or options.source.name!='transactions.csv' or any((options.review_error_only,options.review_cell_only,options.review_guide_only,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--focus-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.review_error_only:
        if not options.review or not options.source or options.source.name!='transactions.csv' or any((options.review_cell_only,options.review_guide_only,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--review-error-only requires --review/transactions.csv and excludes other profiles/engine/report-upload options')
    if options.review_cell_only:
        if not options.review or any((options.source,options.review_guide_only,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--review-cell-only requires --review and excludes source/other profiles/engine/report-upload options')
    if options.review_guide_only:
        if not options.review or any((options.source,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,options.alt_stress,options.persistence,options.persistence_owners,options.query_apply,options.review_offline,options.review_outbox)):
            parser.error('--review-guide-only requires --review and excludes source/other profiles/engine/report-upload options')
    if options.review and options.worker_probe:
        parser.error('--review and --worker-probe use separate artifact/server profiles')
    if options.query_apply and not options.review_query_only:
        parser.error('--query-apply requires --review-query-only')
    if options.review_values_only:
        if not options.review or not options.source or options.source.name!='null-text.csv':
            parser.error('--review-values-only requires --review and --source tests/fixtures/null-text.csv')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.access_only,options.zoom_only,options.review_controls_only,
                options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--review-values-only is a separate value-review gate without other profiles or report-upload options')
    if options.statistics_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--statistics-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.access_only,options.zoom_only,options.review_controls_only,
                options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--statistics-only is a separate source-statistics gate without other profiles or report-upload options')
    if options.review_sql_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--review-sql-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.access_only,options.zoom_only,options.review_controls_only,
                options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--review-sql-only is a separate Generated SQL gate without other profiles or report-upload options')
    if options.review_query_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--review-query-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.statistics_only,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,
                options.filters,options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error('--review-query-only is a separate source-query gate without other profiles or report-upload options')
    if options.analysis_only or options.analysis_columns_only or options.analysis_statistics_only or options.table_statistics_only or options.mixed_statistics_only:
        profile='--mixed-statistics-only' if options.mixed_statistics_only else '--table-statistics-only' if options.table_statistics_only else '--analysis-statistics-only' if options.analysis_statistics_only else '--analysis-columns-only' if options.analysis_columns_only else '--analysis-only'
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error(profile+' requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_sql_only,options.review_values_only,sum((options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.table_statistics_only,options.mixed_statistics_only))>1,options.statistics_only,options.review_query_only,options.access_only,options.zoom_only,options.review_controls_only,options.filters,
                options.busy,options.worker_driver,options.worker_probe,options.platform,
                options.alt_stress,options.persistence,options.review_offline,options.review_outbox)):
            parser.error(profile+' is a separate analytical gate without other focused profiles or report-upload options')
    if options.review_controls_only:
        if not options.review or not options.source or options.source.name!='transactions.csv':
            parser.error('--review-controls-only requires --review and --source tests/fixtures/transactions.csv')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.access_only,options.zoom_only,options.filters,options.busy,options.worker_driver,
                options.worker_probe,options.platform,options.alt_stress,options.persistence,
                options.review_offline,options.review_outbox)):
            parser.error('--review-controls-only is a separate contextual review gate without access/zoom/engine/platform/report-upload options')
    if options.zoom_only:
        if not options.review:
            parser.error('--zoom-only requires --review')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.access_only,options.review_controls_only,options.filters,options.busy,options.worker_driver,options.worker_probe,
                options.platform,options.alt_stress,options.persistence,options.review_offline,
                options.review_outbox)):
            parser.error('--zoom-only is a separate grid gate without access/engine/platform/report-upload options')
        if options.source and options.source.name!='transactions.csv':
            parser.error('--zoom-only source must be tests/fixtures/transactions.csv')
    if options.access_only:
        if not options.review:
            parser.error('--access-only requires --review')
        if any((options.review_sql_only,options.mixed_statistics_only,options.table_statistics_only,options.review_values_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.analysis_only,options.analysis_columns_only,options.source,options.review_controls_only,options.filters,options.busy,options.worker_driver,options.worker_probe,
                options.platform,options.alt_stress,options.persistence,options.review_offline,
                options.review_outbox)):
            parser.error('--access-only is a separate blank-workbook gate without source/engine/platform/report-upload options')
    if options.filters and (not options.source or options.source.name!='transactions.csv'):
        parser.error('--filters requires --source tests/fixtures/transactions.csv')
    if options.busy and not options.source:
        parser.error('--busy requires a real --source')
    if options.review and not options.chart_other_only and not options.source_cache_only and not options.selection_only and not options.export_only and not options.export_cancel_only and not options.copy_only and not options.review_cell_only and not options.review_guide_only and not options.access_only and not options.zoom_only and not options.review_values_only and (options.worker_driver or not options.source or options.source.name!='transactions.csv'):
        parser.error('--review requires --source tests/fixtures/transactions.csv and cannot use --worker-driver')
    if options.canvas_transform and not options.review:
        parser.error('--canvas-transform requires --review')
    if options.platform and not options.review:
        parser.error('--platform requires --review')
    if options.alt_stress and not options.platform:
        parser.error('--alt-stress requires --platform')
    if options.persistence_owners and any((options.persistence,options.worker_probe,options.worker_driver,options.source,options.review,options.filters,options.busy,options.access_only,options.zoom_only,options.review_controls_only,options.analysis_only,options.analysis_columns_only,options.analysis_statistics_only,options.statistics_only,options.review_query_only,options.review_sql_only,options.review_values_only,options.table_statistics_only,options.mixed_statistics_only,options.platform,options.alt_stress,options.canvas_transform,options.query_apply,options.review_outbox,options.review_offline)):
        parser.error('--persistence-owners is a separate normal-artifact v13 proof without other profiles/source/engine/report options')
    if options.persistence and (options.review or options.worker_driver or options.source):
        parser.error('--persistence is a separate normal-artifact file proof without source/review/worker-driver')
    if options.review_outbox and not options.review:
        parser.error('--review-outbox requires --review')
    if options.review_offline and not options.review:
        parser.error('--review-offline requires --review')
    return options

def main():
    overall=time.monotonic();phases={}
    options=parse_options()
    persistence_mode=options.persistence or options.persistence_owners
    if not options.no_build:
        subprocess.run(['python3','tools/build_web.py']+(['--codec-read-driver'] if options.codec_read_driver else ['--worker-driver'] if options.worker_driver else ['--review'] if options.review else []), cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        if options.codec_read_driver:subprocess.run(['make','tests/test_workbook_codec_native'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
        if persistence_mode:subprocess.run(['make','build/web-tests/test-persistence','build/web-tests/test-persistence-sanitize'],cwd=ROOT,check=True,stdout=subprocess.DEVNULL)
    phases['build_seconds']=round(time.monotonic()-overall,3);phase=time.monotonic()
    latest=json.loads((ROOT/('build/web/codec-read-test/latest.json' if options.codec_read_driver else 'build/web/worker-test/latest.json' if options.worker_driver else 'build/web/review/latest.json' if options.review else 'build/web/app/latest.json')).read_text())
    artifact=Path(latest['directory']); manifest=json.loads((artifact/'build.json').read_text())
    expected_kind='codec-read-driver' if options.codec_read_driver else 'worker-driver' if options.worker_driver else 'review' if options.review else 'application'
    if manifest.get('kind')!=expected_kind:raise RuntimeError('Browser artifact kind does not match requested profile')
    if options.codec_read_driver:
        expected_sources=subprocess.check_output(['make','--no-print-directory','web-codec-read-sources'],cwd=ROOT,text=True).splitlines()
        if manifest.get('sources')!=expected_sources:raise RuntimeError('Codec browser artifact source inventory changed')
    for name,digest in manifest['artifacts'].items():
        if hashlib.sha256((artifact/name).read_bytes()).hexdigest()!=digest:
            raise RuntimeError('Browser artifact changed: '+name)
    if options.metadata_stream_probe:
        # The cached application binary is unused; transport assets must match.
        for name in ('official_worker.mjs','page_wire.mjs'):
            if (artifact/name).read_bytes()!=(ROOT/'web'/name).read_bytes():raise RuntimeError('Rebuild matching browser engine assets before metadata streaming: '+name)
    phases['artifact_validation_seconds']=round(time.monotonic()-phase,3);phase=time.monotonic()
    before=source_metadata(ROOT)
    phases['source_metadata_seconds']=round(time.monotonic()-phase,3);phase=time.monotonic()
    output=ROOT/'tests/results/runs'/f'{datetime.datetime.now(datetime.timezone.utc):%Y%m%dT%H%M%S}-browser-{os.getpid()}'
    output.mkdir(parents=True,exist_ok=True)
    with FileLock(native_lock_path(),'hidden shared C browser acceptance'):
        codec_manifest=None;codec_helper=None;codec_helper_hash=None
        if options.codec_read_driver:
            codec_expected=CODEC_FORMAT_ROWS_FIXTURES if options.codec_format_rows else CODEC_SOURCE_BINDINGS_FIXTURES if options.codec_source_bindings else CODEC_SOURCE_VERSIONS_FIXTURES if options.codec_source_versions else CODEC_SOURCE_COUNTERS_FIXTURES if options.codec_source_counters else CODEC_COUNTERS_FIXTURES if options.codec_counters else CODEC_GRID_FIXTURES if options.codec_grid else CODEC_FILTERS_FIXTURES if options.codec_filters else CODEC_PIVOT_MARKS_FIXTURES if options.codec_pivot_marks else CODEC_TABLES_FIXTURES if options.codec_tables else CODEC_SOURCE_OPTIONS_FIXTURES if options.codec_source_options else CODEC_QUERIES_FIXTURES if options.codec_queries else CODEC_OBJECTS_FIXTURES if options.codec_objects else CODEC_PREFERENCES_FIXTURES if options.codec_preferences else CODEC_SORTS_FIXTURES if options.codec_sorts else CODEC_FIXTURES
            codec_helper=ROOT/'tests/test_workbook_codec_native'
            codec_helper_hash=hashlib.sha256(codec_helper.read_bytes()).hexdigest()
            codec_directory=output/'codec-fixtures';codec_directory.mkdir()
            emitted=subprocess.run([str(codec_helper),'--emit-format-rows-fixtures' if options.codec_format_rows else '--emit-source-bindings-fixtures' if options.codec_source_bindings else '--emit-source-versions-fixtures' if options.codec_source_versions else '--emit-source-counters-fixtures' if options.codec_source_counters else '--emit-counters-fixtures' if options.codec_counters else '--emit-grid-fixtures' if options.codec_grid else '--emit-filters-fixtures' if options.codec_filters else '--emit-pivot-marks-fixtures' if options.codec_pivot_marks else '--emit-tables-fixtures' if options.codec_tables else '--emit-source-options-fixtures' if options.codec_source_options else '--emit-queries-fixtures' if options.codec_queries else '--emit-objects-fixtures' if options.codec_objects else '--emit-preferences-fixtures' if options.codec_preferences else '--emit-sorts-fixtures' if options.codec_sorts else '--emit-fixtures',str(codec_directory)],cwd=ROOT,text=True,capture_output=True,timeout=30)
            (output/'codec-native-create.txt').write_text(emitted.stdout+emitted.stderr)
            if emitted.returncode:raise RuntimeError('Native codec fixture emission failed; see '+str(output/'codec-native-create.txt'))
            codec_emitted=json.loads(emitted.stdout)
            codec_manifest=codec_fixture_manifest(codec_directory,codec_emitted,codec_expected)
            (output/'codec-fixture-manifest.json').write_text(json.dumps(codec_manifest,indent=2)+'\n')
        persistence_binary=ROOT/'build/web-tests/test-persistence'
        persistence_sanitizer=ROOT/'build/web-tests/test-persistence-sanitize'
        persistence_hashes={}
        if persistence_mode:
            for executable in (persistence_binary,persistence_sanitizer):
                persistence_hashes[str(executable)]=hashlib.sha256(executable.read_bytes()).hexdigest()
            fixture=output/'native-fixture.dxls'
            created=subprocess.run([str(persistence_binary),'create-owners' if options.persistence_owners else 'create',str(fixture)],cwd=ROOT,text=True,capture_output=True,check=True,timeout=30)
            (output/'persistence-native-create.txt').write_text(created.stdout+created.stderr)
            fixture_hash=hashlib.sha256(fixture.read_bytes()).hexdigest()
        if options.review:
            from review_service import ReviewServer
            from review_queue import ReviewQueue
            from native_review import default_project
            queue=ReviewQueue(options.review_outbox or output/'queue-outbox',default_project())
            server=ReviewServer(('127.0.0.1',0),queue,artifact)
        else:
            server=LoopbackServer(('127.0.0.1',0),functools.partial(Handler,directory=str(artifact)))
            if persistence_mode:server.persistence_fixture=fixture
            if options.metadata_stream_probe:server.metadata_stream_import_map=json.loads((artifact/'vendor.json').read_text())['import_map']
            if options.codec_read_driver:
                server.codec_manifest=codec_manifest;server.codec_requests=[]
                server.codec_files={'/__codec-fixtures/'+entry['name']:(codec_directory/entry['name'],entry) for entry in codec_manifest['files']}
        phases['server_setup_seconds']=round(time.monotonic()-phase,3)
        thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
        begin=time.monotonic()
        try:
            command=['node','tools/test_web_app.mjs',f'http://127.0.0.1:{server.server_port}/',str(output)]
            if options.worker_driver:command.append('--worker-driver')
            if options.metadata_stream_probe:command.append('--metadata-stream-probe')
            if options.codec_read_driver:command.append('--codec-read-driver')
            if options.codec_sorts:command.append('--codec-sorts')
            if options.codec_preferences:command.append('--codec-preferences')
            if options.codec_objects:command.append('--codec-objects')
            if options.codec_queries:command.append('--codec-queries')
            if options.codec_source_options:command.append('--codec-source-options')
            if options.codec_tables:command.append('--codec-tables')
            if options.codec_pivot_marks:command.append('--codec-pivot-marks')
            if options.codec_filters:command.append('--codec-filters')
            if options.codec_grid:command.append('--codec-grid')
            if options.codec_counters:command.append('--codec-counters')
            if options.codec_source_counters:command.append('--codec-source-counters')
            if options.codec_source_versions:command.append('--codec-source-versions')
            if options.codec_source_bindings:command.append('--codec-source-bindings')
            if options.codec_format_rows:command.append('--codec-format-rows')
            if options.worker_probe: command.append('--worker-probe')
            if options.source: command.extend(['--source',str(options.source.resolve(strict=True))])
            if options.filters: command.append('--filters')
            if options.busy: command.append('--busy')
            if options.review: command.append('--review')
            if options.source_labels_only: command.append('--source-labels-only')
            if options.chart_other_only: command.append('--chart-other-only')
            if options.source_bindings_only: command.append('--source-bindings-only')
            if options.inspector_scroll_only: command.append('--inspector-scroll-only')
            if options.inspector_shadow_only: command.append('--inspector-shadow-only')
            if options.inspector_sql_only: command.append('--inspector-sql-only')
            if options.pane_rail_only: command.append('--pane-rail-only')
            if options.pane_shortcuts_only: command.append('--pane-shortcuts-only')
            if options.steps_clip_only: command.append('--steps-clip-only')
            if options.feedback_review_only: command.append('--feedback-review-only')
            if options.workspace_review_only: command.append('--workspace-review-only')
            if options.export_only: command.append('--export-only')
            if options.export_cancel_only: command.append('--export-cancel-only')
            if options.selection_only: command.append('--selection-only')
            if options.source_cache_only: command.append('--source-cache-only')
            if options.copy_only: command.append('--copy-only')
            if options.focus_only: command.append('--focus-only')
            if options.review_error_only: command.append('--review-error-only')
            if options.review_cell_only: command.append('--review-cell-only')
            if options.review_guide_only: command.append('--review-guide-only')
            if options.access_only: command.append('--access-only')
            if options.zoom_only: command.append('--zoom-only')
            if options.review_controls_only: command.append('--review-controls-only')
            if options.analysis_only: command.append('--analysis-only')
            if options.analysis_columns_only: command.append('--analysis-columns-only')
            if options.review_sql_only: command.append('--review-sql-only')
            if options.review_query_only: command.append('--review-query-only')
            if options.mixed_statistics_only: command.append('--mixed-statistics-only')
            if options.table_statistics_only: command.append('--table-statistics-only')
            if options.review_values_only: command.append('--review-values-only')
            if options.analysis_statistics_only: command.append('--analysis-statistics-only')
            if options.statistics_only: command.append('--statistics-only')
            if options.query_apply: command.append('--query-apply')
            if options.review_offline: command.append('--review-offline')
            command.extend(['--dpr',str(options.dpr)])
            if options.canvas_transform:command.append('--canvas-transform')
            if options.platform:command.append('--platform')
            if options.alt_stress:command.append('--alt-stress')
            if options.persistence:command.append('--persistence')
            if options.persistence_owners:command.append('--persistence-owners')
            process=subprocess.Popen(command,cwd=ROOT,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,start_new_session=True)
            try:log,_=process.communicate(timeout=100)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid,signal.SIGTERM)
                try:log,_=process.communicate(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(process.pid,signal.SIGKILL);log,_=process.communicate()
            (output/'driver.txt').write_text(log)
            reader_error=None
            if options.codec_read_driver:
                try:
                    if codec_fixture_manifest(codec_directory,codec_emitted,codec_expected)!=codec_manifest:raise RuntimeError('Codec proof mutated its original archives')
                    if hashlib.sha256(codec_helper.read_bytes()).hexdigest()!=codec_helper_hash:raise RuntimeError('Native codec helper changed during acceptance')
                    expected={entry['name'] for entry in codec_manifest['files']}
                    requested=[entry['name'] for entry in server.codec_requests]
                    if process.returncode==0 and (len(requested)!=len(expected) or set(requested)!=expected):raise RuntimeError('Codec proof did not fetch each immutable native archive exactly once')
                except Exception as error:reader_error=str(error)
                (output/'codec-native-provenance.json').write_text(json.dumps({'helper':str(codec_helper.relative_to(ROOT)),'helper_sha256':codec_helper_hash,'fixtures':codec_manifest,'served_archives':server.codec_requests,'unchanged':not reader_error,'error':reader_error},indent=2)+'\n')
            if persistence_mode and process.returncode==0:
                try:
                    for executable in (persistence_binary,persistence_sanitizer):
                        verified=subprocess.run([str(executable),'verify-owners' if options.persistence_owners else 'verify',str(fixture),str(output/'bp0-browser-export.dxls')],cwd=ROOT,text=True,capture_output=True,timeout=30)
                        (output/('persistence-native-verify-sanitize.txt' if executable==persistence_sanitizer else 'persistence-native-verify.txt')).write_text(verified.stdout+verified.stderr)
                        if verified.returncode or re.search(r'ERROR: AddressSanitizer|runtime error:',verified.stderr):raise RuntimeError('Native workbook readback failed: '+str(executable))
                    if hashlib.sha256(fixture.read_bytes()).hexdigest()!=fixture_hash:raise RuntimeError('Browser/native proof mutated its original fixture')
                    if any(hashlib.sha256(Path(name).read_bytes()).hexdigest()!=digest for name,digest in persistence_hashes.items()):raise RuntimeError('Native proof helper changed during acceptance')
                except Exception as error:reader_error=str(error)
            if options.review and not options.chart_other_only and not options.inspector_sql_only and not options.inspector_shadow_only and not options.inspector_scroll_only and not options.source_bindings_only and not options.pane_shortcuts_only and not options.source_labels_only and not options.access_only and not options.zoom_only and not options.review_controls_only and not options.analysis_only and not options.analysis_columns_only and not options.review_query_only and not options.statistics_only and not options.analysis_statistics_only and not options.review_values_only and not options.table_statistics_only and not options.mixed_statistics_only and not options.review_sql_only and not options.review_guide_only and not options.review_cell_only and not options.review_error_only and not options.focus_only and not options.copy_only and not options.workspace_review_only and not options.feedback_review_only and not options.steps_clip_only and not options.pane_rail_only and not options.export_only and not options.export_cancel_only and not options.selection_only and not options.source_cache_only and process.returncode==0:
                try:
                    from native_review import ReviewStore,default_project
                    report=json.loads((output/'report-manifest.json').read_text());report_id=report['report_id']
                    if not re.fullmatch(r'[A-Za-z0-9_-]{1,95}',report_id):raise RuntimeError('Invalid report archive identity')
                    extracted=output/'extracted';folder=extracted/report_id;folder.mkdir(parents=True)
                    shutil.copy2(output/'report-manifest.json',folder/'manifest.json');shutil.copy2(output/'report-frame.png',folder/'frame.png')
                    retrieved=ReviewStore(extracted,default_project()).read(report_id)
                    if retrieved['manifest']['target']['widget']['id']!='filter.search':raise RuntimeError('Reader retrieved the wrong C target')
                    (output/'reader.json').write_text(json.dumps({'status':'passed','report_id':report_id,'target':retrieved['manifest']['target'],'image':retrieved['image'],'extracted':str(extracted)},indent=2)+'\n')
                except Exception as error:reader_error=str(error)
            after=source_metadata(ROOT)
            unchanged=after['source_sha256']==before['source_sha256'] and after['commit']==before['commit']
            artifacts_unchanged=all(hashlib.sha256((artifact/name).read_bytes()).hexdigest()==digest for name,digest in manifest['artifacts'].items())
            record={'status':'passed' if process.returncode==0 and unchanged and artifacts_unchanged and not reader_error else 'failed','source':before,'artifact':manifest,'reader_error':reader_error,'native_persistence_helpers':persistence_hashes,
                    'command':command,'exit_code':process.returncode,'source_unchanged':unchanged,'artifacts_unchanged':artifacts_unchanged,
                    'browser_seconds':round(time.monotonic()-begin,3),'elapsed_seconds':round(time.monotonic()-overall,3),'setup':phases,'desktop_visibility':'owned headless browser, simulated focus; no user profile'}
            record['profile']='metadata-stream-probe' if options.metadata_stream_probe else 'codec-read-driver' if options.codec_read_driver else 'chart-other-only' if options.chart_other_only else 'inspector-sql-only' if options.inspector_sql_only else 'inspector-shadow-only' if options.inspector_shadow_only else 'inspector-scroll-only' if options.inspector_scroll_only else 'source-bindings-only' if options.source_bindings_only else 'pane-shortcuts-only' if options.pane_shortcuts_only else 'source-labels-only' if options.source_labels_only else 'source-cache-only' if options.source_cache_only else 'selection-only' if options.selection_only else 'export-cancel-only' if options.export_cancel_only else 'export-only' if options.export_only else 'pane-rail-only' if options.pane_rail_only else 'steps-clip-only' if options.steps_clip_only else 'feedback-review-only' if options.feedback_review_only else 'workspace-review-only' if options.workspace_review_only else 'copy-only' if options.copy_only else 'focus-only' if options.focus_only else 'review-error-only' if options.review_error_only else 'review-cell-only' if options.review_cell_only else 'review-guide-only' if options.review_guide_only else 'persistence-owners' if options.persistence_owners else 'review-sql-only' if options.review_sql_only else 'mixed-statistics-only' if options.mixed_statistics_only else 'table-statistics-only' if options.table_statistics_only else 'review-values-only' if options.review_values_only else 'analysis-statistics-only' if options.analysis_statistics_only else 'statistics-only' if options.statistics_only else 'review-query-apply' if options.query_apply else 'review-query-only' if options.review_query_only else 'analysis-columns-only' if options.analysis_columns_only else 'analysis-only' if options.analysis_only else 'review-controls-only' if options.review_controls_only else 'zoom-only' if options.zoom_only else 'access-only' if options.access_only else 'standard'
            if options.metadata_stream_probe:
                record['scope']='Official Worker streaming transport only: synthetic ordered typed SQL, original registration, real reader cleanup, backpressure, cancellation and recovery. No C App, shared column/cell decoder, native archive parity, workbook Open/Save, UI, queue or service promotion.'
            if options.codec_read_driver:
                if options.codec_sorts:record['profile']='codec-sorts-driver'
                if options.codec_preferences:record['profile']='codec-preferences-driver'
                if options.codec_objects:record['profile']='codec-objects-driver'
                if options.codec_queries:record['profile']='codec-queries-driver'
                if options.codec_source_options:record['profile']='codec-source-options-driver'
                if options.codec_tables:record['profile']='codec-tables-driver'
                if options.codec_pivot_marks:record['profile']='codec-pivot-marks-driver'
                if options.codec_filters:record['profile']='codec-filters-driver'
                if options.codec_grid:record['profile']='codec-grid-driver'
                if options.codec_counters:record['profile']='codec-counters-driver'
                if options.codec_source_counters:record['profile']='codec-source-counters-driver'
                if options.codec_source_versions:record['profile']='codec-source-versions-driver'
                if options.codec_source_bindings:record['profile']='codec-source-bindings-driver'
                if options.codec_format_rows:record['profile']='codec-format-rows-driver'
                record['codec_fixture_manifest']=codec_manifest;record['codec_helper_sha256']=codec_helper_hash
                record['scope']='Isolated C partial '+('meta/sheets/complete current-owner definitions/RAW ledger/bindings/streamed base format properties over four immutable native archives' if options.codec_format_rows else 'meta/sheets/complete current-owner definitions/RAW ledger/source-binding Header decoder over four immutable native archives' if options.codec_source_bindings else 'meta/sheets/object-descriptors/independent counters/RAW source-ledger Header decoder over four immutable native archives' if options.codec_source_versions else 'meta/sheets/object-descriptors/independent-object-and-source-version-counters Header decoder over four immutable native archives' if options.codec_source_counters else 'meta/sheets/object-descriptors/exact-allocation-counters Header decoder over four immutable native archives' if options.codec_counters else 'meta/sheets/ACKed-streamed column presentation and sparse cells Header decoder over four immutable native archives' if options.codec_grid else 'meta/sheets/empty-object-descriptors/queries/options/sorts/full-primary-filter Header decoder over four immutable native archives' if options.codec_filters else 'meta/sheets/objects/queries/options/Table-sources-filters-sorts/Pivot-Marks definitions Header decoder over four immutable native archives' if options.codec_pivot_marks else 'meta/sheets/objects/primary-Table-queries/source-options/complete-Table-sources-filters-sorts Header decoder over four immutable native archives' if options.codec_tables else 'meta/sheets/objects/primary-Table-queries/source-options Header decoder over four immutable native archives' if options.codec_source_options else 'meta/sheets/objects/primary-Table-query Header decoder over six immutable native archives including two deliberately invalid query packages' if options.codec_queries else 'meta/sheets/object-descriptor Header decoder over four immutable native archives' if options.codec_objects else 'meta/sheets/preferences Header decoder over six closed native archives including two deliberately invalid packages' if options.codec_preferences else 'meta/sheets/secondary-sort Header decoder over four immutable native archives' if options.codec_sorts else 'meta/sheets Header decoder over seven immutable native v12/v13/v14 archives')+' through the official Worker and existing DXP1 Page decoder. No complete Workbook, App publication, browser Save/Open, later-family migration, physical input or live review queue claim.'
            if options.chart_other_only:
                record['scope']='Actual shared C same-sheet Rows Group/Columns Bucket/SUM Rank/AVG Value/COUNT DISTINCT Customer through real authoring menus and official Worker. Cached source-derived Other7.5/3, exact typed readouts, Bar/Plot/Marks pointer and keyboard actions, presentation-only Undo/Redo and source/manual retention are checked with zero post-publication jobs. No injected analytical state, clipboard activation, report upload, persistence or physical input claim.'
            if options.inspector_sql_only:
                record['scope']='Actual cached SQL inspector at frozen100/150 wide/narrow anatomy, intrinsic footer controls, trusted fractional code/outer scrolling and keyboard/reset ownership. Real five-filter SQL is copied only to an owned pre-loader memory writer; zero UI-only jobs and no OS clipboard/report upload. Actual SQL text and wrapping differ from the mock.'
            if options.inspector_shadow_only:
                record['scope']='Exact original frozen body shadow RGBA on matched white strips at narrow100/150, coverage-eligible transformed samples, real pane/scale/scroll controls and owned cache reuse with zero post-setup queries. Wide/retained/dismissed absence is checked. No model rounding exception, top/bottom/exiting-frame/full GPU parity, physical input, clipboard or saved reports.'
            if options.inspector_scroll_only:
                record['scope']='Six real source Steps at frozen wide/narrow100/150 geometry, trusted nested scrolling/reversal and keyboard reveal, clipped semantic picking and Collapse/reopen lifetime with zero post-setup jobs. No clipboard, saved reports or physical/native parity claim.'
            if options.source_bindings_only:
                record['scope']='Actual same-artifact adapter instrumentation observes captured session registrations and untouched C-built request bindings through real same-name imports, rename/filter/preview and bounded mixed Copy. Controlled real cached-source drop before admission must reject the original vector before SQL/ticket/clipboard publication. One real engine, owned memory clipboard; no fresh token lookup, product hooks, physical chooser, workbook persistence, cross-engine epoch or native source-format claim.'
            if options.pane_shortcuts_only:
                record['scope']='Actual delivered trusted Control/Meta Undo and Redo from read-only Steps/Fields/Workbook/ribbon, genuine F5 refresh and ShiftF11 sheet creation, selected-Pivot primary+AltF pane toggle, and local SQL copy/search/editor history. Clipboard uses the pre-loader owned memory adapter. No reserved browser O/S/W/N, physical input, native AppKit routing, saved reports or browser persistence claim.'
            if options.source_labels_only:
                record['scope']='Actual trusted primary and same-sheet Table imports with identical Unicode/apostrophe/import-like filenames and different real five/three-row sources; original painted labels stay independent of registered keys and renamed sheet/Table titles through filter history, raw previews, query Apply/Undo and object import Undo/Redo. Private fixture failures remain setup failures; idle ownership and zero jobs are checked separately. No browser Save/Open, physical chooser, report upload, clipboard or full registration-epoch claim.'
            if options.source_cache_only:
                record['scope']='Actual successful source Open clears observable matrix/owner state and publishes real new values; returning to the original sheet republishes its unchanged Table/Pivot/manual definitions. Real ShiftF11 fills64 sheets and another valid source Open is rejected without changing the active cached source/model/history/request. Native source-cache coverage owns exact allocation/bookmark retirement; browser checks consumer state only. No new review fields, fake callbacks, clipboard or saved reports.'
            if options.selection_only:
                record['scope']='Actual C primary/Table/Pivot/manual containing ranges through trusted Ctrl/Cmd+A, preserved active cell and reversed directions, real header/frame/used/canvas transitions, unchanged source/pages/history/dirty and zero selection-only jobs. Setup uses real File import and Rows/Values controls; metrics disabled through actual checkboxes. No clipboard, reports, dense enumeration or synthetic selection bridge.'
            if options.export_cancel_only:
                record['scope']='Actual600000-row source Page256/COUNT, trusted kind7 export then Escape before completion/Blob/download, owned cancellation cleanup, normal Undo to six rows and real small Parquet recovery download. A missed pending race fails honestly; no held callbacks, artificial delay, OS clipboard, user Downloads, saved reports or Save claim.'
            if options.export_only:
                record['scope']='Actual shared-C Parquet export into an owned headless Chromium folder, real Blob/file hash and footer, synchronous dispatch failure/recovery, exact BIGINT/DECIMAL and NULL/empty/literal reimport, and original source/manual ownership. No user Downloads, OS clipboard, durable Save or artificial pending/cancellation claim.'
            if options.pane_rail_only:
                record['scope']='Actual C Transformations expanded/retained/expanded/retained/dismissed at four wide/narrow100/150 percent viewports with Compact ribbon, frozen semantic proof before live controls, wide32-unit reservation and narrow unchanged canvas. Presented compositor PNGs measure exact237/242/246 rail interior and retained left-strip RGBA equality to dismissed canvas, with retained source/pages/selection/history and zero post-setup jobs. No physical OS input, new C hooks, clipboard or saved reports.'
            if options.steps_clip_only:
                record['scope']='Focused real City-filtered Steps workflow: actual short100/150 percent Edit semantic proof, trusted live Edit/Cancel/right-details input, SQL/Back paint and source/cache/request retention with zero cycle jobs. One normal Undo restores the original five-row CSV; no navigation/preview/distinct-error workload, clipboard activation or saved reports.'
            if options.feedback_review_only:
                record['scope']='Actual primary Steps/inline SQL, historical preview and passive readouts, plus real distinct scrolling/no-match/later-row conversion error; frozen picking/cancel retains observed source/model/input/request ownership. Genuine pending observations are recorded without fabricated delays; deep held-reply/history proof remains native. No clipboard activation or saved reports.'
            if options.workspace_review_only:
                record['scope']='Actual shared-C Fields/compact Rows/Columns/Marks/Measures/Filters workspace targets at100/150 percent with empty and real Worker-populated same-sheet pivots; disabled sole-measure removal, exact semantic picking/cancel and retained source/model/pages/input/jobs. No clipboard or saved reports; native registry scope checks remain separate.'
            if options.copy_only:
                record['scope']='Actual cached/uncached C range copies into a pre-loader owned memory clipboard, asynchronous pending/success/failure and frozen acknowledgement ownership; exact NULL/empty/literal/quote TSV and retained workbook selection. No OS clipboard, reports or export acceptance.'
            if options.focus_only:
                record['scope']='Actual primary source retained through Transformations keyboard focus→pointer Help, measured Guide Close, and File outside-click dismissal before second Help activation; no additional jobs/reports or guessed Fields coordinates.'
            if options.review_error_only:
                record['scope']='Real validated source query execution failure, actual idle Error baseline, wide100/short150 wrapped text/actions/disabled browser Relink picking and cancellation without jobs; normal Close/Escape and source Undo restore actual rows. No Copy/Relink activation, reports or fabricated clipping/pending state.'
            if options.review_cell_only:
                record['scope']='Actual blank-cell F2/trusted UTF-8 and quote editing, partial selection/hscroll and single input picking at100/150%; capture cancellation retains draft/history/model and adds no jobs. Normal Escape preserves sparse NULL; real Enter then Undo restores missing cell. No clipboard, reports or physical typing claim.'
            if options.review_guide_only:
                record['scope']='Actual blank-workbook Help/Guide, wide100 and short150 text/key/action/scroll/backdrop picking and composer cancellation, retained nonzero scroll/focus/document state and ordinary dismissal; no database jobs, saved reports or physical gesture acceptance.'
            if options.persistence_owners:
                record['scope']='BP0 real native v13 owner-only workbook rewritten/exported/reopened in one official Worker, then native release/ASan verification of exact uint64 object lifetimes, sparse cells and spatial formats. Original fixture/helper hashes stay unchanged; no browser Save/Open codec/controller or OPFS acceptance.'
                record['persistence_fixture']={'path':str(fixture),'sha256':fixture_hash,'version':13}
            if options.review_sql_only:
                record['scope']='Actual selected-pivot primary Generated SQL and cached pivot SQL with distinct pinned origins, real multiline source Apply, nonzero SQL wheel/clamp and content/Copy/Close/backdrop picking; cancellation retains source/matrix/manual state with no extra jobs. No Copy activation, clipboard or saved reports.'
            if options.mixed_statistics_only:
                record['scope']='Actual A1:N19 mixed statistics across imported primary/Table data, Rows matrix, sparse text and missing cells; global typed distinct/readout, NULL-only metrics, owner/page retention and no idle jobs. Engine limits and physical gestures remain separate.'
            if options.table_statistics_only:
                record['scope']='Actual shared C File Import table chooser, cancellation/destination ownership, same-sheet imported Table statistics and retained primary source/matrix/sparse cells; no database chooser, persistence or general rejected-file memory-recovery acceptance.'
            if options.review_values_only:
                record['scope']='Actual C Statistics metric checkbox/cache picking and Cell Inspector NULL/empty/literal text with pinned source origins, typed owned values and capture/composer cancellation; no Copy activation, clipboard, reports or forced pending-query acceptance.'
            if options.analysis_statistics_only:
                record['scope']='Actual C Rows/Values and Columns matrix-cell selection statistics through the official Worker, object/revision and sparse/source retention, NULL-aware result counts and query-free idle cache; mixed/manual statistics, forced in-flight cancellation and report upload remain separate.'
            if options.statistics_only:
                record['scope']='Actual C primary-source selection statistics through the official Worker, filtered-source revision/readout invalidation, retained status/dialog values and no viewport/idle resubmission; no mixed/manual/object statistics, forced in-flight cancellation or report upload.'
            if options.analysis_columns_only:
                record['scope']='Shared C same-sheet Rows City/Columns Approved/SUM Amount, real official Worker Columns matrix and source-derived footer totals, source/manual retention; no broad analytical or report-upload acceptance.'
            if options.review_query_only:
                record['scope']='Actual primary source-query editor reached through selected-pivot Transformations, frozen target/draft/owner retention with no report-triggered SQL; '+('explicit real Worker validation and applied source publication. ' if options.query_apply else 'actual editor Cancel preserves the displayed source. ')+'One additional real missing-column validation exposes owned status/error feedback and actual TextLines/wheel clamp; frozen picking/cancel and normal Escape retain observed revision/selection/source/model/pages with no extra jobs. Full input-history and pending-reply ownership remain separate native gates.'
            if options.analysis_only:
                record['scope']='Shared C same-sheet empty pivot, Rows City/SUM Amount assignment, real official Worker matrix/grand totals, return to source and sparse-cell retention; no Columns pivot, broad analytics or report-upload acceptance.'
            if options.review_controls_only:
                record['scope']='Shared C source/manual contextual targets, editable width/Move drafts and guarded Close, modal ownership and frozen-state cancellation; real source import, no analytical-object or report-upload acceptance.'
            if options.zoom_only:
                record['scope']='Shared C blank/source grid zoom, fixed header geometry and rendered label bounds, pointer selection/resize/filter and independent UI scale; no report upload or physical gesture acceptance.'
            if options.access_only:
                record['scope']='Shared C access keys and File/document interactions on a blank workbook; no Worker query, clipboard delivery or report-upload acceptance.'
            (output/'summary.json').write_text(json.dumps(record,indent=2)+'\n')
            print(json.dumps({'status':record['status'],'result':str(output),'elapsed_seconds':record['elapsed_seconds']}))
            return 0 if record['status']=='passed' else 1
        finally:
            server.shutdown();server.server_close();thread.join()
if __name__=='__main__':
    raise SystemExit(main())
