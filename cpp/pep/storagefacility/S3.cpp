#include <pep/storagefacility/S3.hpp>

#include <pep/utils/Sha.hpp>
#include <pep/utils/Platform.hpp>
#include <pep/utils/UriEncode.hpp>
#include <pep/storagefacility/PageHash.hpp>
#include <pep/utils/Configuration.hpp>

#include <sstream>

#include <boost/algorithm/hex.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>


// See S3.hpp for (more) documentation.

namespace pep::s3::request
{

  void Sign(HttpRequest& request, const Credentials& credentials)
  {
    if(request.hasHeader("Authorization"))
      throw std::runtime_error("Authorization header already set");

    if(!request.hasHeader("X-Amz-Date"))
      request.setHeader("X-Amz-Date", XAmzDateHeader());

    if(!request.hasHeader("X-Amz-Content-Sha256"))
      request.setHeader("X-Amz-Content-Sha256",
          XAmzContentSha256Header(request.getBodyparts()));

    request.setHeader("Authorization",
        AuthorizationHeader(request, credentials));
  }


  std::string XAmzContentSha256Header(
      std::vector<std::shared_ptr<std::string>> bodyparts) {

    Sha256 hasher;
    for (auto part : bodyparts)
      hasher.update(*part);
    return boost::algorithm::to_lower_copy(boost::algorithm::hex(
          hasher.digest()));
  }


  std::string XAmzDateHeader() {
    return TimeInAmzIso8601();
  }

}


namespace pep::s3
{
  // The AuthorizationHeader function is based on:
  //
  // https://docs.aws.amazon.com
  //    /AmazonS3/latest/API/sig-v4-header-based-auth.html
  //
  // We need some helper functions and a "Context" struct,
  // which we'll stow in in the following namespace.
  namespace authorization_header
  {

    struct Context {
      const HttpRequest& request;
      const Credentials& credentials;
      const std::vector<std::string>& sign_headers;
      std::string signed_headers = ""; // e.g. "host;range;x-amz-date"
    };


    std::string ComputeAuthorizationHeader(Context& c);
  }


  std::string request::AuthorizationHeader(
      const HttpRequest& r, const Credentials& c,
      const std::vector<std::string>& s)
  {
    authorization_header::Context context = { r, c, s };
    return authorization_header::ComputeAuthorizationHeader(context);
  }


  namespace authorization_header
  {

    std::string GetScope(const Context&c);
    std::string ComputeSignature(Context& c);


    std::string ComputeAuthorizationHeader(Context& c)
    {
      if (!c.request.hasHeader("X-Amz-Date"))
        throw std::runtime_error("X-Amz-Date not set");

      std::string datetime = c.request.header("X-Amz-Date");

      if( datetime.length() != 16
          || datetime[8] != 'T'
          || datetime[15] != 'Z' )
        throw std::runtime_error("Unsupported X-Amz-Date format");

      if (!c.request.hasHeader("X-Amz-Content-Sha256"))
        throw std::runtime_error("X-Amz-Content-Sha256 not set");

      // We do not allow spaces in access keys, for these are not dealt with
      // correctly by minio.

      if (c.credentials.accessKey.find(' ') != std::string::npos)
        throw std::runtime_error("There is a space (' ') in the access key; "
            "not all S3 servers can't deal with that.");

      // Credentials are sometimes restricted further to the following form.
      //
      //   [A-Z0-9\/]{20}      for access keys
      //   [a-zA-Z0-9\/+]{40}  for secret keys
      //
      // But we'll be lenient.

      std::string signature = ComputeSignature(c);
      // NB. ComputeSignature sets c.signed_headers

      std::ostringstream ss;

      ss  << "AWS4-HMAC-SHA256 "
          << "Credential="
            << c.credentials.accessKey
            << "/" << GetScope(c) << ", "
          << "SignedHeaders=" << c.signed_headers << ", "
          << "Signature=" << signature;

      return ss.str();
    }

    std::string GetDateTime(const Context& c) {
      return c.request.header("X-Amz-Date");
    }

    std::string GetDate(const Context& c) {
      // Assuming X-Amz-Date is of the form yyyymmddThhmmssZ
      return GetDateTime(c).substr(0,8);
    }


    std::string GetScope(const Context &c) {
      std::ostringstream ss;

      ss  << GetDate(c) << "/" << c.credentials.region
          << "/" << c.credentials.service << "/aws4_request";

      return ss.str();
    }


    std::string ComputeSigningKey(const Context& c);
    std::string ComputeStringToSign(Context& c);


    std::string ComputeSignature(Context& c) {
      return boost::algorithm::to_lower_copy(boost::algorithm::hex(
            Sha256::HMac(ComputeSigningKey(c),
              ComputeStringToSign(c))));
    }


    std::string ComputeSigningKey(const Context& c) {
      std::string prekey = "AWS4" + c.credentials.secret;

      prekey = Sha256::HMac(prekey, GetDate(c));
      prekey = Sha256::HMac(prekey, c.credentials.region);
      prekey = Sha256::HMac(prekey, c.credentials.service);

      return Sha256::HMac(prekey, "aws4_request");
    }


