package pep

import (
	"crypto/x509"
	"encoding/json"
	"encoding/pem"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"sync"
	"time"

	"github.com/bwesterb/go-ristretto"
	"github.com/hashicorp/go-multierror"
	"gopkg.in/yaml.v2"

	"pep.cs.ru.nl/gopep/pep_protocol"
)

type ContextOptions struct {
	Timeout time.Duration

	// If set to true, NewContext() will succeed even though one of the
	// servers it tries to connect to is down.  The client will try to
	// reconnect on activity.
	Patient bool
}

type Context struct {
	options ContextOptions

	// TODO connect on-demand
	ksConn *KeyServerConn
	amConn *AccessManagerConn
	sfConn *StorageFacilityConn
	rsConn *RegistrationServerConn
	tsConn *TranscryptorConn
	asConn *AuthserverConn

	mux        sync.Mutex // protects secrets, secretsChanged
	secrets    Secrets
	cons       Constellation
	rootCaCert *x509.Certificate

	secretsPath    string
	secretsChanged bool
}

// Convenience struct to store secrets
type Secrets struct {
	SigningKey   *CertifiedSigningPrivateKey
	DataKey      *ristretto.Scalar
	PseudonymKey *ristretto.Scalar
}

type EnumerateAndOpenFilesOptions struct {
	ColumnGroups          []string
	Columns               []string
	ParticipantGroups     []string
	PolymorphicPseudonyms []*Triple
}

type EnumerateFilesOptions struct {
	ColumnGroups          []string
	Columns               []string
	ParticipantGroups     []string
	PolymorphicPseudonyms []*Triple

	// Request additional write access
	RequestWriteAccess bool
}

func NewContext(constellationPath, secretsPath string, options ContextOptions) (
	*Context, error) {
	ctx := Context{
		secretsPath: secretsPath,
		options:     options,
	}

	consBuf, err := ioutil.ReadFile(constellationPath)
	if err != nil {
		return nil, fmt.Errorf("read constellation file: %v", err)
	}
	err = yaml.Unmarshal(consBuf, &ctx.cons)
	if err != nil {
		return nil, fmt.Errorf("yaml.Unmarshal constellation file: %v")
	}

	if _, err := os.Stat(secretsPath); !os.IsNotExist(err) {
		secretsBuf, err := ioutil.ReadFile(secretsPath)
		if err != nil {
			return nil, fmt.Errorf("read secrets file: %v", err)
		}
		err = json.Unmarshal(secretsBuf, &ctx.secrets)
		if err != nil {
			return nil, fmt.Errorf("json.Unmarshal secrets file: %v", err)
		}
	}

	var rootCaPem []byte
	if ctx.cons.RootCaPath != "" {
		rootCaPem, err = ioutil.ReadFile(ctx.cons.RootCaPath)
		if err != nil {
			return nil, fmt.Errorf("Failed to read root CA file %s: %s",
				ctx.cons.RootCaPath, err)
		}
	} else {
		rootCaPem = []byte(ctx.cons.RootCaPem)
	}

	rootCaPemBlock, _ := pem.Decode(rootCaPem)
	if rootCaPem == nil {
		return nil, fmt.Errorf("Found no PEM block in root CA")
	}
	ctx.rootCaCert, err = x509.ParseCertificate(rootCaPemBlock.Bytes)
	if err != nil {
		return nil, fmt.Errorf("Failed to parse root CA PEM: %s", err)
	}

	err = ctx.connectAll()
	if err != nil {
		return nil, fmt.Errorf("connectAll(): %v", err)
	}

	return &ctx, nil
}

func (ctx *Context) Enrolled() bool {
	return ctx.EnrolledUntil(time.Now())
}

// Returns whether the context is enrolled now, and is expected to still
// be at instant t.
func (ctx *Context) EnrolledUntil(t time.Time) bool {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	if ctx.secrets.SigningKey == nil {
		return false
	}
	return !ctx.secrets.SigningKey.CertificateChain.ExpiredOn(t)
}

