"""Project contract loader and validator."""
import json
import os
from pathlib import Path
import yaml
from jsonschema import validate as validate_schema


class ContractLoader:
    @staticmethod
    def load(path: str) -> dict:
        with open(path, "r") as f:
            contract = yaml.safe_load(f)
        ContractLoader.validate(contract)
        return contract

    @staticmethod
    def validate(contract: dict) -> None:
        schema_path = Path(__file__).parent.parent / "schema" / "project-contract.schema.json"
        with open(schema_path, "r") as f:
            schema = json.load(f)
        validate_schema(instance=contract, schema=schema)

    @staticmethod
    def default_contract() -> dict:
        return {
            "version": 1,
            "project": {"name": "unknown", "type": "generic"},
            "probes": ["build"],
            "policy": {"fail_on": ["P0", "P1"]},
            "artifacts": {"logcat": False, "native_logs": False, "probe_results": True},
        }
