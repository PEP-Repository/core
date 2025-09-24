package pep

import (
	"github.com/bwesterb/go-ristretto"
)

// A constellation is the shared public configuration between the PEP users
// and all its facilities.
type Constellation struct {
	// Address of Transcryptor
	TranscryptorAddr string `yaml:"transcryptorAddr"`

	// Address of AM
	AccessManagerAddr string `yaml:"accessManagerAddr"`

	// Address of KS
	KeyServerAddr string `yaml:"keyServerAddr"`

	// Address of StorageFacility
	StorageFacilityAddr string `yaml:"storageFacilityAddr"`

	// Address of RegistrationServer
	RegistrationServerAddr string `yaml:"registrationServerAddr"`

	// Address of Authserver
	AuthserverAddr string `yaml:"authserverAddr"`

	// Certificate of root CA.  Either this or RootCaPath should be set.
	RootCaPem string `yaml:"rootCaPEM"`

	// Certificate of root CA.  Either this or RootCaPem should be set.
	RootCaPath string `yaml:"rootCaPath"`

	// Public master key of the data PEP subsystem
	DataPK ristretto.Point `yaml:"dataPk"`

	// Public master key of the pseudonym PEP subsystem
	PseudonymPK ristretto.Point `yaml:"pseudonymPk"`
}
