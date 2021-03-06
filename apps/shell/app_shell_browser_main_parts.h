// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_BROWSER_MAIN_PARTS_H_
#define APPS_SHELL_APP_SHELL_BROWSER_MAIN_PARTS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_main_parts.h"

namespace base {
class FilePath;
}

namespace content {
class ShellBrowserContext;
struct MainFunctionParams;
}

namespace views {
class ViewsDelegate;
}

namespace wm {
class WMTestHelper;
}

namespace apps {

class AppShellBrowserContext;

// Handles initialization of AppShell.
class AppShellBrowserMainParts : public content::BrowserMainParts {
 public:
  explicit AppShellBrowserMainParts(
      const content::MainFunctionParams& parameters);
  virtual ~AppShellBrowserMainParts();

  AppShellBrowserContext* browser_context() {
    return browser_context_.get();
  }

  // Creates the window that hosts the apps.
  void CreateRootWindow();

  // Launches an application from a directory.
  void LoadAndLaunchApp(const base::FilePath& app_dir);

  // BrowserMainParts overrides.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;

 private:
  scoped_ptr<AppShellBrowserContext> browser_context_;

  // Enable a minimal set of views::corewm to be initialized.
  scoped_ptr<wm::WMTestHelper> wm_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(AppShellBrowserMainParts);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_BROWSER_MAIN_PARTS_H_
