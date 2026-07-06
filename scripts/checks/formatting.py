"""Code formatting checker using clang-format.

Checks whether C/C++ source files conform to the project's .clang-format style.
"""

from pathlib import Path
from typing import List, Optional

from .base import BaseChecker, CheckResult, Severity


class FormattingChecker(BaseChecker):
    name = "formatting"

    def __init__(self, project_root=None, verbose=False, fix=False):
        super().__init__(project_root, verbose, fix)
        self.clang_format: Optional[Path] = None
        self.style_file = self.project_root / ".clang-format"

    def is_available(self) -> bool:
        path = self.which("clang-format")
        if not path:
            self.log("clang-format is not installed")
            return False
        self.clang_format = Path(path)
        if not self.style_file.exists():
            self.log(f".clang-format not found: {self.style_file}")
            return False
        return True

    def run(self) -> CheckResult:
        result = CheckResult(checker_name=self.name)
        source_files = self.find_source_files([".c", ".cpp", ".h"], relative=False)

        if not source_files:
            result.add_info("No C/C++ source files found")
            return result

        self.log(f"Checking {len(source_files)} file(s)")

        if self.fix:
            return self._fix_formatting(source_files, result)
        else:
            return self._check_formatting(source_files, result)

    def _check_formatting(self, source_files: List[Path], result: CheckResult) -> CheckResult:
        cmd = [
            str(self.clang_format),
            "--dry-run",
            "--Werror",
            "--style=file",
        ] + [str(f) for f in source_files]

        proc = self.run_cmd(cmd)
        if proc.returncode == 0:
            result.add_info("All files are properly formatted")
        else:
            result.add_error("Some files need formatting (run with --fix to auto-format)")
            if proc.stdout.strip():
                for line in proc.stdout.strip().split("\n"):
                    result.add_info(line)
            if proc.stderr.strip():
                result.add_info(proc.stderr.strip())
        return result

    def _fix_formatting(self, source_files: List[Path], result: CheckResult) -> CheckResult:
        cmd = [
            str(self.clang_format),
            "-i",
            "--style=file",
        ] + [str(f) for f in source_files]

        proc = self.run_cmd(cmd)
        if proc.returncode == 0:
            result.add_info(f"Formatted {len(source_files)} file(s)")
        else:
            result.add_error("Formatting failed")
            if proc.stderr.strip():
                result.add_info(proc.stderr.strip())
        return result
