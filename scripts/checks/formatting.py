"""Code formatting checker using clang-format.

Checks whether C/C++ source files conform to the project's .clang-format style.
"""

import glob
import os
import re
from pathlib import Path
from typing import List, Optional, Set

from .base import BaseChecker, CheckResult, Severity

REQUIRED_MAJOR_VERSION = 20


class FormattingChecker(BaseChecker):
    name = "formatting"

    def __init__(self, project_root=None, verbose=False, fix=False, clang_format_bin=None):
        super().__init__(project_root, verbose, fix)
        self.clang_format: Optional[Path] = None
        self.clang_format_bin: Optional[str] = clang_format_bin
        self.style_file = self.project_root / ".clang-format"

    @staticmethod
    def _find_candidates() -> Set[str]:
        candidates: Set[str] = set()

        for d in os.environ.get("PATH", "").split(os.pathsep):
            d = d.strip()
            if not d:
                continue
            for pattern in [os.path.join(d, "clang-format"), os.path.join(d, "clang-format-*")]:
                for match in glob.glob(pattern):
                    if os.path.isfile(match) and os.access(match, os.X_OK):
                        candidates.add(match)

        for prefix in ["/usr/local/opt", "/opt/homebrew/opt"]:
            for match in glob.glob(os.path.join(prefix, "llvm@*", "bin", "clang-format")):
                if os.path.isfile(match) and os.access(match, os.X_OK):
                    candidates.add(match)

        for d in ["/usr/bin", "/usr/local/bin"]:
            for match in glob.glob(os.path.join(d, "clang-format*")):
                if os.path.isfile(match) and os.access(match, os.X_OK):
                    candidates.add(match)

        return candidates

    @staticmethod
    def _parse_version(binary: str) -> int:
        import subprocess
        try:
            out = subprocess.run(
                [binary, "--version"],
                capture_output=True, text=True, timeout=10,
            )
            if out.returncode == 0:
                m = re.search(r"clang-format version (\d+)\.", out.stdout)
                if m:
                    return int(m.group(1))
        except Exception:
            pass
        return 0

    def is_available(self) -> bool:
        if self.clang_format_bin:
            self.clang_format = Path(self.clang_format_bin)
            if not self.clang_format.exists():
                self.log(f"clang-format not found: {self.clang_format_bin}")
                self.clang_format = None
            else:
                self.log(f"Using user-specified clang-format: {self.clang_format}")
        else:
            candidates = sorted(self._find_candidates())
            if not candidates:
                self.log("No clang-format binaries found on system")
            for path in candidates:
                ver = self._parse_version(path)
                self.log(f"Candidate {path} -> version {ver}")
                if ver == REQUIRED_MAJOR_VERSION:
                    self.clang_format = Path(path)
                    self.log(f"Found clang-format {ver}: {path}")
                    break

            if not self.clang_format:
                self.log(f"clang-format version {REQUIRED_MAJOR_VERSION} is not installed")

        if self.clang_format and not self.style_file.exists():
            self.log(f".clang-format not found: {self.style_file}")
            self.clang_format = None

        return True

    def run(self) -> CheckResult:
        result = CheckResult(checker_name=self.name)

        if not self.clang_format:
            result.passed = False
            result.add_error(f"clang-format version {REQUIRED_MAJOR_VERSION} is required but not found")
            result.add_info("Install it via:")
            result.add_info("  macOS:   brew install llvm@20")
            result.add_info("  Ubuntu:  sudo apt-get install clang-format-20")
            result.add_info("Or specify path: --clang-format /path/to/clang-format-20")
            return result

        proc = self.run_cmd([str(self.clang_format), "--version"])
        if proc.returncode == 0:
            self.log(f"Using {proc.stdout.strip()}")
        result.add_info(proc.stdout.strip() if proc.returncode == 0 else "clang-format version unknown")

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
