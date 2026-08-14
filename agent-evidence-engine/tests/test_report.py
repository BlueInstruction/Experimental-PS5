"""Test report generation."""
from engine.report import ReportGenerator


def test_pass_report():
    gen = ReportGenerator()
    report = gen.generate(
        contract={"project": {"name": "test"}},
        probe_results={"build": "PASS"},
        findings=[],
        evidence=[],
        verdict={"status": "PASS", "severity": "NONE", "fail_on": ["P0"]},
    )
    assert report["status"] == "PASS"
    assert report["agent_action"]["required"] is False


def test_fail_report():
    gen = ReportGenerator()
    report = gen.generate(
        contract={"project": {"name": "test"}},
        probe_results={"build": "FAIL"},
        findings=[{"id": "BUILD-001", "severity": "P0", "title": "APK missing"}],
        evidence=[],
        verdict={"status": "FAIL", "severity": "P0", "fail_on": ["P0"]},
    )
    assert report["status"] == "FAIL"
    assert report["agent_action"]["required"] is True
