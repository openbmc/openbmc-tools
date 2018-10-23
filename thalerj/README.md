# openbmctool Documentation

This provides documentation beyond what is in the tool\'s help text.

## Enabling and Disabling Local BMC User Accounts

The local user accounts on the BMC, such as root, can be disabled, queried,
and re-enabled with the 'local_users' sub-command.

Important:  After disabling local users, an LDAP user will need to be used
for further iteraction with the BMC, including if using openbmctool to
enable local users again.

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
