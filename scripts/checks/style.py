"""Code style checker.

- If clang-tidy is available: uses clang-tidy for style checks
- Otherwise: falls back to custom regex-based rules scanning common style issues
"""

from pathlib import Path
from typing import List, Optional, Tuple

from .base import BaseChecker, CheckResult, Severity


class StyleChecker(BaseChecker):
    name = "style"

    CUSTOM_RULES: List[Tuple[str, str, str]] = [
        ("TODO/FIXME without issue reference", r"\b(TODO|FIXME)\s*(?!:.*#\d+)", "warning"),
        ("Use of sprintf is forbidden (use snprintf)", r"\bsprintf\s*\(", "error"),
        ("Use of gets is forbidden", r"\bgets\s*\(", "error"),
        ("Use of scanf (use sscanf with bounds check)", r"\bscanf\s*\(", "warning"),
        ("goto statement found", r"\bgoto\s+", "warning"),
        ("Suspicious label indent", r"^(?!\s)[\w_]+:\s*$", "warning"),
    ]

    def __init__(self, project_root=None, verbose=False, fix=False):
        super().__init__(project_root, verbose, fix)
        self.clang_tidy: Optional[Path] = None
        self.compile_db: Optional[Path] = None

    def is_available(self) -> bool:
        return True

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

        clang_tidy_path = self.which("clang-tidy")
        if clang_tidy_path:
            self.clang_tidy = Path(clang_tidy_path)
            self.compile_db = self._find_compile_db()
            if self.compile_db:
                self.log("Running clang-tidy style checks")
                return self._run_clang_tidy(result)
            else:
                self.log("compile_commands.json not found, falling back to custom rules")

        self.log("clang-tidy not available, running custom rule checks")
        return self._run_custom_rules(result)

    def _run_clang_tidy(self, result: CheckResult) -> CheckResult:
        source_files = self.find_source_files([".c", ".cpp"], relative=False)
        if not source_files:
            result.add_info("No source files found")
            return result

        for sf in source_files:
            sf_str = str(sf)
            self.log(f"  Checking {sf.relative_to(self.project_root)}")
            cmd = [
                str(self.clang_tidy),
                "-p", str(self.compile_db.parent),
                sf_str,
                "--quiet",
            ]
            proc = self.run_cmd(cmd, timeout=120)
            if proc.returncode != 0 and proc.stdout.strip():
                for line in proc.stdout.strip().split("\n"):
                    if "warning:" in line or "error:" in line:
                        result.add_warning(f"{sf.relative_to(self.project_root)}: {line.strip()}")
                    else:
                        result.add_info(line.strip())

        return result

    def _run_custom_rules(self, result: CheckResult) -> CheckResult:
        import re

        source_files = self.find_source_files([".c", ".cpp", ".h", ".hpp"], relative=True)
        self.log(f"Scanning {len(source_files)} source file(s)")

        for sf in source_files:
            sf_str = str(sf).replace("\\", "/")
            content = (self.project_root / sf).read_text(encoding="utf-8", errors="replace")
            lines = content.split("\n")

            for rule_desc, pattern, sev in self.CUSTOM_RULES:
                regex = re.compile(pattern)
                for i, line in enumerate(lines, 1):
                    if regex.search(line):
                        msg = f"{rule_desc}: {sf_str}:{i}"
                        if sev == "error":
                            result.add_error(msg)
                        else:
                            result.add_warning(msg)

        return result
