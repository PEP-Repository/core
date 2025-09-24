#!/usr/bin/env python3

import binascii
import argparse
import json
import os.path
import base64
import hmac
import hashlib
import time

class Program:
    def run(self):
        parser = argparse.ArgumentParser()
        parser.add_argument("--secret-json",
                help="loads the token secret from this file, "
                        "if the file exists.",
                type=secret_in_json_file,
                default="OAuthTokenSecret.json")
        parser.add_argument("--secret",
                help="pass the token secret directly",
                type=token_secret)
        parser.set_defaults(func=self.help, parser=parser)

        subparsers = parser.add_subparsers()
        verify_parser = subparsers.add_parser("verify")
        verify_parser.add_argument("token", type=token)
        verify_parser.set_defaults(func=self.verify, parser=verify_parser)


        self.args = parser.parse_args()
        
        self.secret = self.args.secret_json \
                if self.args.secret_json != None \
                else self.args.secret
        
        self.args.func()

    def help(self):
        self.args.parser.print_help()

    def verify(self):
        if self.secret==None:
            raise ValueError("no secret specified: "
                    "no --secret given, and the --secret-json file "
                    "does not exist.")
        print(self.args.token.payload.decode("utf-8"))
        res = self.args.token.verify(self.secret)
        if res.valid:
            print("token is valid")
            return
        print("token is not valid:")
        print(res.reason)

# argument types

def token_secret(secret_hex):
    if len(secret_hex)!=64:
        raise argparse.ArgumentTypeError("the length of the secret "
                "is not 64 characters.")
    return binascii.unhexlify(secret_hex)

def secret_in_json_file(name):
    json = json_file(name)
    if "OAuthTokenSecret" not in json:
        raise argparse.ArgumentTypeError("json file contains no field "
                "'OAuthTokenSecret'")
    return token_secret(json["OAuthTokenSecret"])

def json_file(name):
    if not os.path.isfile(name):
        return None
    with open(name, "r") as f:
        return json.load(f)

def token(token):
    try:
        return token_(token)
    except Exception as e:
        raise argparse.ArgumentTypeError(repr(e))

def token_(token):
    parts = token.split(".")

    if len(parts)!=2:
        diagnosis =  "the provided token has only one part." if len(parts)==1 \
                else "the provided token has %s parts." % len(parts) 
        raise argparse.ArgumentTypeError("a token should consists of"
                " two parts separated by a '.'; " + diagnosis)

    payload_b64,hmac_b64 = parts

    payload = base64decode(payload_b64)
    hmac = base64decode(hmac_b64)
    
    try:
        token = Token(payload, hmac)
    except ValueError as e:
        raise argparse.ArgumentTypeError("invalid payload: %s" % e)

    return token

def base64decode(b64):
    b64 += "="*(len(b64)%4) # add padding when missing
    return base64.urlsafe_b64decode(b64)


class Token:
    def __init__(self, payload, hmac):
        self.payload = payload
        self.hmac = hmac

        try:
            self.payload_json = json.loads(payload)
        except json.decoder.JSONDecodeError as e:
            raise ValueError("invalid json: %s" % e)
        if 'exp' not in self.payload_json:
            raise ValueError("'exp' field of not set")

    def payload_hmac(self, secret):
        return hmac.new(secret, self.payload, 
                digestmod=hashlib.sha256).digest()


    def verify(self, secret):
        h = self.payload_hmac(secret)
        if h!=self.hmac:
            return VerificationResult(False, "the hmac of the payload is "
                    "%s instead of %s." % (
                        base64.urlsafe_b64encode(h).decode("utf-8"),
                        base64.urlsafe_b64encode(self.hmac).decode("utf-8")))
        assert 'exp' in self.payload_json
        expires_at = int(self.payload_json['exp'])
        if expires_at < time.time():
            return VerificationResult(False, "expired %s ago"
                    % describe_seconds(int(time.time()-expires_at)))
        return VerificationResult(True)

    
class VerificationResult:
    def __init__(self, valid, reason=None):
        self.valid = valid
        self.reason = reason

def describe_seconds(s):
    seconds = s % 60
    s //= 60
    minutes = s % 60
    s //= 60
    hours = s % 24
    s //= 24
    days = s

    parts = []
    if days>0:
        part = "%s day" % days
        if days>1:
            part += "s"
        parts.append(part)
    if hours>0:
        part = "%s hour" % hours
        if hours>1:
            part += "s"
        parts.append(part)
    if minutes>0:
        part = "%s minute" % minutes
        if minutes>1:
            part += "s"
        parts.append(part)
    if seconds>0:
        part = "%s second" % seconds
        if seconds>1:
            part += "s"
        parts.append(part)
    
    if len(parts)==0:
        return "0 seconds"
    if len(parts)==1:
        return parts[0]
    return " ".join(parts[:-1]) + " and " + parts[-1]

if __name__=="__main__":
    Program().run()