func (ctx *Context) connectAll() error {
	var wg sync.WaitGroup
	nJobs := 6
	wg.Add(nJobs)
	errChan := make(chan error, nJobs)

	report := func(err error) {
		if err != nil {
			errChan <- err
		}
		wg.Done()
	}

	baseConf := ClientConf{
		CaCert:  ctx.rootCaCert,
		Timeout: ctx.options.Timeout,
		Patient: ctx.options.Patient,
	}
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.KeyServerAddr
		ctx.ksConn, err = DialKeyServer(conf)
		report(err)
	}()
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.AccessManagerAddr
		ctx.amConn, err = DialAccessManager(conf)
		report(err)
	}()
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.StorageFacilityAddr
		ctx.sfConn, err = DialStorageFacility(conf)
		report(err)
	}()
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.RegistrationServerAddr
		ctx.rsConn, err = DialRegistrationServer(conf)
		report(err)
	}()
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.TranscryptorAddr
		ctx.tsConn, err = DialTranscryptor(conf)
		report(err)
	}()
	go func() {
		var err error
		conf := baseConf
		conf.Addr = ctx.cons.AuthserverAddr
		ctx.asConn, err = DialAuthserver(conf)
		report(err)
	}()

	wg.Wait()
	close(errChan)
	var multErr error
	for err := range errChan {
		multErr = multierror.Append(multErr, err)
	}
	return multErr
}

// enroll user, but return the secrets instead of storing them in the context
func (ctx *Context) EnrollUserReturnSecrets(oauthToken string) (Secrets, error) {
	var secrets Secrets

	sk, err := ctx.
		ksConn.EnrollUser(oauthToken)

	if err != nil {
		return secrets, err
	}

	// Request keycomponents from accessmanager and transcryptor
	var wg sync.WaitGroup
	var pkc1, dkc1, pkc2, dkc2 ristretto.Scalar
	wg.Add(2)
	errChan := make(chan error, 2)

	go func() {
		var err error
		pkc1, dkc1, err = ctx.amConn.RequestUserKeyComponents(&sk)

		if err != nil {
			errChan <- err
		}
		wg.Done()
	}()
	go func() {
		var err error
		pkc2, dkc2, err = ctx.tsConn.RequestUserKeyComponents(&sk)

		if err != nil {
			errChan <- err
		}
		wg.Done()
	}()

	wg.Wait()
	close(errChan)
	var multErr error
	for err := range errChan {
		multErr = multierror.Append(multErr, err)
	}

	if multErr != nil {
		return secrets, multErr
	}

	// Combine keycomponents
	var pk, dk ristretto.Scalar
	dk.Mul(&dkc1, &dkc2)
	pk.Mul(&pkc1, &pkc2)

	secrets.DataKey = &dk
	secrets.PseudonymKey = &pk
	secrets.SigningKey = &sk

	return secrets, nil
}

func (ctx *Context) EnrollUser(oauthToken string) error {
	secrets, err := ctx.EnrollUserReturnSecrets(oauthToken)
	if err != nil {
		return err
	}

	ctx.mux.Lock()
	defer ctx.mux.Unlock()

	ctx.secrets = secrets
	ctx.secretsChanged = true

	return nil
}

// Derive a polymorphic pseudonym for the given identity
func (ctx *Context) DerivePolymorphicPseudonym(id string) (*Triple, error) {
	var ret Triple
	var p ristretto.Point
	p.Derive([]byte(id))
	err := ret.Encrypt(&p, &ctx.cons.PseudonymPK)
	if err != nil {
		return nil, fmt.Errorf("Cannot derive polymorphic pseudonym: %v. Is the pseudonym public key configured?", err)
	}
	return &ret, nil
}

// Generates a cryptographic key and computes a polymorphic encryption key
// from it.
func (ctx *Context) GenerateEncryptionKey() (*Triple, []byte, error) {
	var p ristretto.Point
	var pKey Triple
	p.Rand()
	err := pKey.Encrypt(&p, &ctx.cons.DataPK)
	if err != nil {
		return nil, nil, fmt.Errorf("Cannot generate encryption key: %v. Is the data public key configured?", err)
	}
	return &pKey, SymmetricKeyFromPoint(&p), nil
}

// Open file versions on the storage facility.
func (ctx *Context) HistoryAndOpenFiles(opts EnumerateAndOpenFilesOptions) (
	[]*FileReader, []FileInfo, error) {
	fs, err := ctx.FileVersions(
		EnumerateFilesOptions{
			ParticipantGroups:     opts.ParticipantGroups,
			ColumnGroups:          opts.ColumnGroups,
			PolymorphicPseudonyms: opts.PolymorphicPseudonyms,
			Columns:               opts.Columns,
		})
	if err != nil {
		return nil, nil, fmt.Errorf("FileVersions: %v", err)
	}
	
	rs, err := ctx.OpenFiles(fs)
	if err != nil {
		return nil, nil, fmt.Errorf("OpenFiles: %v", err)
	}
	
	return rs, fs, nil
}

