package pep

// Interfaces to PEP servers.

import (
	"crypto/x509"
	"errors"
	"fmt"
	"time"

	"generated.pep.cs.ru.nl/gopep/pep_proto"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"github.com/bwesterb/go-ristretto"
)

// Client configuration
type ClientConf struct {
	// Server to connect to
	Addr string

	// Root certificate. One of the following must be set.
	CaFile string // path to certificate
	CaPem  string // PEM encoded certficate
	CaCert *x509.Certificate

	// Timeout to use.  Default is no timeout.
	Timeout time.Duration

	// If set to true, Connect() will succeed even though the server being
	// connected to is down.  The client will try to reconnect on activity.
	Patient bool
}

type KeyServerConn pep_protocol.Conn
type AccessManagerConn pep_protocol.Conn
type StorageFacilityConn pep_protocol.Conn
type TranscryptorConn pep_protocol.Conn
type RegistrationServerConn pep_protocol.Conn
type AuthserverConn pep_protocol.Conn

type SignedTicket2 struct {
	unpacked *pep_proto.Ticket2
	packed   *pep_proto.SignedTicket2
}

type HandlePageFunc func(*pep_proto.DataPayloadPage)

func wrapSignedTicket2(packed *pep_proto.SignedTicket2) *SignedTicket2 {
	return &SignedTicket2{packed: packed}
}

// Connect to the KeyServer
func DialKeyServer(conf ClientConf) (*KeyServerConn, error) {
	conn, err := dial(conf, "KeyServer")
	return (*KeyServerConn)(conn), err
}

// Connect to the AccessManager
func DialAccessManager(conf ClientConf) (*AccessManagerConn, error) {
	conn, err := dial(conf, "AccessManager")
	return (*AccessManagerConn)(conn), err
}

// Connect to the StorageFacility
func DialStorageFacility(conf ClientConf) (*StorageFacilityConn, error) {
	conn, err := dial(conf, "StorageFacility")
	return (*StorageFacilityConn)(conn), err
}

// Connect to the Transcryptor
func DialTranscryptor(conf ClientConf) (*TranscryptorConn, error) {
	conn, err := dial(conf, "Transcryptor")
	return (*TranscryptorConn)(conn), err
}

// Connect to the RegistrationServer
func DialRegistrationServer(conf ClientConf) (*RegistrationServerConn, error) {
	conn, err := dial(conf, "RegistrationServer")
	return (*RegistrationServerConn)(conn), err
}

// Connect to the Authserver
func DialAuthserver(conf ClientConf) (*AuthserverConn, error) {
	conn, err := dial(conf, "Authserver")
	return (*AuthserverConn)(conn), err
}

func dial(conf ClientConf, cn string) (*pep_protocol.Conn, error) {
	conn, err := pep_protocol.Connect(pep_protocol.ClientConf{
		Addr:             conf.Addr,
		CaFile:           conf.CaFile,
		CaPem:            conf.CaPem,
		CaCert:           conf.CaCert,
		ExpectedServerCN: cn,
		Timeout:          conf.Timeout,
		Patient:          conf.Patient,
	})
	if err != nil {
		return nil, err
	}
	return conn, nil
}

func (c *KeyServerConn) Conn() *pep_protocol.Conn          { return (*pep_protocol.Conn)(c) }
func (c *AccessManagerConn) Conn() *pep_protocol.Conn      { return (*pep_protocol.Conn)(c) }
func (c *StorageFacilityConn) Conn() *pep_protocol.Conn    { return (*pep_protocol.Conn)(c) }
func (c *TranscryptorConn) Conn() *pep_protocol.Conn       { return (*pep_protocol.Conn)(c) }
func (c *RegistrationServerConn) Conn() *pep_protocol.Conn { return (*pep_protocol.Conn)(c) }
func (c *AuthserverConn) Conn() *pep_protocol.Conn         { return (*pep_protocol.Conn)(c) }

