#pragma once

#include <pep/networking/HTTPMessage.hpp>
#include <pep/storagefacility/S3Credentials.hpp>

// Tools to communicate with S3-compatible storage via the REST API.
//
// Requests to S3 are sent via relatively straighforward http(s) requests.
//
// The main difficulty in composing such requests is in following
// the authorization mechanism devised by Amazon: each request must include
// a carefully "Authorization" header, which, more or less, signs
// the request, and binds it to a region, date, and access key.
//
// For more information, see
//
//   https://docs.aws.amazon.com/AmazonS3/latest/API/


namespace pep::s3::request
{
  // Computes the value of the "Authorization" header.
  //
  // The headers "X-Amz-Date" and "X-Amz-Content-Sha256" should already be set.
  // (See helper functions below, or -better- use the "Sign" function.)
  //
  // Signs all headers by default, but if needed (for example, when conforming
  // to the examples from the test suite,) the set of headers to be signed
  // can be specified with the "sign_headers" argument.
  //
  // Restrictions/limitations:
  //
  //  1. the request.uriWithoutQuery should be a path
  //     such as "/bucket/object" without a host such as in
  //     "https://server.com/bucket/object".
  //
  //  2. the request.uriWithoutQuery should not be UriEncoded.
  //
  //  3. double headers such as
  //
  //       Header-One: A
  //       Header-One: B
  //
  //    are not supported (by HTTPRequest), but you can pass
  //
  //       Header-One: A,B
  //
  //  4. header's values can't contain double spaces, as in
  //     the get-header-value-trim test case of the AWS Signature Version 4
  //     test suite, see
  //     https://docs.aws.amazon.com/general/latest/gr
  //                                            /signature-v4-test-suite.html
  //
  //  5. double query parameters are not supported such as
  //
  //       /?Param1=A&Param1=B
  //
  std::string AuthorizationHeader(const HttpRequest& request,
      const Credentials& credentials,
      const std::vector<std::string>& sign_headers={});

  // Computes the value of the "X-Amz-Content-Sha256" header for the given body.
  std::string XAmzContentSha256Header(
     std::vector<std::shared_ptr<std::string>> bodyparts);

  inline std::string XAmzContentSha256Header(const std::string& body) {
    return XAmzContentSha256Header(std::vector<std::shared_ptr<std::string>>{
        std::make_shared<std::string>(body)});
  }

  // Computes the value of the "X-Amz-Date" header.
  std::string XAmzDateHeader();

  // Adds an "Authorization" header to the given HttpRequest,
  // and, if needed, "X-Amz-Content-Sha256" and "X-Amz-Date" headers, too.
  void Sign(HttpRequest& request, const Credentials& credentials);
}

namespace pep::s3
{
  // Returns current time formatted  in the Iso8601-variation used by
  // amazon S3: yyyymmddThhmmssZ.
  std::string TimeInAmzIso8601(void);

  // Computes the ETag we should expect after uploading the given object
  // in one part (using the PUT Object command, without server-side encryption.)
  std::string ETag(const std::string& object);
}
