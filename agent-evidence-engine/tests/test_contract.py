"""Test contract loading and validation."""
import pytest
from engine.contract import ContractLoader


def test_load_valid_contract():
    loader = ContractLoader()
    contract = loader.load("tests/fixtures/contracts/valid.yml")
    assert contract["version"] == 1
    assert contract["project"]["name"] == "test-project"


def test_default_contract():
    c = ContractLoader.default_contract()
    assert c["probes"] == ["build"]
    assert "P0" in c["policy"]["fail_on"]
