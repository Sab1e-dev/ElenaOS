"""Base checker class and shared utilities."""

import fnmatch
import subprocess
import sys
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Optional


class Severity(Enum):
    ERROR = "error"
    WARNING = "warning"
    INFO = "info"

    def _order(self) -> int:
        return {Severity.ERROR: 3, Severity.WARNING: 2, Severity.INFO: 1}[self]

    def __ge__(self, other: "Severity") -> bool:
        if not isinstance(other, Severity):
            return NotImplemented
        return self._order() >= other._order()

    def __gt__(self, other: "Severity") -> bool:
        if not isinstance(other, Severity):
            return NotImplemented
        return self._order() > other._order()


@dataclass
class CheckResult:
    checker_name: str
    severity: Severity = Severity.INFO
    messages: List[str] = field(default_factory=list)
    passed: bool = True

    def add_error(self, msg: str):
        self.messages.append(f"[ERROR] {msg}")
        self.passed = False
        self.severity = max(self.severity, Severity.ERROR)

    def add_warning(self, msg: str):
        self.messages.append(f"[WARNING] {msg}")
        self.severity = max(self.severity, Severity.WARNING)

    def add_info(self, msg: str):
        self.messages.append(f"[INFO] {msg}")

    def merge(self, other: "CheckResult"):
        self.messages.extend(other.messages)
        self.passed = self.passed and other.passed
        self.severity = max(self.severity, other.severity)


class BaseChecker(ABC):
    name: str = ""

    def __init__(self, project_root: Optional[Path] = None, verbose: bool = False, fix: bool = False):
        if project_root is None:
            project_root = self._find_project_root()
        self.project_root = Path(project_root)
        self.verbose = verbose
        self.fix = fix

    @staticmethod
    def _find_project_root() -> Path:
        return Path(__file__).resolve().parents[2]

    def log(self, msg: str):
        if self.verbose:
            print(f"  [{self.name}] {msg}", file=sys.stderr)

    @abstractmethod
    def is_available(self) -> bool:
        ...

    @abstractmethod
    def run(self) -> CheckResult:
        ...

    def find_source_files(self, extensions: List[str], relative: bool = False) -> List[Path]:
        files = []
        search_root = self.project_root / "src"
        if search_root.exists():
            for ext in extensions:
                files.extend(search_root.rglob(f"*{ext}"))

        if relative:
            files = [f.relative_to(self.project_root) for f in files]
        return sorted(files)

    @staticmethod
    def matches_any_glob(path: str, patterns: List[str]) -> bool:
        for pattern in patterns:
            if fnmatch.fnmatch(path, pattern):
                return True
        return False

    def run_cmd(self, cmd: List[str], capture: bool = True, timeout: int = 300) -> subprocess.CompletedProcess:
        try:
            return subprocess.run(
                cmd,
                capture_output=capture,
                text=True,
                timeout=timeout,
                cwd=str(self.project_root),
            )
        except FileNotFoundError:
            return subprocess.CompletedProcess(cmd, 127, stdout="", stderr=f"Command not found: {cmd[0]}")
        except subprocess.TimeoutExpired:
            return subprocess.CompletedProcess(cmd, 124, stdout="", stderr=f"Command timed out: {' '.join(cmd)}")

    def which(self, name: str) -> Optional[str]:
        result = self.run_cmd(["which", name], capture=True)
        if result.returncode == 0:
            return result.stdout.strip()
        return None
