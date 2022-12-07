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
            "analyze-reviews", help="Determine points for reviews"
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

            project = data["project"]
            id_number = data["number"]
            author = data["owner"]["username"]

            if not acceptable.project(project):
                print("Rejected project:", project, id_number)

            comments_per_user: Dict[str, int] = defaultdict(int)

            for patch_set in data["patchSets"]:
                created_on = data["createdOn"]

                if created_on > before or created_on < after:
                    continue

                if "comments" not in patch_set:
                    continue

                for comment in patch_set["comments"]:
                    reviewer = comment["reviewer"]["username"]

                    if reviewer == author:
                        continue
                    if not acceptable.file(project, comment["file"]):
                        continue

                    comments_per_user[reviewer] += 1

            print(project, id_number)
            for user, count in comments_per_user.items():
                if count < 3:
                    continue
                print("    ", user, count)
                changes_per_user[user].append(id_number)

        with open(os.path.join(args.dir, "reviews.json"), "w") as outfile:
            outfile.write(json.dumps(changes_per_user, indent=4))

        return 0
