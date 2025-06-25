package pep

// Code to upload/download/enumerate files from the storage facility

import (
	"generated.pep.cs.ru.nl/gopep/pep_proto"
	"pep.cs.ru.nl/gopep/pep_protocol"

	"github.com/golang/protobuf/proto"

	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/binary"
	"fmt"
	"io"
	"reflect"
	"sync/atomic"
)

// Information about a file returned by an enumeration request.
//
// Set fields vary by source of FileInfo.
type FileInfo struct {
	// Storage facility primary key
	Identifier string

	// Metadata
	Metadata Metadata

	// Polymorphic encryption key
	EncryptedKey Triple

	// Size of the pages
	PageSize uint32

	// Total filesize
	FileSize uint64

	// Whether the file is verified
	Verified bool

	// Ticket used for the request
	Ticket2 *SignedTicket2

	// Polymorphic pseudonym to which this file belongs
	PolymorphicPseudonym *Triple

	// Local pseudonyms for the participant to which this file belongs.
	Pseudonyms *LocalPseudonyms

	// Column to which this file belong
	Column string
}

// streamWriter contains the state shared between the file writers
// on the same stream.
type streamWriter struct {
	stream     *pep_protocol.Stream
	feedbackCs []chan<- (string)
	unfinished int32
}

type FileWriter struct {
	// Yields the StorageFacility primary key of the file,
	// after all filewriters on the same stream have closed.
	IdentifierC <-chan (string)

	sw       *streamWriter
	pageSize uint32

	gcm cipher.AEAD

	buffer     []byte
	bufferUsed uint32
	pageNo     uint64
	closed     bool

	metadata *Metadata
	index    uint32 // index of the associated DataStoreEntry2
}

type FileReader struct {
	gcm cipher.AEAD

	stream *pep_protocol.Stream

	fullBuffer []byte
	usedBuffer []byte
	toRead     int64

	metadata *Metadata
}

func newFileReader(stream *pep_protocol.Stream, key []byte,
	metadata *Metadata, fileSize uint64) (*FileReader, error) {

	blockCipher, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	gcm, err := cipher.NewGCMWithNonceSize(blockCipher, 16)
	if err != nil {
		return nil, err
	}

	return &FileReader{
		stream:     stream,
		gcm:        gcm,
		metadata:   metadata,
		fullBuffer: nil,
		usedBuffer: []byte{},
		toRead:     int64(fileSize),
	}, nil
}

func (r *FileReader) Read(p []byte) (int, error) {
	for len(r.usedBuffer) == 0 {
		if r.toRead <= 0 {
			return 0, io.EOF
		}
		rawResp, err := r.stream.ReceiveAndParse()
		if err != nil {
			return 0, err
		}
		resp, ok := rawResp.(*pep_proto.DataPayloadPage)
		if !ok {
			return 0, fmt.Errorf(
				"StorageFacility replied with unknown message: %v", rawResp)
		}

		if r.fullBuffer == nil {
			r.fullBuffer = make([]byte, len(resp.PayloadData))
		}

		ad, err := computeAdditionalData(r.metadata, resp.PageNumber)

		if err != nil {
			return 0, err
		}

		ct := append(resp.PayloadData, resp.CryptoMac...)
		_, err = r.gcm.Open(r.fullBuffer[:0], resp.CryptoNonce, ct, ad)
		if err != nil {
			return 0, err
		}
		r.toRead -= int64(len(resp.PayloadData))
		r.usedBuffer = r.fullBuffer[:len(resp.PayloadData)]
	}

	toCopy := len(r.usedBuffer)
	if toCopy > len(p) {
		toCopy = len(p)
	}

	copy(p[:], r.usedBuffer[:toCopy])
	r.usedBuffer = r.usedBuffer[toCopy:]
	return toCopy, nil
}

func (r *FileReader) Close() error {
	return r.stream.Close()
}

func computeAdditionalData(metadata *Metadata, pageNumber uint64) ([]byte, error) {
	switch metadata.EncryptionScheme {
	case pep_proto.EncryptionScheme_V1:
		return proto.Marshal(metadata.proto())
	case pep_proto.EncryptionScheme_V2:
		fallthrough
	case pep_proto.EncryptionScheme_V3:
		ret := make([]byte, 8)
		binary.BigEndian.PutUint64(ret, pageNumber)
		return ret, nil
	}
	return nil, fmt.Errorf("No such page encryption scheme: %v", metadata.EncryptionScheme)
}

