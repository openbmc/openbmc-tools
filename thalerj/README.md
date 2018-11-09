## BMC LDAP Configuration

In BMC LDAP is used for remote authentication, BMC doesn't support remote user management functionality.

BMC supports secure/non secure LDAP configuration.

### Create LDAP Configuration

#### NonSecure
```
openbmctool.py <connection options> ldap enable --uri="ldap://<ldap server IP/hostname>" --bindDN=<bindDN> --baseDN=<basDN> --bindPassword=<bindPassword> --scope="sub/one/base" --serverType="OpenLDAP/ActiveDirectory"

```
NOTE: configuring FQDN(Fully qualified domain name/ hostname) in the "uri" parameter
requires that DNS server should be configured on the BMC.

How to configure the DNS server on the BMC
NOTE: Currently openbmctool doesn't have support for this.

#### Secure
```
openbmctool.py <connection options> ldap enable --uri="ldaps://<ldap server IP/hostname>" --bindDN=<bindDN> --baseDN=<basDN> --bindPassword=<bindPassword> --scope="sub/one/base" --serverType="OpenLDAP/ActiveDirectory"

```
NOTE: Expected Error

a) xyz.openbmc_project.Common.Error.NoCACertificate

BMC as a client need to  verify the server's (eg LDAP server) certificate as the same has been signed by a CA which the BMC is not aware of.
The service action would be for the admin to upload the CA certificate on the BMC.

How to upload the CA certificate on the BMC: refer section "Update LDAP root certificate"

### Delete/Erase LDAP Configuration
```
openbmctool.py <connection options> ldap disable

```

### Add privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper create --groupName=<groupName> --privilege="priv-admin/priv-user"

```

### Delete privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper delete --group_name=<groupName>
```

### List privilege mapping

```
openbmctool.py <connection options> ldap privilege-mapper list
```

Normal workflow for LDAP configuration would be as below

1) Configure the DNS server
2) Configure LDAP
   a) Configure CA certificate if secure LDAP server is being configured.
   b) Create LDAP Configuration with local user.
3) Configure user privilege.

## Note:

a) If user tries to login with LDAP credentials and have not added the privilege
mapping for the LDAP credentials then user will get the following
http error code and message.

403, 'LDAP group privilege mapping does not exist'.

Action: Add the privilege(refer section "Add privilege mapping")


b) If any time user gets the following error message "Insufficient Privilege"
It may happen that on BMC required privileges is not there for the user.

Action: Add the privilege(refer section "Add privilege mapping") with
privilege=priv-admin

c) Once LDAP is setup, connection options of openbmctool works with both LDAP
and local users.