// Open files on the storage facility that have previously been enumerated.
func (ctx *Context) OpenFiles(fs []FileInfo) (
	[]*FileReader, error) {
	
	secrets := ctx.Secrets()
	
	if len(fs) == 0 {
		return []*FileReader{}, nil
	}

	keys := make([]*Triple, len(fs))
	metadata := make([]*Metadata, len(fs))
	for i := 0; i < len(fs); i++ {
		keys[i] = &fs[i].EncryptedKey
		metadata[i] = &fs[i].Metadata
	}

	// Request the encryption key
	encKeys, err := ctx.amConn.RequestEncryptionKeys(secrets.SigningKey,
		fs[0].Ticket2, metadata, keys, false)
	if err != nil {
		return nil, fmt.Errorf("RequestEncryptionKey: %v", err)
	}

	rs := make([]*FileReader, len(fs))
	for i := 0; i < len(fs); i++ {
		key := SymmetricKeyFromPoint(encKeys[i].Decrypt(secrets.DataKey))

		rs[i], err = ctx.sfConn.Open(secrets.SigningKey, fs[i].Ticket2,
			fs[i].Identifier, key, &fs[i].Metadata, fs[i].FileSize)
		if err != nil {
			return nil, fmt.Errorf("sfConn.Open: %v", err)
		}
	}

	return rs, nil
}

// Enumerate and reads files on the storage facility for the given polymorphic
// pseudonym and query.
func (ctx *Context) EnumerateAndReadFiles(opts EnumerateAndOpenFilesOptions) (
	[][]byte, []FileInfo, error) {
	
	fs, err := ctx.EnumerateFiles(
		EnumerateFilesOptions{
			ParticipantGroups:     opts.ParticipantGroups,
			ColumnGroups:          opts.ColumnGroups,
			PolymorphicPseudonyms: opts.PolymorphicPseudonyms,
			Columns:               opts.Columns,
		})
	if err != nil {
		return nil, nil, fmt.Errorf("EnumerateFiles: %v", err)
	}
	
	rs, err := ctx.OpenFiles(fs)
	if err != nil {
		return nil, nil, fmt.Errorf("OpenFiles: %v", err)
	}
	
	ret, err := ReadOpenFiles(rs)
	if err != nil {
		return nil, nil, err
	}
	
	return ret, fs, nil
}

// Reads file histories on the storage facility for the given polymorphic
// pseudonym and query.
func (ctx *Context) HistoryAndReadFiles(opts EnumerateAndOpenFilesOptions) (
	[][]byte, []FileInfo, error) {
	rs, fs, err := ctx.HistoryAndOpenFiles(opts)
	if err != nil {
		return nil, nil, err
	}
	
	ret, err := ReadOpenFiles(rs)
	if err != nil {
		return nil, nil, err
	}
	
	return ret, fs, nil
}

// Reads all contents from the given file readers
func ReadOpenFiles(rs []*FileReader) ([][]byte, error) {
	ret := make([][]byte, len(rs))
	var err error

	for i := 0; i < len(rs); i++ {
		ret[i], err = ioutil.ReadAll(rs[i])
		if err != nil {
			// TODO attach more fileinfo to error
			return nil, err
		}
		err = rs[i].Close()
		if err != nil {
			return nil, err
		}
	}

	return ret, nil
}

// Request a ticket and get associated signing key for the given polymorphic
// pseudonym and query.
func (ctx *Context) GetTicketAndSigningKey(opts EnumerateFilesOptions) (
	*SignedTicket2, *CertifiedSigningPrivateKey, error) {
	
	// Request ticket
	modes := []string{"read"}
	if opts.RequestWriteAccess {
		modes = append(modes, "write")
	}

	var signingKey *CertifiedSigningPrivateKey = ctx.SigningKey()
	if signingKey == nil {
		return nil, nil, fmt.Errorf("not enrolled")
	}

	ticket, err := ctx.amConn.RequestTicket2(signingKey,
		TicketRequest{
			Modes:                 modes,
			PolymorphicPseudonyms: opts.PolymorphicPseudonyms,
			ParticipantGroups:     opts.ParticipantGroups,
			ColumnGroups:          opts.ColumnGroups,
			Columns:               opts.Columns,
		})
	if err != nil {
		return nil, nil, err
	}
	
	return ticket, signingKey, nil
}

