#!/usr/bin/python3

import logging
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import urllib.request
from argparse import ArgumentParser

logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger("debug_dump")

ipks_dir = ""


def sig_handler(signum, frame):
    pass


signal.signal(signal.SIGINT, sig_handler)

# Check required envs
ipks_url_template = os.getenv("IPKS_URL_TEMPLATE")
release_revision_regex_template = os.getenv("RELEASE_REVISION_REGEX_TEMPLATE")

if (
    ipks_url_template is None
    or "{BUILD_TYPE}" not in ipks_url_template
    or "{MACHINE}" not in ipks_url_template
    or "{REVISION}" not in ipks_url_template
):
    logger.error(
        "IPKS_URL_TEMPLATE containing {BUILD_TYPE}, {MACHINE}, and {REVISION} is required"
    )
    exit(1)

if release_revision_regex_template is None:
    logger.error(
        "RELEASE_REVISION_REGEX_TEMPLATE is reuiqred to check if a revision string is dev or release"
    )
    exit(1)


def fetch(url, local_file):
    """Fetch the URL and save to local"""
    logger.info("Downloading from {} to {}".format(url, local_file))
    urllib.request.urlretrieve(url, local_file)


def extract(tar_file, target_dir):
    """Extra file to target dir, return the dir name in tar"""
    import tarfile

    with tarfile.open(tar_file) as f:
        f.extractall(target_dir)
        return os.path.commonprefix(f.getnames())


