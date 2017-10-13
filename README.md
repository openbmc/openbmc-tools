# The OpenBMC Tools Collection

The goal of this repository is to collect the two-minute hacks you write to
automate interactions with OpenBMC systems.

It's highly likely the scripts don't meet your needs - they could be
undocumented, dysfunctional, utterly broken, or sometimes casually `rm -rf ~`.
Don't even think about looking for tests.

## Repository Rules

* _Always_ inspect what you will be executing
* Some hacking on your part is to be expected

You have been warned.

## If you're still with us

Then this repository aims to be the default destination for your otherwise
un-homed scripts. As such we are setting the bar for submission pretty low,
and we aim to make the process as easy as possible:

## Sending patches

However you want to send patches, we will probably cope:

* Pull-requests [on github](https://github.com/openbmc/openbmc-tools)
* Patches [sent to the mailing list](https://lists.ozlabs.org/listinfo/openbmc)
* If you really must [use Gerrit](https://gerrit.openbmc-project.xyz/) we will
  figure that out too

## What we will do once we have your patches

Look, the `rm -rf ~` thing was a joke, we will be keeping an eye on all of you
for such shenanigans. But so long as your patches look sane with a cursory
glance you can expect them to be applied. To be honest, even Perl will be
considered moderately sane.

## What you must have in your patches

We don't ask for much, but you need to give us at least a
[Signed-off-by](https://developercertificate.org/), and put your work under the
Apache 2.0 license. Licensing everything under Apache 2.0 will just hurt our
heads less. Lets keep the lawyers off our backs. ^

^ Any exceptions must be accompanied by a LICENSE file in the relevant
subdirectory, and be compatible with Apache 2.0. You thought you would get away
without any fine print?

## How you consume the repository

Probably with difficulty. Don't expect the layout to remain static, or scripts
to continue to exist from one commit to the next.
