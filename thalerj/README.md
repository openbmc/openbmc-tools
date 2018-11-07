## BMC Certificate management

Certificate management allows to replace the existing certificate and private
key file with another (possibly certification Authority (CA) signed)
certificate and private key file. Certificate management allows the user to
install server, client and root certificates.

### Update Https server certificate.
```
openbmctool <connection options> certificate update server https -f <File>
```
File: The PEM file containing both certificate and private key

### Update LDAP client certificate.
```
openbmctool <connection options> certificate update client ldap -f <File>
```
File: The PEM file containing both certificate and private key.

### Update LDAP root certificate.
```
openbmctool <connection options> certificate update authority ldap -f <File>
```
File: The PEM file containing only certificate.


### Delete Https server certificate.
```
openbmctool <connection options> certificate delete server https
```
Deleting a certificate will create a new self signed certificate and will
install the same.

### Delete LDAP client certificate.
```
openbmctool <connection options> certificate delete client ldap
```

### Delete LDAP root certificate.
```
openbmctool <connection options> certificate delete authority ldap
```
Deleting root certificate can cause LDAP service outage. Please refer LDAP
documentition before using this command.
