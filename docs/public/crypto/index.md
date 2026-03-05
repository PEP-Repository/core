# PEP Crypto

Originally based on [2020 PEP blueprint](https://docs.pages.pep.cs.ru.nl/private/ops/main/uploads/blueprint_aug_2020_main.pdf).

## Authentication

- Servers authenticated using TLS certificates
- Messages are signed using PEP certificates and transmitted over the TLS connection
  - PEP certificates are signed with server or user intermediate CA
    - Both intermediate CAs are renewed yearly and signed with PEP root certificate
  - Server certificates
    - Subject CN/OU = server name
    - Renewed yearly
  - User certificates
    - Subject CN = user name, OU = user group
    - Issued on enrollment by Key Server on presentation of OAuth token
      - User authenticates with OAuth2 via Authserver in collaboration with Access Manager
        - Contains user name + user group + issuance & expiry
        - MACed using auth token secret
        - Prove identity to IdP, e.g. SURFconext
        - More details in [#928](https://gitlab.pep.cs.ru.nl/pep/core/-/issues/928)
- [Tickets](./ticket_request.md) are used for access management to specific subjects & columns

## Cell

- Payload, encrypted using authenticated AES-GCM encryption
- Metadata, partially encrypted
- AES key, encrypted using ElGamal

## Ellyptic Curve Crypto

- Curve25519: $\mathbb{E}$
- Coordinates in $\mathbb{F}_{2^{255} - 19}$
  - Code: `CurveScalar`
- Designated base point $B$ generating large subgroup $\mathbb{Z}B$
  - Code: `CurvePoint`
- Small subgroup $\mathbb{Z}Q$ of 8 elements (including $\bold 0$)
- Each element in $\mathbb{E}$ is the sum of one in $\mathbb{Z}B$ and $\mathbb{Z}Q$ (including $\bold 0$, which is in both)
- Dangers:
  - Point must lie on $\mathbb{E}$
  - Point must be in $\mathbb{Z}B$, because otherwise we can compute the logarithm (modulo 8)
- Even point $P$: $P=2A$ for some $A$
  - Even points: $2\mathbb{E}$
  - $\mathbb{Z}B$ are all even
- Hence $\mathbb{Z}B \cong \mathbb{E}/\mathbb{Z}Q \cong 2\mathbb{E}/2\mathbb{Z}Q$
  - Mod out (even) points from small subgroup
  - Ristretto: $\mathbb{R} := 2\mathbb{E}/2\mathbb{Z}Q$
    - Each point has equivalence class of 4 points
    - Use extended coordinates to cache value and avoid inversions

## ElGamal

- Key pair: private $x$, public $xB$
- Encryption of $M$: $\operatorname{Enc}_{(xB),n}(M) = (nB, M+n \cdot (xB), (xB))$
  - Public key explicitly included for rerandomization operation below
  - $n$: random nonce
  - Code: `ElgamalEncryption`
- Decryption: $\operatorname{Dec}_x (a,b,c) = b - xa = (M+nxB) - x \cdot (nB) = M$ if $c = xB$
- Polymorphic operations ($x$ not required):
  - ReRandomize with $r$: $(nB + \underline{rB}, M+nxB + \underline{r \cdot xB}, xB)$
    - $M$ stays intact and can be decrypted using same $x$
  - ReShuffle with $s$: $(\underline{s} \cdot nB, \underline{s} \cdot (M+nxB), xB)$
    - $M \mapsto sM$ but can be decrypted using same $x$
  - ReKey with $k$: $(\underline{k^{-1}} \cdot nB, M+nxB, \underline{k} \cdot xB)$
    - $M$ stays intact but can now be decrypted using $sx$
  - Combined RSK: $(\underline{k^{-1}} \cdot \underline{s} \cdot (nB + \underline{rB}), \underline{s} \cdot (M+nxB + \underline{r \cdot xB}), \underline{k} \cdot xB)$

## Identifiers

Also see [glossary](../user_documentation/glossary.md).

- Origin ID (randomly generated, human readable, e.g. `POM1234567890`)
  - Polymorphic Pseudonym (encrypted Origin ID, can be randomized)
    - Local Pseudonym (32 bytes; variants: user, transcryptor, storage facility, access manager)
      - Brief Local Pseudonym (prefix + first (e.g. 10) characters of HEX representation of local pseudonym, e.g. `POM12345ABCDE`)
- Reference ID (mapping, human readable for uploaders, e.g. `POM1PL1234549`)

## Pseudonyms

Pseudonyms are encrypted with ElGamal.

- __Master pseudonym encryption key__ of the system
  - Private: $x_\mathrm{p} = x_{\mathrm{p},\mathrm{AM}} x_{\mathrm{p},\mathrm{T}}$
  - Public: $x_\mathrm{p}B$
  - Split between Access Manager and Transcryptor
- On enrollment, party $p$ obtains __local pseudonym encryption key__ $k_{p,\mathrm{p},\mathrm{AM}} k_{p,\mathrm{p},\mathrm{T}} x_\mathrm{p}$
  - Obtained from AM & T in _key components_ $k_{p,\mathrm{p},\mathrm{AM}} x_{\mathrm{p},\mathrm{AM}}$ & $k_{p,\mathrm{p},\mathrm{T}} x_{\mathrm{p},\mathrm{T}}$
  - _Pseudonym encryption key factor_<br/> $k_{p,\mathrm{p},z} = \operatorname{hmac-sha512}(\kappa_{\mathrm{p},z}, \operatorname{sha256}(\textnormal{00 00 00 01 00 00 00}\ t \parallel p))$
    - $\kappa_{\mathrm{p},z}$: _pseudonym encryption key factor secret_ (symmetric key) known by server $z$ (AM/T)
    - $p$: PEP *certificate* (differs each enrollment) for users, or server name for servers
    - $t$: facility type like above
- For Participant ID $a$ we have _participant point_ $M_a$
  - $M_a = \operatorname{elligator2}(\operatorname{sha512}(a))$
- __Polymorphic pseudonym__: $\operatorname{Enc}_{x_\mathrm{p}B, n}(M_a)$
  - $n$: Random

Pseudonyms are shuffled to obtain different local pseudonyms.

- __Pseudonymisation key__ for access group $p$: $s_{p,\mathrm{AM}} s_{p,\mathrm{T}}$
  - _Pseudonymisation key factor_<br/> $s_{p,z} = \operatorname{hmac-sha512}(\sigma_z, \operatorname{sha256}(\textnormal{00 00 00 01 00 00 00}\ t \parallel p))$
    - $p$: access group or server name
    - $t$: facility type  ($01$ = users, $02$-$05$ = SF, AM, T, RS), corresponds to $b$
    - $\sigma_z$: _pseudonymisation key factor secret_ (symmetric key) known by server $z$ (AM/T)
- Translate polymorphic pseudonym $\operatorname{Enc}_{x_\mathrm{p}B, n}(M_a)$ for party $p$:
  - RSK by AM:
    - ReRandomize: $\operatorname{Enc}_{x_\mathrm{p}B, \underline{n}}(M_a)$
    - ReShuffle: $\operatorname{Enc}_{x_\mathrm{p}B, \underline{n'}}(\underline{s_{p,\mathrm{AM}}} M_a)$
    - ReKey: $\operatorname{Enc}_{\underline{k_{p,\mathrm{p},\mathrm{AM}}} x_\mathrm{p}B, \underline{n''}}(s_{p,\mathrm{AM}} M_a)$
  - Then same RSK by T:<br/> $\operatorname{Enc}_{\underline{k_{p,\mathrm{p},\mathrm{T}}} k_{p,\mathrm{p},\mathrm{AM}} x_\mathrm{p}B, \underline{n'''}}(\underline{s_{p,\mathrm{T}}} s_{p,\mathrm{AM}} M_a)$
  - $\mathrm{Enc}_{k_{p,\mathrm{p},\mathrm{T}} k_{p,\mathrm{p},\mathrm{AM}} x_\mathrm{p}B, n}(s_{p,\mathrm{T}} s_{p,\mathrm{AM}} M_a)$ is the _encrypted local pseudonym_
  - __Local pseudonym__ $s_{p,\mathrm{AM}} s_{p,\mathrm{T}} M_a$ obtained by decrypting with the local pseudonym encryption key $k_{p,\mathrm{p},\mathrm{AM}} k_{p,\mathrm{p},\mathrm{T}} x_\mathrm{p}$ (above)



## Data

- __Master data encryption key__ of the system
  - Private: $x_\mathrm{d} = x_{\mathrm{d},\mathrm{AM}} x_{\mathrm{d},\mathrm{T}}$
  - Public: $x_\mathrm{d}B$
- On enrollment, client obtains __local data encryption key__ $k_{\mathrm{d},\mathrm{AM}} k_{\mathrm{d},\mathrm{T}} x_\mathrm{d}$
  - _Data encryption key factor_<br/> $k_{\mathrm{d},c} = \operatorname{hmac-sha512}(\kappa_{\mathrm{d},c}, \operatorname{sha256}(\textnormal{00 00 00 02 00 00 00}\ t \parallel p))$
    - _Data encryption key factor secret_ $\kappa_{\mathrm{d},c}$ (symmetric key) known by server $c$
    - $t,p$: like above
- Cells are encrypted using AES key $\operatorname{sha256}(K)$
  - $K$ is a curve point
  - Additional Authenticated Data (old versions were different): page number
  - Generated by uploading client, ElGamal-encrypted with master data encryption key to obtain _polymorphic encryption key_
  - Key is 'blinded' by AM while ElGamal-encrypted: $b^{-1} K$ (old versions were different)
    - _Blinding key_ $b = \operatorname{hmac-sha512}(\beta, 3 \parallel \textnormal{timestamp} \parallel \textnormal{tag} \parallel s_{\mathrm{AM},\mathrm{AM}} s_{\mathrm{AM},\mathrm{T}} M_a \parallel \textnormal{binded metadata})$
      - $\beta$: _blinding key secret_ (symmetric key) known to AM
      - $s_{\mathrm{AM},\mathrm{AM}} s_{\mathrm{AM},\mathrm{T}} M_a$: AM's local pseudonym for $a$

The blinding key is there to prevent the following attack. A malicious Storage Facility colludes with a user U to download cell C1 that U does not have access to. U requests a ticket for another cell C2 that they do have access to. The Storage Facility then actually returns C1 to U. U now RSKs the encrypted cell via AM & TS and can decrypt C1. The blinding key prevents this, because if the ticket does not match the cell, the derived blinding key will be wrong and scramble the ciphertext. Note that AEAD (AES-GCM) encryption is not a solution to this, as that check only happens client-side.


## Proofs

### Schnorr's identification protocol

Zero Knowledge Proof that you know logarithm $x$ of $X = xB$.

- Prover chooses random nonce $r$
- Sends commitment $R = rB$ to verifier
- Verifier sends random challenge $c$
- Prover sends $z = r + cx$
- Verifier checks $zB \overset?= R + cX$

#### Non-Interactive variant

Transformed using Fiat-Shamir heuristic to use cryptographic hash for challenge.

- Prover chooses random nonce $r$
- Computes commitment $R = rB$
- Computes challenge $c = h(X,R)$
- Computes $z = r + cx$
- Publishes $R,z$ besides $X$
- Verifier also computes challenge $c = h(X,R)$
- Checks $zB \overset?= R + cX$

### Scalar multiplication

Prove that you know scalar $x$ in $N = xM$ without giving $x$.

Chaum-Pedersen protocol + Fiat-Shamir heuristic, similar to Schnorr NI-ZKP:

- Prover chooses random nonce $r$
- Computes commitments $R_B = rB$ & $R_M = rM$
- Computes challenge $c = h(M,N,R_B,R_M)$
- Computes $z = r + cx$
- Publishes $R_B,R_M,z, X=xB$ besides $M,N$
- Verifier also computes challenge $c = h(M,N,R_B,R_M)$
- Checks $zB \overset?= R_B + cX$
- Checks $zM \overset?= R_M + cN$

We also add $X$ in the challenge hash. _Is this redundant?_

### RSK proof

For a full RSK (see equation above), you need 3 multiplication proofs:

1. $\underline{sk^{-1}} \cdot (nB + rB)$
2. $\underline{s} \cdot (M+nxB + rxB)$
3. $\underline{r} \cdot xB$

Check multiplication proofs using additional info dependent on just reshuffle & rekey factors:

- Rekeyed public key $kxB$ (check that public key is as expected)
- $sB$ (for proof 2)
- $sk^{-1}B$ (for proof 1)

This way, transcryptors enforce use of consistent scalars $s_{p,z}, k_{p,\mathrm{p}}$ for reshuffling & rekeying pseudonyms.

Note that $rB$ and $rxB$ to verify proof 3 are attached with the proof.

### In PEP

In PEP, the Access Manager proves to the Transcryptor that it performed the RSKs correctly: with consistent factors, starting at the same polymorphic pseudonym. The Transcryptor can then [log](https://docs.pages.pep.cs.ru.nl/private/ops/main/technical_design/design-logger/) the consistent set of pseudonyms for auditing.
