import subprocess
import tempfile
import os
from os.path import join, getsize

# TODO
# - Make build directory an input parameter
# - Make squashfs file an input parameter
# - Fix 80 char violations and run through pep8

BUILD_DIR = "/home/ed/openbmc-openbmc"
# files below this size wont be attempted
FILE_SIZE_LIMIT = 0

SQUASHFS = BUILD_DIR + \
    "/build/tmp/deploy/images/wolfpass/intel-platforms-wolfpass.squashfs-xz"

original_size = getsize(SQUASHFS)
print("squashfs size: ".format(original_size))

results = []

with tempfile.TemporaryDirectory() as tempremovedfile:
    with tempfile.TemporaryDirectory() as tempsquashfsdir:
        print("writing to " + tempsquashfsdir)
        command = ["unsquashfs", "-d",
                   os.path.join(tempsquashfsdir, "squashfs-root"),  SQUASHFS]
        print(" ".join(command))
        subprocess.check_call(command)
        squashfsdir = tempsquashfsdir + "/squashfs-root"

        files_to_test = []
        for root, dirs, files in os.walk(squashfsdir):
            for name in files + dirs:
                filepath = os.path.join(root, name)
                if not os.path.islink(filepath):
                    # ensure files/dirs can be renamed
                    os.chmod(filepath,0o711)
                    if getsize(filepath) > FILE_SIZE_LIMIT:
                        files_to_test.append(filepath)

        print("{} files to attempt removing".format(len(files_to_test)))

        for filepath in files_to_test:
            name = os.path.basename(filepath)
            newname = os.path.join(tempremovedfile, name)
            os.rename(filepath, newname)
            with tempfile.TemporaryDirectory() as newsquashfsroot:
                subprocess.check_output(
                    ["mksquashfs", tempsquashfsdir, newsquashfsroot + "/test", "-comp", "xz"])

                results.append((filepath.replace(squashfsdir, ""),
                                original_size - getsize(newsquashfsroot + "/test")))
            os.rename(newname, filepath)

            print("{:>6} of {}".format(len(results), len(files_to_test)))

results.sort(key=lambda x: x[1], reverse=True)

with open("results.txt", 'w') as result_file:
    for filepath, size in results:
        result = "{:>10}: {}".format(size, filepath)
        print(result)
        result_file.write(result + "\n")
