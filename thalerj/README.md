# openbmctool Documentation

This provides documentation beyond what is in the tool's help text.

## Connecting to a system

An IP address or hostname, password, and username are required for
connecting to a BMC.

These are passed in with the following options:
- `-H`: The hostname or IP for the BMC
- `-U`: The username
- `-P`: The password, provided in-line
- `-A`: Prompt for a password. Can be used in place of `-P`.

## Enabling and Disabling Local BMC User Accounts

The local user accounts on the BMC, such as root, can be disabled, queried,
and re-enabled with the 'local_users' sub-command.

Important:  After disabling local users, an LDAP user will need to be used
for further interaction with the BMC, including if using openbmctool to
enable local users again.

To view current local user account status:
```
openbmctool <connection options> local_users queryenabled

```

To disable all local user accounts:
```
openbmctool <connection options> local_users disableall

```

To re-enable all local user accounts:
```
openbmctool <connection options> local_users enableall

```
