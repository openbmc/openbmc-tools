#!/usr/bin/env python3

r"""
BMC FFDC will at times include the journal in json format
(journalctl -o json-pretty ).  This is a quick and dirty script which
will convert that json output into the standard journalctl output
"""

import datetime
import json
import re
from argparse import ArgumentParser
from datetime import timezone


def jpretty_to_python(buf):
    entries = []

    for entry in re.findall("^{$(.+?)^}$", buf, re.DOTALL | re.MULTILINE):
        entries += [json.loads("{{{}}}".format(entry))]

    return entries


def format_timestamp_utc(us_timestamp, use_utc=False):
    """Convert microseconds since epoch to formatted timestamp (with microseconds)."""
    ts = float(us_timestamp) / 1000000
    tz = timezone.utc if use_utc else None
    dt = datetime.datetime.fromtimestamp(ts, tz)
    return dt.strftime("%b %d %H:%M:%S.%f")


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument(
        "journalfile", metavar="FILE", help="the file to parse"
    )
    parser.add_argument(
        "--localtime",
        action="store_true",
        help="Display timestamps in local time (default is UTC)",
    )
    args = parser.parse_args()

    with open(args.journalfile) as fd:
        entries = jpretty_to_python(fd.read())
        entries = sorted(entries, key=lambda k: k["__REALTIME_TIMESTAMP"])

        for e in entries:
            e["ts"] = format_timestamp_utc(
                e["__REALTIME_TIMESTAMP"], use_utc=not args.localtime
            )
            try:
                print(
                    f'{e["ts"]} {e["_HOSTNAME"]} {e["SYSLOG_IDENTIFIER"]}:'
                    f' {e["MESSAGE"]}'
                )
            except Exception:
                print("Unable to parse msg: " + str(e))
                continue
