// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_details.h"
#include "components/policy/core/common/schema.h"
#include "policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// This unittest tests the code generated by
// chrome/tools/build/generate_policy_source.py.

namespace policy {

TEST(GeneratePolicySource, ChromeSchemaData) {
  Schema schema = Schema::Wrap(GetChromeSchemaData());
  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, schema.type());

  Schema subschema = schema.GetAdditionalProperties();
  EXPECT_FALSE(subschema.valid());

  subschema = schema.GetProperty("no such policy exists");
  EXPECT_FALSE(subschema.valid());

  subschema = schema.GetProperty(key::kAlternateErrorPagesEnabled);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::TYPE_BOOLEAN, subschema.type());

  subschema = schema.GetProperty(key::kIncognitoModeAvailability);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::TYPE_INTEGER, subschema.type());

  subschema = schema.GetProperty(key::kProxyMode);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::TYPE_STRING, subschema.type());

  subschema = schema.GetProperty(key::kCookiesAllowedForUrls);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::TYPE_LIST, subschema.type());
  ASSERT_TRUE(subschema.GetItems().valid());
  EXPECT_EQ(base::Value::TYPE_STRING, subschema.GetItems().type());

  subschema = schema.GetProperty(key::kProxySettings);
  ASSERT_TRUE(subschema.valid());
  EXPECT_EQ(base::Value::TYPE_DICTIONARY, subschema.type());
  EXPECT_FALSE(subschema.GetAdditionalProperties().valid());
  EXPECT_FALSE(subschema.GetProperty("no such proxy key exists").valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyMode).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyServer).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyServerMode).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyPacUrl).valid());
  ASSERT_TRUE(subschema.GetProperty(key::kProxyBypassList).valid());

  // Verify that all the Chrome policies are there.
  for (Schema::Iterator it = schema.GetPropertiesIterator();
       !it.IsAtEnd(); it.Advance()) {
    EXPECT_TRUE(it.key());
    EXPECT_FALSE(std::string(it.key()).empty());
    EXPECT_TRUE(GetChromePolicyDetails(it.key()));
  }

  // The properties are iterated in order.
  const char* kExpectedProperties[] = {
    key::kProxyBypassList,
    key::kProxyMode,
    key::kProxyPacUrl,
    key::kProxyServer,
    key::kProxyServerMode,
    NULL,
  };
  const char** next = kExpectedProperties;
  for (Schema::Iterator it(subschema.GetPropertiesIterator());
       !it.IsAtEnd(); it.Advance(), ++next) {
    ASSERT_TRUE(*next != NULL);
    EXPECT_STREQ(*next, it.key());
    ASSERT_TRUE(it.schema().valid());
    EXPECT_EQ(base::Value::TYPE_STRING, it.schema().type());
  }
  EXPECT_TRUE(*next == NULL);
}

TEST(GeneratePolicySource, PolicyDetails) {
  EXPECT_FALSE(GetChromePolicyDetails(""));
  EXPECT_FALSE(GetChromePolicyDetails("no such policy"));
  EXPECT_FALSE(GetChromePolicyDetails("AlternateErrorPagesEnable"));
  EXPECT_FALSE(GetChromePolicyDetails("alternateErrorPagesEnabled"));
  EXPECT_FALSE(GetChromePolicyDetails("AAlternateErrorPagesEnabled"));

  const PolicyDetails* details =
      GetChromePolicyDetails(key::kAlternateErrorPagesEnabled);
  ASSERT_TRUE(details);
  EXPECT_FALSE(details->is_deprecated);
  EXPECT_FALSE(details->is_device_policy);
  EXPECT_EQ(5, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);

  details = GetChromePolicyDetails(key::kJavascriptEnabled);
  ASSERT_TRUE(details);
  EXPECT_TRUE(details->is_deprecated);
  EXPECT_FALSE(details->is_device_policy);
  EXPECT_EQ(9, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);

#if defined(OS_CHROMEOS)
  details = GetChromePolicyDetails(key::kDevicePolicyRefreshRate);
  ASSERT_TRUE(details);
  EXPECT_FALSE(details->is_deprecated);
  EXPECT_TRUE(details->is_device_policy);
  EXPECT_EQ(90, details->id);
  EXPECT_EQ(0u, details->max_external_data_size);
#endif

  // TODO(bartfab): add a test that verifies a max_external_data_size larger
  // than 0, once a type 'external' policy is added.
}

}  // namespace policy
