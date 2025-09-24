#include <pep/storagefacility/S3.hpp>
#include <gtest/gtest.h>

using boost::urls::url;

using namespace pep::s3;
using namespace pep::s3::request;

namespace {
  // These test cases were taken from the "Signature Version 4 Test Suite":
  // https://docs.aws.amazon.com/general/latest/gr/signature-v4-test-suite.html
  const Credentials test_credentials = {
    "AKIDEXAMPLE", // access key
    "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", // secret
    "service", // service
  };

  // We do not support signing all requests appearing in the Test Suite.
  // Specifically, the following test cases are not supported.
  //
  // get-header-key-duplicate
  // get-header-value-{order,trim,multiline}
  //   (We do not support double headers and the trimming of whitespaces
  //    inside header values.)
  //
  // get-vanilla-query-order-{key,value}
  //   (We do not support double query parameters.)
  //
  // get-relative/* 
  //   (Paths to requests to Amazon S3 are never normalized.)
  //
  // post-sts-token/*
  //   (We do not support the AWS Security Token Service.)
  //
  // 
  //
  //
  //
  // See also the "Restrictions/Limitations" remark in S3.hpp.
  
  TEST(S3, AH_get_unreserved) {
    HttpRequest r("example.amazonaws.com",
      pep::networking::HttpMethod::GET, url("/-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), "", {
        {"Host", "example.amazonaws.com"},
        {"X-Amz-Date", "20150830T123600Z"}
      }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=07ef7494c76fa4850883e2b006601f940f8a34d404d0cfa977f52a65bbf5f24f");
  }


  TEST(S3, AH_get_utf8) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url().set_path("/\xe1\x88\xb4"), "", {
            {"Host", "example.amazonaws.com"},
            {"X-Amz-Date", "20150830T123600Z"}
          }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));

    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=8318018e0b0f223aa2bbf98705b62bb787dc9c0e678f255a891fd03141be5d85");
  }




  TEST(S3, AH_get_vanilla) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/"), "", {
            {"Host", "example.amazonaws.com"},
            {"X-Amz-Date", "20150830T123600Z"}
          }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5fa00fa31553b73ebf1942676e86291e8372ff2a2260956d9b8aae1d763fbf31");
  }


  TEST(S3, AH_get_vanilla_empty_query_key) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/?Param1=value1"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=a67d582fa61cc504c4bae71f336f98b97f1ea3c7a6bfe1b6e45aec72011b9aeb");
  }

  // NB. get_vanilla_query is exactly the same as get_vanilla,
  //     and therefore not included.
  
  TEST(S3, AH_get_vanilla_empty_query_order_key_case) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/?Param2=value2&Param1=value1"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=b97d918cfa904a5beff61c982a1b6f458b799221646efd99d3219ec94cdf2500");
  }
  

  TEST(S3, AH_get_vanilla_query_unreserved) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/?-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz=-._~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=9c3e54bfcdf0b19771a7f523ee5669cdf59bc7cc0884027167c21bb143a40197");
  }


  TEST(S3, AH_get_vanilla_utf8_query) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/").set_query("\xe1\x88\xb4=bar"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=2cdec8eed098649ff3a119c94853b13c643bcf08f8b0a1d91e12c9027818dd04");
  }


  TEST(S3, AH_post_header_key_case) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);
    
    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));

    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=5da7c1a2acd57cee7505fc6676e4e544621c30862966e37dddb68e92efbe5d6b");
  }


  TEST(S3, AH_post_header_key_sort) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/"), "", {
          {"Host", "example.amazonaws.com"},
          {"My-Header1", "value1"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials,
          {"Host", "X-Amz-Date", "My-Header1"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=c5410059b04c1ee005303aed430f6e6645f61f4dc9e1461ec8f8916fdf18852c");
  }


  TEST(S3, AH_post_header_value_case) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/"), "", {
          {"Host", "example.amazonaws.com"},
          {"My-Header1", "VALUE1"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, 
          {"Host", "X-Amz-Date", "My-Header1"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;my-header1;x-amz-date, Signature=cdbc9802e29d2942e5e10b5bccfdd67c5f22c7c4e8ae67b53629efa58b974b7d");
  }

  // NB. post-vanilla begin the same as post-header-key-case 
  // is not included.
  

  TEST(S3, AH_post_vanilla_empty_query_value) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/?Param1=value1"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, {"Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=host;x-amz-date, Signature=28038455d6de14eafc1f9222cf5aa6f1a96197d7deb8263271d420d138af7f11");
  }

  // NB. post-vanilla-query is the same as post-vanilla-empty-query-value,
  // and therefore not included.
  

  TEST(S3, AH_post_x_www_form_urlencoded) {
    // NB. There seems to be an error in this test case: in both  the .req
    // and .creq files the "content-length" header is included,
    // but not in the .authz file.  Not including the content-length header
    // gives the authorization header mentioned in the .authz-file:
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/"), "Param1=value1", {
          {"Content-Type", "application/x-www-form-urlencoded"},
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"},
          //{"Content-Length", "13"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_EQ(AuthorizationHeader(r, test_credentials, 
          {"Content-Type", "Host", "X-Amz-Date"}),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-type;host;x-amz-date, Signature=ff11897932ad3f4e8b18135d722051e5ac45fc38421b1da7b9d196a0fe09473a");
  }


  // NB. There seems to be an error in post_x_www_form_urlencoded_parameters,
  //     namely that shasum -a256 post_x_www_form_urlencoded_parameters.creq is 
  //
  //       40329ab1037d77f10eb46ab0981b2b18f47473e491aa6b4ea30b7e8c7b8b625b
  //
  //     instead of the 
  //
  //       2e1cf7ed91881a30569e46552437e4156c823447bf1781b921b5d486c568dd1c
  //
  //     mentioned in post_x_www_form_urlencoded_parameters.sts.
  // 
  //     Since I could find no way to fix Amazon's error, 
  //     I've disabled the test.
