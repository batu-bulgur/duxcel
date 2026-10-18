#!/usr/bin/env python3
"""Codec harness boundaries without browser, engine, GUI or shared fixtures."""
import contextlib
import functools
import io
import json
from pathlib import Path
import subprocess
import tempfile
import threading
import unittest
from unittest import mock
from urllib.error import HTTPError
from urllib.request import urlopen
import test_web_app as harness
import test as runner


class CodecHarnessTests(unittest.TestCase):
    def fixtures(self, directory, inventory=harness.CODEC_FIXTURES):
        files=[]
        for name,(version,layout) in inventory.items():
            path=directory/name;path.write_bytes(name.encode());path.chmod(0o444)
            files.append(dict(name=name,version=version,layout=layout))
        return dict(schema='duxcel.codec-fixtures',version=1,files=files)

    def test_mode_rejects_other_profiles_before_any_process(self):
        self.assertTrue(harness.parse_options(['--codec-read-driver','--no-build']).codec_read_driver)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-sorts','--no-build']).codec_sorts)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-preferences','--no-build']).codec_preferences)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-objects','--no-build']).codec_objects)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-queries','--no-build']).codec_queries)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-source-options','--no-build']).codec_source_options)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-tables','--no-build']).codec_tables)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-pivot-marks','--no-build']).codec_pivot_marks)
        self.assertTrue(harness.parse_options(['--codec-read-driver','--codec-filters','--no-build']).codec_filters)
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-filters'])
        for family in ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-filters',family])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-pivot-marks'])
        for family in ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-pivot-marks',family])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-tables'])
        for family in ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-tables',family])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-sorts'])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-preferences'])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-sorts','--codec-preferences'])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-objects'])
        for family in ('--codec-sorts','--codec-preferences'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-objects',family])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-queries'])
        for family in ('--codec-sorts','--codec-preferences','--codec-objects'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-queries',family])
        with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-source-options'])
        for family in ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries'):
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(['--codec-read-driver','--codec-source-options',family])
        for option in ('--worker-driver','--review','--persistence','--canvas-transform','--dpr=2'):
            with self.subTest(option=option),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                harness.parse_options(['--codec-read-driver',option])

    def test_metadata_stream_probe_requires_matching_cached_assets_and_no_app_profile(self):
        self.assertTrue(harness.parse_options(['--metadata-stream-probe','--no-build']).metadata_stream_probe)
        for arguments in (['--metadata-stream-probe'],['--metadata-stream-probe','--no-build','--review'],['--metadata-stream-probe','--no-build','--codec-read-driver'],['--metadata-stream-probe','--no-build','--worker-driver'],['--metadata-stream-probe','--no-build','--source','x.csv'],['--metadata-stream-probe','--no-build','--dpr','2']):
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)

    def test_grid_mode_requires_independent_driver_and_exact_four_fixture_inventory(self):
        options=harness.parse_options(['--codec-read-driver','--codec-grid','--no-build'])
        self.assertTrue(options.codec_grid and options.no_build);self.assertEqual(options.dpr,1)
        invalid=[['--codec-grid'],*(['--codec-read-driver','--codec-grid',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        self.assertEqual(harness.CODEC_GRID_FIXTURES,{'grid-v12.dxls':(12,25),'grid-v13.dxls':(13,25),'grid-v14.dxls':(14,25),'grid-v14-reordered.dxls':(14,25)})
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_GRID_FIXTURES)
            manifest=harness.codec_fixture_manifest(path,emitted,harness.CODEC_GRID_FIXTURES)
            self.assertEqual([row['name'] for row in manifest['files']],list(harness.CODEC_GRID_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Incomplete native codec fixture set'):harness.codec_fixture_manifest(path,emitted)

    def test_full_tier_grid_command_is_serial_after_filters_and_requires_its_actual_pass_marker(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;grid=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--grid-only']
            self.assertEqual(len(grid),2)
            for index,call in grid:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--filters-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native grid metadata 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native primary filters 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_node_grid_report_guards_reject_missing_or_changed_runtime_proofs(self):
        source=Path(__file__).with_name('test_web_app.mjs').read_text()
        start=source.index('function validateCodecGridReport(report,check) {');function=source[start:source.index('\nconst filters=',start)]
        report=dict(mode='grid',fixtures=4,headers=6,queries=62,rejections=5,stale=2,normalFixtureQueries=[8]*4,
            familyExecutions=dict(meta=12,sheets=12,columns=12,cells=10,describe=16),
            acceptedFeeds=dict(columns256=9,columns29=7,columns7=3,columnsEof=10,cells256=6,cells29=3,cells7=3,cellsEof=6),
            matchedDataCallbacks=dict(columns=21,cells=15),matchedEofCallbacks=dict(columns=10,cells=6),completedStreams=dict(columns=10,cells=6),errorStreams=dict(columns=2,cells=3),poisonedDataChunks=dict(columns=19,cells=12),
            chunkCallbacks=53,acceptedAcks=47,doneCallbacks=22,staleChunkCallbacks=1,staleDoneCallbacks=1,heldCallbacks=1,cancelledCallbacks=1,rejectedFeeds=5,staleScalarReservationBytes=42,staleTerminalReservationBytes=2070,
            rejectedFirstRows=[256,256,256,256,0],maximumCompletedHeaderOwned=100000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2097300,
            transport=dict(stream=dict(submissions=22,chunkCallbacks=53,acceptedAcks=47,doneCallbacks=22,held=1,released=1,cancelled=1)))
        for name in ('lateWidthRejected','requiredColumnNameRejected','requiredCellTextRejected','duplicateCellRejected','budgetRejected','heldSecondChunkCancelled','wholeCandidateRetry','freshRecovery','longColumnNameExact','maximumCellTextExact','actualEmptyCellPresent','missingCellAbsent'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.familyExecutions.cells--,r=>r.acceptedFeeds.cellsEof--,r=>r.completedStreams.columns--,
 r=>r.poisonedDataChunks.cells--,r=>r.staleDoneCallbacks=0,r=>r.acceptedAcks++,r=>r.staleScalarReservationBytes--,
 r=>r.rejectedFirstRows[3]=0,r=>r.missingCellAbsent=false,r=>r.maximumCompletedHeaderOwned=2097153,
 r=>r.peakOwnedInput=4194305,r=>r.transport.stream.doneCallbacks--];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: actual grid report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 13 corrupted receipts',result.stdout)

    def test_manifest_covers_bytes_and_refuses_unowned_extra_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);emitted=self.fixtures(directory)
            before=harness.codec_fixture_manifest(directory,emitted)
            file=directory/'v12.dxls';file.chmod(0o644);file.write_bytes(b'changed');file.chmod(0o444)
            self.assertNotEqual(before,harness.codec_fixture_manifest(directory,emitted))
            (directory/'missing-source.csv').write_text('unrelated')
            with self.assertRaisesRegex(RuntimeError,'Unexpected files'):
                harness.codec_fixture_manifest(directory,emitted)

    def test_manifest_refuses_duplicate_names_schema_changes_and_links(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);emitted=self.fixtures(directory)
            duplicate=json.loads(json.dumps(emitted));duplicate['files'][1]=duplicate['files'][0]
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(directory,duplicate)
            wrong=json.loads(json.dumps(emitted));wrong['files'][0]['layout']=24
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(directory,wrong)
            target=directory/'v12.dxls';target.unlink();target.symlink_to('v13.dxls')
            with self.assertRaisesRegex(RuntimeError,'read-only bounded'):
                harness.codec_fixture_manifest(directory,emitted)

    def test_family_archives_require_the_explicit_family_fixture_set(self):
        with tempfile.TemporaryDirectory() as temporary:
            for index,inventory in enumerate((harness.CODEC_SORTS_FIXTURES,harness.CODEC_PREFERENCES_FIXTURES,harness.CODEC_OBJECTS_FIXTURES,harness.CODEC_QUERIES_FIXTURES,harness.CODEC_SOURCE_OPTIONS_FIXTURES,harness.CODEC_TABLES_FIXTURES,harness.CODEC_PIVOT_MARKS_FIXTURES,harness.CODEC_FILTERS_FIXTURES)):
                directory=Path(temporary)/str(index);directory.mkdir();emitted=self.fixtures(directory,inventory)
                manifest=harness.codec_fixture_manifest(directory,emitted,inventory)
                self.assertEqual({entry['name'] for entry in manifest['files']},set(inventory))
                with self.assertRaisesRegex(RuntimeError,'Incomplete native codec fixture set'):
                    harness.codec_fixture_manifest(directory,emitted)

    def test_reserved_routes_are_sealed_and_never_fall_back_to_static_files(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);fixtures=directory/'fixtures';fixtures.mkdir()
            manifest=harness.codec_fixture_manifest(fixtures,self.fixtures(fixtures))
            static=directory/'static';(static/'__codec-fixtures').mkdir(parents=True)
            (static/'__codec-fixtures/unlisted.dxls').write_bytes(b'should never be served')
            server=harness.LoopbackServer(('127.0.0.1',0),functools.partial(harness.Handler,directory=str(static)))
            server.codec_manifest=manifest;server.codec_requests=[]
            server.codec_files={'/__codec-fixtures/'+item['name']:(fixtures/item['name'],item) for item in manifest['files']}
            thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
            base=f'http://127.0.0.1:{server.server_port}'
            try:
                with self.assertRaises(HTTPError) as caught:urlopen(base+'/__metadata-stream-probe.html',timeout=2)
                self.assertEqual(caught.exception.code,404);caught.exception.close()
                server.metadata_stream_import_map={'imports':{'test':'./vendor/test.mjs'}}
                with urlopen(base+'/__metadata-stream-probe.html',timeout=2) as response:
                    html=response.read().decode();self.assertIn('importmap',html);self.assertNotIn('<canvas',html);self.assertNotIn('host.mjs',html)
                with urlopen(base+'/__codec-fixtures/manifest.json',timeout=2) as response:
                    self.assertEqual(json.load(response),manifest)
                with urlopen(base+'/__codec-fixtures/v12.dxls',timeout=2) as response:
                    self.assertEqual(response.read(),b'v12.dxls')
                with self.assertRaises(HTTPError) as caught:urlopen(base+'/__codec-fixtures/unlisted.dxls',timeout=2)
                self.assertEqual(caught.exception.code,404)
                caught.exception.close()
                file=fixtures/'v12.dxls';file.chmod(0o644);file.write_bytes(b'tampered');file.chmod(0o444)
                with self.assertRaises(HTTPError) as caught:urlopen(base+'/__codec-fixtures/v12.dxls',timeout=2)
                self.assertEqual(caught.exception.code,409)
                caught.exception.close()
                self.assertEqual([item['name'] for item in server.codec_requests],['v12.dxls'])
            finally:server.shutdown();server.server_close();thread.join()


if __name__=='__main__':unittest.main()
