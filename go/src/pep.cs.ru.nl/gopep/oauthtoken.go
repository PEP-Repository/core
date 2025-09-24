package pep

// Contains the unfortunately named OAuthToken, created by the authentication
// server to let a user authenticate herself against the keyserver.

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"
)

// OAuthToken is a misnomer.  A better name would be AuthToken.  The OAuthToken
// is created by the AuthenticationServer for an (to-the-AS-)authenticated user
// which uses it to authenticate herself against the KeyServer for enrollment.
// To this end, the AuthenticationServer and KeyServer share a secret, called
// the OAuthTokenSecret (and saved in ~.json).
type OAuthTokenData struct {
	User      string `json:"sub"`
	Group     string `json:"group"`
	ExpiresAt int64  `json:"exp,string"`
	IssuedAt  int64  `json:"iat,string"`
}

// Creates an OAuthToken.  See OAuthTokenData.
func CreateOAuthToken(oauthTokenSecret []byte, user, group string,
	duration time.Duration) string {
	now := time.Now()
	data := OAuthTokenData{
		User:      user,
		Group:     group,
		ExpiresAt: now.Add(duration).Unix(),
		IssuedAt:  now.Unix(),
	}
	dataBuf, err := json.Marshal(&data)
	if err != nil {
		panic(err)
	}
	macer := hmac.New(sha256.New, oauthTokenSecret)
	macer.Write(dataBuf)
	mac := macer.Sum(nil)
	return fmt.Sprintf("%s.%s",
		base64.RawURLEncoding.EncodeToString(dataBuf),
		base64.RawURLEncoding.EncodeToString(mac))
}

// Parses an OAuthToken.
//
// Warning: this function does not check validity of the token.
func ParseOAuthToken(token string) (data OAuthTokenData, mac []byte, err error) {
	bits := strings.SplitN(token, ".", 2)
	if len(bits) != 2 {
		err = errors.New("OAuthToken malformed: missing '.'")
		return
	}
	bits0, err := base64.RawURLEncoding.DecodeString(bits[0])
	if err != nil {
		return
	}
	bits1, err := base64.RawURLEncoding.DecodeString(bits[1])
	if err != nil {
		return
	}
	err = json.Unmarshal(bits0, &data)
	if err != nil {
		return
	}
	mac = bits1
	return
}
