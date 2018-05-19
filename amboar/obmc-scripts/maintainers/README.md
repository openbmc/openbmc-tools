A Collection of Python Tools to Manipulate MAINTAINERS Files
============================================================

OpenBMC defines its own style of MAINTAINERS file that is almost but not
entirely alike the Linux kernel's MAINTAINERS file.

Historically the MAINTAINERS file was kept in the openbmc/docs repository and
described the maintainers for all repositories under the OpenBMC Github
organisation. Due to its separation from the repositories it was describing,
openbmc/docs:MAINTAINERS was both incomplete and out-of-date.

These scripts were developed to resolve unmaintained state of MAINTAINERS by
distributing the information into each associated repository.

What's in the Box?
------------------

`maintainers.py` is the core library that handles parsing and assembling
MAINTAINERS files. An AST can be obtained with `parse_block()`, and the content
of a MAINTAINERS file can be obtained by passing an AST to `assemble_block()`

`split_maintainers.py` is the script used to split the monolithic MAINTAINERS
file in openbmc/docs into per-repository MAINTAINERS files and post the patches
to Gerrit.

`obmc-gerrit` is a helper script for pushing changes to Gerrit. For a
repository with an OpenBMC-compatible MAINTAINERS file at its top level,
`obmc-gerrit` will parse the MAINTAINERS file and mangle the `git push`
`REFSPEC` such that the maintainers and reviewers listed for the repository are
automatically added to the changes pushed:

```
$ ./obmc-gerrit push gerrit HEAD:refs/for/master
```
