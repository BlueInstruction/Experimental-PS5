"""Main engine runner — orchestrates probes, collects evidence, applies policy."""
import json
import time
from datetime import datetime, timezone
from .evidence import EvidenceCollector
from .policy import PolicyEngine
from .report import ReportGenerator


class EngineRunner:
    def __init__(self, contract: dict):
        self.contract = contract
        self.evidence = EvidenceCollector()
        self.policy = PolicyEngine(contract.get("policy", {}))
        self.reporter = ReportGenerator()

    def run(self) -> dict:
        results = {}
        all_evidence = []
        all_findings = []

        for probe_name in self.contract.get("probes", []):
            probe = self._get_probe(probe_name)
            if probe is None:
                results[probe_name] = "SKIPPED"
                continue

            start = time.monotonic()
            result = probe.run(self.contract)
            duration = (time.monotonic() - start) * 1000

            results[probe_name] = result["status"]
            all_evidence.extend(result.get("evidence", []))
            all_findings.extend(result.get("findings", []))

        verdict = self.policy.evaluate(results, all_findings)
        report = self.reporter.generate(
            contract=self.contract,
            probe_results=results,
            findings=all_findings,
            evidence=all_evidence,
            verdict=verdict,
        )
        return report

    def _get_probe(self, name: str):
        probes = {
            "build": self._import_probe("probes.build.build_probe", "BuildProbe"),
            "native-loader": self._import_probe("probes.native.native_loader_probe", "NativeLoaderProbe"),
            "vulkan-dummy": self._import_probe("probes.vulkan.vulkan_dummy_probe", "VulkanDummyProbe"),
        }
        return probes.get(name)

    def _import_probe(self, module_path: str, class_name: str):
        try:
            mod = __import__(module_path, fromlist=[class_name])
            return getattr(mod, class_name)()
        except Exception:
            return None

    def write_report(self, report: dict) -> None:
        os.makedirs("evidence-output", exist_ok=True)
        path = "evidence-output/agent-report.json"
        with open(path, "w") as f:
            json.dump(report, f, indent=2)
        print(f"Report written to {path}")
        print(f"Status: {report['status']}  Severity: {report.get('severity', 'NONE')}")


import os
