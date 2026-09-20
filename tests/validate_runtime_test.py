"""Exercise the real subprocess validator with controlled completion/leak output."""
import sys
import tempfile
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from validate_runtime import run_check

class RuntimeValidation(unittest.TestCase):
    def invoke(self, trailing='', strict=False, exit_code=0):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            (project / 'start-linux.sh').write_text(
                "cat <<'VALIDATION_OUTPUT'\nPASS: fixture completed\n"
                "ORIGINAL QUIZ INPUT: 0 failures\n" + trailing +
                "\nVALIDATION_OUTPUT\nexit " + str(exit_code) + "\n")
            return run_check(project, 'original_quiz_input', project / 'run.log', 2, strict)

    def test_clean_strict_run(self):
        result = self.invoke(strict=True)
        self.assertTrue(result['passed'])
        self.assertFalse(result['objectdb_leaks'])

    def test_leak_warning_is_reported_without_strict_mode(self):
        result = self.invoke('WARNING: ObjectDB instances leaked at exit')
        self.assertTrue(result['passed'])
        self.assertTrue(result['objectdb_leaks'])

    def test_strict_rejects_warning_despite_success_marker(self):
        self.assertFalse(self.invoke('WARNING: ObjectDB instances leaked at exit', True)['passed'])

    def test_strict_rejects_verbose_leak_line(self):
        self.assertFalse(self.invoke('Leaked instance: AudioStreamWAV:1234', True)['passed'])

    def test_unrelated_warning_is_not_mislabeled(self):
        result = self.invoke('WARNING: unrelated test message', True)
        self.assertTrue(result['passed'])
        self.assertFalse(result['objectdb_leaks'])

    def test_process_failure_still_fails(self):
        self.assertFalse(self.invoke(strict=True, exit_code=2)['passed'])

if __name__ == '__main__':
    unittest.main()
