#!/usr/bin/python3
import os
import re
from typing import Any, Dict

import yaml


# Load configuration from YAML file
def load_config() -> Dict[str, Any]:
    config_path = os.path.join(
        os.path.dirname(__file__), "..", "config", "rejected_patterns.yaml"
    )
    with open(config_path, "r") as f:
        return yaml.safe_load(f)


# Load config once at module level
CONFIG = load_config()


def project(name: str) -> bool:
    reject_regex = CONFIG.get("rejected_project_regex", [])
    reject_repo = CONFIG.get("rejected_repos", [])

    for r in reject_repo:
        if r == name:
            return False

    for r in reject_regex:
        if re.match(r, name):
            return False

    return True


def file(proj: str, filename: str) -> bool:
    reject_regex = CONFIG.get("rejected_file_regex", {})
    reject_files = CONFIG.get("rejected_files", [])

    for r in reject_files:
        if r == filename:
            return False

    for r in reject_regex.get(proj, []) + reject_regex.get("all", []):
        if re.match(r, filename):
            return False

    return True