// Performs the KeyServer step of user enrollment
func (c *KeyServerConn) EnrollUser(oauthToken string) (
	signingKey CertifiedSigningPrivateKey,
	err error) {
	err = signingKey.PrivateKey.Generate()
	if err != nil {
		return
	}
	tokenData, _, err := ParseOAuthToken(oauthToken)
	if err != nil {
		return
	}
	csr, err := signingKey.PrivateKey.CreateCSR(tokenData.User, tokenData.Group)
	if err != nil {
		return
	}
	//log.Printf("Sending EnrollmentRequest to KeyServer")
	rawResp, err := c.Conn().Request(&pep_proto.EnrollmentRequest{
		OauthToken: oauthToken,
		CertificateSigningRequest: &pep_proto.X509CertificateSigningRequest{
			Data: csr,
		},
	})
	if err != nil {
		return
	}
	resp, ok := rawResp.(*pep_proto.EnrollmentResponse)
	if !ok {
		err = errors.New("KS.EnrollUser: wrong reply message type received")
		return
	}
	signingKey.CertificateChain.SetProto(resp.CertificateChain)
	return
}

func requestKeyComponent(conn *pep_protocol.Conn,
	sk *CertifiedSigningPrivateKey,
	facility FacilityType) (
	pseudonymisationKeyComponent ristretto.Scalar,
	dataKeyComponent ristretto.Scalar,
	err error) {
	req, err := sk.SignMessage(&pep_proto.KeyComponentRequest{})
	if err != nil {
		return
	}
	rawResp, err := conn.Request(req)
	if err != nil {
		return
	}
	resp, ok := rawResp.(*pep_proto.KeyComponentResponse)
	if !ok {
		err = errors.New("requestKeyComponent: wrong reply message type received")
		return
	}
	err = pseudonymisationKeyComponent.UnmarshalBinary(
		resp.PseudonymisationKeyComponent.CurveScalar)
	if err != nil {
		return
	}
	if resp.EncryptionKeyComponent != nil {
		err = dataKeyComponent.UnmarshalBinary(
			resp.EncryptionKeyComponent.CurveScalar)
		if err != nil {
			return
		}
	}
	return
}

// Request key components of the AM.  Part of user enrollment.
func (c *AccessManagerConn) RequestUserKeyComponents(
	sk *CertifiedSigningPrivateKey) (
	pseudonymisationKeyComponent ristretto.Scalar,
	dataKeyComponent ristretto.Scalar,
	err error) {
	//log.Printf("Requesting AM key components")
	return requestKeyComponent(c.Conn(), sk, FTUser)
}

// Request key components of the transcryptor.  Part of user enrollment.
func (c *TranscryptorConn) RequestUserKeyComponents(
	sk *CertifiedSigningPrivateKey) (
	pseudonymisationKeyComponent ristretto.Scalar,
	dataKeyComponent ristretto.Scalar,
	err error) {
	//log.Printf("Requesting Transcryptor key components")
	return requestKeyComponent(c.Conn(), sk, FTUser)
}

// Request a new-style ticket from the AM.
func (c *AccessManagerConn) RequestTicket2(sk *CertifiedSigningPrivateKey,
	request TicketRequest) (ticket *SignedTicket2, err error) {
	req := &pep_proto.SignedTicketRequest2{}

	// Create the two signatures: one for the AM and one for the logger.
	req.Data, err = pep_protocol.Pack(request.proto())
	if err != nil {
		return
	}
	req.Signature, err = sk.MessageSigner(false)(req.Data)
	if err != nil {
		return
	}
	req.LogSignature, err = sk.MessageSigner(true)(req.Data)
	if err != nil {
		return
	}

	// Now, send the request.
	rawResp, err := c.Conn().Request(req)
	if err != nil {
		return
	}
	resp, ok := rawResp.(*pep_proto.SignedTicket2)
	if !ok {
		err = errors.New("AM.RequestTicket2: wrong reply message type received")
		return
	}
	ticket = wrapSignedTicket2(resp)
	return
}

// Same as RequestEncryptionKeys, but acts on a single triple.
func (c *AccessManagerConn) RequestEncryptionKey(sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2, metadata *Metadata, key *Triple, blind bool) (
	*Triple, error) {
	keys, err := c.RequestEncryptionKeys(sk, ticket, []*Metadata{metadata},
		[]*Triple{key}, blind)
	if err != nil {
		return nil, err
	}
	return &keys[0], nil
}

