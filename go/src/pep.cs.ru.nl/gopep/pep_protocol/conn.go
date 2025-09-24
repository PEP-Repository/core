package pep_protocol

// Implements the idiosyncratic multiplexing network connection over
// TLS used in PEP.

import (
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"log"
	"math/rand"
	"net"
	"strings"
	"sync"
	"time"

	"generated.pep.cs.ru.nl/gopep/pep_proto"

	"github.com/phf/go-queue/queue"
)

type ClientConf struct {
	// Server to connect to
	Addr string

	//  Root certificate.  Exactgly one of the following must be set.
	CaFile string            // path to file
	CaPem  string            // PEM of certificate
	CaCert *x509.Certificate // parsed certificate

	// If empty, the server certificate is checked against the hostname or
	// IP address in Addr.  If set to "*", any server common name is accepted
	// (if signed by the CA off course).  In the other cases, the server
	// certificate common name is checked against ExpectedServerCN.
	ExpectedServerCN string

	// Timeout to put on read and write operations.  The default is no timeout.
	Timeout time.Duration

	// If set to true, Connect() will succeed even though the server being
	// connected to is down.  The client will try to reconnect on activity.
	Patient bool
}

// Connect as a client to the given PEP server.
func Connect(conf ClientConf) (*Conn, error) {
	// Prepare the state
	c := &Conn{
		streamId:   1,
		streams:    make(map[uint32]*Stream),
		clientConf: &conf,
		timeout:    conf.Timeout,
	}

	var err error
	caCertPool := x509.NewCertPool()

	if conf.CaCert == nil {
		var caCert []byte

		// Prepare TLS connection
		if conf.CaFile != "" {
			caCert, err = ioutil.ReadFile(conf.CaFile)
			if err != nil {
				return nil, err
			}
		} else {
			caCert = []byte(conf.CaPem)
		}
		caCertPool.AppendCertsFromPEM(caCert)
	} else {
		caCertPool.AddCert(conf.CaCert)
	}

	host, _, err := net.SplitHostPort(conf.Addr)
	if err != nil {
		return nil, err
	}
	c.tlsConf = &tls.Config{
		RootCAs:    caCertPool,
		ServerName: host,
	}

	if conf.ExpectedServerCN != "" {
		c.tlsConf.InsecureSkipVerify = true
		c.tlsConf.VerifyPeerCertificate = func(rawCerts [][]byte,
			verifiedChains [][]*x509.Certificate) error {
			if len(rawCerts) == 0 {
				return fmt.Errorf("Missing certificate")
			}
			cert, err := x509.ParseCertificate(rawCerts[0])
			if err != nil {
				return err
			}
			intermediatesPool := x509.NewCertPool()
			for i := 1; i < len(rawCerts); i++ {
				intCert, err := x509.ParseCertificate(rawCerts[i])
				if err != nil {
					return err
				}
				intermediatesPool.AddCert(intCert)
			}
			options := x509.VerifyOptions{
				Roots:         caCertPool,
				Intermediates: intermediatesPool,
				KeyUsages:     []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
			}
			if conf.ExpectedServerCN != "*" {
				options.DNSName = conf.ExpectedServerCN
			}
			_, err = cert.Verify(options)
			return err
		}
	}

	err = c.connect()
	if err != nil && !conf.Patient {
		return nil, err
	}

	return c, nil
}

func (c *Conn) log(fmts string, args ...interface{}) {
	tmp := fmt.Sprintf(fmts, args...)
	if c.clientConf != nil && c.clientConf.ExpectedServerCN != "*" {
		log.Printf("%s: %s", c.clientConf.ExpectedServerCN, tmp)
	}
}

func (c *Conn) Closed() bool {
	c.lock.Lock()
	defer c.lock.Unlock()
	return c.closed
}