/*
  TEST(S3, AH_post_x_www_form_urlencoded_parameters) {
      
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::POST, url("/"), "Param1=value1", {
          {"Content-Type", "application/x-www-form-urlencoded; charset=utf-8"},
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"},
          {"Content-Length", "13"}
        }, false);

    authorization_header::Context c({r, 
        test_credentials, r.header("X-Amz-Date")});
    
    EXPECT_EQ(ComputeCanonicalRequest(c),
        "POST\n"
        "/\n" 
        "\n"
        "content-length:13\n"
        "content-type:application/x-www-form-urlencoded; charset=utf-8\n"
        "host:example.amazonaws.com\n"
        "x-amz-date:20150830T123600Z\n"
        "\n"
        "content-length;content-type;host;x-amz-date\n"
        "9095672bbd1f56dfc5b65f3e153adc8731a4a654192329106275f4c7b24d0b6e");

    EXPECT_EQ(ComputeStringToSign(c),
        "AWS4-HMAC-SHA256\n"
        "20150830T123600Z\n"
        "20150830/us-east-1/service/aws4_request\n"
        "2e1cf7ed91881a30569e46552437e4156c823447bf1781b921b5d486c568dd1c");

    EXPECT_EQ(AuthorizationHeader(r, test_credentials),
        "AWS4-HMAC-SHA256 Credential=AKIDEXAMPLE/20150830/us-east-1/service/aws4_request, SignedHeaders=content-length;content-type;host;x-amz-date, Signature=1a72ec8f64bd914b0e42e42607c7fbce7fb2c7465f63e3092b3b0d39fa77a6fe");
  } */




  TEST(S3, TimeInAmzIso8601) {
    EXPECT_TRUE(TimeInAmzIso8601().length()==16);
  }


  TEST(S3, SpaceInAccessKey) {
    HttpRequest r("example.amazonaws.com",
        pep::networking::HttpMethod::GET, url("/"), "", {
          {"Host", "example.amazonaws.com"},
          {"X-Amz-Date", "20150830T123600Z"}
        }, false);

    r.setHeader("X-Amz-Content-Sha256", XAmzContentSha256Header(r.getBody()));
    
    EXPECT_NO_THROW(AuthorizationHeader(r, {
          "AKIDEXAMPLE", // access key *without* a space (' ')
          "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", // secret
          "service", // service
      }, {"Host", "X-Amz-Date"}));
    EXPECT_ANY_THROW(AuthorizationHeader(r, {
          "AKID EXAMPLE", // access key *with* a space (' ')
          "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY", // secret
          "service", // service
      }, {"Host", "X-Amz-Date"}));
  }


}

