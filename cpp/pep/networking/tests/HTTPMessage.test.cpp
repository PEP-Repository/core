#include <gtest/gtest.h>

#include <pep/networking/HTTPMessage.hpp>

TEST(HTTPRequest, Uri) {
  pep::HTTPRequest::Uri uri("/foo/bar?a=x&abc=xyz");
  ASSERT_FALSE(uri.getProtocol());
  ASSERT_FALSE(uri.getHostname());
  ASSERT_FALSE(uri.getPort());
  ASSERT_EQ(uri.getPath(), "/foo/bar");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("https://www.example.com:8080/foo/bar?a=x&abc=xyz");
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "https");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 8080);
  ASSERT_EQ(uri.getPath(), "/foo/bar");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("https://www.example.com/foo/bar?a=x&abc=xyz");
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "https");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 443);
  ASSERT_EQ(uri.getPath(), "/foo/bar");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("http://www.example.com/foo/bar?a=x&abc=xyz");
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "http");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 80);
  ASSERT_EQ(uri.getPath(), "/foo/bar");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("https://www.example.com?a=x&abc=xyz");
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "https");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 443);
  ASSERT_EQ(uri.getPath(), "/");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("https://www.example.com/?a=x&abc=xyz");
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "https");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 443);
  ASSERT_EQ(uri.getPath(), "/");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");

  uri = pep::HTTPRequest::Uri("https://www.example.com/?a=x&abc=xyz", { { "foo", "bar"}});
  ASSERT_TRUE(uri.getProtocol());
  ASSERT_EQ(*uri.getProtocol(), "https");
  ASSERT_TRUE(uri.getHostname());
  ASSERT_EQ(*uri.getHostname(), "www.example.com");
  ASSERT_TRUE(uri.getPort());
  ASSERT_EQ(*uri.getPort(), 443);
  ASSERT_EQ(uri.getPath(), "/");
  ASSERT_EQ(uri.query("a"), "x");
  ASSERT_EQ(uri.query("abc"), "xyz");
  ASSERT_EQ(uri.query("foo"), "bar");
}