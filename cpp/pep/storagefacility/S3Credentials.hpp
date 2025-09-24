#pragma once

#include <pep/networking/HTTPMessage.hpp>

namespace pep::s3 
{
  // Information needed to access S3.
  struct Credentials {
    std::string accessKey, secret; // that is, username & password
    
    std::string service = "s3";

    // Amazon's original S3 cloud storage is region bound;
    // I'm not sure the other S3 providers care about the value of this field.
    // The value "us-east-1" is the default in all examples.
    std::string region = "us-east-1";
  };

  using HttpRequest = pep::HTTPRequest;
}
