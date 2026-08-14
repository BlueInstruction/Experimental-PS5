"""Test the engine runner."""
from engine.runner import EngineRunner
from engine.contract import ContractLoader


def test_runner_with_build_probe():
    contract = ContractLoader.default_contract()
    runner = EngineRunner(contract)
    # Runner will try to run the "build" probe
    # In test mode, the probe checks if APK exists (it won't in CI)
    report = runner.run()
    assert "status" in report
    assert "probes" in report
