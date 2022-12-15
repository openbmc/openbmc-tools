#!/usr/bin/python3

import argparse
import json
import os
import re
from collections import defaultdict
from typing import Dict

import libvoters.acceptable as acceptable
from libvoters.time import TimeOfDay, timestamp


class subcmd:
    def __init__(self, parser: argparse._SubParsersAction) -> None:
        p = parser.add_parser(
            "analyze-commits", help="Determine points for commits"
        )

        p.add_argument(
            "--before",
            "-b",
            help="Before timestamp (YYYY-MM-DD)",
            required=True,
        )
        p.add_argument(
            "--after",
            "-a",
            help="After timestamp (YYYY-MM-DD)",
            required=True,
        )

        p.set_defaults(cmd=self)

    def run(self, args: argparse.Namespace) -> int:
        before = timestamp(args.before, TimeOfDay.AM)
        after = timestamp(args.after, TimeOfDay.PM)

        changes_per_user: Dict[str, list[int]] = defaultdict(list)

        for f in sorted(os.listdir(args.dir)):
            path = os.path.join(args.dir, f)
            if not os.path.isfile(path):
                continue

            if not re.match(r"[0-9]*\.json", f):
                continue

            with open(path, "r") as file:
                data = json.load(file)

            if data["status"] != "MERGED":
                continue

            merged_at = 0
            for c in data["comments"]:
                if "timestamp" not in c:
                    continue
                if "message" in c and re.match(
                    "Change has been successfully .*", c["message"]
                ):
                    merged_at = c["timestamp"]

            if merged_at == 0:
                raise RuntimeError(f"Missing merge timestamp on {f}")

            if merged_at > before or merged_at < after:
                continue

            project = data["project"]
            id_number = data["number"]
            user = data["owner"]["username"]

            if not acceptable.project(project):
                print("Rejected project:", project, id_number)
                continue

            changes = 0
            touched_files = []
            for file_data in sorted(
                data["patchSets"], key=lambda x: x["number"]
            )[-1][
                "files"
            ]:  # type: Dict[str, Any]
                if not acceptable.file(project, file_data["file"]):
                    continue
                changes += int(file_data["insertions"]) + abs(
                    int(file_data["deletions"])
                )
                touched_files.append(file_data["file"])

            if changes < 10:
                print("Rejected for limited changes:", project, id_number)
                continue

            print(project, id_number, user)
            for f in touched_files:
                print(f"    {f}")

            changes_per_user[user].append(id_number)

        with open(os.path.join(args.dir, "commits.json"), "w") as outfile:
            outfile.write(json.dumps(changes_per_user, indent=4))

        return 0
