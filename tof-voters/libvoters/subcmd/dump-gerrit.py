#!/usr/bin/python3

import argparse
import json
import os
from sh import ssh  # type: ignore


class subcmd:
    def __init__(self, parser: argparse._SubParsersAction) -> None:
        p = parser.add_parser(
            "dump-gerrit", help="Dump commit data from Gerrit"
        )
        p.add_argument(
            "--server",
            "-s",
            help="Gerrit server SSH alias (default=openbmc.gerrit)",
            default="openbmc.gerrit",
        )
        p.add_argument(
            "--after",
            "-a",
            help="Timestamp for Gerrit 'after:' directive (ex. YYYY-MM-DD)",
            required=True,
        )
        p.set_defaults(cmd=self)

    def run(self, args: argparse.Namespace) -> int:
        data_path: str = args.dir

        if os.path.exists(data_path) and not os.path.isdir(data_path):
            print(f"Path {data_path} exists but is not a directory.")
            return 1

        if not os.path.exists(data_path):
            os.mkdir(data_path)

        query = list(
            ssh(
                args.server,
                "gerrit",
                "query",
                "--format=json",
                "--patch-sets",
                "--comments",
                "--files",
                "--no-limit",
                f"after:{args.after} AND delta:>=10",
            )
        )[
            0:-1
        ]  # The last result from Gerrit is a query stat result.

        for change in query:
            data = json.loads(change)
            formatted_data = json.dumps(data, indent=4)

            with open(
                os.path.join(data_path, str(data["number"]) + ".json"),
                "w",
            ) as file:
                file.write(formatted_data)
        return 0
