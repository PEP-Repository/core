package pep_protocol

// Parses and creates the network messages used in PEP.

import (
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"reflect"
	"strconv"
	"strings"

	"github.com/OneOfOne/xxhash"
	"github.com/golang/protobuf/proto"

	"generated.pep.cs.ru.nl/gopep/pep_proto"

	"pep.cs.ru.nl/gopep/build"
)

// TODO autogenerate
// See https://github.com/golang/protobuf/issues/194
var messageNames = []string{
	"X509CertificateSigningRequest",
	"EncryptionKeyRequest",
	"SignedEncryptionKeyRequest",
	"EncryptionKeyResponse",
	"TranscryptorRequest",
	"TranscryptorResponse",
	"EnrollmentRequest",
	"EnrollmentResponse",
	"KeyComponentRequest",
	"SignedKeyComponentRequest",
	"KeyComponentResponse",
	"RekeyRequest",
	"RekeyResponse",
	"RegistrationRequest",
	"SignedRegistrationRequest",
	"RegistrationResponse",
	"MetricsRequest",
	"MetricsResponse",
	"SignedMetricsRequest",
	"PingRequest",
	"PingResponse",
	"SignedPingResponse",
	"ChecksumChainNamesRequest",
	"ChecksumChainNamesResponse",
	"ChecksumChainRequest",
	"ChecksumChainResponse",
	"SignedChecksumChainNamesRequest",
	"SignedChecksumChainRequest",
	"DataPayloadPage",
	"Error",
	"SignedTicket2",
	"Ticket2",
	"SignedTicketRequest2",
	"DataEnumerationResponse2",
	"SignedDataReadRequest2",
	"SignedDataEnumerationRequest2",
	"DataStoreResponse2",
	"SignedDataStoreRequest2",
	"VersionResponse",
	"VersionRequest",
	"SignedDataHistoryRequest2",
	"DataHistoryResponse2",
}
var magicToMessage map[uint32]reflect.Type

type Message proto.Message

type messageHeader struct {
	Size uint32

	// Four bit-flags and a stream id:
	// RESPONSE           1 bit
	// CLOSE              1 bit
	// ERROR              1 bit
	// PAYLOAD            1 bit
	// StreamId           28 bits
	FlagsAndStreamId uint32
}

func (h *messageHeader) SetStreamId(id uint32) {
	h.FlagsAndStreamId = (h.FlagsAndStreamId & 0xf0000000) | (id & 0x0fffffff)
}

func (h *messageHeader) StreamId() uint32 {
	return h.FlagsAndStreamId & 0x0fffffff
}

func (h *messageHeader) IsRequest() bool {
	return (h.FlagsAndStreamId & 0x80000000) == 0
}

func (h *messageHeader) IsResponse() bool {
	return !h.IsRequest()
}

func (h *messageHeader) SetIsRequest() {
	h.FlagsAndStreamId &= 0x7fffffff
}

func (h *messageHeader) SetIsResponse() {
	h.FlagsAndStreamId |= 0x80000000
}

func (h *messageHeader) Close() bool {
	return h.FlagsAndStreamId&0x40000000 != 0
}

func (h *messageHeader) SetClose(v bool) {
	h.FlagsAndStreamId &= 0xbfffffff
	if v {
		h.FlagsAndStreamId |= 0x40000000
	}
}

func (h *messageHeader) IsError() bool {
	return h.FlagsAndStreamId&0x20000000 != 0
}

func (h *messageHeader) SetIsError(v bool) {
	h.FlagsAndStreamId &= 0xdfffffff
	if v {
		h.FlagsAndStreamId |= 0x20000000
	}
}

func (h *messageHeader) HasPayload() bool {
	return h.FlagsAndStreamId&0x10000000 != 0
}

func (h *messageHeader) SetHasPayload(v bool) {
	h.FlagsAndStreamId &= 0xefffffff
	if v {
		h.FlagsAndStreamId |= 0x10000000
	}
}

func (h messageHeader) String() string {
	flags := []string{}
	if h.IsResponse() {
		flags = append(flags, "resp")
	}
	if h.Close() {
		flags = append(flags, "close")
	}
	if h.HasPayload() {
		flags = append(flags, "payl")
	}
	if h.IsError() {
		flags = append(flags, "err")
	}
	return fmt.Sprintf("[size=%d stream=%d flags=%s]",
		h.Size, h.StreamId(), strings.Join(flags, ","))
}

// Creates an empty proto.Message of the type with the given magic
func zeroMessage(m uint32) Message {
	tpe, ok := magicToMessage[m]
	if !ok {
		return nil
	}
	return reflect.New(tpe.Elem()).Interface().(Message)
}

func Pack(x Message) ([]byte, error) {
	protoBytes, err := proto.Marshal(x)
	if err != nil {
		return nil, err
	}
	buf := append([]byte{0, 0, 0, 0}, protoBytes...)
	binary.BigEndian.PutUint32(buf, magic(x))
	return buf, nil
}

