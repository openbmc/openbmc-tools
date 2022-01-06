#!/usr/bin/python3

import argparse
from importlib import import_module
from typing import List

subcommands = ["dump-gerrit"]


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

    commands = []
    for c in subcommands:
        commands.append(
            import_module("libvoters.subcmd." + c).subcmd(subparser)  # type: ignore
        )

    args = parser.parse_args()

    if "cmd" not in args:
        print("Missing subcommand!")
        return 1

    return int(args.cmd.run(args))
