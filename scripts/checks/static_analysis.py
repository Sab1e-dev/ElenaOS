"""Static analysis checker.

- clang-tidy deep analysis (--checks='*')
- cppcheck (if available)
"""

from pathlib import Path
from typing import List, Optional

from .base import BaseChecker, CheckResult, Severity


class StaticAnalysisChecker(BaseChecker):
    name = "static_analysis"

    def __init__(self, project_root=None, verbose=False, fix=False):
        super().__init__(project_root, verbose, fix)
        self.clang_tidy: Optional[Path] = None
        self.cppcheck: Optional[Path] = None
        self.compile_db: Optional[Path] = None

    def is_available(self) -> bool:
        has_tidy = self.which("clang-tidy") is not None
        has_cppcheck = self.which("cppcheck") is not None
        return has_tidy or has_cppcheck

    def _find_compile_db(self) -> Optional[Path]:
        candidates = [
            self.project_root / "build" / "compile_commands.json",
            self.project_root.parent / "build-wasm" / "compile_commands.json",
            self.project_root.parent / "build" / "compile_commands.json",
        ]
        for candidate in candidates:
            if candidate.exists():
                return candidate
        return None

    def run(self) -> CheckResult:
        result = CheckResult(checker_name=self.name)
        available = False

        clang_tidy_path = self.which("clang-tidy")
        if clang_tidy_path:
            self.clang_tidy = Path(clang_tidy_path)
            self.compile_db = self._find_compile_db()
            if self.compile_db:
                available = True
                self.log("Running clang-tidy static analysis")
                tidy_result = self._run_clang_tidy_static()
                result.merge(tidy_result)
            else:
                result.add_info("compile_commands.json not found, skipping clang-tidy static analysis")

        cppcheck_path = self.which("cppcheck")
        if cppcheck_path:
            self.cppcheck = Path(cppcheck_path)
            available = True
            self.log("Running cppcheck static analysis")
            cppcheck_result = self._run_cppcheck()
            result.merge(cppcheck_result)

        if not available:
            result.add_info("No static analysis tools available, skipping")

        return result

    def _run_clang_tidy_static(self) -> CheckResult:
        result = CheckResult(checker_name=f"{self.name}/clang-tidy")
        source_files = self.find_source_files([".c", ".cpp"], relative=False)

        for sf in source_files:
            sf_str = str(sf)
            self.log(f"  Analyzing {sf.relative_to(self.project_root)}")
            cmd = [
                str(self.clang_tidy),
                "-p", str(self.compile_db.parent),
                "-checks=*",
                sf_str,
                "--quiet",
            ]
            proc = self.run_cmd(cmd, timeout=120)
            if proc.returncode != 0 and proc.stdout.strip():
                for line in proc.stdout.strip().split("\n"):
                    if "warning:" in line:
                        result.add_warning(f"{sf.relative_to(self.project_root)}: {line.strip()}")
                    elif "error:" in line:
                        result.add_error(f"{sf.relative_to(self.project_root)}: {line.strip()}")
                    else:
                        result.add_info(line.strip())

        return result

    def _run_cppcheck(self) -> CheckResult:
        result = CheckResult(checker_name=f"{self.name}/cppcheck")

        src_dir = self.project_root / "src"
        if not src_dir.exists():
            result.add_info("No source directories to analyze")
            return result

        cmd = [
            str(self.cppcheck),
            "--enable=all",
            "--inconclusive",
            "--quiet",
            "--error-exitcode=0",
            str(src_dir),
        ]

        proc = self.run_cmd(cmd, timeout=300)
        if proc.stdout.strip():
            for line in proc.stdout.strip().split("\n"):
                if "error:" in line.lower() or "(error)" in line.lower():
                    result.add_error(line.strip())
                elif "warning:" in line.lower() or "(warning)" in line.lower():
                    result.add_warning(line.strip())
                else:
                    result.add_info(line.strip())

        if proc.stderr.strip():
            for line in proc.stderr.strip().split("\n"):
                result.add_error(line.strip())

        return result
