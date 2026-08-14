"""Policy engine — evaluates probe results and findings to produce a verdict."""


class PolicyEngine:
    def __init__(self, policy_config: dict):
        self.fail_on = set(policy_config.get("fail_on", ["P0", "P1"]))

    def evaluate(self, probe_results: dict, findings: list) -> dict:
        any_fail = any(v == "FAIL" for v in probe_results.values())
        any_inconclusive = any(v == "INCONCLUSIVE" for v in probe_results.values())

        max_severity = "NONE"
        for f in findings:
            sev = f.get("severity", "P3")
            if sev in self.fail_on and sev > max_severity:
                max_severity = sev

        if any_fail:
            status = "FAIL"
        elif any_inconclusive:
            status = "INCONCLUSIVE"
        else:
            status = "PASS"

        severity = max_severity if status == "FAIL" else "NONE"

        return {
            "status": status,
            "severity": severity,
            "fail_on": list(self.fail_on),
        }
