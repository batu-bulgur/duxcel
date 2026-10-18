#!/usr/bin/env python3
"""Small parser and real-C99 object-cache contracts; no SDK, engine or windows."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import web_object_cache as cache


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class ParserTests(unittest.TestCase):
    def test_make_escapes_continuations_and_literal_backslashes(self):
        text = ('duxcel_object: src/a.c dir/with\\ space.h hash\\#cash$$.h '
                'back\\slash.h double\\\\slash.h tail\\\\ \\\n next.h\n')
        self.assertEqual(cache.parse_depfile(text), sorted([
            'src/a.c', 'dir/with space.h', 'hash#cash$.h', 'back\\slash.h',
            'double\\\\slash.h', 'tail\\', 'next.h']))
        self.assertEqual(cache.parse_depfile('duxcel_object: a.c \\\r\n a\\:b.h\r\n'), ['a.c', 'a:b.h'])

    def test_non_ascii_whitespace_is_a_literal_make_filename_character(self):
        self.assertEqual(cache.parse_depfile('duxcel_object: a.c non\u00a0breaking.h\n'), ['a.c', 'non\u00a0breaking.h'])

    def test_comments_duplicates_and_no_make_variable_evaluation(self):
        self.assertEqual(cache.parse_depfile('duxcel_object: a.c a.c escaped\\#x.h # ignored\n'),
                         ['a.c', 'escaped#x.h'])
        for text in ('other: a.c', 'duxcel_object:', 'duxcel_object: $(shell bad)',
                     'duxcel_object: single$.h', 'duxcel_object: a\0.h'):
            with self.subTest(text=text), self.assertRaises(cache.ObjectCacheError):
                cache.parse_depfile(text)


class TinyCompilerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='duxcel cache #$ ')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        for name in ('src', 'include', 'early', 'late', 'generated'):
            (self.root/name).mkdir()
        clang = shutil.which('clang')
        if not clang:
            self.skipTest('existing clang is required for the real-C99 contract')
        self.clang = clang
        (self.root/'include/shared.h').write_text('int answer(void);\n')
        (self.root/'include/transitive.h').write_text('#define VALUE 17\n')
        (self.root/'include/value.h').write_text('#include "transitive.h"\n')
        (self.root/'src/first.c').write_text('#include "shared.h"\n#include "value.h"\nint answer(void){return VALUE;}\n')
        (self.root/'src/main.c').write_text('#include "shared.h"\nint main(void){return answer()!=17;}\n')
        self.wrapper = self.root/'compiler.py'
        self.wrapper.write_text('''import json,os,pathlib,subprocess,sys
args=sys.argv[1:]
with pathlib.Path(os.environ['OBJECT_LOG']).open('a') as out:out.write(json.dumps(args)+'\\n')
phase=os.environ.get('OBJECT_MUTATE_PHASE')
if '-c' in args and phase=='before':pathlib.Path(os.environ['OBJECT_MUTATE_PATH']).write_text(os.environ['OBJECT_MUTATE_TEXT'])
result=subprocess.run([os.environ['OBJECT_CLANG'],*args])
if '-c' in args and phase=='after':pathlib.Path(os.environ['OBJECT_MUTATE_PATH']).write_text(os.environ['OBJECT_MUTATE_TEXT'])
sys.exit(result.returncode)
''')
        self.env = dict(os.environ, OBJECT_LOG=str(self.root/'commands.jsonl'), OBJECT_CLANG=clang)
        self.flags = ['-std=c99', '-Wall', '-Wextra', '-Werror', '-Iinclude']
        self.log = (self.root/'build.txt').open('w+')
        self.addCleanup(self.log.close)

    def commands(self):
        path = self.root/'commands.jsonl'
        return [json.loads(line) for line in path.read_text().splitlines()] if path.exists() else []

    def validator(self):
        before = {str(p):sha(p) for directory in ('src', 'include', 'early', 'late', 'generated')
                  for p in (self.root/directory).rglob('*') if p.is_file()}
        def validate():
            after = {str(p):sha(p) for directory in ('src', 'include', 'early', 'late', 'generated')
                     for p in (self.root/directory).rglob('*') if p.is_file()}
            if before != after:
                raise RuntimeError('source freeze changed')
        return validate

    def ensure(self, source='src/first.c', mode='app', configuration='fixture-v1', flags=None,
               env=None, validate=None):
        return cache.ensure_object(compiler=[sys.executable, str(self.wrapper)],
            flags=self.flags if flags is None else flags, source=source,
            target=self.root/'objects'/mode/Path(source).with_suffix('.o'),
            cwd=self.root, env=self.env if env is None else env, log=self.log,
            configuration=configuration, validate_source=validate or self.validator())

    def triplet(self, source='src/first.c', mode='app'):
        obj = self.root/'objects'/mode/Path(source).with_suffix('.o')
        return [obj, obj.with_suffix('.d'), obj.with_suffix('.json')]

    def published(self):
        return {str(p.relative_to(self.root)):p.read_bytes() for p in self.triplet()}

    def assert_no_temporaries(self):
        self.assertFalse(list((self.root/'objects').rglob('.object-*')))

    def test_cold_warm_and_real_link(self):
        self.assertTrue(self.ensure())
        self.assertTrue(self.ensure('src/main.c'))
        before = self.published()
        self.assertFalse(self.ensure())
        self.assertEqual(self.published(), before)
        calls = self.commands()
        self.assertEqual(sum('-MM' in c for c in calls), 3)
        self.assertEqual(sum('-c' in c for c in calls), 2)
        executable = self.root/'tiny'
        subprocess.run([self.clang, str(self.triplet()[0]), str(self.triplet('src/main.c')[0]), '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True)
        self.assert_no_temporaries()

    def test_unused_transitive_shared_and_same_timestamp_content(self):
        self.ensure(); self.ensure('src/main.c')
        (self.root/'include/unused.h').write_text('/* no consumer */\n')
        self.assertFalse(self.ensure()); self.assertFalse(self.ensure('src/main.c'))
        p = self.root/'include/transitive.h'; stamp=p.stat().st_mtime_ns
        p.write_text('#define VALUE 18\n'); os.utime(p, ns=(stamp,stamp))
        self.assertTrue(self.ensure()); self.assertFalse(self.ensure('src/main.c'))
        p=self.root/'include/shared.h'; p.write_text('int answer(void); /* changed */\n')
        self.assertTrue(self.ensure()); self.assertTrue(self.ensure('src/main.c'))

    def test_new_earlier_include_shadow_and_symlink_target(self):
        (self.root/'late/choice.h').write_text('#define CHOICE 1\n')
        (self.root/'src/first.c').write_text('#include <choice.h>\nint answer(void){return CHOICE;}\n')
        flags=['-std=c99', '-Iearly', '-Ilate']
        self.assertTrue(self.ensure(flags=flags))
        (self.root/'early/choice.h').write_text('#define CHOICE 2\n')
        self.assertTrue(self.ensure(flags=flags))
        record=json.loads(self.triplet()[2].read_text())
        self.assertIn('early/choice.h',record['dependencies']); self.assertNotIn('late/choice.h',record['dependencies'])
        (self.root/'early/choice.h').unlink()
        one=self.root/'late/one.h'; two=self.root/'late/two.h'
        one.write_text('#define CHOICE 2\n'); two.write_text(one.read_text())
        (self.root/'early/choice.h').symlink_to(one)
        self.assertTrue(self.ensure(flags=flags)); (self.root/'early/choice.h').unlink(); (self.root/'early/choice.h').symlink_to(two)
        self.assertTrue(self.ensure(flags=flags))
        self.assertEqual(json.loads(self.triplet()[2].read_text())['dependencies']['early/choice.h']['resolved'],str(two))

    def test_generated_force_include_only_its_actual_consumer(self):
        header=self.root/'generated/review.h'; header.write_text('#define REVIEW 1\n')
        flags=[*self.flags,'-include','generated/review.h']
        self.assertTrue(self.ensure(flags=flags,mode='review'))
        self.assertFalse(self.ensure(flags=flags,mode='review'))
        self.assertTrue(self.ensure('src/main.c'))
        header.write_text('#define REVIEW 2\n')
        self.assertTrue(self.ensure(flags=flags,mode='review')); self.assertFalse(self.ensure('src/main.c'))

    def test_actual_compiler_depfile_special_paths(self):
        names=['with space.h','hash#cash$.h']
        for name in names:(self.root/'include'/name).write_text('/* exact filename */\n')
        (self.root/'src/first.c').write_text(''.join('#include "'+name+'"\n' for name in names)+'int answer(void){return 17;}\n')
        self.assertTrue(self.ensure()); self.assertFalse(self.ensure())
        record=json.loads(self.triplet()[2].read_text())
        for name in names:self.assertIn('include/'+name,record['dependencies'])

    def test_clang_ambiguous_literal_backslash_dependency_fails_closed(self):
        self.ensure(); before=self.published()
        name='back\\slash.h';(self.root/'include'/name).write_text('/* literal POSIX backslash */\n')
        (self.root/'src/first.c').write_text('#include "'+name+'"\nint answer(void){return 17;}\n')
        # This Clang emits include/back/slash.h, not the existing literal name.
        # Do not invent a filesystem mapping for a lossy compiler spelling.
        with self.assertRaisesRegex(cache.ObjectCacheError,'dependency disappeared'):
            self.ensure()
        self.assertEqual(self.published(),before);self.assert_no_temporaries()

    def test_missing_corrupt_sidecars_and_corrupt_object_rebuild(self):
        self.ensure()
        for index in (1,2,0):
            with self.subTest(index=index):
                self.triplet()[index].unlink(); self.assertTrue(self.ensure()); self.assertFalse(self.ensure())
        for index,content in ((1,b'not a Make depfile'),(2,b'[]'),(2,b'{broken'),(0,b'broken object')):
            with self.subTest(index=index,content=content):
                self.triplet()[index].write_bytes(content); self.assertTrue(self.ensure()); self.assertFalse(self.ensure())
        p=self.triplet()[2]
        for invalid_version in (999,True):
            record=json.loads(p.read_text());record['version']=invalid_version;p.write_text(json.dumps(record))
            self.assertTrue(self.ensure())
        self.assert_no_temporaries()

    def test_deleted_dependency_fails_without_reusing_or_replacing_old_object(self):
        self.ensure(); before=self.published(); p=self.root/'include/transitive.h';p.unlink()
        with self.assertRaises(cache.ObjectCacheError):self.ensure()
        self.assertEqual(self.published(),before)
        p.write_text('#define VALUE 18\n'); self.assertTrue(self.ensure());self.assert_no_temporaries()

    def test_configuration_and_mode_are_independent(self):
        self.assertTrue(self.ensure()); self.assertTrue(self.ensure(mode='review',configuration='review-v1'))
        before=self.published();self.assertFalse(self.ensure());self.assertEqual(self.published(),before)
        self.assertTrue(self.ensure(configuration='new-toolchain'))
        self.assertTrue(self.ensure(flags=[*self.flags,'-DNEW_CONFIGURATION=1'],configuration='new-toolchain'))

    def test_real_compile_failure_preserves_triplet_and_recovers(self):
        self.ensure();before=self.published();p=self.root/'src/first.c';old=p.read_text();p.write_text('int broken = ;\n')
        with self.assertRaises(cache.ObjectCacheError):self.ensure()
        self.assertEqual(self.published(),before);self.assert_no_temporaries()
        p.write_text(old+'/* recovery */\n');self.assertTrue(self.ensure())

    def test_interrupt_preserves_triplet_and_cleans_unique_temporaries(self):
        self.ensure();before=self.published();run=subprocess.run
        def interrupted(args,**kwargs):
            if '-c' in args:raise KeyboardInterrupt()
            return run(args,**kwargs)
        with mock.patch.object(cache.subprocess,'run',side_effect=interrupted),self.assertRaises(KeyboardInterrupt):
            self.ensure(configuration='force-compile')
        self.assertEqual(self.published(),before);self.assert_no_temporaries()

    def test_dependency_mutation_after_compile_rejects_and_recovers(self):
        for rel,changed in [('include/transitive.h','#define VALUE 19\n'),('src/first.c','int answer(void){return 20;}\n')]:
            self.ensure();before=self.published();p=self.root/rel
            env=dict(self.env,OBJECT_MUTATE_PHASE='after',OBJECT_MUTATE_PATH=str(p),OBJECT_MUTATE_TEXT=changed)
            with self.assertRaisesRegex(cache.ObjectCacheError,'changed during compilation'):
                self.ensure(configuration='force-compile',env=env,validate=lambda:None)
            self.assertEqual(self.published(),before);self.assert_no_temporaries();self.assertTrue(self.ensure(configuration='force-compile'))

    def test_new_dependency_during_compile_rejects_closure(self):
        (self.root/'late/choice.h').write_text('#define CHOICE 1\n');(self.root/'src/first.c').write_text('#include <choice.h>\nint answer(void){return CHOICE;}\n')
        flags=['-std=c99','-Iearly','-Ilate'];self.ensure(flags=flags);before=self.published()
        env=dict(self.env,OBJECT_MUTATE_PHASE='before',OBJECT_MUTATE_PATH=str(self.root/'early/choice.h'),OBJECT_MUTATE_TEXT='#define CHOICE 2\n')
        with self.assertRaisesRegex(cache.ObjectCacheError,'closure or bytes changed'):
            self.ensure(flags=flags,configuration='force-compile',env=env,validate=lambda:None)
        self.assertEqual(self.published(),before);self.assertTrue(self.ensure(flags=flags,configuration='force-compile'))

    def test_source_freeze_callback_runs_on_reuse_and_before_publication(self):
        self.ensure();before=self.published();calls=[]
        def refuse_reuse():
            calls.append(1)
            if len(calls)==3:raise RuntimeError('frozen reuse')
        with self.assertRaisesRegex(RuntimeError,'frozen reuse'):self.ensure(validate=refuse_reuse)
        self.assertEqual(self.published(),before);self.assertEqual(len(calls),3)
        calls=[]
        def refuse_publication():
            calls.append(1)
            if len(calls)==3:raise RuntimeError('frozen publish')
        with self.assertRaisesRegex(RuntimeError,'frozen publish'):self.ensure(configuration='new-config',validate=refuse_publication)
        self.assertEqual(self.published(),before);self.assert_no_temporaries()

    def test_publication_interruption_metadata_last_prevents_false_reuse(self):
        self.ensure();old=self.published();p=self.root/'src/first.c';p.write_text(p.read_text()+'/* edit */\n');replace=Path.replace
        def fail_metadata(path,target):
            if path.name=='metadata.json':raise OSError('publication interrupted')
            return replace(path,target)
        with mock.patch.object(Path,'replace',fail_metadata),self.assertRaisesRegex(OSError,'interrupted'):
            self.ensure()
        self.assertEqual(self.triplet()[2].read_bytes(),old[str(self.triplet()[2].relative_to(self.root))])
        self.assertTrue(self.ensure());self.assertFalse(self.ensure());self.assert_no_temporaries()


if __name__ == '__main__':
    unittest.main()