func (c *Conn) runReader() {
	defer func() {
		c.lock.Lock()
		c.readerRunning = false
		for _, stream := range c.streams {
			stream.lock.Lock()
			stream.readerRunning = false
			stream.incoming.Broadcast()
			stream.lock.Unlock()
		}
		c.lock.Unlock()
		c.readerFinishWg.Done()
	}()
	for {
		// Read the message header
		header := messageHeader{}
		err := binary.Read(c.conn, binary.BigEndian, &header)
		if err != nil {
			if !c.Closed() {
				log.Printf("runReader: Read(): %v", err)
			}
			return
		}

		// Read the message
		data := make([]byte, header.Size)
		_, err = io.ReadAtLeast(c.conn, data, int(header.Size))
		if err != nil {
			if !c.Closed() {
				log.Printf("runReader: ReadAtLeast(): %v", err)
			}
			return
		}

		msg := RawMessage{
			Data:       data,
			IsError:    header.IsError(),
			Close:      header.Close(),
			HasPayload: header.HasPayload(),
			IsResponse: header.IsResponse(),
		}

		if header.Size == 0 && header.StreamId() == 0 {
			// This is a keep-alive message.  We ignore it.
			continue
		}

		if header.IsRequest() {
			parsedMsg, err := msg.Parse()
			if err != nil {
				c.log("Incoming raw packet of unknown type with header %s", header)
				continue
			}
			_, ok := parsedMsg.(*pep_proto.VersionRequest)
			if ok && header.HasPayload() {
				version, err := GetCurrentBuildVersionResponse()
				if err != nil {
					c.log("Failed to determine own version: %v", err)
					continue
				}
				data, err := Pack(version)
				if err != nil {
					c.log("Failed to serialize VersionResponse: %v", err)
					continue
				}
				go c._send(header.StreamId(), RawMessage{
					Data:       data,
					HasPayload: true,
					IsResponse: true,
					Close:      true,
				})

				continue
			}

			c.log("Unexpected incoming request %s: %v with header %s",
				nameOf(parsedMsg), parsedMsg, header)
			continue
		}

		c.lock.Lock()
		stream, ok := c.streams[header.StreamId()]
		c.lock.Unlock()

		if !ok {
			if header.Size == 0 && header.Close() {
				// This is a close message for a stream we probably already
				// closed, so there is no need to worry about it.
				continue
			}

			c.log("Incoming response with header %s for unknown (or closed) stream", header)
			continue
		}

		stream.lock.Lock()
		stream.inBuf.PushBack(&msg)
		if msg.Close {
			stream.__close()
			c.deregisterStream(stream)
		} else {
			stream.incoming.Signal()
		}
		stream.lock.Unlock()
	}
}

type Conn struct {
	conn *tls.Conn

	// If this is a client->server connection, this field contains the
	// information required to reconnect.
	clientConf *ClientConf
	tlsConf    *tls.Config
	rootCa     *x509.Certificate

	wLock          sync.Mutex      // protects writes
	lock           sync.Mutex      // protects closed, streams, streamId and readerRunning
	readerFinishWg *sync.WaitGroup // waitgroup for runReader to finish
	readerRunning  bool
	closed         bool // whether Close() was called from this side

	// next free stream id
	streamId uint32

	streams map[uint32]*Stream

	timeout time.Duration
}

// A halfway-parsed message received over a stream.
// Depending on the context, Data is a protobuf message or not.
type RawMessage struct {
	Data []byte

	// Flags
	IsError    bool // ERROR
	HasPayload bool // PAYLOAD
	Close      bool // CLOSE
	IsResponse bool // RESPONSE
}

// Parse the data as a protobuf message
func (r *RawMessage) Parse() (Message, error) {
	return Unpack(r.Data)
}

// Returns a messageHeader for the message
func (r *RawMessage) header(streamId uint32) (h messageHeader) {
	h.SetIsError(r.IsError)
	h.SetClose(r.Close)
	h.SetHasPayload(r.HasPayload)
	h.SetStreamId(streamId)
	h.Size = uint32(len(r.Data))
	if r.IsResponse {
		h.SetIsResponse()
	}
	return
}

// Represents a stream over the TLS connection.
type Stream struct {
	conn   *Conn
	id     uint32 // streamId of this stream
	closed bool   // Determines whether stream.Receive results in EOF

	// Keeps track of whether a close flag was send by Close or SendLastData
	// Note: this value is not updated when a close flag is send directly
	//  via conn._send(...), for the sake of efficiency.
	close_flag_send bool

	readerRunning bool
	inBuf         queue.Queue // buffer of incoming messages
	lock          sync.Mutex
	incoming      *sync.Cond // to wait on incoming messages

	// If set to true, will include the RESPONSE flag to every message sent
	Responding bool
}

// Wait for a raw packet.  Returns io.EOF error when stream is closed.
func (s *Stream) Receive() (*RawMessage, error) {
	s.lock.Lock()
	for s.inBuf.Len() == 0 {
		if s.closed || !s.readerRunning {
			s.lock.Unlock()
			return nil, io.EOF
		}
		s.incoming.Wait()
	}
	ret := s.inBuf.PopFront()
	s.lock.Unlock()
	return ret.(*RawMessage), nil
}

func (s *Stream) ReceiveAndParse() (Message, error) {
	raw, err := s.Receive()
	if err != nil {
		return nil, err
	}
	msg, err := raw.Parse()
	if err != nil {
		return nil, err
	}
	if raw.IsError {
		return nil, fmt.Errorf("received error: %v",
			msg.(*pep_proto.Error))
	}
	return msg, nil
}