class dump_env:
    """Create a temp dir to process the dump"""

    def __init__(self, path, revision, machine):
        self.path = path
        self.revision = revision
        self.machine = machine
        self.wd = tempfile.mkdtemp(prefix="ipkdbg_")
        self.dump_tar = "{}/dump.tar.gz".format(self.wd)
        logger.debug("Processing dump in {}".format(self.wd))

        # 1. fetch the dump
        self.fetch()

        # 2. handle the dump, fetch and unarchive and parse
        self.handle_coredump(self.path)

        if self.revision == "" or self.machine == "":
            logger.error(
                "The revision or machine is empty, auto fill is only available for tarball dump"
            )
            sys.exit(1)

        # 3. process the revision
        self.process_revision()

        # 4. check if coredump file is found
        if self.coredump_file is None:
            logger.warning("Unable to find coredump file, exit.")
            sys.exit(1)

        # 5. get the core execfn
        self.get_core_execfn()

        # 6. fetch ipks
        self.fetch_ipks()

    def __del__(self):
        logger.debug("Removing {}".format(self.wd))
        shutil.rmtree(self.wd)

    def process_revision(self):
        """Extract the git revision from the revision string.
        e.g. return a846f85d4 for 2.12.0-dev-1341-ga846f85d4
        or release tag if it's found as a release tag
        """
        import re

        x = re.match(
            r"[0-9]+\.[0-9]+\.[0-9]+-dev-[0-9]+-g([a-z0-9]+)", self.revision
        )
        if x is not None and x.group(1) is not None:
            self.revision = x.group(1)
            self.build_type = "dev"
        elif re.match(release_revision_regex_template, self.revision):
            self.build_type = "release"
        else:
            raise Exception("Invalid revision: {}".format(self.revision))

        logger.info(
            "Extracted {} revision: {}".format(self.build_type, self.revision)
        )

    def extract_dump(self):
        """Extra and return the coredump file"""
        self.dir_in_tar = extract(self.dump_tar, self.wd)
        logger.info("Found dump dir {}".format(self.dir_in_tar))

    def handle_coredump_file(self, path):
        if not os.path.exists(path):
            raise Exception("Coredump file not found: {}".format(path))

        # Check is a real coredump file
        cmd = "file {}".format(path)
        if "core file" not in os.popen(cmd).read():
            raise Exception("Not a coredump file: {}".format(path))

        shutil.copy(path, self.wd + "/core")
        self.coredump_file = self.wd + "/core"
        logger.info("Found coredump {}".format(self.coredump_file))

    def handle_coredump_zst(self, path):
        if not os.path.exists(path):
            raise Exception("Coredump zst not found: {}".format(path))

        cmd = "file {}".format(path)
        if "Zstandard compressed data" not in os.popen(cmd).read():
            raise Exception("Not a coredump zst file: {}".format(path))

        os.system("zstd -d {} -o {}".format(path, self.wd + "/core"))
        self.coredump_file = self.wd + "/core"
        logger.info("Found coredump {}".format(self.coredump_file))

    def handle_os_release(self, path):
        if not os.path.exists(path):
            raise Exception("os-release not found: {}".format(path))
        # get machine and revision
        # e.g.
        # OPENBMC_TARGET_MACHINE="xxxx"
        # EXTENDED_VERSION="xxx.xxx.xxx"
        with open(path, "r") as f:
            for line in f:
                if "OPENBMC_TARGET_MACHINE=" in line:
                    self.machine = line.split("=")[1].strip('\n"')
                    logger.info("Found machine {}".format(self.machine))
                elif "VERSION_ID=" in line:
                    self.revision = line.split("=")[1].strip('\n"')
                    logger.info("Found revision {}".format(self.revision))

    def handle_coredump_tar(self, path):
        if not os.path.exists(path):
            raise Exception("Dump tar not found: {}".format(path))

        cmd = "file {}".format(path)
        if "gzip compressed data" not in os.popen(cmd).read():
            raise Exception("Not a tar.gz file: {}".format(path))

        self.extract_dump()
        dir = os.path.join(self.wd, self.dir_in_tar)
        # get coredump zst file
        for filename in os.scandir(dir):
            if filename.is_file():
                logger.debug(filename.path)
                f = os.path.basename(filename)
                if f.startswith("core.") and f.endswith(".zst"):
                    self.handle_coredump_zst(filename.path)
                elif f == "os-release":
                    self.handle_os_release(filename.path)

    def handle_coredump(self, path):
        cmd = "file {}".format(path)
        output = os.popen(cmd).read()
        if "core file" in output:
            self.handle_coredump_file(path)
        elif "Zstandard compressed data" in output:
            self.handle_coredump_zst(path)
        elif "gzip compressed data" in output:
            self.handle_coredump_tar(path)
        else:
            raise Exception("Unknown coredump file: {}".format(path))

    def fetch(self):
        if self.path.startswith("http"):
            fetch(self.path, self.dump_tar)
            self.path = self.dump_tar
        elif not os.path.exists(self.path):
            # local file do nothing
            raise Exception("Dump file not found: {}".format(self.path))

    def fetch_ipks(self):
        """Download the ipks and serve as local http server"""
        ipk_url = ipks_url_template.format(
            BUILD_TYPE=self.build_type,
            MACHINE=self.machine,
            REVISION=self.revision,
        )
        local_tar = "{}/ipks.tar".format(self.wd)
        fetch(ipk_url, local_tar)
        dir_in_tar = extract(local_tar, self.wd)
        global ipks_dir
        ipks_dir = "{}/{}".format(self.wd, dir_in_tar)
        logger.info("Prepared ipks in {}".format(ipks_dir))

    def get_core_execfn(self):
        """Get the core file's execfn"""
        cmd = "file {}".format(self.coredump_file)
        logger.debug(cmd)
        output = subprocess.check_output(cmd, shell=True)

        """output e.g.
        core.mapperx.0.b6ed0f5bfb444d93874e94f2742b3ffb.307.1672993867000000: ELF 32-bit LSB core file, ARM, version 1 (SYSV), SVR4-style, from '/usr/bin/mapperx', real uid: 0, effective uid: 0, real gid: 0, effective gid: 0, execfn: '/usr/bin/mapperx', platform: 'v7l'
        """
        import re

        x = re.search("execfn: '(.*)',", output.decode("utf-8"))
        if x is not None and x.group(1) is not None:
            self.core_execfn = x.group(1)
            logger.info("Found core execfn {}".format(self.core_execfn))
            return
        raise Exception(
            "No core execfn found in {}".format(self.coredump_file)
        )

    def debug(self, f, packages):
        logger.info(
            "BMC revision: {}, to start gdb on {}{}".format(
                self.revision,
                f,
                (
                    " with packages {}".format(packages)
                    if packages is not None
                    else ""
                ),
            )
        )
        # Prepare env, e.g.
        # IPKDBG_CONF_LOC=OpenBMC/debug/ipkdbg
        # IPKDBG_CONF_HOST=sysimage.byted.org
        # IPKDBG_CONF_MNT=firmware
        # IPKDBG_CONF_ROOT=dev/g220b
        run_env = os.environ.copy()
        run_env["IPKDBG_CONF_LOC"] = "OpenBMC/debug/ipkdbg"
        run_env["IPKDBG_CONF_HOST"] = "sysimage.byted.org"
        run_env["IPKDBG_CONF_MNT"] = "firmware"
        run_env["IPKDBG_CONF_ROOT"] = "{}/{}".format(
            self.build_type, self.machine
        )

        run_env = os.environ.copy()
        run_env["IPKDBG_SERVER_OVERRIDE"] = "file://{}".format(ipks_dir)

        # Example ./ipkdbg feaf5f01cd /usr/bin/swampd /path/to/core.swampd phosphor-pid-control phosphor-pid-control-dbg
        try:
            subprocess.run(
                ["./ipkdbg", self.revision, f, self.coredump_file]
                + (packages if packages else []),
                env=run_env,
            )
            logger.info("ipkdbg exit")
        except KeyboardInterrupt:
            logger.warning("Interrupted")
        except Exception as ex:
            logger.warning("Exception received: {}".format(ex))


if __name__ == "__main__":
    # A wrapper for ipkdbg to use it in Bytedance env
    parser = ArgumentParser(description="Debug BMC dump")
    parser.add_argument(
        "-u",
        "--path",
        dest="path",
        help="The path of the dump file, supported: http/https, local tarball, local zst, local coredump",
        required=True,
    )
    parser.add_argument(
        "-r",
        "--revision",
        dest="revision",
        help="The revision of the build",
        required=False,
        default="",
    )
    parser.add_argument(
        "-f",
        "--file",
        dest="file",
        help="The full path of the program file to debug",
        required=False,
    )
    parser.add_argument(
        "-p",
        "--packages",
        dest="packages",
        nargs="*",
        help="The list of ipk packages for debugging",
    )
    parser.add_argument(
        "-m",
        "--machine",
        dest="machine",
        help="The machine name, e.g. g220b",
        required=False,
        default="",
    )
    args = vars(parser.parse_args())

    print(args)

    env = dump_env(args["path"], args["revision"], args["machine"])

    env.debug(
        args["file"] if args["file"] is not None else env.core_execfn,
        args["packages"],
    )
