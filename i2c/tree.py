#!/bin/python

import re
import sys

from anytree import Node, RenderTree

usage = """
Usage: using local file or stdin to parse i2c bus info to tree
    1. ssh user@<ip> "i2cdetect -l" > i2c_bus_info.txt
    2. python tree.py i2c_bus_info.txt

    or

    ssh user@<ip> "i2cdetect -l" | python tree.py
"""

node_dict = {}
root = Node("root")


def parse_line(line):
    line = line.strip()
    regex = re.compile(r"(i2c-\d+)\s+i2c\s+(.*)\s+I2C adapter")
    m = regex.match(line)
    if m:
        bus = m.group(1)
        name = m.group(2).strip()
        return (bus, name)
    else:
        return None


def draw(line):
    i2c = parse_line(line)
    if not i2c:
        return

    bus, name = i2c

    if "mux" not in name:
        node_dict[bus] = Node(bus, parent=root)
    else:
        mux_regex = re.compile(r"i2c-(\d+)-mux \(chan_id (\d+)\)")
        m = mux_regex.match(name)
        if m:
            i2c = m.group(1)
            if "i2c-" + i2c in node_dict:
                node_dict[bus] = Node(bus, parent=node_dict["i2c-" + i2c])


def parse_from_file(file):
    with open(file, "r") as f:
        lines = f.readlines()
        for line in lines:
            draw(line)


def parse_from_stdin():
    lines = sys.stdin.readlines()
    for line in lines:
        draw(line)


def print_tree():
    for pre, fill, node in RenderTree(root):
        print("%s%s" % (pre, node.name))


if __name__ == "__main__":
    if len(sys.argv) == 2:
        if (
            sys.argv[1] == "-h"
            or sys.argv[1] == "--help"
            or sys.argv[1] == "help"
            or sys.argv[1] == "?"
        ):
            print(usage)
            sys.exit(0)

    try:
        if len(sys.argv) == 2:
            file = sys.argv[1]
            parse_from_file(file)
        else:
            parse_from_stdin()
    except Exception as e:
        print(e)
        print(usage)
        sys.exit(1)

    print_tree()
