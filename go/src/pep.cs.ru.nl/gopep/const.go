// go:generate enumer -type=FacilityType -yaml -output=const_enumer.go

package pep

// Contains helpers to deal with the constants, enums and flags in the
// PEP protocol.

import (
	"time"

	"generated.pep.cs.ru.nl/gopep/pep_proto"
)

// Represents the type of the facility, eg. user or storage facility
type FacilityType int

const (
	FTZero FacilityType = iota
	FTUser
	FTStorageFacility
	FTAccessManager
	FTTranscryptor
)

// File metadata
type Metadata struct {
	// Filetype, ie. participantinfo
	Tag string

	// Timestamp of file
	Timestamp time.Time

	EncryptionScheme pep_proto.EncryptionScheme
}

func (m *Metadata) proto() *pep_proto.Metadata {
	var ret pep_proto.Metadata
	ret.Tag = m.Tag
	millis := m.Timestamp.UnixNano() / 1000000
	// UnixNano() doesn't go before 1678 or after 2262.
	if millis/1000 != m.Timestamp.Unix() {
		millis = m.Timestamp.Unix() * 1000
	}
	ret.Timestamp = &pep_proto.Timestamp{
		EpochMillis: millis,
	}
	ret.EncryptionScheme = m.EncryptionScheme
	return &ret
}

func (m *Metadata) setProto(v *pep_proto.Metadata) {
	m.Tag = v.Tag
	ts := v.Timestamp.EpochMillis
	m.Timestamp = time.Unix(ts/1000, (ts%1000)*1000000)
	m.EncryptionScheme = v.EncryptionScheme
}

// New-style ticket request
type TicketRequest struct {
	Modes                 []string  // Modes of access to request
	ParticipantGroups     []string  // groups of participants to request access to
	PolymorphicPseudonyms []*Triple // (additional) pps to request access to
	ColumnGroups          []string  // column groups to request access to
	Columns               []string  // (additional) columns to request access to
}

func (t *TicketRequest) proto() *pep_proto.TicketRequest2 {
	var ret pep_proto.TicketRequest2
	ret.PolymorphicPseudonyms = make([]*pep_proto.ElgamalEncryption,
		len(t.PolymorphicPseudonyms))
	for i := 0; i < len(t.PolymorphicPseudonyms); i++ {
		ret.PolymorphicPseudonyms[i] = t.PolymorphicPseudonyms[i].proto()
	}
	ret.Modes = t.Modes
	ret.ParticipantGroups = t.ParticipantGroups
	ret.ColumnGroups = t.ColumnGroups
	ret.Columns = t.Columns
	return &ret
}

type LocalPseudonyms struct {
	AccessManager   Triple
	StorageFacility Triple
	Polymorphic     Triple
}

func (p *LocalPseudonyms) setProto(v *pep_proto.LocalPseudonyms) {
	p.AccessManager.setProto(v.AccessManager)
	p.StorageFacility.setProto(v.StorageFacility)
	p.Polymorphic.setProto(v.Polymorphic)
}
