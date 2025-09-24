# How we use apache

The authserver consists of two components:

- a C++ application which uses the PEP-protocol for administration, and has a built-in HTTP server for the OAuth Protocol
- Apache, which is configured as a reverse proxy in front of the HTTP-server of the C++ application, which handles the authentication.

We run these components in separate docker containers.

We currently support the following authentication sources in the apache container:

- SURFconext: this uses Shibboleth for the SAML protocol. It also requires the entity ID of the authserver to be set as follows in `001-login-options.conf`:

  ```plaintext
  <Location />
        ShibRequestSetting entityIDSelf "https://pep-demo.cs.ru.nl/shibboleth"
  </Location>
  ```

  `ShibRequestSetting` can unforunately only be used inside a `<Location>` block. In this example we set it for `/`, but it can also be set to a more specific location. Normally, SURFconext authentication uses the location `/login/surfconext` (see `shibboleth.conf`), so you can also set it to that. But using a less specific location makes sure it keeps working, also when the surfconext location changes, or more locations are added that use the same entityID.
- Google: This is based on OpenID Connect
- Test users: This is implemented entirely in Apache configuration, and a very small bit of javascript. It presents the user with buttons that immediately log the user in as a certain user. This is only for testing purposes, and should not be enabled in production. If, however, it is mistakenly enabled on production, this will not immediately grant access to anyone, because the test users are not added to the production authserver using `pepcli user`.

All of these authsources are configured in core. They are only enabled if the corresponding `Define` is defined. This is done in the file `001-login-options.conf`. This file can be overwritten in project configurations, in order to customize the enabled authentication sources.

A screen where the user can select an authentication source is implemented using [server side includes](https://httpd.apache.org/docs/current/howto/ssi.html). This is called the *login-picker*. In `ClientConfig.json` you can either point to this login-picker, or directly to a single authentication source, if only one authentication source is used in an environment. That authentication source must still be enabled in `001-login-options.conf`.
