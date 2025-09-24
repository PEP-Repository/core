/*
 * Decode HTML entities. C++ wrapper for C code provided by https://stackoverflow.com/a/2078568 and https://stackoverflow.com/a/1082191
 */
#ifndef ENTITIES_HPP

#include <string>

std::string decode_html_entities_utf8(const std::string& source);

#define ENTITIES_HPP

#endif //ENTITIES_HPP
