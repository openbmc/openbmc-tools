#!/usr/bin/python3

import argparse
import json
import os
import re
from typing import Set, Tuple

import libvoters.acceptable as acceptable


class subcmd:
    def __init__(self, parser: argparse._SubParsersAction) -> None:
        p = parser.add_parser(
            "verify-files",
            help="Check all files against `acceptable` settings.",
        )
        p.set_defaults(cmd=self)

    def run(self, args: argparse.Namespace) -> int:
        data_path: str = args.dir

        if not os.path.exists(data_path) or not os.path.isdir(data_path):
            print(
                f"Data directory {data_path} does not exist or is not a directory."
            )
            return 1

        # Set to store unique (project, file) pairs
        unique_files: Set[Tuple[str, str]] = set()
        rejected_projects: Set[str] = set()

        # Read all JSON files from data directory
        for filename in sorted(os.listdir(data_path)):
            if not re.match(r"[0-9]+\.json", filename):
                continue

            file_path = os.path.join(data_path, filename)
            if not os.path.isfile(file_path):
                continue

            try:
                with open(file_path, "r") as f:
                    data = json.load(f)

                project = data.get("project", "")
                if not project:
                    continue

                # Check if project is acceptable
                if not acceptable.project(project):
                    if project not in rejected_projects:
                        print(f"Rejected project: {project}")
                        rejected_projects.add(project)
                    continue

                # Extract files from the last patchSet
                patch_sets = data.get("patchSets", [])
                if patch_sets:
                    last_patch_set = sorted(
                        patch_sets, key=lambda x: x["number"]
                    )[-1]
                    files = last_patch_set.get("files", [])
                    for file_data in files:
                        file_name = file_data.get("file", "")
                        if file_name:
                            unique_files.add((project, file_name))

            except (json.JSONDecodeError, KeyError) as e:
                print(f"Error processing {filename}: {e}")
                continue

        # Verify each unique file
        for project, file_name in sorted(unique_files):
            result = acceptable.file(project, file_name)
            print(f"{project}:{file_name} -> {result}")

        return 0
