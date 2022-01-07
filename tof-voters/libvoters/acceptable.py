#!/usr/bin/python3
import re


def project(name: str) -> bool:
    reject_regex = [
        ".*-oem",
        "openbmc/ibm-.*",
        "openbmc/intel-.*",
        "openbmc/openpower-.*",
        "openbmc/google-.*",
        "openbmc/meta-.*",
    ]

    reject_repo = [
        "openbmc/s2600wf-misc",
    ]

    for r in reject_repo:
        if r == name:
            return False

    for r in reject_regex:
        if re.match(r, name):
            return False

    return True


def file(proj: str, filename: str) -> bool:
    reject_regex = {
        "all": [
            ".*/google/",
            ".*/ibm/",
            ".*/intel/",
            "MAINTAINERS",
            "OWNERS",
            "ibm-.*",
            "ibm_.*",
        ],
        "openbmc/entity-manager": ["configurations/.*"],
        "openbmc/libmctp": ["docs/bindings/vendor-.*"],
        "openbmc/openbmc": ["meta-(?!phosphor).*", "poky/.*"],
        "openbmc/openbmc-test-automation": ["oem/.*", "openpower/.*"],
        "openbmc/phosphor-debug-collector": [
            "dump-extensions/.*",
            "tools/dreport.d/ibm.d/.*",
        ],
        "openbmc/phosphor-fan-presence": [".*/config_files/.*"],
        "openbmc/phosphor-power": [".*/config_files/.*"],
        "openbmc/phosphor-led-manager": ["configs/.*"],
        "openbmc/phosphor-logging": [".*/openpower-pels/.*"],
        "openbmc/webui-vue": [
            "src/env/.*",
        ],
    }

    reject_files = ["/COMMIT_MSG"]

    for r in reject_files:
        if r == filename:
            return False

    for r in reject_regex.get(proj, []) + reject_regex.get("all", []):
        if re.match(r, filename):
            return False

    return True
