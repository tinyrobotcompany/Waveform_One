import importlib.util
from pathlib import Path
import unittest

SPEC = importlib.util.spec_from_file_location('scope', Path(__file__).parents[1] / 'ci-scope.py')
scope = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(scope)


class ScopeTests(unittest.TestCase):
    def test_documentation_and_changesets_skip_builds(self):
        self.assertEqual(scope.select(['README.md', 'firmware/esp32/visualizer/README.md',
                                       '.changeset/example.md']),
                         {'firmware': [], 'pi': False, 'tests': False})

    def test_app_source_selects_only_that_app(self):
        for app in scope.APPS:
            result = scope.select([f'firmware/esp32/{app}/main/main.cpp'])
            expected = [a for a in scope.APPS if a in ({app, 'visualizer'} if app == 'mic_test' else {app})]
            self.assertEqual(result, {'firmware': expected, 'pi': False, 'tests': True})

    def test_shared_audio_header_rebuilds_both_consumers(self):
        self.assertEqual(scope.select(['firmware/esp32/mic_test/main/audio_pipeline.h'])['firmware'],
                         ['mic_test', 'visualizer'])

    def test_build_configuration_rebuilds_app(self):
        for name in ['CMakeLists.txt', 'sdkconfig.defaults', 'dependencies.lock', 'main/idf_component.yml']:
            self.assertEqual(scope.select(['firmware/esp32/visualizer/' + name])['firmware'], ['visualizer'])

    def test_shared_firmware_changes_rebuild_all(self):
        self.assertEqual(scope.select(['firmware/esp32/components/common/code.c'])['firmware'], scope.APPS)

    def test_pi_source_and_lockfile_select_pi(self):
        for name in ['src/lib.rs', 'Cargo.lock']:
            self.assertEqual(scope.select(['pi/core/' + name]),
                             {'firmware': [], 'pi': True, 'tests': True})

    def test_packaging_change_builds_production_artifact(self):
        self.assertEqual(scope.select(['scripts/package-firmware.py'])['firmware'], ['visualizer'])

    def test_validation_workflow_and_filter_changes_validate_everything(self):
        for path in ['.github/workflows/validate.yml', '.github/workflows/pr-checks.yml', 'scripts/ci-scope.py']:
            self.assertEqual(scope.select([path]), {'firmware': scope.APPS, 'pi': True, 'tests': True})

    def test_automation_tests_do_not_rebuild_firmware(self):
        self.assertEqual(scope.select(['.github/scripts/codex-pr-review.mjs']),
                         {'firmware': [], 'pi': False, 'tests': True})
