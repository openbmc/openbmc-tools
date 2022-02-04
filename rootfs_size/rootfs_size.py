#!/usr/bin/python3

import subprocess
import tempfile
import os
from os.path import join, getsize
import argparse
from multiprocessing import Pool, cpu_count
import shutil
import sys

# Set command line arguments
parser = argparse.ArgumentParser(
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("-b", "--build_dir",
                    dest="BUILD_DIR",
                    default="/home/ed/openbmc-openbmc",
                    help="Build directory path.")

parser.add_argument("-s", "--squashfs_file",
                    dest="SQUASHFS_FILE",
                    default="/build/tmp/deploy/images/wolfpass" +
                    "/intel-platforms-wolfpass.squashfs-xz",
                    help="Squashfs file.")

parser.add_argument("-t", "--threads",
                    dest="threads",
                    default=0,
                    type=int,
                    help="Number of threads to use (defaults to cpu count)")

args = parser.parse_args()

# files below this size wont be attempted
FILE_SIZE_LIMIT = 0

SQUASHFS = args.SQUASHFS_FILE
if not os.path.isabs(args.SQUASHFS_FILE):
    SQUASHFS = args.BUILD_DIR + args.SQUASHFS_FILE

original_size = getsize(SQUASHFS)
print("squashfs size: {}".format(original_size))

results = []


def get_unsquash_results(filepath):
    with tempfile.TemporaryDirectory() as newsquashfsroot:
        input_path = os.path.join(newsquashfsroot, "input")
        shutil.copytree(squashfsdir, input_path, symlinks=True,
                        ignore_dangling_symlinks=True)
        file_to_remove = os.path.join(input_path, filepath)
        try:
            os.remove(file_to_remove)
        except IsADirectoryError:
            shutil.rmtree(file_to_remove)
        subprocess.check_output(
            ["mksquashfs", input_path,
             newsquashfsroot + "/test", "-comp", "xz", '-processors', '1'])

        return ((filepath.replace(squashfsdir, ""),
                 original_size -
                 getsize(newsquashfsroot + "/test")))


with tempfile.TemporaryDirectory() as tempsquashfsdir:
    print("writing to " + tempsquashfsdir)
    squashfsdir = os.path.join(tempsquashfsdir, "squashfs-root")
    #squashfsdir = os.path.join("/tmp", "squashfs-root")
    command = ["unsquashfs", "-d", squashfsdir, SQUASHFS]
    print(" ".join(command))
    subprocess.check_call(command)

    files_to_test = []
    for root, dirs, files in os.walk(squashfsdir):
        for name in files + dirs:
            filepath = os.path.join(root, name)
            if not os.path.islink(filepath):
                if getsize(filepath) > FILE_SIZE_LIMIT:
                    files_to_test.append(
                        os.path.relpath(filepath, squashfsdir))

    print("{} files to attempt removing".format(len(files_to_test)))

    threads = args.threads;
    if threads <= 0:
      threads = int(cpu_count())
    print("Using {} threads".format(threads))
    with Pool(threads) as p:
        for i, res in enumerate(p.imap_unordered(get_unsquash_results, files_to_test)):
            results.append(res)
            sys.stderr.write('\rdone {:.1f}%'.format(
                100 * (i/len(files_to_test))))

results.sort(key=lambda x: x[1], reverse=True)

with open("results.txt", 'w') as result_file:
    for filepath, size in results:
        result = "{:>10}: {}".format(size, filepath)
        print(result)
        result_file.write(result + "\n")