    std::string ComputeCanonicalRequest(Context& c);


    std::string ComputeStringToSign(Context& c) {
      std::ostringstream ss;

      ss  << "AWS4-HMAC-SHA256\n"
          << GetDateTime(c) << "\n"
          << GetScope(c) << "\n"
          << boost::algorithm::to_lower_copy(boost::algorithm::hex(
                Sha256().digest(ComputeCanonicalRequest(c))));

      return ss.str();
    }


    std::string ComputeCanonicalQuery(const Context& c);
    std::string ComputeCanonicalHeaders(Context& c);


    std::string ComputeCanonicalRequest(Context& c) {
      std::ostringstream ss;

      ss  << c.request.getMethod() << "\n"
          // NB. We've required that request.getUriWithoutQuery()
          // contains no domain name, and is not already URI-encoded,
          // so that we can easily compute the "CanonicalURI" here:
          << UriEncode(c.request.getUri().getPath(), false) << "\n"
          << ComputeCanonicalQuery(c) << "\n"
          << ComputeCanonicalHeaders(c) << "\n" // sets c.signed_headers
          << c.signed_headers << "\n"
          << c.request.header("X-Amz-Content-Sha256");

      return ss.str();
    }


    std::string ComputeCanonicalQuery(const Context& c)
    {
      const std::map<std::string, std::string>& queries(c.request.getQueries());
      std::vector<std::string> keys;
      keys.reserve(queries.size());

      for (const auto& [key, value] : queries)
        keys.push_back(key);

      std::sort(keys.begin(), keys.end());

      std::ostringstream ss;

      for (const auto& key : keys) {
        if (&key != &keys.front()) {
          ss << "&";
        }
        ss << UriEncode(key) << "=" << UriEncode(queries.at(key));
      }

      return ss.str();
    }


    std::string ComputeCanonicalHeaders(Context& c) {
      const std::map<std::string, std::string, CaseInsensitiveCompare>& headers(c.request.getHeaders());
      std::vector<std::string> keys;

      if(c.sign_headers.empty()) {
        // sign all headers
        for (const auto& [key, value] : headers)
          keys.push_back(key);
      } else
        keys = c.sign_headers;

      std::sort(keys.begin(), keys.end());

      std::ostringstream ss_result;
      std::ostringstream ss_signed_headers;

      for (const auto& key : keys) {
        std::string key_lower_case = boost::algorithm::to_lower_copy(key);
        ss_result << key_lower_case << ":"
                  << boost::algorithm::trim_copy(headers.at(key)) << "\n";
        if (&key != &keys.front()) {
          ss_signed_headers << ';';
        }
        ss_signed_headers << key_lower_case;
      }

      c.signed_headers = ss_signed_headers.str();

      return ss_result.str();
    }


  }


  std::string TimeInAmzIso8601() {
    std::ostringstream ss;
    std::time_t t(std::time(nullptr));

    std::tm tm;
    if (!gmtime_r(&t, &tm)) {
      throw std::runtime_error("Failed to convert time");
    }
    ss << std::put_time(&tm, "%Y%m%dT%H%M%SZ");

    return ss.str();
  }


  std::string ETag(const std::string& object) {
    return pep::ETag(object);  // see storagefacility/PageHash.hpp
  }
}

namespace pep
{
  void PropertySerializer<s3::Credentials>::write(
      boost::property_tree::ptree& destination,
      const s3::Credentials& value) const {

    SerializeProperties(destination, "AccessKey", value.accessKey);
    SerializeProperties(destination, "Secret", value.secret);
    SerializeProperties(destination, "Service", value.service);
    SerializeProperties(destination, "Region", value.region);
  }

  s3::Credentials PropertySerializer<s3::Credentials>::read(
      const boost::property_tree::ptree& source,
      const MultiTypeTransform& transform) const {

    s3::Credentials result;

    // To allow the 'Secret' to be stored in a separate file,
    // we use the following '#include' mechanism.
    auto include = DeserializeProperties<std::optional<std::filesystem::path>>
        (source, "Include", transform);

    if (include.has_value()) {
      Configuration config = Configuration::FromFile(*include);
      result = config.get<s3::Credentials>("");
    }

    auto accessKeyOp = DeserializeProperties<std::optional<std::string>>(source,
        "AccessKey", transform);
    if (accessKeyOp.has_value())
      result.accessKey = *accessKeyOp;

    if (result.accessKey.empty())
      throw std::runtime_error("Deserializing S3 Credentials: AccessKey not set");

    auto secretKeyOp = DeserializeProperties<std::optional<std::string>>(source,
        "Secret", transform);
    if (secretKeyOp.has_value())
      result.secret = *secretKeyOp;

    if (result.secret.empty())
      throw std::runtime_error("Deserializing S3 Credentials: Secret not set");

    auto serviceOp = DeserializeProperties<std::optional<std::string>>(source,
        "Service", transform);
    if (serviceOp.has_value())
      result.service = *serviceOp;

    auto regionOp = DeserializeProperties<std::optional<std::string>>(source,
        "Region", transform);
    if (regionOp.has_value())
      result.region = *regionOp;

    return result;
  }
}



