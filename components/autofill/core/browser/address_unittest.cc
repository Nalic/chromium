// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Test that country data can be properly returned as either a country code or a
// localized country name.
TEST(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Make sure that nothing breaks when the country code is missing.
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), country);

  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), country);

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"));
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), country);
}

// Test that we properly detect country codes appropriate for each country.
TEST(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Test basic conversion.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("United States"),
      "en-US");
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test basic synonym detection.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("canADA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test country code detection.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("JP"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("JP"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Japan"), country);

  // Test that we ignore unknown countries.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("Unknown"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);

  // Test setting the country based on an HTML field type.
  AutofillType html_type_country_code =
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  address.SetInfo(html_type_country_code, ASCIIToUTF16("US"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity when setting the country based on an HTML field
  // type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("cA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test setting the country based on invalid data with an HTML field type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("unknown"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);
}

// Test that we properly match typed values to stored country data.
TEST(AddressTest, IsCountry) {
  Address address;
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  const char* const kValidMatches[] = {
    "United States",
    "USA",
    "US",
    "United states",
    "us"
  };
  for (size_t i = 0; i < arraysize(kValidMatches); ++i) {
    SCOPED_TRACE(kValidMatches[i]);
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kValidMatches[i]), "US",
                             &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {
    "United",
    "Garbage"
  };
  for (size_t i = 0; i < arraysize(kInvalidMatches); ++i) {
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kInvalidMatches[i]), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, base::string16());
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  ServerFieldTypeSet matching_types;
  address.GetMatchingTypes(ASCIIToUTF16("Garbage"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}

// Verifies that Address::GetInfo() correctly combines address lines.
TEST(AddressTest, GetStreetAddress) {
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Address has no address lines.
  Address address;
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_EQ(base::string16(), address.GetInfo(type, "en-US"));

  // Address has only line 1.
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_TRUE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave."), address.GetInfo(type, "en-US"));

  // Address has lines 1 and 2.
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "Apt. 42"),
            address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS));
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave.\n"
                         "Apt. 42"),
            address.GetInfo(type, "en-US"));
}

// Verifies that Address::SetRawInfo() is able to split address lines correctly.
TEST(AddressTest, SetRawStreetAddress) {
  const base::string16 empty_street_address;
  const base::string16 short_street_address = ASCIIToUTF16("456 Nowhere Ln.");
  const base::string16 long_street_address =
      ASCIIToUTF16("123 Example Ave.\n"
                   "Apt. 42\n"
                   "(The one with the blue door)");

  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // Address lines beyond the first two are simply dropped.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  EXPECT_EQ(ASCIIToUTF16("123 Example Ave."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // A short address should clear out unused address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, short_street_address);
  EXPECT_EQ(ASCIIToUTF16("456 Nowhere Ln."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // An empty address should clear out all address lines.
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, long_street_address);
  address.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, empty_street_address);
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
}

// Verifies that Address::SetInfo() rejects setting data for
// ADDRESS_HOME_STREET_ADDRESS without newlines, as there is no good general way
// to parse that data into the consituent address lines. Addresses without
// newlines should be set properly.
TEST(AddressTest, SetStreetAddress) {
  const base::string16 empty_street_address;
  const base::string16 one_line_street_address =
      ASCIIToUTF16("456 New St., Apt. 17");
  const base::string16 multi_line_street_address =
      ASCIIToUTF16("789 Fancy Pkwy.\n"
                   "Unit 3.14");
  const AutofillType type = AutofillType(ADDRESS_HOME_STREET_ADDRESS);

  // Start with a non-empty address.
  Address address;
  address.SetRawInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("123 Example Ave."));
  address.SetRawInfo(ADDRESS_HOME_LINE2, ASCIIToUTF16("Apt. 42"));
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());

  // Attempting to set a one-line address should fail, as the single line might
  // actually represent multiple logical lines, combined into one due to the
  // user having to work around constraints imposed by the website.
  EXPECT_FALSE(address.SetInfo(type, one_line_street_address, "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // Attempting to set a multi-line address should succeed.
  EXPECT_TRUE(address.SetInfo(type, multi_line_street_address, "en-US"));
  EXPECT_EQ(ASCIIToUTF16("789 Fancy Pkwy."),
            address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Unit 3.14"), address.GetRawInfo(ADDRESS_HOME_LINE2));

  // Attempting to set an empty address should also succeed, and clear out the
  // previously stored data.
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE1).empty());
  EXPECT_FALSE(address.GetRawInfo(ADDRESS_HOME_LINE2).empty());
  EXPECT_TRUE(address.SetInfo(type, empty_street_address, "en-US"));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_LINE2));
}

}  // namespace autofill
