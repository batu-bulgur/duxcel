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
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text()
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

    def test_counters_mode_requires_independent_driver_and_exact_four_fixture_inventory(self):
        options=harness.parse_options(['--codec-read-driver','--codec-counters','--no-build'])
        self.assertTrue(options.codec_counters and options.no_build);self.assertEqual(options.dpr,1)
        invalid=[['--codec-counters'],*(['--codec-read-driver','--codec-counters',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--codec-grid','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        self.assertEqual(harness.CODEC_COUNTERS_FIXTURES,{'counters-v12.dxls':(12,25),'counters-v13.dxls':(13,25),'counters-v14.dxls':(14,25),'counters-v14-reordered.dxls':(14,25)})
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_COUNTERS_FIXTURES)
            manifest=harness.codec_fixture_manifest(path,emitted,harness.CODEC_COUNTERS_FIXTURES)
            self.assertEqual([row['name'] for row in manifest['files']],list(harness.CODEC_COUNTERS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):harness.codec_fixture_manifest(path,emitted,harness.CODEC_GRID_FIXTURES)

    def test_full_tier_counters_command_is_serial_after_grid_and_requires_its_actual_pass_marker(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;counters=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--counters-only']
            self.assertEqual(len(counters),2)
            for index,call in counters:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--grid-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native object counters 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native workbook header 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_node_counters_report_guards_reject_missing_or_changed_runtime_proofs(self):
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text()
        start=source.index('function validateCodecCountersReport(report,check) {');function=source[start:source.index('\nfunction validateCodecGridReport',start)]
        report=dict(mode='counters',fixtures=4,headers=5,queries=54,rejections=4,stale=1,normalFixtureQueries=[6,8,8,8],
            familyExecutions=dict(meta=10,sheets=10,objects=10,objectCounters=9,describe=15),
            acceptedFeeds=dict(objects56=10,counters32=11),validCounterGroups=4,v12Defaults=1,poisonedCounterResults=4,rejectedFeeds=4,staleCallbacks=1,
            rejectedFirstRows=[32,32,32,0],maximumCompletedHeaderOwned=100000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2097300,
            staleWireReservationBytes=10000,staleTotalReservationBytes=21)
        for name in ('lowHighWaterRejected','requiredNullRejected','identityRejected','budgetRejected','stalePreserved','freshRecovery','v12NoCounterQuery','exactRetiredCounter','exactMaximumCounter'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.normalFixtureQueries[0]=8,r=>r.familyExecutions.objectCounters--,r=>r.acceptedFeeds.counters32--,
 r=>r.validCounterGroups--,r=>r.v12Defaults=0,r=>r.poisonedCounterResults--,r=>r.rejectedFeeds--,r=>r.staleCallbacks=0,
 r=>r.rejectedFirstRows[2]=0,r=>r.v12NoCounterQuery=false,r=>r.exactMaximumCounter=false,r=>r.maximumCompletedHeaderOwned=2097153,
 r=>r.peakOwnedInput=4194305,r=>r.staleWireReservationBytes=0,r=>r.staleWireReservationBytes=6291457,r=>r.staleTotalReservationBytes=20];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: actual counter report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 17 corrupted receipts',result.stdout)

    def test_source_counters_mode_requires_independent_driver_and_exact_four_fixture_inventory(self):
        options=harness.parse_options(['--codec-read-driver','--codec-source-counters','--no-build'])
        self.assertTrue(options.codec_source_counters and options.no_build);self.assertEqual(options.dpr,1)
        invalid=[['--codec-source-counters'],*(['--codec-read-driver','--codec-source-counters',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--codec-grid','--codec-counters','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        self.assertEqual(harness.CODEC_SOURCE_COUNTERS_FIXTURES,{'source-counters-v12.dxls':(12,25),'source-counters-v13.dxls':(13,25),'source-counters-v14.dxls':(14,25),'source-counters-v14-reordered.dxls':(14,25)})
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_SOURCE_COUNTERS_FIXTURES)
            manifest=harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_COUNTERS_FIXTURES)
            self.assertEqual([row['name'] for row in manifest['files']],list(harness.CODEC_SOURCE_COUNTERS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):harness.codec_fixture_manifest(path,emitted,harness.CODEC_COUNTERS_FIXTURES)

    def test_full_tier_source_counters_command_is_serial_after_counters_and_requires_its_actual_pass_marker(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;counters=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--source-counters-only']
            self.assertEqual(len(counters),2)
            for index,call in counters:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--counters-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native source version counters 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native object counters 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_node_source_counters_report_guards_reject_missing_or_changed_runtime_proofs(self):
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text()
        start=source.index('function validateCodecSourceCountersReport(report,check) {');function=source[start:source.index('\nfunction validateCodecCountersReport',start)]
        report=dict(mode='source-counters',fixtures=4,headers=5,queries=64,rejections=4,stale=1,callbacks=63,normalFixtureQueries=[6,8,10,10],
            familyExecutions=dict(meta=10,sheets=10,objects=10,objectCounters=9,sourceCounters=8,describe=17),
            acceptedFeeds=dict(objects56=10,objectCounters32=18,sourceCounters32=9),validObjectCounterGroups=9,validSourceCounterGroups=3,
            objectCounterDefaults=1,sourceCounterDefaults=2,poisonedObjectCounterResults=9,poisonedSourceCounterResults=3,rejectedFeeds=4,staleCallbacks=1,
            rejectedFirstRows=[32,32,32,0],maximumCompletedHeaderOwned=100000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2097300,
            staleWireReservationBytes=10000,staleTotalReservationBytes=21)
        for name in ('requiredNullRejected','identityRejected','missingRowRejected','budgetRejected','stalePreserved','freshRecovery','olderNoSourceCounterQuery','exactSourceMaximum','independentDomains'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.callbacks--,r=>r.normalFixtureQueries[1]=10,r=>r.familyExecutions.sourceCounters--,
 r=>r.acceptedFeeds.objectCounters32--,r=>r.acceptedFeeds.sourceCounters32--,r=>r.validObjectCounterGroups--,r=>r.validSourceCounterGroups--,
 r=>r.objectCounterDefaults=0,r=>r.sourceCounterDefaults--,r=>r.poisonedObjectCounterResults--,r=>r.poisonedSourceCounterResults--,
 r=>r.rejectedFeeds--,r=>r.staleCallbacks=0,r=>r.rejectedFirstRows[2]=0,r=>r.olderNoSourceCounterQuery=false,r=>r.exactSourceMaximum=false,
 r=>r.independentDomains=false,r=>r.missingRowRejected=false,r=>r.maximumCompletedHeaderOwned=2097153,r=>r.peakOwnedInput=4194305,
 r=>r.staleWireReservationBytes=0,r=>r.staleWireReservationBytes=6291457,r=>r.staleTotalReservationBytes=20];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: actual source-counter report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 24 corrupted receipts',result.stdout)

    def test_source_versions_mode_owns_exact_four_fixtures_and_rejects_mixed_profiles(self):
        options=harness.parse_options(['--codec-read-driver','--codec-source-versions','--no-build'])
        self.assertTrue(options.codec_source_versions and options.no_build)
        self.assertEqual(harness.CODEC_SOURCE_VERSIONS_FIXTURES,{'source-versions-v12.dxls':(12,25),'source-versions-v13.dxls':(13,25),'source-versions-v14.dxls':(14,25),'source-versions-v14-reordered.dxls':(14,25)})
        invalid=[['--codec-source-versions'],*(['--codec-read-driver','--codec-source-versions',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--codec-grid','--codec-counters','--codec-source-counters','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_SOURCE_VERSIONS_FIXTURES)
            manifest=harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_VERSIONS_FIXTURES)
            self.assertEqual([row['name'] for row in manifest['files']],list(harness.CODEC_SOURCE_VERSIONS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_COUNTERS_FIXTURES)

    def test_full_tier_ledger_runs_after_source_counters_and_requires_ledger_completion(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;ledgers=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--source-versions-only']
            self.assertEqual(len(ledgers),2)
            for index,call in ledgers:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--source-counters-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native source versions 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native source version counters 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_node_source_versions_guards_refuse_missing_or_changed_runtime_evidence(self):
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text()
        start=source.index('function validateCodecSourceVersionsReport(report,check) {')
        function=source[start:source.index('\nfunction validateCodecSourceCountersReport',start)]
        report=dict(mode='source-versions',fixtures=4,headers=5,queries=98,rejections=8,stale=2,callbacks=86,normalFixtureQueries=[6,8,12,12],
            familyExecutions=dict(meta=14,sheets=14,objects=14,objectCounters=13,sourceCounters=12,sourceVersions=12,describe=19),
            acceptedFeeds=dict(objects56=14,objectCounters32=26,sourceCounters32=24,sourceVersions256=10,sourceVersions29=2,sourceVersions5=1,sourceVersionsEof=3),
            objectDefaults=1,sourceCounterDefaults=2,ledgerDefaults=2,sharedDecoderRejections=6,hexAdaptationRejections=2,matchedDataCallbacks=21,matchedEofCallbacks=3,
            acceptedAcks=16,chunkCallbacks=25,doneCallbacks=12,completedStreams=3,errorStreams=8,poisonedDataChunks=19,staleChunks=1,staleDoneCallbacks=1,heldCallbacks=1,cancelledCallbacks=1,
            rejectedFirstRows=[256,256,256,256,256,256,256,0],maximumCompletedHeaderOwned=200000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2200000,
            staleWireReservationBytes=10000,staleScalarReservationBytes=42,staleTerminalReservationBytes=2070,
            transport=dict(sourceVersionPhysicalChunks=25,stream=dict(submissions=12,chunkCallbacks=25,acceptedAcks=16,doneCallbacks=12,held=1,released=1,cancelled=1)))
        for name in ('requiredNullRejected','pendingEncodingRejected','malformedRawRejected','resultTagRejected','oddHexRejected','nonHexRejected','emptyBlobRejected','budgetRejected','stalePreserved','freshRecovery','olderNoLedgerQuery','exactRawBytes','independentLocators','archivedOwnersRetained','exactMaximumVersion'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.callbacks--,r=>r.normalFixtureQueries[1]=12,r=>r.familyExecutions.sourceVersions--,
 r=>r.acceptedFeeds.sourceVersions256--,r=>r.acceptedFeeds.sourceVersionsEof--,r=>r.sharedDecoderRejections--,r=>r.hexAdaptationRejections--,
 r=>r.ledgerDefaults=0,r=>r.objectDefaults=0,r=>r.poisonedDataChunks--,r=>r.staleChunks=0,r=>r.staleDoneCallbacks=0,r=>r.cancelledCallbacks=0,
 r=>r.acceptedAcks++,r=>r.rejectedFirstRows[4]=0,r=>r.exactRawBytes=false,r=>r.independentLocators=false,r=>r.archivedOwnersRetained=false,
 r=>r.exactMaximumVersion=false,r=>r.olderNoLedgerQuery=false,r=>r.emptyBlobRejected=false,r=>r.budgetRejected=false,
 r=>r.maximumCompletedHeaderOwned=2097153,r=>r.peakOwnedInput=4194305,r=>r.staleWireReservationBytes=0,
 r=>r.staleWireReservationBytes=6291457,r=>r.staleScalarReservationBytes=41,r=>r.transport.stream.acceptedAcks++,r=>r.transport.stream.doneCallbacks--,
 r=>r.transport.sourceVersionPhysicalChunks--,r=>r.heldCallbacks=0,r=>r.staleTerminalReservationBytes=2069];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: actual ledger report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 33 corrupted receipts',result.stdout)

    def test_source_bindings_mode_refuses_mixed_profiles_and_seals_exact_inventory(self):
        options=harness.parse_options(['--codec-read-driver','--codec-source-bindings','--no-build'])
        self.assertTrue(options.codec_source_bindings and options.no_build)
        self.assertEqual(harness.CODEC_SOURCE_BINDINGS_FIXTURES,{'source-bindings-v12.dxls':(12,25),'source-bindings-v13.dxls':(13,25),'source-bindings-v14.dxls':(14,25),'source-bindings-v14-reordered.dxls':(14,25)})
        invalid=[['--codec-source-bindings'],*(['--codec-read-driver','--codec-source-bindings',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--codec-grid','--codec-counters','--codec-source-counters','--codec-source-versions','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_SOURCE_BINDINGS_FIXTURES)
            self.assertEqual([row['name'] for row in harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_BINDINGS_FIXTURES)['files']],list(harness.CODEC_SOURCE_BINDINGS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_VERSIONS_FIXTURES)

    def test_full_tier_bindings_follow_ledger_and_require_their_own_completion(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;bindings=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--source-bindings-only']
            self.assertEqual(len(bindings),2)
            for index,call in bindings:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--source-versions-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native source bindings 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native source versions 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_binding_report_guards_refuse_incomplete_or_changed_evidence(self):
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text();start=source.index('function validateCodecSourceBindingsReport(report,check) {')
        function=source[start:source.index('\nfunction validateCodecSourceVersionsReport',start)]
        report=dict(mode='source-bindings',fixtures=4,headers=5,queries=166,rejections=5,stale=1,callbacks=156,normalFixtureQueries=[16,18,24,24],
            familyExecutions=dict(meta=11,sheets=11,objects=11,queries=11,sourceOptions=11,tableSources=11,tableFilters=11,tableSorts=11,objectCounters=10,sourceCounters=9,sourceVersions=9,sourceBindings=9,describe=41),
            acceptedFeeds=dict(objects56=11,queries21=11,sourceOptionsEmpty=11,tableSources18=11,tableFiltersEmpty=11,tableSortsEmpty=11,objectCounters32=20,sourceCounters32=18,sourceVersions256=8,sourceVersions32=8,sourceVersions8=1,sourceVersionsEof=9,sourceBindings32=14,sourceBindings18=3),
            objectDefaults=1,sourceCounterDefaults=2,ledgerDefaults=2,bindingDefaults=2,ledgerStreams=9,matchedLedgerDataCallbacks=17,matchedLedgerEofCallbacks=9,ledgerChunkCallbacks=26,ledgerAcceptedAcks=26,ledgerDoneCallbacks=9,ledgerPoisonedDataChunks=17,
            completedBindingResults=3,poisonedBindingResults=8,rejectedFeeds=5,staleCallbacks=1,rejectedFirstRows=[64,64,64,64,0],staleWireReservationBytes=10000,staleTotalReservationBytes=21,
            maximumCompletedHeaderOwned=200000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2200000,
            transport=dict(sourceVersionPhysicalChunks=26,stream=dict(submissions=9,chunkCallbacks=26,acceptedAcks=26,doneCallbacks=9,held=0,released=0,cancelled=0)))
        for name in ('requiredNullRejected','foreignOwnerRejected','missingOwnerRejected','danglingVersionRejected','budgetRejected','stalePreserved','freshRecovery','olderNoBindingQuery','exactRawBytes','independentLocators','deferredBindingsRetained','analysisVersionsRemainZero'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.callbacks--,r=>r.normalFixtureQueries[1]=24,r=>r.familyExecutions.sourceBindings--,
 r=>r.acceptedFeeds.sourceBindings32--,r=>r.acceptedFeeds.sourceVersionsEof--,r=>r.bindingDefaults=0,r=>r.objectDefaults=0,
 r=>r.poisonedBindingResults--,r=>r.rejectedFeeds--,r=>r.staleCallbacks=0,r=>r.ledgerAcceptedAcks++,r=>r.ledgerDoneCallbacks--,
 r=>r.rejectedFirstRows[2]=0,r=>r.exactRawBytes=false,r=>r.independentLocators=false,r=>r.deferredBindingsRetained=false,
 r=>r.analysisVersionsRemainZero=false,r=>r.olderNoBindingQuery=false,r=>r.danglingVersionRejected=false,r=>r.budgetRejected=false,
 r=>r.maximumCompletedHeaderOwned=2097153,r=>r.peakOwnedInput=4194305,r=>r.staleWireReservationBytes=0,
 r=>r.staleWireReservationBytes=6291457,r=>r.staleTotalReservationBytes=20,r=>r.transport.stream.acceptedAcks++,
 r=>r.transport.stream.doneCallbacks--,r=>r.transport.sourceVersionPhysicalChunks--];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: binding report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 29 corrupted receipts',result.stdout)

    def test_format_rows_require_explicit_mode_and_exact_fixture_inventory(self):
        options=harness.parse_options(['--codec-read-driver','--codec-format-rows'])
        self.assertTrue(options.codec_format_rows)
        self.assertEqual(harness.CODEC_FORMAT_ROWS_FIXTURES,{'format-rows-v12.dxls':(12,25),'format-rows-v13.dxls':(13,25),'format-rows-v14.dxls':(14,25),'format-rows-v14-reordered.dxls':(14,25)})
        invalid=[['--codec-format-rows'],*(['--codec-read-driver','--codec-format-rows',family] for family in
            ('--codec-sorts','--codec-preferences','--codec-objects','--codec-queries','--codec-source-options','--codec-tables','--codec-pivot-marks','--codec-filters','--codec-grid','--codec-counters','--codec-source-counters','--codec-source-versions','--codec-source-bindings','--review','--worker-driver','--metadata-stream-probe','--dpr=2'))]
        for arguments in invalid:
            with self.subTest(arguments=arguments),contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):harness.parse_options(arguments)
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary);emitted=self.fixtures(path,harness.CODEC_FORMAT_ROWS_FIXTURES)
            self.assertEqual([row['name'] for row in harness.codec_fixture_manifest(path,emitted,harness.CODEC_FORMAT_ROWS_FIXTURES)['files']],list(harness.CODEC_FORMAT_ROWS_FIXTURES))
            with self.assertRaisesRegex(RuntimeError,'Unexpected native codec fixture'):
                harness.codec_fixture_manifest(path,emitted,harness.CODEC_SOURCE_BINDINGS_FIXTURES)

    def test_full_tier_format_rows_follow_bindings_with_separate_completion(self):
        args=runner.parser().parse_args(['full','--config','both','--no-build'])
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'Makefile').write_text('');instance=runner.Runner(args,root)
            with mock.patch.object(instance,'command',side_effect=lambda name,command,*a,**k:{'name':name,'command':command,'status':'passed'}) as command, \
                 mock.patch.object(instance,'headless'),mock.patch.object(instance,'prepare_native'),mock.patch.object(instance,'retain_binaries'), \
                 mock.patch.object(instance,'background_check'),mock.patch.object(instance,'native_scenarios'), \
                 mock.patch.object(runner,'FileLock',side_effect=lambda *a,**k:contextlib.nullcontext()):
                instance.full()
            calls=command.call_args_list;formats=[(n,call) for n,call in enumerate(calls) if call.args[1][-1]=='--format-rows-only']
            self.assertEqual(len(formats),2)
            for index,call in formats:
                binary=call.args[1][0];self.assertEqual(calls[index-1].args[1],[binary,'--source-bindings-only'])
                pattern=call.kwargs['completion_pattern'];runner.validate_output('PASS: native format rows 123 checks;\n',0,completion_pattern=pattern)
                with self.assertRaises(RuntimeError):runner.validate_output('PASS: native source bindings 123 checks;\n',0,completion_pattern=pattern)

    def test_actual_format_report_guards_reject_incomplete_or_changed_evidence(self):
        source=Path(__file__).resolve().parents[2].joinpath('web/tests/driver.mjs').read_text();start=source.index('function validateCodecFormatRowsReport(report,check) {')
        function=source[start:source.index('\nfunction validateCodecSourceBindingsReport',start)]
        report=dict(mode='format-rows',fixtures=4,headers=5,queries=207,rejections=7,stale=2,callbacks=183,normalFixtureQueries=[18,20,26,26],
            familyExecutions=dict(meta=13,sheets=13,objects=13,queries=13,sourceOptions=13,tableSources=13,tableFilters=13,tableSorts=13,objectCounters=12,sourceCounters=11,sourceVersions=11,sourceBindings=11,formatRows=13,describe=45),
            acceptedFeeds=dict(objects56=13,queries21=13,sourceOptionsEmpty=13,tableSources18=13,tableFiltersEmpty=13,tableSortsEmpty=13,objectCounters32=24,sourceCounters32=22,sourceVersions256=10,sourceVersions32=10,sourceVersions8=1,sourceVersionsEof=11,sourceBindings32=22,sourceBindings18=11,formatRows256=10,formatRows29=2,formatRows7=3,formatRowsEof=5),
            objectDefaults=1,sourceCounterDefaults=2,ledgerDefaults=2,bindingDefaults=2,ledgerStreams=11,matchedLedgerDataCallbacks=21,matchedLedgerEofCallbacks=11,ledgerChunkCallbacks=32,ledgerAcceptedAcks=32,ledgerDoneCallbacks=11,ledgerPoisonedDataChunks=21,
            completedBindingResults=11,poisonedBindingResults=11,formatStreams=13,matchedFormatDataCallbacks=22,matchedFormatEofCallbacks=5,formatChunkCallbacks=28,formatAcceptedAcks=20,formatDoneCallbacks=13,completedFormatStreams=5,errorFormatStreams=7,poisonedFormatDataChunks=22,
            staleChunkCallbacks=1,staleDoneCallbacks=1,heldCallbacks=1,cancelledCallbacks=1,rejectedFeeds=7,rejectedFirstRows=[256,256,256,256,256,512,0],staleWireReservationBytes=10000,staleScalarReservationBytes=42,staleTerminalReservationBytes=2070,
            maximumCompletedHeaderOwned=200000,headerOwnedLimit=2097152,inputOwnedLimit=4194304,peakOwnedInput=2200000,
            transport=dict(held=0,released=0,sourceVersionPhysicalChunks=32,formatRowPhysicalChunks=28,stream=dict(submissions=24,chunkCallbacks=60,acceptedAcks=52,doneCallbacks=24,held=1,released=1,cancelled=1)))
        for name in ('requiredNullRejected','invalidMaskRejected','losslessFlagsRejected','rangeRejected','positionRejected','sheetLimitRejected','budgetRejected','heldSecondChunkCancelled','wholeCandidateRetry','freshRecovery','exactRawBytes','independentLocators','deferredBindingsRetained','analysisVersionsRemainZero','exactProperties','maximumCoordinateExact','unmaskedPropertiesRetained'):report[name]=True
        script="""import assert from 'node:assert/strict';import vm from 'node:vm';
const validate=vm.runInNewContext('('+FUNCTION+')'),good=REPORT;
const check=(condition,message)=>{if(!condition)throw new Error(message);};validate(good,check);
const mutations=[r=>r.queries--,r=>r.callbacks--,r=>r.normalFixtureQueries[1]=26,r=>r.familyExecutions.formatRows--,
 r=>r.acceptedFeeds.formatRows256--,r=>r.acceptedFeeds.formatRowsEof--,r=>r.bindingDefaults=0,r=>r.objectDefaults=0,
 r=>r.poisonedFormatDataChunks--,r=>r.rejectedFeeds--,r=>r.staleChunkCallbacks=0,r=>r.staleDoneCallbacks=0,
 r=>r.formatAcceptedAcks++,r=>r.ledgerDoneCallbacks--,r=>r.rejectedFirstRows[5]=256,r=>r.losslessFlagsRejected=false,
 r=>r.exactProperties=false,r=>r.maximumCoordinateExact=false,r=>r.unmaskedPropertiesRetained=false,r=>r.heldSecondChunkCancelled=false,
 r=>r.independentLocators=false,r=>r.deferredBindingsRetained=false,r=>r.analysisVersionsRemainZero=false,r=>r.budgetRejected=false,
 r=>r.maximumCompletedHeaderOwned=2097153,r=>r.peakOwnedInput=4194305,r=>r.staleWireReservationBytes=0,
 r=>r.staleWireReservationBytes=6291457,r=>r.staleScalarReservationBytes=41,r=>r.staleTerminalReservationBytes=2069,
 r=>r.transport.stream.acceptedAcks++,r=>r.transport.stream.doneCallbacks--,r=>r.transport.formatRowPhysicalChunks--,
 r=>r.transport.sourceVersionPhysicalChunks--,r=>r.transport.held=1,r=>r.transport.released=1];
for(const mutate of mutations){const bad=structuredClone(good);mutate(bad);assert.throws(()=>validate(bad,check));}
console.log('PASS: format-row report guards rejected '+mutations.length+' corrupted receipts');
""".replace('FUNCTION',json.dumps(function)).replace('REPORT',json.dumps(report))
        result=subprocess.run(['node','--input-type=module','-e',script],capture_output=True,text=True,timeout=10)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr);self.assertIn('rejected 36 corrupted receipts',result.stdout)

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
