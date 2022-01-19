#!/usr/bin/python3

import argparse
from importlib import import_module
from typing import List

def main() -> int:
    parser = argparse.ArgumentParser(description="Obtain TOF voter metrics")
    parser.add_argument(
        "--data-directory",
        "-d",
        help="Data directory (default 'data')",
        dest="dir",
        default="data",
    )

    subparser = parser.add_subparsers(help="Available subcommands")

    commands = [
        import_module("libvoters.subcmd.analyze-commits"),
        import_module("libvoters.subcmd.analyze-reviews"),
        import_module("libvoters.subcmd.dump-gerrit"),
        import_module("libvoters.subcmd.report"),
    ]
    commands = [x.subcmd(subparser) for x in commands] # type: ignore

    args = parser.parse_args()

    if "cmd" not in args:
        print("Missing subcommand!")
        return 1

    return int(args.cmd.run(args))
