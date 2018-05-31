# The OpenBMC Tools Collection

The goal of this repository is to collect the two-minute hacks you write to
automate interactions with OpenBMC systems.

It's highly likely the scripts don't meet your needs - they could be
undocumented, dysfunctional or utterly broken. Please help us improve!

## Repository Rules

* _Always_ inspect what you will be executing
* Some hacking on your part is to be expected

## If you're still with us

Then this repository aims to be the default destination for your otherwise
un-homed scripts. As such we are setting the bar for submission pretty low,
and we aim to make the process as easy as possible:

## Sending patches

However you want to send patches, we will probably cope:

* Pull-requests [on github](https://github.com/openbmc/openbmc-tools)
* Patches [sent to the mailing list](https://lists.ozlabs.org/listinfo/openbmc)
* Through [Gerrit](https://gerrit.openbmc-project.xyz/)

Do note that you will need to be party to the [OpenBMC
CLA](https://github.com/openbmc/docs/blob/master/contributing.md#submitting-changes-via-gerrit-server)
before your contributions can be accepted.

## What we will do once we have your patches

So long as your patches look sane with a cursory glance you can expect them to
be applied. We may push back in the event that similar tools already exist or
there are egregious issues.

## What you must have in your patches

We don't ask for much, but you need to give us at least a
[Signed-off-by](https://developercertificate.org/), and put your work under the
Apache 2.0 license. Licensing everything under Apache 2.0 will just hurt our
heads less. Lets keep the lawyers off our backs. ^

^Any exceptions must be accompanied by a LICENSE file in the relevant
subdirectory, and be compatible with Apache 2.0. You thought you would get away
without any fine print?

## How you consume the repository

There's no standard way to install the scripts housed in the here, so adding
parts of the repository to your PATH might be a bit of a dice-roll. We may also
move or remove scripts from time to time as part of housekeeping. It's probably
best to copy things out if you need stability.
