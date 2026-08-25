"""Code style checker.

- If clang-tidy is available: uses clang-tidy for style checks
- Otherwise: falls back to custom regex-based rules scanning common style issues
- Also checks section-divider format and ugly single-line comments
"""

import re
from pathlib import Path
from typing import List, Optional, Tuple

from .base import BaseChecker, CheckResult, Severity

# ── Divider constants ────────────────────────────────────────────
_TOTAL_WIDTH = 65
_DASHES_BASE = _TOTAL_WIDTH - 6  # 59


def _make_standard_divider(text: str, indent: str = '') -> str:
    """Build the canonical 65-char section divider."""
    dashes = max(1, _DASHES_BASE - len(text))
    return f'{indent}/* {text} {"-" * dashes}*/'


def _extract_divider_text(line: str) -> Optional[Tuple[str, str]]:
    """If line is a section-divider comment, return (text, indent)."""
    stripped = line.strip()
    if not (stripped.startswith('/*') and stripped.endswith('*/')):
        return None
    inner = stripped[2:-2].strip()
    if not inner:
        return None
    if not re.search(r'[-*=_#]{3,}', inner):
        return None

    indent = line[:len(line) - len(line.lstrip())]

    m = re.match(r'^\*{2,}\s+(.+?)\s+\*{2,}$', inner)
    if m: return (m.group(1), indent)
    m = re.match(r'^=+\s+(.+?)\s+=+$', inner)
    if m: return (m.group(1), indent)
    m = re.match(r'^-{1,}\s+(.+?)\s+-{1,}$', inner)
    if m: return (m.group(1), indent)
    m = re.match(r'^[-*=_#]{3,}\s+(.+?)$', inner)
    if m: return (m.group(1), indent)
    m = re.match(r'^(.+?)\s+[-*=_#]{3,}$', inner)
    if m: return (m.group(1), indent)

    return None


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
    # Ugly-comment & divider checks are handled by _run_divider_and_comment_check()
    # (supports --fix for Step/N. stripping and divider reformatting)

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

        # Also run divider + ugly-comment checks even with clang-tidy
        div_result = self._run_divider_and_comment_check()
        result.merge(div_result)

        return result

    def _run_custom_rules(self, result: CheckResult) -> CheckResult:
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

        # Divider format check + fix
        div_result = self._run_divider_and_comment_check()
        result.merge(div_result)

        return result

    # ── Divider & Ugly Comment Fix ────────────────────────────────

    _CHINESE_RE = re.compile(r'[一-鿿㐀-䶿豈-﫿]')
    _STEP_STRIP_RE = re.compile(r'^(Step|step)\s*\d+\s*[-—:]\s*')
    _NUM_STRIP_RE = re.compile(r'^\d+\.\s+')

    def _run_divider_and_comment_check(self) -> CheckResult:
        """Check section dividers and ugly single-line comments. Supports --fix."""
        result = CheckResult(checker_name=f"{self.name}/comments")
        source_files = self.find_source_files([".c", ".h"], relative=False)

        for sf in source_files:
            rel = sf.relative_to(self.project_root)
            sf_str = str(rel).replace("\\", "/")
            content = sf.read_text(encoding="utf-8", errors="replace")
            lines = content.split("\n")

            new_lines = []
            modified = False

            for lineno, line in enumerate(lines, 1):
                new_line = line
                stripped = line.strip()

                # ── Divider format check ──
                div_info = _extract_divider_text(line)
                if div_info:
                    text, indent = div_info
                    expected = _make_standard_divider(text, indent)
                    # Compare stripped versions since expected may have indent
                    if stripped != expected.strip():
                        result.add_warning(
                            f"Non-standard divider: {sf_str}:{lineno}"
                        )
                        if self.fix:
                            new_line = expected
                            modified = True

                # ── Ugly single-line comment check ──
                comment_data = self._parse_single_line_comment(stripped)
                if comment_data:
                    comment_text, prefix, suffix, style = comment_data
                    fixed_text = comment_text
                    comment_fixed = False

                    # Step N check
                    step_m = self._STEP_STRIP_RE.search(comment_text)
                    if step_m:
                        result.add_warning(
                            f"Step numbering in comment: {sf_str}:{lineno}"
                        )
                        if self.fix:
                            fixed_text = self._strip_and_cap(comment_text, step_m)
                            comment_fixed = True

                    # N. check
                    num_m = self._NUM_STRIP_RE.search(comment_text)
                    if num_m:
                        result.add_warning(
                            f"Numbered list in comment: {sf_str}:{lineno}"
                        )
                        if self.fix:
                            fixed_text = self._strip_and_cap(comment_text, num_m)
                            comment_fixed = True

                    # Chinese / 方案 / 步骤 — report only (no auto-fix)
                    if self._CHINESE_RE.search(comment_text):
                        result.add_warning(
                            f"Chinese text in comment: {sf_str}:{lineno}"
                        )
                    if re.search(r'方案|步骤', comment_text):
                        result.add_warning(
                            f"'方案'/'步骤' in comment: {sf_str}:{lineno}"
                        )

                    if self.fix and comment_fixed and fixed_text != comment_text:
                        new_line = self._rebuild_comment_line(
                            fixed_text, prefix, suffix, style
                        )

                new_lines.append(new_line)

            if self.fix and modified:
                fixed_content = '\n'.join(new_lines)
                sf.write_text(fixed_content, encoding='utf-8')
                self.log(f"Fixed dividers/comments in {sf_str}")

        return result

    @staticmethod
    def _parse_single_line_comment(stripped: str) -> Optional[Tuple[str, str, str, str]]:
        """Parse a single-line comment. Returns (text, prefix, suffix, style)."""
        # // style
        m = re.search(r'(?<!:)//(.*)$', stripped)
        if m:
            return (m.group(1).lstrip(), stripped[:m.start(0)] + '// ', '', 'slash_slash')
        # /* ... */ style
        m = re.match(r'^(\s*/\*\s*)(.+?)(\s*\*/\s*)$', stripped)
        if m:
            return (m.group(2), m.group(1), m.group(3), 'slash_star')
        return None

    @staticmethod
    def _strip_and_cap(text: str, match: re.Match) -> str:
        """Strip a matched prefix and capitalize the first letter."""
        result = text[:match.start()] + text[match.end():]
        m2 = re.search(r'\S', result)
        if m2:
            i = m2.start()
            result = result[:i] + result[i].upper() + result[i+1:]
        return result

    @staticmethod
    def _rebuild_comment_line(text: str, prefix: str, suffix: str, style: str) -> str:
        """Rebuild a comment line from parts."""
        if style == 'slash_slash':
            return f'{prefix}{text}'
        return f'{prefix}{text}{suffix}'
