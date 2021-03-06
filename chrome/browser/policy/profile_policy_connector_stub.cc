// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include "chrome/browser/policy/policy_service_stub.h"

namespace policy {

ProfilePolicyConnector::ProfilePolicyConnector() {}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
    bool force_immediate_load,
    SchemaRegistry* schema_registry,
    CloudPolicyManager* user_cloud_policy_manager) {
  policy_service_.reset(new PolicyServiceStub());
}

void ProfilePolicyConnector::InitForTesting(scoped_ptr<PolicyService> service) {
  policy_service_ = service.Pass();
}

void ProfilePolicyConnector::Shutdown() {}

}  // namespace policy
