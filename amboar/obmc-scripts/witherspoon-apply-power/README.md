Description
===========

Avoids the chassis-on systemd target on Witherspoon. Instead, just whack the
appropriate GPIOs to switch the power on.

This script is *specific* to Witherspoon. Do not run it on other platforms.

Example
-------

```
$ ./apply-power root@my-witherspoon
```
