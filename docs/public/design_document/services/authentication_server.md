# Authentication Server

## OAuth2 Authorization Code Flow

The applications `pepAssessor` and `pepLogon` use the _Authorization Code Flow_ of OAuth2 ([RFC6749](https://www.rfc-editor.org/rfc/rfc6749))
, with PKCE ([RFC7636](https://datatracker.ietf.org/doc/html/rfc7636)) to communicate with the authserver. This is described in the following sequence diagram:

```mermaid
sequenceDiagram
    box User device
        participant PEP Client
        participant Browser
    end
    participant Authserver
    participant Keyserver

    PEP Client->>PEP Client:Generate random Code Verifier <br />and Code Challenge = sha256(code verifier)

    PEP Client->>Browser:Open https://auth.example.com/auth?<br />client_id=123&redirect_uri=http://127.0.0.1:16515/&<br />code_challenge=<<code_challenge>>&code_challenge_method=S256<br />&response_type=code

    Browser->>Authserver:GET https://auth.example.com/auth?<br />client_id=123&redirect_uri=http://127.0.0.1:16515/&<br />code_challenge=<<code_challenge>>&code_challenge_method=S256&<br />&response_type=code

    Note over Browser, Authserver: Authorize the user, e.g. via SURFconext

    Authserver-->>Browser:Redirect to http://127.0.0.1:16515/?code=<<Authorization code>>
    Browser->>PEP Client:GET http://127.0.0.1:16515/?code=<<Authorization code>>

    PEP Client->>Authserver:<<Authorization code>><<Code Verifier>>
    Authserver->>Authserver:verify:<br />Code Challenge == sha256(Code Verifier)
    Authserver->>PEP Client:OAuth token containing:<br /><<subject>><<group>><<issued at>><<expires at>><<hmac>>

    Note over PEP Client, Keyserver: End of OAuth2 Authorization Code flow

    PEP Client->>PEP Client: Generate CSR containing<br /><<subject>><<group>>User

    PEP Client->>Keyserver: <<CSR>><<OAuth token>>

    Keyserver->>Keyserver: verify OAuth token hmac<br />sign certificate
    Keyserver->>PEP Client: <<certificate>>
```

This leaves out how the user is actually authenticated by the authserver. See [How we use apache](authserver-apache.md) for more details about this.