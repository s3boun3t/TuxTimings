#!/usr/bin/env python3
"""Delivery fixtures only: temporary Git trees, staging, fake pkexec, no backend."""
import os
import json
from pathlib import Path
import shutil
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

def run(*args, cwd=None, env=None, check=True):
    return subprocess.run(args, cwd=cwd, env=env, text=True, capture_output=True, check=check)


def git(path, *args, check=True):
    return run('git', '-C', str(path), *args, check=check)


def init_repo(path):
    path.mkdir()
    git(path, 'init', '-q', '-b', 'main')
    git(path, 'config', 'user.email', 'fixture@example.invalid')
    git(path, 'config', 'user.name', 'Fixture')


class StagingTests(unittest.TestCase):
    def test_arch_staging_contains_included_aod_header(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings stage ') as tmp:
            srcdir = Path(tmp) / 'source with spaces'
            pkgdir = Path(tmp) / 'package with spaces'
            (srcdir / 'TuxTimings-main').mkdir(parents=True)
            shutil.copytree(ROOT / 'Linux', srcdir / 'TuxTimings-main/Linux',
                            ignore=shutil.ignore_patterns('*.o', 'tuxtimings', 'build'))
            run('bash', '-c', 'source "$1"; srcdir=$2; pkgdir=$3; package_tuxtimings-dkms',
                'fixture', str(ROOT / 'PKGBUILD'), str(srcdir), str(pkgdir))
            aod = next((pkgdir / 'usr/src').glob('aod-voltages-*'))
            self.assertTrue((aod / 'aod_aml.h').is_file(), 'AOD includes aod_aml.h: DKMS source must contain it')
            self.assertEqual((aod / 'aod_aml.h').read_bytes(),
                             (ROOT / 'Linux/src/aod-voltages/aod_aml.h').read_bytes())

    def test_direct_staging_matches_arch_recipe(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings direct stage ') as tmp:
            destination = Path(tmp) / 'stage with spaces'
            run('bash', str(ROOT / 'install.sh'), '--stage-dkms', str(destination))
            for module in ['aod-voltages', 'tuxbench']:
                staged = next(destination.glob(module + '-*'))
                source = ROOT / 'Linux/src' / module
                for item in ['Makefile', 'dkms.conf', *[p.name for p in source.glob('*.c')],
                             *[p.name for p in source.glob('*.h')]]:
                    self.assertEqual((staged / item).read_bytes(), (source / item).read_bytes())


class UpdateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tuxtimings update ')
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.remote = self.base / 'remote'
        init_repo(self.remote)
        shutil.copy2(ROOT / 'update.sh', self.remote / 'update.sh')
        (self.remote / 'value').write_text('old\n')
        git(self.remote, 'add', '.')
        git(self.remote, 'commit', '-qm', 'initial')
        self.local = self.base / 'work tree'
        run('git', 'clone', '-q', str(self.remote), str(self.local))
        git(self.local, 'config', 'user.email', 'fixture@example.invalid')
        git(self.local, 'config', 'user.name', 'Fixture')
        (self.remote / 'value').write_text('new\n')
        git(self.remote, 'commit', '-qam', 'remote update')

    def update(self):
        return run('bash', str(self.local / 'update.sh'), cwd=self.local, check=False)

    def test_dirty_work_branch_and_untracked_are_preserved(self):
        git(self.local, 'switch', '-qc', 'topic')
        (self.local / 'value').write_text('local edit\n')
        (self.local / 'precious untracked').write_text('keep\n')
        head = git(self.local, 'rev-parse', 'HEAD').stdout
        result = self.update()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(git(self.local, 'branch', '--show-current').stdout, 'topic\n')
        self.assertEqual(git(self.local, 'rev-parse', 'HEAD').stdout, head)
        self.assertEqual((self.local / 'value').read_text(), 'local edit\n')
        self.assertEqual((self.local / 'precious untracked').read_text(), 'keep\n')

    def test_clean_clone_fast_forwards(self):
        result = self.update()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.local / 'value').read_text(), 'new\n')
        self.assertEqual(git(self.local, 'branch', '--show-current').stdout, 'main\n')

    def test_divergence_is_refused_without_reset(self):
        (self.local / 'local-only').write_text('keep')
        git(self.local, 'add', '.')
        git(self.local, 'commit', '-qm', 'local work')
        head = git(self.local, 'rev-parse', 'HEAD').stdout
        result = self.update()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(git(self.local, 'rev-parse', 'HEAD').stdout, head)
        self.assertTrue((self.local / 'local-only').exists())

    def test_ignored_local_file_is_never_overwritten(self):
        # Git merge normally allows replacing ignored files with tracked ones.
        # The updater promises to preserve local files, including build output.
        (self.local / '.git/info/exclude').write_text('local-build\n')
        (self.local / 'local-build').write_text('precious build\n')
        (self.remote / 'local-build').write_text('remote replacement\n')
        git(self.remote, 'add', 'local-build')
        git(self.remote, 'commit', '-qm', 'introduce ignored local path')
        head = git(self.local, 'rev-parse', 'HEAD').stdout
        result = self.update()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(git(self.local, 'rev-parse', 'HEAD').stdout, head)
        self.assertEqual((self.local / 'local-build').read_text(), 'precious build\n')

    def test_clean_work_branch_keeps_its_name(self):
        git(self.local, 'switch', '-qc', 'topic')
        git(self.local, 'branch', '--set-upstream-to', 'origin/main')
        result = self.update()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(git(self.local, 'branch', '--show-current').stdout, 'topic\n')
        self.assertEqual((self.local / 'value').read_text(), 'new\n')

    def test_untracked_only_and_staged_edits_are_preserved(self):
        for staged in [False, True]:
            precious = self.local / 'precious untracked'
            precious.write_text('keep')
            if staged:
                git(self.local, 'add', 'precious untracked')
            head = git(self.local, 'rev-parse', 'HEAD').stdout
            result = self.update()
            self.assertNotEqual(result.returncode, 0)
            self.assertEqual(git(self.local, 'rev-parse', 'HEAD').stdout, head)
            self.assertEqual(precious.read_text(), 'keep')


