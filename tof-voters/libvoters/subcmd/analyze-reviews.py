#!/usr/bin/python3

import argparse
import json
import os
import re
from collections import defaultdict
from typing import Dict

import libvoters.acceptable as acceptable
from libvoters import (
    UserChanges,
    UserComments,
    changes_factory,
    comments_factory,
)
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

        changes_per_user: Dict[str, UserChanges] = defaultdict(changes_factory)

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

            comments_per_user: Dict[str, UserComments] = defaultdict(
                comments_factory
            )

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

                    user = comments_per_user[reviewer]
                    user["name"] = comment["reviewer"]["name"]
                    # We actually have a case where a reviewer does not have an email recorded[1]:
                    #
                    # [1]: https://gerrit.openbmc.org/c/openbmc/phosphor-pid-control/+/60303/comment/ceff60b9_9d2debe0/
                    #
                    # {"file": "conf.hpp",
                    #  "line": 39,
                    #  "reviewer": {"name": "Akshat Jain", "username": "AkshatZen"},
                    #  "message": "If we design SensorInput as base class and have derived ..."}
                    # Traceback (most recent call last):
                    #   File "/mnt/host/andrew/home/andrew/src/openbmc/openbmc-tools/tof-voters/./voters", line 7, in <module>
                    #     sys.exit(main())
                    #              ^^^^^^
                    #   File "/mnt/host/andrew/home/andrew/src/openbmc/openbmc-tools/tof-voters/libvoters/entry_point.py", line 33, in main
                    #     return int(args.cmd.run(args))
                    #                ^^^^^^^^^^^^^^^^^^
                    #   File "/mnt/host/andrew/home/andrew/src/openbmc/openbmc-tools/tof-voters/libvoters/subcmd/analyze-reviews.py", line 82, in run
                    #     user["email"] = comment["reviewer"]["email"]
                    #                     ~~~~~~~~~~~~~~~~~~~^^^^^^^^^
                    # KeyError: 'email'
                    if "email" in comment["reviewer"]:
                        user["email"] = comment["reviewer"]["email"]
                    user["comments"] += 1

            print(project, id_number)
            for username, review in comments_per_user.items():
                if review["comments"] < 3:
                    continue
                print("    ", user, review["comments"])
                user = changes_per_user[username]
                user["name"] = review["name"]
                user["email"] = review["email"]
                user["changes"].append(id_number)

        with open(os.path.join(args.dir, "reviews.json"), "w") as outfile:
            outfile.write(json.dumps(changes_per_user, indent=4))

        return 0