// RequestEncryptionKeys either blinds the provided polymorphic encryption keys
// (if blind is true) or unblinds and rekeys it to for-us-encrypted keys.
func (c *AccessManagerConn) RequestEncryptionKeys(sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2, metadata []*Metadata, keys []*Triple, blind bool) (
	[]Triple, error) {
	blindmode := pep_proto.KeyBlindMode_BLIND_MODE_UNBLIND
	if blind {
		blindmode = pep_proto.KeyBlindMode_BLIND_MODE_BLIND
	}
	entries := make([]*pep_proto.KeyRequestEntry, len(keys))
	for i := 0; i < len(keys); i++ {
		entries[i] = &pep_proto.KeyRequestEntry{
			Metadata:               metadata[i].proto(),
			PolymorphEncryptionKey: keys[i].proto(),
			BlindMode:              blindmode,
		}
	}
	req, err := sk.SignMessage(&pep_proto.EncryptionKeyRequest{
		Ticket2: ticket.proto(),
		Entries: entries,
	})
	if err != nil {
		return nil, err
	}
	rawResp, err := c.Conn().Request(req)
	if err != nil {
		return nil, err
	}
	resp, ok := rawResp.(*pep_proto.EncryptionKeyResponse)
	if !ok {
		return nil, errors.New("AM.RequestEncryptionKey: wrong reply message type received")
	}
	if len(resp.Keys) != len(keys) {
		return nil, errors.New("AM.RequestEncryptionKey: missing key(s) in response")
	}
	ret := make([]Triple, len(resp.Keys))
	for i := 0; i < len(resp.Keys); i++ {
		ret[i].setProto(resp.Keys[i])
	}
	return ret, nil
}

// Open a file on the storage facility for reading
func (c *StorageFacilityConn) Open(
	sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2, identifier string,
	key []byte, metadata *Metadata, fileSize uint64) (
	*FileReader, error) {
	req, err := sk.SignMessage(&pep_proto.DataReadRequest2{
		Ticket: ticket.proto(),
		Ids:    [][]byte{[]byte(identifier)},
	})
	if err != nil {
		return nil, err
	}
	stream, err := c.Conn().RequestStream(req)
	if err != nil {
		return nil, fmt.Errorf("RequestStream: %v", err)
	}

	r, err := newFileReader(stream, key, metadata, fileSize)
	if err != nil {
		return nil, fmt.Errorf("newFileReader: %v", err)
	}

	// Perform an empty read, to fill the buffer and catch errors upfront
	_, err = r.Read([]byte{})
	if err != nil {
		return nil, fmt.Errorf("Read: %v", err)
	}

	return r, nil
}

type SFFileCreationInfo struct {
	Metadata             *Metadata
	PolymorphicPseudonym *Triple
	Column               string
	EncryptedKey         *Triple
	Key                  []byte
}

// Store files on the storage facility
func (c *StorageFacilityConn) Create(
	sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2,
	files []SFFileCreationInfo) ([]*FileWriter, error) {

	entries := make([]*pep_proto.DataStoreEntry2, len(files))
	for i, file := range files {
		colIdx := ticket.ColumnIndex(file.Column)
		ppIdx := ticket.PPIndex(file.PolymorphicPseudonym)
		if ppIdx == nil {
			return nil, fmt.Errorf(
				"PP %v is not in provided ticket",
				file.PolymorphicPseudonym)
		}
		if colIdx == nil {
			return nil, fmt.Errorf(
				"Column %s is not in provided ticket",
				file.Column)
		}

		entries[i] = &pep_proto.DataStoreEntry2{
			Metadata:       file.Metadata.proto(),
			PolymorphicKey: file.EncryptedKey.proto(),
			ColumnIndex:    *colIdx,
			PseudonymIndex: *ppIdx,
		}
	}

	req, err := sk.SignMessage(&pep_proto.DataStoreRequest2{
		Ticket:  ticket.proto(),
		Entries: entries,
	})
	if err != nil {
		return nil, err
	}
	stream, err := c.Conn().RequestStream(req)
	if err != nil {
		return nil, err
	}

	metadatas := make([]*Metadata, len(files))
	keys := make([][]byte, len(files))

	for i, file := range files {
		metadatas[i] = file.Metadata
		keys[i] = file.Key
	}

	return newFileWriters(stream, keys, metadatas)
}

// Enumerate and optionally retreives files from the storage facility using the
// new-style ticket.  Function returns immediately when the list of
// files has been received.
func (c *StorageFacilityConn) Enumerate2(
	sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2) ([]FileInfo, error) {
	req, err := sk.SignMessage(&pep_proto.DataEnumerationRequest2{
		Ticket:              ticket.proto(),
	})
	if err != nil {
		return nil, err
	}
	stream, err := c.Conn().RequestStream(req)
	if err != nil {
		return nil, err
	}
	
	return c.SendDataEnumerationRequest(stream, ticket)
}

