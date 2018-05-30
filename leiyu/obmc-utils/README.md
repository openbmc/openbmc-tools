# openbmc-utils

* `upload_and_update.py`
A tool to upload a tarball to TFTP server and update BMC with it.
**Note**: It uses legacy methods to update BMC so it may break in future when the code update is refactored.

* `update-bmc.py`
A tool to update BMC with phosphor-software-manager.
This tool should support both ubifs and non-ubifs flash layout.
**Note**: It depends on https://gerrit.openbmc-project.xyz/#/q/topic:obmc-non-ubi
