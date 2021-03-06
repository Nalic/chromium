// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_SERVICE_STUB_H_
#define CHROME_BROWSER_POLICY_POLICY_SERVICE_STUB_H_

#include "base/basictypes.h"
#include "chrome/browser/policy/policy_service.h"
#include "components/policy/core/common/policy_map.h"

namespace policy {

// A stub implementation, that is used when ENABLE_CONFIGURATION_POLICY is not
// set. This allows client code to compile without requiring #ifdefs.
class PolicyServiceStub : public PolicyService {
 public:
  PolicyServiceStub();
  virtual ~PolicyServiceStub();

  virtual void AddObserver(PolicyDomain domain,
                           Observer* observer) OVERRIDE;

  virtual void RemoveObserver(PolicyDomain domain,
                              Observer* observer) OVERRIDE;

  virtual const PolicyMap& GetPolicies(
      const PolicyNamespace& ns) const OVERRIDE;

  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;

  virtual void RefreshPolicies(const base::Closure& callback) OVERRIDE;
 private:
  const PolicyMap kEmpty_;

  DISALLOW_COPY_AND_ASSIGN(PolicyServiceStub);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_SERVICE_STUB_H_
