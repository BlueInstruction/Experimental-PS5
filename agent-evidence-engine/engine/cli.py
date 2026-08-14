"""CLI entry point for agent-evidence-engine.

Usage:
    aee run --contract <path> [--fail-on P0,P1]
    aee validate --contract <path>
    aee report --run-id <id>
"""
import argparse
import sys
from .runner import EngineRunner
from .contract import ContractLoader


def main():
    parser = argparse.ArgumentParser(
        prog="aee",
        description="Agent Evidence Engine — deterministic runtime validation",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    run_p = sub.add_parser("run", help="Run all probes and generate agent-report.json")
    run_p.add_argument("--contract", required=True, help="Path to project contract YAML")
    run_p.add_argument("--fail-on", default="P0,P1", help="Severity levels that cause exit code 1")

    val_p = sub.add_parser("validate", help="Validate a project contract")
    val_p.add_argument("--contract", required=True, help="Path to project contract YAML")

    args = parser.parse_args()

    if args.command == "run":
        contract = ContractLoader.load(args.contract)
        runner = EngineRunner(contract)
        report = runner.run()
        runner.write_report(report)
        fail_levels = args.fail_on.split(",")
        if report["status"] == "FAIL" and report.get("severity", "NONE") in fail_levels:
            sys.exit(1)
    elif args.command == "validate":
        contract = ContractLoader.load(args.contract)
        print(f"Contract valid: {contract['project']['name']}")
        print(f"  probes: {contract['probes']}")
        print(f"  fail_on: {contract['policy']['fail_on']}")


if __name__ == "__main__":
    main()
