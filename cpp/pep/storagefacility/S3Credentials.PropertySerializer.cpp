#include <pep/storagefacility/S3Credentials.PropertySerializer.hpp>
#include <pep/utils/Configuration.hpp>

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
      const DeserializationContext& context) const {

    s3::Credentials result;

    // To allow the 'Secret' to be stored in a separate file,
    // we use the following '#include' mechanism.
    auto include = DeserializeProperties<std::optional<std::filesystem::path>>
        (source, "Include", context);

    if (include.has_value()) {
      Configuration config = Configuration::FromFile(*include);
      result = config.get<s3::Credentials>("");
    }

    auto accessKeyOp = DeserializeProperties<std::optional<std::string>>(source,
        "AccessKey", context);
    if (accessKeyOp.has_value())
      result.accessKey = *accessKeyOp;

    if (result.accessKey.empty())
      throw std::runtime_error("Deserializing S3 Credentials: AccessKey not set");

    auto secretKeyOp = DeserializeProperties<std::optional<std::string>>(source,
        "Secret", context);
    if (secretKeyOp.has_value())
      result.secret = *secretKeyOp;

    if (result.secret.empty())
      throw std::runtime_error("Deserializing S3 Credentials: Secret not set");

    auto serviceOp = DeserializeProperties<std::optional<std::string>>(source,
        "Service", context);
    if (serviceOp.has_value())
      result.service = *serviceOp;

    auto regionOp = DeserializeProperties<std::optional<std::string>>(source,
        "Region", context);
    if (regionOp.has_value())
      result.region = *regionOp;

    return result;
  }
}
