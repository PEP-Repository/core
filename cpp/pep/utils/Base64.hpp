#pragma once

#include <string>

namespace pep {
/*!
 * \brief Decode base64 URL encoded data. Based on code from http://sakhnik.blogspot.nl/2007/04/base64-transformations-in-c.html
 *
 * \param base64 String containing base64 encoded data
 * \return std::string Decoded data
 */
std::string decodeBase64URL(std::string base64);

/*!
 * \brief Encode data using base64 URL. Based on code from http://sakhnik.blogspot.nl/2007/04/base64-transformations-in-c.html
 *
 * \param data String containing data to be encoded
 * \return std::string Base64 URL encoded data
 */
std::string encodeBase64URL(std::string data);
}
