# openbmctool Documentation

This provides documentation beyond what is in the tool\'s help text.

## Enabling and Disabling Local BMC User Accounts

The local user accounts on the BMC, such as root, can be disabled, queried,
and re-enabled with the 'local_users' sub-command.  Before disabling the
users, a non-local user would need to be created so that the BMC can still be
interacted with, including using openbmctool.

To view current local user account status:
```
openbmctool <connection options> local_users queryenabled

User: root  Enabled: 1
```

To disable all local user accounts:
```
openbmctool <connection options> local_users disableall

Disabling root
```

To re-enable all local user accounts:
```
openbmctool <connection options> local_users enableall

Enabling root
```