// Close the connection.
func (c *Conn) Close() {
	c.lock.Lock()
	defer c.lock.Unlock()

	if c.closed {
		return
	}
	c.closed = true
	c.closeStreams()
}

// Close all and socket streams.  Assumes lock is held.
// This function is called before an automatic reconnect after an error (as
// we can't trust that no packets were lost.)
func (c *Conn) closeStreams() {
	if c.conn != nil {
		c.conn.Close()
	}
	for _, stream := range c.streams {
		func() {
			stream.lock.Lock()
			defer stream.lock.Unlock()

			stream.__close()
		}()
	}
	c.streams = make(map[uint32]*Stream)
}

// Connect the socket.  Only usable if this is a client->server connection.
// Assumes both wLock and lock are held.
func (c *Conn) connect() (err error) {
	c.readerFinishWg = &sync.WaitGroup{}
	dlr := net.Dialer{Timeout: 30 * time.Second}
	c.conn, err = tls.DialWithDialer(&dlr, "tcp", c.clientConf.Addr, c.tlsConf)
	if err != nil {
		return err
	}
	c.readerFinishWg.Add(1)
	for _, stream := range c.streams {
		// It's ok if a receiver thinks there is no reader, even though
		// there is one --- so we don't need to lock stream.lock.
		stream.readerRunning = true
	}
	c.readerRunning = true
	go c.runReader()
	return nil
}

func (s *Stream) announceToSendCloseFlag() bool {
	s.lock.Lock()
	defer s.lock.Unlock()

	if !s.close_flag_send {
		s.close_flag_send = true
		return true
	}

	return false
}

// Close the connection.
func (s *Stream) Close() error {
	var err error

	if s.announceToSendCloseFlag() {
		err = s.conn._send(s.id, RawMessage{
			Data:       []byte{},
			IsResponse: s.Responding,
			Close:      true,
		})
	}

	s._close()
	return err
}

// Sends a raw message (ie. not a protobuf message) over the stream.
func (s *Stream) SendData(data []byte) error {
	return s.conn._send(s.id, RawMessage{
		Data:       data,
		HasPayload: true,
		IsResponse: s.Responding,
	})
}

// Sends a raw message with the close flag.
// Does not fully close the stream: use Close() after this to do so.
func (s *Stream) SendLastData(data []byte) error {
	if !s.announceToSendCloseFlag() {
		return fmt.Errorf("close flag already send")
	}

	err := s.conn._send(s.id, RawMessage{
		Data:       data,
		HasPayload: true,
		IsResponse: s.Responding,
		Close:      true,
	})
	return err
}

// Start a new stream by sending the given packet.
func (c *Conn) newStream(msg RawMessage) (*Stream, error) {
	// Allocate streamId and register stream
	stream := &Stream{
		conn: c,
	}
	stream.incoming = sync.NewCond(&stream.lock)

	c.lock.Lock()
	stream.id = c.streamId
	c.streamId += 1
	stream.readerRunning = c.readerRunning
	c.streams[stream.id] = stream
	c.lock.Unlock()

	firstTry := true
	for {
		// Try to send packet
		err := c._send(stream.id, msg)
		if err == nil {
			break
		}

		if !firstTry {
			stream._close()
			return nil, err
		}

		// Try to reconnect the socket and send the packet again.
		c.lock.Lock()
		c.wLock.Lock()
		delete(c.streams, stream.id)  // We deregister our stream...
		c.closeStreams()              //  ... so we can close all others ...
		c.streams[stream.id] = stream //  ... but keep this one open.
		firstTry = false
		c.readerFinishWg.Wait() // Wait for the old reader to finish
		err = c.connect()
		c.wLock.Unlock()
		c.lock.Unlock()

		if err != nil {
			return nil, err
		}
	}

	return stream, nil
}

