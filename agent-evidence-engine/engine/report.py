"""Report generator — creates agent-report.json."""
import os
from datetime import datetime, timezone
from . import __version__


class ReportGenerator:
    def generate(self, contract, probe_results, findings, evidence, verdict):
        return {
            "schema_version": 1,
            "engine_version": __version__,
            "run": {
                "id": str(os.environ.get("GITHUB_RUN_ID", "local")),
                "commit": os.environ.get("GITHUB_SHA", "unknown")[:12],
                "timestamp": datetime.now(timezone.utc).isoformat(),
            },
            "status": verdict["status"],
            "severity": verdict["severity"],
            "summary": self._summary(verdict, probe_results, findings),
            "findings": findings,
            "probes": probe_results,
            "agent_action": self._agent_action(verdict, findings),
        }

    def _summary(self, verdict, probes, findings):
        if verdict["status"] == "PASS":
            return "All probes passed."
        fail_probes = [k for k, v in probes.items() if v == "FAIL"]
        if fail_probes:
            return f"Failed probes: {', '.join(fail_probes)}"
        return "Inconclusive results detected."

    def _agent_action(self, verdict, findings):
        if verdict["status"] == "PASS":
            return {"required": False, "instruction": ""}
        p1_findings = [f for f in findings if f.get("severity") in ("P0", "P1")]
        if p1_findings:
            titles = "; ".join(f["title"] for f in p1_findings[:3])
            return {"required": True, "instruction": f"Investigate: {titles}"}
        return {"required": True, "instruction": "Review probe evidence for details."}