func (sw *streamWriter) Send(buf []byte, final bool) error {

	if !final {
		return sw.stream.SendData(buf)
	}

	// record we finished, and check if we must send the final message
	unfinished := atomic.AddInt32(&sw.unfinished, -1)

	if unfinished < 0 {
		return fmt.Errorf(
			"too many 'final' messages were send over" +
				" this stream")
	}

	if unfinished > 0 {
		return sw.stream.SendData(buf)
	}

	// the honour is ours
	if err := sw.stream.SendLastData(buf); err != nil {
		return err
	}

	// wait for response
	rawResp, err := sw.stream.ReceiveAndParse()
	if err != nil {
		return err
	}
	resp, ok := rawResp.(*pep_proto.DataStoreResponse2)
	if !ok {
		return fmt.Errorf("StorageFacility response of wrong type: %v",
			reflect.TypeOf(rawResp))
	}

	if len(resp.Ids) != len(sw.feedbackCs) {
		return fmt.Errorf("StorageFacility returned incorrect number " +
			"of Ids in DataStoreResponse2")
	}

	for i, id := range resp.Ids {
		sw.feedbackCs[i] <- string(id)
		close(sw.feedbackCs[i])
	}

	return sw.stream.Close()
}

func newFileWriters(
	stream *pep_protocol.Stream,
	keys [][]byte,
	metadatas []*Metadata) (fws []*FileWriter, err error) {

	if len(keys) != len(metadatas) {
		return nil, fmt.Errorf("len(keys)!=len(metadatas)")
	}

	N := len(keys)

	sw := &streamWriter{
		stream:     stream,
		feedbackCs: make([]chan<- (string), N),
		unfinished: (int32)(N),
	}

	fws = make([]*FileWriter, N)

	for i := 0; i < N; i++ {
		fc := make(chan (string), 1)
		// "1", because we do not want to be blocked when reporting
		// the files' identities

		sw.feedbackCs[i] = fc

		fws[i], err = newFileWriter(sw, uint32(i),
			fc, keys[i], metadatas[i])
		if err != nil {
			return nil, fmt.Errorf("file #%v: %w", i, err)
		}
	}

	return
}

func newFileWriter(sw *streamWriter, index uint32, identifierC <-chan (string),
	key []byte, metadata *Metadata) (

	*FileWriter, error) {
	blockCipher, err := aes.NewCipher(key)
	if err != nil {
		return nil, err
	}
	gcm, err := cipher.NewGCMWithNonceSize(blockCipher, 16)
	if err != nil {
		return nil, err
	}

	pageSize := uint32(1024 * 1024 * 8 / 10) // Use only 80% of (debug build) capacity

	return &FileWriter{
		IdentifierC: identifierC,
		sw:          sw,
		gcm:         gcm,
		pageSize:    pageSize,
		metadata:    metadata,
		buffer:      make([]byte, pageSize),
		index:       index,
	}, nil
}

func (w *FileWriter) write(d []byte, final bool) error {
	var nonce [16]byte
	rand.Read(nonce[:])
	ad, err := computeAdditionalData(w.metadata, w.pageNo)

	if err != nil {
		return err
	}

	maccedCipherText := w.gcm.Seal(nil, nonce[:], d, ad)

	buf, err := pep_protocol.Pack(&pep_proto.DataPayloadPage{
		PayloadData: maccedCipherText[:len(maccedCipherText)-16],
		CryptoNonce: nonce[:],
		CryptoMac:   maccedCipherText[len(maccedCipherText)-16:],
		PageNumber:  w.pageNo,
		Index:       w.index,
	})
	if err != nil {
		return err
	}

	return w.sw.Send(buf, final)
}

func (w *FileWriter) Write(d []byte) (int, error) {
	nWritten := 0
	for len(d) > 0 {
		toCopy := uint32(len(d))
		if toCopy+w.bufferUsed > w.pageSize {
			toCopy = w.pageSize - w.bufferUsed
		}
		copy(w.buffer[w.bufferUsed:], d[:toCopy])
		nWritten += int(toCopy)
		w.bufferUsed += toCopy
		d = d[toCopy:]

		if w.bufferUsed == w.pageSize {
			err := w.write(w.buffer[:], false)
			if err != nil {
				return nWritten, err
			}

			w.pageNo += 1
			w.bufferUsed = 0
		}
	}
	return nWritten, nil
}

func (w *FileWriter) Close() error {
	if !w.closed {
		// Send final message
		w.closed = true
		err := w.write(w.buffer[:w.bufferUsed], true)
		if err != nil {
			return err
		}

	}
	return nil
}
