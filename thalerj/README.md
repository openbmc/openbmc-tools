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

## Remote logging via rsyslog

The BMC has the ability to stream out local logs (that go to the systemd journal)
via [rsyslog](https://www.rsyslog.com/).

The BMC will send everything. Any kind of filtering and appropriate storage will
have to be managed on the rsyslog server. Various examples are available on the
internet. Here are few pointers:
https://www.rsyslog.com/storing-and-forwarding-remote-messages/
https://www.rsyslog.com/doc/rsyslog%255Fconf%255Ffilter.html
https://www.thegeekdiary.com/understanding-rsyslog-filter-options/

### Configuring rsyslog server for remote logging

```
openbmctool <connection options> logging remote_logging_config -a <IP address> -p <port>
```

The IP address and port to be provided are of the remote rsyslog server.
Once this command is run, the remote rsyslog server will start receiving logs
from the BMC.

Hostname can be specified instead of IP address, if DNS is configured on the BMC.

### Disabling remote logging

```
openbmctool <connection options> logging remote_logging disable
```

It is recommended to disable remote logging before switching remote logging from
an existing remote server to a new one (i.e before re-running the remote_logging_config
option).

### Querying remote logging config

```
openbmctool <connection options> logging remote_logging view
```

This will print out the configured remote rsyslog server's IP address and port,
in JSON format.

## BMC Certificate management

Certificate management allows replacing the existing certificate and private
key file with another (possibly certification Authority (CA) signed)
certificate and private key file. Certificate management allows the user to
install server, client and root certificates.

### Update HTTPS server certificate
```
openbmctool <connection options> certificate update server https -f <File>
```
File: The [PEM](https://en.wikipedia.org/wiki/Privacy-Enhanced_Mail) file
      containing both certificate and private key.

### Update LDAP client certificate
```
openbmctool <connection options> certificate update client ldap -f <File>
```
File: The PEM file containing both certificate and private key.

### Update LDAP root certificate
```
openbmctool <connection options> certificate update authority ldap -f <File>
```
File: The PEM file containing only certificate.


### Delete HTTPS server certificate
```
openbmctool <connection options> certificate delete server https
```
Deleting a certificate will create a new self signed certificate and will
install the same.

### Delete LDAP client certificate
```
openbmctool <connection options> certificate delete client ldap
```

### Delete LDAP root certificate
```
openbmctool <connection options> certificate delete authority ldap
```
Deleting the root certificate can cause an LDAP service outage. Please refer to
the LDAP documentation before using this command.