func (c *StorageFacilityConn) SendDataEnumerationRequest(
	stream *pep_protocol.Stream,
	ticket *SignedTicket2) ([]FileInfo, error) {

	var entries []*pep_proto.DataEnumerationEntry2
	for {
		rawResp, err := stream.ReceiveAndParse()
		if err != nil {
			return nil, err
		}
		resp, ok := rawResp.(*pep_proto.DataEnumerationResponse2)
		if !ok {
			return nil, errors.New("SF.Enumerate2: wrong reply message type received")
		}
		entries = append(entries, resp.Entries...)
		if !resp.HasMore {
			break
		}
	}
	fs := make([]FileInfo, len(entries))
	cols := ticket.Columns()
	pps := ticket.Pseudonyms()
	for i := 0; i < len(entries); i++ {
		fs[i].Identifier = string(entries[i].Id)
		fs[i].Metadata.setProto(entries[i].Metadata)
		fs[i].EncryptedKey.setProto(entries[i].PolymorphicKey)
		fs[i].FileSize = entries[i].FileSize
		fs[i].Ticket2 = ticket
		colIdx := int(entries[i].ColumnIndex)
		ppIdx := int(entries[i].PseudonymIndex)
		if colIdx >= len(cols) {
			return nil, fmt.Errorf("SF.Enumerate2: column index for entry %d is out of range (%d >= %d)", i, colIdx, len(cols))
		}
		if ppIdx >= len(pps) {
			return nil, fmt.Errorf("SF.Enumerate2: PP index for entry %d is out of range (%d >= %d)", i, ppIdx, len(pps))
		}
		fs[i].Column = cols[colIdx]
		fs[i].Pseudonyms = &pps[ppIdx]
	}

	return fs, nil
}

// Retreives files from the storage facility using the
// new-style ticket.  Function returns immediately when the list of
// files has been received.
func (c *StorageFacilityConn) RetrieveMetadata2(
	ids [][]byte,
	sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2) ([]FileInfo, error) {
	
	req, err := sk.SignMessage(&pep_proto.MetadataReadRequest2{
		Ids:                 ids,
		Ticket:              ticket.proto(),
	})
	if err != nil {
		return nil, err
	}
	
	stream, err := c.Conn().RequestStream(req)
	if err != nil {
		return nil, err
	}
	
	return c.SendDataEnumerationRequest(stream, ticket)
}

func (c *StorageFacilityConn) History2(
	sk *CertifiedSigningPrivateKey,
	ticket *SignedTicket2) ([]FileInfo, error) {
	
	// TODO: consolidate duplicate code with Enumerate2
	
	req, err := sk.SignMessage(&pep_proto.DataHistoryRequest2{
		Ticket:              ticket.proto(),
	})
	if err != nil {
		return nil, err
	}
	stream, err := c.Conn().RequestStream(req)
	if err != nil {
		return nil, err
	}
	
	rawResp, err := stream.ReceiveAndParse()
	if err != nil {
		return nil, err
	}
	resp, ok := rawResp.(*pep_proto.DataHistoryResponse2)
	if !ok {
		return nil, errors.New("SF.History2: wrong reply message type received")
	}
	entries := resp.Entries

	fs := make([]FileInfo, len(entries))
	cols := ticket.Columns()
	pps := ticket.Pseudonyms()
	for i := 0; i < len(entries); i++ {
		fs[i].Identifier = string(entries[i].Id)
		// TODO: copy Timestamp
		fs[i].Ticket2 = ticket
		colIdx := int(entries[i].ColumnIndex)
		ppIdx := int(entries[i].PseudonymIndex)
		if colIdx >= len(cols) {
			return nil, fmt.Errorf("SF.History2: column index for entry %d is out of range (%d >= %d)", i, colIdx, len(cols))
		}
		if ppIdx >= len(pps) {
			return nil, fmt.Errorf("SF.History2: PP index for entry %d is out of range (%d >= %d)", i, ppIdx, len(pps))
		}
		fs[i].Column = cols[colIdx]
		fs[i].Pseudonyms = &pps[ppIdx]
	}
	
	return fs, nil
}

