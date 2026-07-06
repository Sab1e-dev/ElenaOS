"""Architecture constraint checker.

Scans source code against YAML-defined architecture rules and reports violations.
Allowed files support glob patterns (e.g., "src/port/**").
"""

import re
from pathlib import Path
from typing import Dict, List

from .base import BaseChecker, CheckResult, Severity


class ArchitectureChecker(BaseChecker):
    name = "architecture"

    def __init__(self, project_root=None, verbose=False, fix=False):
        super().__init__(project_root, verbose, fix)
        self.rules_file = Path(__file__).parent / "config" / "architecture_rules.yaml"
        self.rules: List[Dict] = []

    def is_available(self) -> bool:
        if not self.rules_file.exists():
            self.log(f"Rules file not found: {self.rules_file}")
            return False
        try:
            import yaml as _  # noqa: F401
        except ImportError:
            self.log("PyYAML is not installed, run: pip install pyyaml")
            return False
        return True

    def _load_rules(self):
        import yaml

        with open(self.rules_file, "r", encoding="utf-8") as f:
            data = yaml.safe_load(f)
        self.rules = data.get("rules", [])
        self.log(f"Loaded {len(self.rules)} architecture rule(s)")

    def run(self) -> CheckResult:
        result = CheckResult(checker_name=self.name)
        self._load_rules()

        source_files = self.find_source_files([".c", ".cpp", ".h"], relative=True)
        self.log(f"Scanning {len(source_files)} source file(s)")

        for rule in self.rules:
            rule_result = self._check_rule(rule, source_files)
            result.merge(rule_result)

        return result

    @staticmethod
    def _is_comment_line(stripped: str, in_block: bool) -> bool:
        if not stripped:
            return True
        if in_block:
            return True
        if stripped.startswith("//"):
            return True
        if stripped.startswith("/*"):
            return True
        if stripped == "*/":
            return True
        return False

    def _is_allowed(self, file_path: str, allowed: List[str]) -> bool:
        return self.matches_any_glob(file_path, allowed)

    def _check_rule(self, rule: Dict, source_files: List[Path]) -> CheckResult:
        rule_name = rule.get("name", "unnamed")
        pattern = rule.get("pattern", "")
        allowed = rule.get("allowed_files", [])
        sev_str = rule.get("severity", "error")
        severity = Severity.ERROR if sev_str == "error" else Severity.WARNING

        result = CheckResult(checker_name=f"{self.name}/{rule_name}")
        regex = re.compile(pattern)

        for sf in source_files:
            sf_str = str(sf).replace("\\", "/")
            content = (self.project_root / sf).read_text(encoding="utf-8", errors="replace")
            lines = content.split("\n")

            in_block = False
            for i, line in enumerate(lines, 1):
                stripped = line.strip()
                if stripped.startswith("/*"):
                    in_block = True
                if "*/" in stripped:
                    in_block = False
                    continue
                if self._is_comment_line(stripped, in_block):
                    continue
                if regex.search(line):
                    if allowed and self._is_allowed(sf_str, allowed):
                        self.log(f"  {sf_str}:{i} — allowed")
                        continue
                    msg = (
                        f"{rule.get('description', rule_name)}: "
                        f"violation found in {sf_str}:{i}"
                    )
                    if severity == Severity.ERROR:
                        result.add_error(msg)
                    else:
                        result.add_warning(msg)
                    self.log(msg)

        return result
