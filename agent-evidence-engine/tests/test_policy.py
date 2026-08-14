"""Test policy engine."""
from engine.policy import PolicyEngine


def test_all_pass():
    p = PolicyEngine({"fail_on": ["P0", "P1"]})
    v = p.evaluate({"build": "PASS", "native": "PASS"}, [])
    assert v["status"] == "PASS"


def test_fail_on_probe():
    p = PolicyEngine({"fail_on": ["P0", "P1"]})
    v = p.evaluate({"build": "PASS", "native": "FAIL"}, [])
    assert v["status"] == "FAIL"


def test_inconclusive():
    p = PolicyEngine({"fail_on": ["P0"]})
    v = p.evaluate({"build": "INCONCLUSIVE"}, [])
    assert v["status"] == "INCONCLUSIVE"
