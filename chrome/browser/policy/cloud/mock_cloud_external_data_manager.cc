// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/mock_cloud_external_data_manager.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace policy {

MockCloudExternalDataManager::MockCloudExternalDataManager() {
}

MockCloudExternalDataManager::~MockCloudExternalDataManager() {
}

scoped_ptr<ExternalDataFetcher>
    MockCloudExternalDataManager::CreateExternalDataFetcher(
        const std::string& policy) {
  return make_scoped_ptr(new ExternalDataFetcher(weak_factory_.GetWeakPtr(),
                                                 policy));
}

}  // namespace policy