// Enumerate files on the storage facility for the given polymorphic
// pseudonym and query.
func (ctx *Context) EnumerateFiles(opts EnumerateFilesOptions) (
	[]FileInfo, error) {

	// Request ticket
	ticket, signingKey, err := ctx.GetTicketAndSigningKey(opts)
	if err != nil {
		return nil, err
	}

	// Enumerate files
	fs, err := ctx.sfConn.Enumerate2(
		signingKey,
		ticket,
	)
	if err != nil {
		return nil, err
	}

	return fs, nil
}

// Get file versions on the storage facility for the given polymorphic
// pseudonym and query.
func (ctx *Context) FileVersions(opts EnumerateFilesOptions) (
	[]FileInfo, error) {

	// Request ticket
	ticket, signingKey, err := ctx.GetTicketAndSigningKey(opts)
	if err != nil {
		return nil, err
	}

	// Get history entries
	hs, err := ctx.sfConn.History2(
		signingKey,
		ticket,
	)
	if err != nil {
		return nil, err
	}
	
	// Collect IDs of stored data versions
	var ids [][]byte
	for i := 0; i < len(hs); i++ {
		if len(hs[i].Identifier) > 0 { // Skip deletions
			ids = append(ids, []byte(hs[i].Identifier))
		}
	}
	
	if len(ids) == 0 {
		return []FileInfo{}, nil
	}
	
	// Retrieve data
	fs, err := ctx.sfConn.RetrieveMetadata2(ids, signingKey, ticket)
		if err != nil {
		return nil, err
	}

	return fs, nil
}

// Opens a file on the storage facility using a FileInfo object returned
// by EnumerateFiles().
//
// Note: info.PolymorphicPseudonym has to be set, which EnumerateFiles() does.
func (ctx *Context) OpenFile(info *FileInfo) (*FileReader, error) {
	var ticket *SignedTicket2
	var err error

	secrets := ctx.Secrets()

	if secrets.SigningKey == nil {
		return nil, fmt.Errorf("not enrolled")
	}

	if info.Ticket2 != nil && info.Ticket2.HasMode("read") {
		ticket = info.Ticket2
	} else {
		// Request a ticket
		ticket, err = ctx.amConn.RequestTicket2(secrets.SigningKey,
			TicketRequest{
				Modes:                 []string{"read"},
				PolymorphicPseudonyms: []*Triple{info.PolymorphicPseudonym},
				Columns:               []string{info.Column},
			})
		if err != nil {
			return nil, err
		}
	}

	// Request the encryption key
	encKey, err := ctx.amConn.RequestEncryptionKey(secrets.SigningKey,
		ticket, &info.Metadata, &info.EncryptedKey, false)
	if err != nil {
		return nil, err
	}

	key := SymmetricKeyFromPoint(encKey.Decrypt(secrets.DataKey))

	return ctx.sfConn.Open(secrets.SigningKey, ticket, info.Identifier,
		key, &info.Metadata, info.FileSize)
}

// Creates a new file under the given query with the given metadata
// for the polymorphicPseudonym, write the given bytes to it
// and return the identifier under which it was stored.
func (ctx *Context) CreateFileFromBytes(polymorphicPseudonym *Triple,
	column string, contents []byte) (string, error) {
	w, err := ctx.CreateFile(polymorphicPseudonym, column)
	if err != nil {
		return "", fmt.Errorf("CreateFile: %v", err)
	}

	_, err = w.Write(contents)
	if err != nil {
		return "", fmt.Errorf("Write: %v", err)
	}

	err = w.Close()
	if err != nil {
		return "", fmt.Errorf("Close: %v", err)
	}

	return <-w.IdentifierC, nil
}

// Creates a new file under the given column with the given metadata
// for the polymorphicPseudonym and returns an io.Writer for it.
func (ctx *Context) CreateFile(polymorphicPseudonym *Triple, column string) (
	*FileWriter, error) {

	ws, err := ctx.CreateFiles([]Cell{Cell{
		Column:               column,
		PolymorphicPseudonym: polymorphicPseudonym,
	}})

	if err != nil {
		return nil, err
	}
	if len(ws) != 1 {
		log.Fatalf("Internal error: CreateFiles did not return " +
			"the correct number of *FileWriter")
	}
	return ws[0], nil
}

type Cell struct {
	Column               string
	PolymorphicPseudonym *Triple
}

