## BMC LDAP Configuration

In BMC LDAP is used for remote authentication,BMC doesn't support remote user management functionality.

BMC supports secure/non secure ldap configuration.

### Create LDAP Configuration

#### NonSecure
```
openbmctool.py <connection options> ldap enable --uri="ldap://<ldap server IP/hostname>" --bindDN="" --baseDN="" --bindPassword="" --scope="sub/one/base" --serverType="OpenLDAP/ActiveDirectory"

```
NOTE:- configuring FQDN(Fully qualiflied domain name/ hostname) in "uri" parameter
requires that DNS server should be configured on the BMC.

How to configure the DNS server on the BMC
NOTE:- Currently openbmctool doesn't have support for this.

#### Secure
```
openbmctool.py -H <connection options> ldap enable --uri="ldaps://<ldap server IP/hostname>" --bindDN="" --baseDN="" --bindPassword="" --scope="sub/one/base" --serverType="OpenLDAP/ActiveDirectory"

```
NOTE:- Expected Error

a) xyz.openbmc_project.Common.Error.NoCACertificate

BMC as a client need to  verify the server's (ed LDAP server) certificate as the same has been signed by a CA which the BMC is not aware of.
The service action would be for the admin to upload the CA certificate on the BMC.

How to upload the CA certificate on the BMC: refer section "Update LDAP root certificate"

### Delete/Erase LDAP Configuration
```
openbmctool.py <connection options> ldap disable

```

NOTE:-
If any time user gets the following error message "Insufficient Privilege"
It may happen that on BMC privilege mapping is not there for the user.
Following command would be useful to add the privilege mapping.

### Add privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper create --groupName="Domain Users" --privilege="priv-admin"

```

### Delete privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper delete --group_name="Domain Users"
```

### List privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper list
```

Normal workflow for ldap would be as below

1) Configure the DNS server
2) Configure LDAP
   a) Configure CA certificate if secure LDAP server is being configured.
   b) Create LDAP Configuration.
3) Configure user privilege.