func Unpack(buf []byte) (Message, error) {
	if len(buf) < 4 {
		return nil, errors.New("message.Unpack(): message too short")
	}
	magic := binary.BigEndian.Uint32(buf)
	ret := zeroMessage(magic)
	if ret == nil {
		return nil, fmt.Errorf("Unknown message type")
	}
	err := proto.Unmarshal(buf[4:], ret)
	if err != nil {
		return nil, err
	}
	return ret, nil
}

// Function type that signs a message.
//
// See CertifiedSigningPrivateKey.signMessage().
type MessageSigningFunc func(data []byte) (*pep_proto.Signature, error)

// Turns a message M into its corresponding SignedM using the
// provided MessageSigningFunc.
func SignMessage(msg Message, sign MessageSigningFunc) (Message, error) {
	name := nameOf(msg)
	sTpe := proto.MessageType("pep.proto.Signed" + name)
	if sTpe == nil {
		return nil, fmt.Errorf("No such message: %s", "Signed"+name)
	}
	sMsgVal := reflect.New(sTpe.Elem())
	data, err := Pack(msg)
	if err != nil {
		return nil, err
	}
	sig, err := sign(data)
	if err != nil {
		return nil, err
	}
	sMsgVal.Elem().FieldByName("Signature").Set(reflect.ValueOf(sig))
	sMsgVal.Elem().FieldByName("Data").SetBytes(data)
	return sMsgVal.Interface().(Message), nil
}

// If this is a signed message (SignedTicket2, SignedEncryptionKeyRequest, ...)
// returns the wrapped message.  Does not check the signature (yet).
func OpenSigned(m Message) (Message, error) {
	data := reflect.ValueOf(m).Elem().FieldByName("Data").Interface().([]byte)
	return Unpack(data)
}

// Returns the name of the message
func nameOf(msg Message) string {
	fullName := proto.MessageName(msg)
	nameBits := strings.Split(fullName, ".")
	name := nameBits[len(nameBits)-1]
	return name
}

// Returns the type identifier ("magic") for the given message.
func magic(x Message) uint32 {
	return magicForName(nameOf(x))
}

func magicForName(name string) uint32 {
	return xxhash.Checksum32S([]byte(name), 0xcafebabe)
}

func init() {
	magicToMessage = make(map[uint32]reflect.Type)
	for _, name := range messageNames {
		protoname := "pep.proto." + name
		tpe := proto.MessageType(protoname)
		if tpe == nil {
			log.Panicf("Could not find type %v", protoname)
		}
		magicToMessage[magicForName(name)] = tpe
	}
}

func GetCurrentBuildVersionResponse() (*pep_proto.VersionResponse, error) {
	// Create return value with compile time version info
	result := &pep_proto.VersionResponse{
		ProjectPath:      build.BuildProjectPath,
		Target:           build.BuildTarget,
		MajorVersion:     build.BuildMajorVersion,
		MinorVersion:     build.BuildMinorVersion,
		Reference:        build.BuildReference,
		PipelineId:       build.BuildPipelineId,
		JobId:            build.BuildJobId,
		Commit:           build.BuildCommit,
		ProtocolChecksum: (fmt.Sprintf("%02x", build.MANUAL_PROTOCOL_CHECKSUM_COMPONENT) + pep_proto.MESSAGES_PROTO_CHECKSUM)[0:20],
	}

	// If a configVersion.json exists, add its data to our return value
	jsonFile, err := os.Open("configVersion.json")
	if err == nil {
		defer jsonFile.Close()

		bytes, err := ioutil.ReadAll(jsonFile)
		if err != nil {
			return nil, err
		}

		type ConfigVersionJson struct {
			Reference      string `json:"reference"`
			PipelineId     int    `json:"pipelineId"`
			MajorVersion   int    `json:"majorVersion"`
			MinorVersion   int    `json:"minorVersion"`
			JobId          int    `json:"jobId"`
			Commit         string `json:"commit"`
			ProjectPath    string `json:"projectPath"`
			ProjectCaption string `json:"projectCaption"`
		}

		var configVersionJson ConfigVersionJson
		json.Unmarshal(bytes, &configVersionJson)

		result.ConfigVersion = &pep_proto.ConfigVersion{
			Reference:      configVersionJson.Reference,
			MajorVersion:   strconv.Itoa(configVersionJson.MajorVersion),
			MinorVersion:   strconv.Itoa(configVersionJson.MinorVersion),
			PipelineId:     strconv.Itoa(configVersionJson.PipelineId),
			JobId:          strconv.Itoa(configVersionJson.JobId),
			Commit:         configVersionJson.Commit,
			ProjectPath:    configVersionJson.ProjectPath,
			ProjectCaption: configVersionJson.ProjectCaption,
		}
	}

	return result, nil
}
