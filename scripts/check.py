#!/usr/bin/env python3
"""ElenixOS unified system check entry point.

Usage:
    python3 scripts/check.py                   # Run all checks
    python3 scripts/check.py --check arch,fmt  # Run specific checks
    python3 scripts/check.py --check fmt --fix # Auto-fix formatting
    python3 scripts/check.py --json            # JSON output (CI)
    python3 scripts/check.py --warn-as-error   # Treat warnings as errors
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Type

from checks.architecture import ArchitectureChecker
from checks.base import BaseChecker, CheckResult, Severity
from checks.formatting import FormattingChecker
from checks.static_analysis import StaticAnalysisChecker
from checks.style import StyleChecker

CHECKER_MAP: Dict[str, Type[BaseChecker]] = {
    "architecture": ArchitectureChecker,
    "arch": ArchitectureChecker,
    "formatting": FormattingChecker,
    "fmt": FormattingChecker,
    "style": StyleChecker,
    "static_analysis": StaticAnalysisChecker,
    "sa": StaticAnalysisChecker,
}

EXIT_SUCCESS = 0
EXIT_ERROR = 1
EXIT_WARNING = 2


def parse_args():
    parser = argparse.ArgumentParser(
        description="ElenixOS system check tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--check", "-c",
        type=str,
        default="all",
        help="Comma-separated checks. Options: architecture/arch, formatting/fmt, style, static_analysis/sa (default: all)",
    )
    parser.add_argument("--fix", "-f", action="store_true", help="Attempt to auto-fix issues (currently only formatting)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--json", "-j", action="store_true", help="JSON output for CI")
    parser.add_argument("--warn-as-error", "-W", action="store_true", help="Treat warnings as errors")
    parser.add_argument("--list-checks", "-l", action="store_true", help="List all available checks")
    return parser.parse_args()


def get_checkers(names: str, project_root: Path, verbose: bool, fix: bool) -> List[BaseChecker]:
    if names == "all":
        names = "architecture,formatting,style,static_analysis"

    checkers = []
    for name in names.split(","):
        name = name.strip()
        if name in CHECKER_MAP:
            cls = CHECKER_MAP[name]
            checkers.append(cls(project_root=project_root, verbose=verbose, fix=fix))
        else:
            print(f"Unknown check: {name}. Use --list-checks to see available checks", file=sys.stderr)

    return checkers


def run_check(checker: BaseChecker, verbose: bool) -> CheckResult:
    if verbose:
        print(f"\n=== {checker.name} ===", file=sys.stderr)

    if not checker.is_available():
        msg = f"{checker.name}: tool not available, skipping"
        if verbose:
            print(f"  {msg}", file=sys.stderr)
        result = CheckResult(checker_name=checker.name)
        result.add_info(msg)
        return result

    if verbose:
        print("  Running...", file=sys.stderr)

    return checker.run()


def print_results(results: List[CheckResult], warn_as_error: bool):
    total_errors = 0
    total_warnings = 0
    overall_passed = True

    for r in results:
        passed = r.passed
        if warn_as_error and r.severity >= Severity.WARNING:
            passed = False

        if not passed:
            overall_passed = False

        status = "PASS" if passed else "FAIL"

        print(f"\n[{status}] {r.checker_name}")
        for msg in r.messages:
            print(f"  {msg}")
            if msg.startswith("[ERROR]"):
                total_errors += 1
            elif msg.startswith("[WARNING]"):
                total_warnings += 1

    print(f"\n{'=' * 50}")
    print(f"Total: {total_errors} error(s), {total_warnings} warning(s)")
    if overall_passed:
        print("Result: ALL CHECKS PASSED")
    else:
        print("Result: SOME CHECKS FAILED")

    return overall_passed


def print_json(results: List[CheckResult], warn_as_error: bool):
    output = {
        "results": [],
        "summary": {"errors": 0, "warnings": 0, "passed": True},
    }

    for r in results:
        errors = sum(1 for m in r.messages if m.startswith("[ERROR]"))
        warnings = sum(1 for m in r.messages if m.startswith("[WARNING]"))

        passed = r.passed
        if warn_as_error and r.severity >= Severity.WARNING:
            passed = False

        output["results"].append({
            "name": r.checker_name,
            "passed": passed,
            "errors": errors,
            "warnings": warnings,
            "messages": r.messages,
        })
        output["summary"]["errors"] += errors
        output["summary"]["warnings"] += warnings
        if not passed:
            output["summary"]["passed"] = False

    print(json.dumps(output, ensure_ascii=False, indent=2))


def main():
    args = parse_args()

    if args.list_checks:
        print("Available checks:")
        seen = set()
        for name, cls in CHECKER_MAP.items():
            if name not in seen:
                seen.add(name)
                doc = cls.__doc__.split("\n")[1].strip() if cls.__doc__ else ""
                print(f"  {name:20s} — {doc}")
        return

    script_dir = Path(__file__).resolve().parent
    sys.path.insert(0, str(script_dir))
    project_root = script_dir.parent

    checkers = get_checkers(args.check, project_root, args.verbose, args.fix)
    if not checkers:
        print("No valid checks to run", file=sys.stderr)
        sys.exit(EXIT_ERROR)

    results = []
    for checker in checkers:
        result = run_check(checker, args.verbose)
        results.append(result)

    if args.json:
        print_json(results, args.warn_as_error)
    else:
        print_results(results, args.warn_as_error)

    all_passed = all(
        (not args.warn_as_error or r.severity < Severity.WARNING) and r.passed
        for r in results
    )
    sys.exit(EXIT_SUCCESS if all_passed else EXIT_ERROR)


if __name__ == "__main__":
    main()
