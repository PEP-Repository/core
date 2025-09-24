package shared

import (
	"io/ioutil"
	"log"
	"os"

	"gopkg.in/yaml.v2"
)

var TokenFile TokenFileType

type TokenFileType struct {
	// Generate using `./apps/pepToken`
	Token      string `yaml:"token"` //deprecated, use OAuthToken, but kept for warnings
	OAuthToken string `yaml:"OAuthToken"`

	GitlabAPIToken string `yaml:"gitlabAPIToken"`
}

func LoadTokenFile() {
	if Conf.Token != "" {
		log.Fatalf("%v: 'token' is deprecated; "+
			"use 'tokenFile' instead.", Args.ConfPath)
	}

	if Conf.TokenFile == "" {
		log.Fatalf("Error: %v: 'tokenFile' not set", Args.ConfPath)
	}

	if _, err := os.Stat(Conf.TokenFile); os.IsNotExist(err) {
		log.Fatalf("Error: could not find token file: %s\n\n",
			Conf.TokenFile)
	}

	buf, err := ioutil.ReadFile(Conf.TokenFile)
	if err != nil {
		log.Fatalf("Could not read token file %s: %v",
			Conf.TokenFile, err)
	}
	err = yaml.Unmarshal(buf, &TokenFile)
	if err != nil {
		log.Fatalf("Could not parse token file: %v", err)
	}

	if TokenFile.Token != "" {
		log.Fatalf("Error: %v: 'token' is deprecated;"+
			" use 'OAuthToken' instead", Conf.TokenFile)
	}
}
