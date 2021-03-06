// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_aura_initializer.h"

#include "base/base_paths.h"
#include "base/path_service.h"
#include "ui/base/resource/resource_bundle.h"

namespace aura {
namespace test {

TestAuraInitializer::TestAuraInitializer() {
  base::FilePath pak_file;
  PathService::Get(base::DIR_MODULE, &pak_file);
  pak_file = pak_file.AppendASCII("ui_test.pak");
  ui::ResourceBundle::InitSharedInstanceWithPakPath(pak_file);
}

TestAuraInitializer::~TestAuraInitializer() {
  ui::ResourceBundle::CleanupSharedInstance();
}

}  // namespace test
}  // namespace aura
