# BMC Certificate management

Certificate management allows to replace the existing certificate and private
key file with another (possibly certification Authority (CA) signed)
certificate and private key file. Certificate management allows the user to
install server, client and root certificates.

### Update Https server certificate.
```
openbmctool <connection options> certificate update server https -f <File>
File : Certificate and Private Key file in .PEM format.
```

### Update LDAP client certificate.
```
openbmctool <connection options> certificate update client ldap -f <File>
File : Certificate and Private Key file in .PEM format.
```

### Update LDAP root certificate.
```
openbmctool <connection options> certificate update authority ldap -f <File>
File : Certificate file in .PEM format.
```

### Delete Https server certificate.
```
openbmctool <connection options> certificate delete server https
```

### Delete LDAP client certificate.
```
openbmctool <connection options> certificate delete client ldap
```

### Dlete LDAP root certificate.
```
openbmctool <connection options> certificate delete authority ldap
Note: This can cause LDAP service outage. Please refer LDAP documentition before
      using this command.
```