// Send a message and expect a single reply
func (c *Conn) Request(msg Message) (resp Message, err error) {
	stream, err := c.RequestStream(msg)
	if err != nil {
		return nil, err
	}

	errs := []string{}
	sawClose := false

	for {
		// Wait for a response
		rawResp, err := stream.Receive()
		if err == io.EOF {
			break
		} else if err != nil {
			errs = append(errs, fmt.Sprintf("Receive(): %v", err))
			break
		}

		if !rawResp.IsResponse {
			errs = append(errs,
				"Received response without RESPONSE flag")
		}

		if rawResp.Close {
			if sawClose {
				errs = append(errs,
					"Received more than one message with CLOSE flag")
			}
			sawClose = true
		}

		var errstr string

		if rawResp.IsError {
			resp2, err2 := rawResp.Parse()
			if err2 != nil {
				errstr = fmt.Sprintf(
					"Reply with ERROR flag set which could not be parsed: %v",
					err2)
			} else {
				respErr, ok := resp2.(*pep_proto.Error)
				if ok {
					errstr = fmt.Sprintf("Received Error: %s", respErr.Description)
				} else {
					errstr = "Reply with ERROR flag set which is not an Error object"
				}
			}
		} else if !rawResp.HasPayload {
			if len(rawResp.Data) > 0 {
				errstr = "Received reply without PAYLOAD flag," +
					" but with data."
			}

			if !rawResp.Close {
				errstr = "Received reply without data and no " +
					"PAYLOAD, ERROR or CLOSE flag."
			}

			// no data, and a CLOSE flag - that's fine
		} else if resp != nil {
			errstr = "Unexpected second message with a PAYLOAD flag"
			// If the second message with payload is valid,
			// use requestStream instead.
		} else {
			resp, err = rawResp.Parse()
			if err != nil {
				errstr = err.Error()
			}
		}

		if errstr != "" {
			errs = append(errs, errstr)
		}
	}

	stream.Close() // makes sure we send a closing message

	if resp == nil {
		errs = append(errs, "Got no response with PAYLOAD flag")
	}

	if len(errs) > 0 {
		err = errors.New(strings.Join(errs, "\n"))
	}

	return resp, err
}

// Send a message and expect one or more replies in a stream.
func (c *Conn) RequestStream(msg Message) (*Stream, error) {
	data, err := Pack(msg)
	if err != nil {
		return nil, err
	}

	// Send
	return c.newStream(RawMessage{Data: data})
}

// Close the stream without deregistering it. Assumes lock is held.
func (s *Stream) __close() {
	if s.closed {
		return
	}
	s.closed = true
	s.incoming.Broadcast()
}

// Close the stream (without notifying the peer)
func (s *Stream) _close() {
	s.lock.Lock()
	s.__close()
	s.lock.Unlock()
	s.conn.deregisterStream(s)
}

func (c *Conn) deregisterStream(s *Stream) {
	c.lock.Lock()
	delete(c.streams, s.id)
	c.lock.Unlock()
}

// Send a raw message over the socket.
func (c *Conn) _send(streamId uint32, msg RawMessage) error {
	header := msg.header(streamId)
	c.wLock.Lock()
	defer c.wLock.Unlock()
	if c.conn == nil {
		return fmt.Errorf("Not connected (yet?)")
	}
	if c.timeout != 0 {
		c.conn.SetWriteDeadline(time.Now().Add(c.timeout))
		var zeroTime time.Time
		defer c.conn.SetWriteDeadline(zeroTime)
	}
	err := binary.Write(c.conn, binary.BigEndian, &header)
	if err != nil {
		return err
	}
	_, err = c.conn.Write(msg.Data)
	return err
}

// Sends a ping (PingRequest) and waits for a pong.
func (c *Conn) Ping() error {
	id := uint64(rand.Int63())
	rawResp, err := c.Request(&pep_proto.PingRequest{Id: id})
	if err != nil {
		return err
	}
	var actualResp *pep_proto.PingResponse
	switch resp := rawResp.(type) {
	case *pep_proto.PingResponse:
		actualResp = resp
	case *pep_proto.SignedPingResponse:
		almostActualResp, err := OpenSigned(rawResp)
		if err != nil {
			return fmt.Errorf("Ping(): couldn't open signed response")
		}
		actualResp = almostActualResp.(*pep_proto.PingResponse)
	default:
		return fmt.Errorf("Ping(): wrong response type")
	}
	if actualResp.Id != id {
		return fmt.Errorf("Ping(): wrong response id")
	}
	return nil
}

func (c *Conn) ensureConnected() error {
	c.lock.Lock()
	defer c.lock.Unlock()
	if c.conn != nil {
		return nil
	}
	c.wLock.Lock()
	defer c.wLock.Unlock()
	return c.connect()
}

// Returns TLS peer certificates of the connection
func (c *Conn) PeerCertificates() []*x509.Certificate {
	if c.ensureConnected() != nil {
		return nil
	}
	return c.conn.ConnectionState().PeerCertificates
}

// Requests version information on the running server's software
func (c *Conn) Version() (*pep_proto.VersionResponse, error) {
	rawResp, err := c.Request(&pep_proto.VersionRequest{})
	if err != nil {
		return nil, err
	}
	resp, ok := rawResp.(*pep_proto.VersionResponse)
	if !ok {
		return nil, fmt.Errorf("Version(): wrong response type")
	}
	return resp, nil
}