// Lists the checksum chains on a server
func ListChecksumChains(sk *CertifiedSigningPrivateKey, conn *pep_protocol.Conn) (
	[]string, error) {
	req, err := sk.SignMessage(&pep_proto.ChecksumChainNamesRequest{})
	if err != nil {
		return nil, err
	}
	rawResp, err := conn.Request(req)
	if err != nil {
		return nil, err
	}
	resp, ok := rawResp.(*pep_proto.ChecksumChainNamesResponse)
	if !ok {
		return nil, errors.New(
			"ListChecksumChains: unexpected reply message type received")
	}
	return resp.Names, nil
}

// XORs all checksums in the chain up to the given checkpoint.
// Also returns the maximum checkpoint encountered.  If no checkpoint
// is provided, all checksums in the chain are XORed together and the
// checkpoint returned is the latest checkpoint.
func QueryChecksumChain(sk *CertifiedSigningPrivateKey, conn *pep_protocol.Conn,
	chain string, checkpoint []byte) (
	xorredChecksums []byte, maxCheckpoint []byte, err error) {
	req, err := sk.SignMessage(&pep_proto.ChecksumChainRequest{
		Name:       chain,
		Checkpoint: checkpoint,
	})
	if err != nil {
		return nil, nil, err
	}
	rawResp, err := conn.Request(req)
	if err != nil {
		return nil, nil, err
	}
	resp, ok := rawResp.(*pep_proto.ChecksumChainResponse)
	if !ok {
		return nil, nil, errors.New(
			"QueryChecksumChain: unexpected reply message type received")
	}
	return resp.XorredChecksums, resp.Checkpoint, nil
}

// Retrieves metrics
func RetrieveMetrics(sk *CertifiedSigningPrivateKey,
	conn *pep_protocol.Conn) ([]byte, error) {
	req, err := sk.SignMessage(&pep_proto.MetricsRequest{})
	if err != nil {
		return nil, err
	}
	rawResp, err := conn.Request(req)
	if err != nil {
		return nil, err
	}
	resp, ok := rawResp.(*pep_proto.MetricsResponse)
	if !ok {
		return nil, errors.New(
			"RetrieveMetrics: unexpected reply message type received")
	}
	return resp.Metrics, nil
}

func (t *SignedTicket2) ticket() *pep_proto.Ticket2 {
	// TODO is locking required here?
	if t.unpacked == nil {
		msg, err := pep_protocol.OpenSigned(t.packed)
		if err != nil {
			panic(fmt.Sprintf("OpenSigned(): %v", err))
		}
		t.unpacked = msg.(*pep_proto.Ticket2)
	}
	return t.unpacked
}

func (t *SignedTicket2) HasMode(mode string) bool {
	for _, mode2 := range t.ticket().Modes {
		if mode2 == mode {
			return true
		}
	}
	return false
}

func (t *SignedTicket2) Columns() []string {
	return t.ticket().Columns
}

func (t *SignedTicket2) Pseudonyms() []LocalPseudonyms {
	tp := t.ticket()
	ret := make([]LocalPseudonyms, len(tp.Pseudonyms))
	for i := 0; i < len(tp.Pseudonyms); i++ {
		ret[i].setProto(tp.Pseudonyms[i])
	}
	return ret
}

func (t *SignedTicket2) ColumnIndex(column string) *uint32 {
	var i uint32
	tp := t.ticket()
	for i = 0; i < uint32(len(tp.Columns)); i++ {
		if tp.Columns[i] == column {
			return &i
		}
	}
	return nil
}

func (t *SignedTicket2) PPIndex(pp *Triple) *uint32 {
	var i uint32
	lps := t.Pseudonyms()
	for i = 0; i < uint32(len(lps)); i++ {
		if lps[i].Polymorphic.Equals(pp) {
			return &i
		}
	}
	return nil
}

func (t *SignedTicket2) proto() *pep_proto.SignedTicket2 {
	if t == nil {
		return nil
	}
	return t.packed
}

func (c *KeyServerConn) Close()          { c.Conn().Close() }
func (c *AccessManagerConn) Close()      { c.Conn().Close() }
func (c *StorageFacilityConn) Close()    { c.Conn().Close() }
func (c *TranscryptorConn) Close()       { c.Conn().Close() }
func (c *RegistrationServerConn) Close() { c.Conn().Close() }
func (c *AuthserverConn) Close()         { c.Conn().Close() }