class BuildUserTests(unittest.TestCase):
    def test_sudo_build_uses_real_user_without_shell_splitting(self):
        for option in [[], ['--deb']]:
            with self.subTest(option=option), tempfile.TemporaryDirectory(prefix='tuxtimings build user ') as tmp:
                source = Path(tmp) / 'source with spaces'
                (source / 'Linux').mkdir(parents=True)
                (source / 'Linux/Makefile').touch()
                fakebin = Path(tmp) / 'fake bin'
                fakebin.mkdir()
                fake_make = fakebin / 'make with spaces'
                fake_make.write_text('#!/bin/sh\nexit 71\n')
                fake_make.chmod(0o755)
                # Replace only the absolute make dependency in the fixture.
                # Both sentinels stop before build/install regardless of branch.
                script = (ROOT / 'install.sh').read_text().replace(
                    'MAKE=/usr/bin/make', 'MAKE=' + shlex.quote(str(fake_make)))
                (source / 'install.sh').write_text(script)
                (fakebin / 'id').write_text('#!/bin/sh\n[ "$1" = -u ] || exit 90\necho 0\n')
                (fakebin / 'id').chmod(0o755)
                spy = fakebin / 'runuser'
                spy.write_text('#!/usr/bin/python3\nimport os,sys,json\nfrom pathlib import Path\n'
                    'Path(os.environ["BUILD_LOG"]).write_text(json.dumps(sys.argv[1:]))\nsys.exit(72)\n')
                spy.chmod(0o755)
                env = os.environ.copy()
                env.update(PATH=str(fakebin) + ':' + env['PATH'],
                           SUDO_USER='fixture-user', BUILD_LOG=str(Path(tmp) / 'argv.json'))
                result = run('bash', str(source / 'install.sh'), *option, env=env, check=False)
                self.assertEqual(result.returncode, 72, result.stdout + result.stderr)
                self.assertEqual(json.loads(Path(env['BUILD_LOG']).read_text()),
                    ['-u', 'fixture-user', '--', str(fake_make), '-C',
                     str(source / 'Linux'), 'PROFILE=portable', 'all'])

    def test_root_without_real_user_refuses_to_build(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings root build ') as tmp:
            source = Path(tmp)
            (source / 'Linux').mkdir()
            (source / 'Linux/Makefile').touch()
            fakebin = source / 'fakebin'
            fakebin.mkdir()
            fake_make = fakebin / 'make'
            fake_make.write_text('#!/bin/sh\nexit 71\n')
            fake_make.chmod(0o755)
            script = (ROOT / 'install.sh').read_text().replace(
                'MAKE=/usr/bin/make', 'MAKE=' + shlex.quote(str(fake_make)))
            (source / 'install.sh').write_text(script)
            (fakebin / 'id').write_text('#!/bin/sh\necho 0\n')
            (fakebin / 'id').chmod(0o755)
            env = os.environ.copy()
            env.update(PATH=str(fakebin) + ':' + env['PATH'], SUDO_USER='root')
            result = run('bash', str(source / 'install.sh'), '--deb', env=env, check=False)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn('unprivileged', result.stderr)


class LauncherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='tuxtimings launcher ')
        cls.base = Path(cls.temp.name)
        cls.binary = cls.base / 'local build with spaces'
        stub = cls.base / 'stubs.c'
        stub.write_text(r'''#include "ui.h"
#include "backend.h"
#include <stdlib.h>
#include <unistd.h>
uid_t __wrap_geteuid(void) { return 1000; }
int backend_is_supported(void) { _exit(97); }
void backend_cleanup(void) { _exit(98); }
GtkApplication *ui_create(int argc, char **argv) { (void)argc; (void)argv; _exit(99); }
''')
        flags = run('pkg-config', '--cflags', '--libs', 'gtk4').stdout.split()
        run('cc', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'Linux/src'),
            str(ROOT / 'Linux/src/main.c'), str(stub), '-Wl,--wrap=geteuid',
            *flags, '-o', str(cls.binary))
        cls.fakebin = cls.base / 'fake bin'
        cls.fakebin.mkdir()
        fake = cls.fakebin / 'pkexec'
        fake.write_text('#!/usr/bin/python3\nimport os,sys\nfrom pathlib import Path\n'
                        'Path(os.environ["ARG_LOG"]).write_bytes(b"\\0".join(os.fsencode(a) for a in sys.argv[1:]))\n'
                        'sys.exit(int(os.environ.get("PKEXEC_STATUS", "126")))\n')
        fake.chmod(0o755)

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def environment(self):
        env = os.environ.copy()
        env['PATH'] = str(self.fakebin) + ':' + env['PATH']
        env['ARG_LOG'] = str(self.base / 'args')
        env['XAUTHORITY'] = '/path with spaces/' + 'a' * 2000
        env['WAYLAND_DISPLAY'] = 'wayland test'
        return env

    def test_version_never_elevates_or_initializes_backend(self):
        result = run(str(self.binary), '--version', env=self.environment(), check=False)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('TuxTimings ', result.stdout)

    def test_exact_self_arguments_and_long_environment(self):
        env = self.environment()
        result = run(str(self.binary), '--arg with spaces', 'quote\"$literal', env=env, check=False)
        self.assertEqual(result.returncode, 126)
        args = (self.base / 'args').read_bytes().decode().split('\0')
        self.assertEqual(args[0], str(self.binary))
        self.assertIn('--env-XAUTHORITY=' + env['XAUTHORITY'], args)
        self.assertIn('--env-GDK_BACKEND=wayland', args)
        self.assertEqual(args[-2:], ['--arg with spaces', 'quote\"$literal'])

    def test_missing_pkexec_fails_without_backend(self):
        env = self.environment()
        env['PATH'] = str(self.base / 'no executable here')
        result = run(str(self.binary), '--argument', env=env, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn('pkexec', result.stderr)

    def test_cancel_status_is_preserved(self):
        env = self.environment()
        env['PKEXEC_STATUS'] = '127'
        result = run(str(self.binary), '--argument', env=env, check=False)
        self.assertEqual(result.returncode, 127)


class IdentityTests(unittest.TestCase):
    def test_generated_metadata_is_readable_for_packaging(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings build metadata ') as tmp:
            destination = Path(tmp) / 'generated'
            run('bash', str(ROOT / 'Linux/tools/build-info.sh'), '--generate', str(destination))
            for name in ['build_info.generated.h', 'build-info.txt']:
                self.assertEqual((destination / name).stat().st_mode & 0o777, 0o644)

    def test_clean_dirty_and_archive_identity(self):
        helper = ROOT / 'Linux/tools/build-info.sh'
        self.assertTrue(helper.exists(), 'Build identity helper must exist')
        with tempfile.TemporaryDirectory(prefix='tuxtimings identity ') as tmp:
            repo = Path(tmp) / 'source'
            init_repo(repo)
            (repo / 'Linux/tools').mkdir(parents=True)
            shutil.copy2(helper, repo / 'Linux/tools/build-info.sh')
            shutil.copy2(ROOT / 'VERSION', repo / 'VERSION')
            git(repo, 'add', '.')
            git(repo, 'commit', '-qm', 'fixture')
            clean = run('bash', str(repo / 'Linux/tools/build-info.sh'), '--id').stdout
            self.assertIn('clean', clean)
            native_env = os.environ.copy()
            native_env['TUXTIMINGS_BUILD_PROFILE'] = 'native'
            native = run('bash', str(repo / 'Linux/tools/build-info.sh'), '--id', env=native_env).stdout
            self.assertIn('native', native)
            self.assertNotEqual(clean, native)
            self.assertIn(git(repo, 'rev-parse', '--short=12', 'HEAD').stdout.strip(), clean)
            (repo / 'VERSION').write_text((repo / 'VERSION').read_text() + '\n')
            dirty = run('bash', str(repo / 'Linux/tools/build-info.sh'), '--id').stdout
            self.assertIn('dirty', dirty)
            shutil.rmtree(repo / '.git')
            archive = run('bash', str(repo / 'Linux/tools/build-info.sh'), '--id').stdout
            self.assertIn('archive', archive)
            self.assertNotEqual(clean, archive)


class PortableBuildTests(unittest.TestCase):
    def test_benchmark_compiles_with_baseline_x86_64(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings portable ') as tmp:
            result = run('cc', '-Wall', '-Wextra', '-Werror', '-O2', '-march=x86-64',
                         '-mtune=generic', '-c', str(ROOT / 'Linux/src/bench.c'),
                         '-o', str(Path(tmp) / 'bench.o'), check=False)
            self.assertEqual(result.returncode, 0, result.stderr)


class PackageTests(unittest.TestCase):
    def test_debian_archive_binary_and_metadata_agree(self):
        with tempfile.TemporaryDirectory(prefix='tuxtimings deb source ') as tmp:
            source = Path(tmp) / 'source with spaces'
            source.mkdir()
            for name in ['install.sh', 'VERSION']:
                shutil.copy2(ROOT / name, source / name)
            shutil.copytree(ROOT / 'Linux', source / 'Linux',
                            ignore=shutil.ignore_patterns('*.o', 'tuxtimings', 'build', '*.bak', '*.pre-*'))
            result = run('bash', str(source / 'install.sh'), '--deb', cwd=source, check=False)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            package = next(source.glob('tuxtimings_*.deb'))
            destination = Path(tmp) / 'extracted'
            run('dpkg-deb', '--extract', str(package), str(destination))
            version = run('dpkg-deb', '--field', str(package), 'Version').stdout.strip()
            executable = destination / 'opt/TuxTimings/bin/tuxtimings'
            identity = run(str(executable), '--version').stdout.strip()
            manifest = (destination / 'usr/share/doc/tuxtimings/build-info.txt').read_text()
            self.assertIn('Version: ' + version + '\n', manifest)
            self.assertIn('Build: ' + identity + '\n', manifest)
            self.assertIn('archive', identity)
            # Rebase the installed absolute path into this isolated package root.
            launcher = destination / 'usr/bin/tuxtimings'
            launcher.write_text(launcher.read_text().replace('/opt/TuxTimings/bin/tuxtimings', shlex.quote(str(executable))))
            self.assertEqual(run('bash', str(launcher), '--version').stdout.strip(), identity)


if __name__ == '__main__':
    unittest.main()