func (ctx *Context) CreateFiles(cells []Cell) ([]*FileWriter, error) {

	var signingKey *CertifiedSigningPrivateKey = ctx.SigningKey()
	if signingKey == nil {
		return nil, fmt.Errorf("not enrolled")
	}

	// Get ticket from AccessManager
	columns := make([]string, 0)
	pps := make([]*Triple, 0)

	for _, cell := range cells {
		columns = append(columns, cell.Column)
		pps = append(pps, cell.PolymorphicPseudonym)
	}

	ticket, err := ctx.amConn.RequestTicket2(signingKey,
		TicketRequest{
			Modes:                 []string{"write"},
			PolymorphicPseudonyms: pps,
			Columns:               columns,
		})
	if err != nil {
		return nil, err
	}

	now := time.Now()

	files := make([]SFFileCreationInfo, 0)
	for _, cell := range cells {
		metadata := &Metadata{
			Tag:       cell.Column,
			Timestamp: now,
		}
		// Generate and blind encryption key
		// TODO: do not use separate requests
		encKey, key, err := ctx.GenerateEncryptionKey()
		if err != nil {
			return nil, err
		}
		blindedEncKey, err := ctx.amConn.RequestEncryptionKey(
			signingKey, ticket,
			metadata, encKey, true)
		if err != nil {
			return nil, err
		}

		files = append(files, SFFileCreationInfo{
			Column:               cell.Column,
			PolymorphicPseudonym: cell.PolymorphicPseudonym,
			EncryptedKey:         blindedEncKey,
			Key:                  key,
			Metadata:             metadata,
		})
	}

	// Open stream with storage facility
	return ctx.sfConn.Create(signingKey, ticket, files)
}

func (ctx *Context) SaveSecrets() error {
	ctx.mux.Lock()
	secrets := ctx.secrets
	secretsChanged := ctx.secretsChanged
	ctx.secretsChanged = false
	ctx.mux.Unlock()
	if !secretsChanged {
		return nil
	}
	log.Printf("Writing secrets to %s", ctx.secretsPath)
	secretsBuf, err := json.Marshal(&secrets)
	if err != nil {
		return err
	}
	err = ioutil.WriteFile(ctx.secretsPath, secretsBuf, 0600)
	if err != nil {
		return err
	}
	return nil
}

func (ctx *Context) Close() {
	if ctx.amConn != nil {
		ctx.amConn.Close()
	}
	if ctx.tsConn != nil {
		ctx.tsConn.Close()
	}
	if ctx.ksConn != nil {
		ctx.ksConn.Close()
	}
	if ctx.sfConn != nil {
		ctx.sfConn.Close()
	}
	err := ctx.SaveSecrets()
	if err != nil {
		log.Printf("Failed to write secrets: %v", err)
	}
}

// Lists PEP servers in constellation
func (ctx *Context) Servers() map[string]*pep_protocol.Conn {
	return map[string]*pep_protocol.Conn{
		"KeyServer":          ctx.ksConn.Conn(),
		"AccessManager":      ctx.amConn.Conn(),
		"Transcryptor":       ctx.tsConn.Conn(),
		"StorageFacility":    ctx.sfConn.Conn(),
		"RegistrationServer": ctx.rsConn.Conn(),
		"Authserver":         ctx.asConn.Conn(),
	}
}

func (ctx *Context) KS() *KeyServerConn          { return ctx.ksConn }
func (ctx *Context) AM() *AccessManagerConn      { return ctx.amConn }
func (ctx *Context) TS() *TranscryptorConn       { return ctx.tsConn }
func (ctx *Context) SF() *StorageFacilityConn    { return ctx.sfConn }
func (ctx *Context) RS() *RegistrationServerConn { return ctx.rsConn }
func (ctx *Context) AS() *AuthserverConn         { return ctx.asConn }
func (ctx *Context) SigningKey() *CertifiedSigningPrivateKey {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	return ctx.secrets.SigningKey
}
func (ctx *Context) DataKey() *ristretto.Scalar {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	return ctx.secrets.DataKey
}
func (ctx *Context) PseudonymKey() *ristretto.Scalar {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	return ctx.secrets.PseudonymKey
}
func (ctx *Context) SetSecrets(secrets Secrets) {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	ctx.secrets = secrets
}
func (ctx *Context) Secrets() Secrets {
	ctx.mux.Lock()
	defer ctx.mux.Unlock()
	return ctx.secrets
}
func (ctx *Context) RootCaCert() *x509.Certificate {
	return ctx.rootCaCert
}
