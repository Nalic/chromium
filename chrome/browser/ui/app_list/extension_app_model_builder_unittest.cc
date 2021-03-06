// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/extension_app_model_builder.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate_impl.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/common/manifest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_item_model.h"

namespace {

const char kHostedAppId[] = "dceacbkfkmllgmjmbhgkpjegnodmildf";
const char kPackagedApp1Id[] = "emfkafnhnpcmabnnkckkchdilgeoekbo";
const char kPackagedApp2Id[] = "jlklkagmeajbjiobondfhiekepofmljl";

// Get a string of all apps in |model| joined with ','.
std::string GetModelContent(app_list::AppListModel* model) {
  std::string content;
  for (size_t i = 0; i < model->item_list()->item_count(); ++i) {
    if (i > 0)
      content += ',';
    content += model->item_list()->item_at(i)->title();
  }
  return content;
}

scoped_refptr<extensions::Extension> MakeApp(const std::string& name,
                                             const std::string& version,
                                             const std::string& url,
                                             const std::string& id) {
  std::string err;
  DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", version);
  value.SetString("app.launch.web_url", url);
  scoped_refptr<extensions::Extension> app =
      extensions::Extension::Create(
          base::FilePath(),
          extensions::Manifest::INTERNAL,
          value,
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT,
          id,
          &err);
  EXPECT_EQ(err, "");
  return app;
}

class TestAppListControllerDelegate : public AppListControllerDelegate {
 public:
  virtual ~TestAppListControllerDelegate() {}
  virtual void DismissView() OVERRIDE {}
  virtual gfx::NativeWindow GetAppListWindow() OVERRIDE { return NULL; }
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE { return gfx::ImageSkia(); }
  virtual bool IsAppPinned(const std::string& extension_id) OVERRIDE {
    return false;
  }
  virtual void PinApp(const std::string& extension_id) OVERRIDE {}
  virtual void UnpinApp(const std::string& extension_id) OVERRIDE {}
  virtual Pinnable GetPinnable() OVERRIDE { return NO_PIN; }
  virtual bool CanDoCreateShortcutsFlow() OVERRIDE { return false; }
  virtual void DoCreateShortcutsFlow(Profile* profile,
                                     const std::string& extension_id) OVERRIDE {
  }
  virtual void CreateNewWindow(Profile* profile, bool incognito) OVERRIDE {}
  virtual void ActivateApp(Profile* profile,
                           const extensions::Extension* extension,
                           AppListSource source,
                           int event_flags) OVERRIDE {}
  virtual void LaunchApp(Profile* profile,
                         const extensions::Extension* extension,
                         AppListSource source,
                         int event_flags) OVERRIDE {}
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE {}
  virtual bool ShouldShowUserIcon() OVERRIDE { return false; }
};

const char kDefaultApps[] = "Packaged App 1,Packaged App 2,Hosted App";
const size_t kDefaultAppCount = 3u;

}  // namespace

class ExtensionAppModelBuilderTest : public ExtensionServiceTestBase {
 public:
  ExtensionAppModelBuilderTest() {}
  virtual ~ExtensionAppModelBuilderTest() {}

  virtual void SetUp() OVERRIDE {
    ExtensionServiceTestBase::SetUp();

    // Load "app_list" extensions test profile.
    // The test profile has 4 extensions:
    // 1 dummy extension, 2 packaged extension apps and 1 hosted extension app.
    base::FilePath source_install_dir = data_dir_
        .AppendASCII("app_list")
        .AppendASCII("Extensions");
    base::FilePath pref_path = source_install_dir
        .DirName()
        .AppendASCII("Preferences");
    InitializeInstalledExtensionService(pref_path, source_install_dir);
    service_->Init();

    // There should be 4 extensions in the test profile.
    const ExtensionSet* extensions = service_->extensions();
    ASSERT_EQ(static_cast<size_t>(4),  extensions->size());

    model_.reset(new app_list::AppListModel);
    controller_.reset(new TestAppListControllerDelegate);
    builder_.reset(new ExtensionAppModelBuilder(
        profile_.get(), model_.get(), controller_.get()));
  }

  virtual void TearDown() OVERRIDE {
    builder_.reset();
    controller_.reset();
    model_.reset();
  }

 protected:
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<TestAppListControllerDelegate> controller_;
  scoped_ptr<ExtensionAppModelBuilder> builder_;

  base::ScopedTempDir second_profile_temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionAppModelBuilderTest);
};

TEST_F(ExtensionAppModelBuilderTest, Build) {
  // The apps list would have 3 extension apps in the profile.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, HideWebStore) {
  // Install a "web store" app.
  scoped_refptr<extensions::Extension> store =
      MakeApp("webstore",
              "0.0",
              "http://google.com",
              std::string(extension_misc::kWebStoreAppId));
  service_->AddExtension(store.get());

  // Install an "enterprise web store" app.
  scoped_refptr<extensions::Extension> enterprise_store =
      MakeApp("enterprise_webstore",
              "0.0",
              "http://google.com",
              std::string(extension_misc::kEnterpriseWebStoreAppId));
  service_->AddExtension(enterprise_store.get());

  // Web stores should be present in the AppListModel.
  app_list::AppListModel model1;
  ExtensionAppModelBuilder builder1(
      profile_.get(), &model1, controller_.get());
  std::string content = GetModelContent(&model1);
  EXPECT_NE(std::string::npos, content.find("webstore"));
  EXPECT_NE(std::string::npos, content.find("enterprise_webstore"));

  // Activate the HideWebStoreIcon policy.
  profile_->GetPrefs()->SetBoolean(prefs::kHideWebStoreIcon, true);

  // Web stores should NOT be in the AppListModel.
  app_list::AppListModel model2;
  ExtensionAppModelBuilder builder2(
      profile_.get(), &model2, controller_.get());
  content = GetModelContent(&model2);
  EXPECT_EQ(std::string::npos, content.find("webstore"));
  EXPECT_EQ(std::string::npos, content.find("enterprise_webstore"));
}

TEST_F(ExtensionAppModelBuilderTest, DisableAndEnable) {
  service_->DisableExtension(kHostedAppId,
                             extensions::Extension::DISABLE_NONE);
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));

  service_->EnableExtension(kHostedAppId);
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, Uninstall) {
  service_->UninstallExtension(kPackagedApp2Id, false, NULL);
  EXPECT_EQ(std::string("Packaged App 1,Hosted App"),
            GetModelContent(model_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppModelBuilderTest, UninstallTerminatedApp) {
  const extensions::Extension* app =
      service_->GetInstalledExtension(kPackagedApp2Id);
  ASSERT_TRUE(app != NULL);

  // Simulate an app termination.
  service_->TrackTerminatedExtensionForTest(app);

  service_->UninstallExtension(kPackagedApp2Id, false, NULL);
  EXPECT_EQ(std::string("Packaged App 1,Hosted App"),
            GetModelContent(model_.get()));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ExtensionAppModelBuilderTest, Reinstall) {
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));

  // Install kPackagedApp1Id again should not create a new entry.
  extensions::InstallTracker* tracker =
      extensions::InstallTrackerFactory::GetForProfile(profile_.get());
  extensions::InstallObserver::ExtensionInstallParams params(
      kPackagedApp1Id, "", gfx::ImageSkia(), true, true);
  tracker->OnBeginExtensionInstall(params);

  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, OrdinalPrefsChange) {
  extensions::AppSorting* sorting = service_->extension_prefs()->app_sorting();

  syncer::StringOrdinal package_app_page =
      sorting->GetPageOrdinal(kPackagedApp1Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page.CreateBefore());
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));

  syncer::StringOrdinal app1_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp1Id);
  syncer::StringOrdinal app2_ordinal =
      sorting->GetAppLaunchOrdinal(kPackagedApp2Id);
  sorting->SetPageOrdinal(kHostedAppId, package_app_page);
  sorting->SetAppLaunchOrdinal(kHostedAppId,
                               app1_ordinal.CreateBetween(app2_ordinal));
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, OnExtensionMoved) {
  extensions::AppSorting* sorting = service_->extension_prefs()->app_sorting();
  sorting->SetPageOrdinal(kHostedAppId,
                          sorting->GetPageOrdinal(kPackagedApp1Id));

  service_->OnExtensionMoved(kHostedAppId, kPackagedApp1Id, kPackagedApp2Id);
  // Old behavior: This would be "Packaged App 1,Hosted App,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));

  service_->OnExtensionMoved(kHostedAppId, kPackagedApp2Id, std::string());
  // Old behavior: This would be restored to the default order.
  // New behavior: Sorting order still doesn't change.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));

  service_->OnExtensionMoved(kHostedAppId, std::string(), kPackagedApp1Id);
  // Old behavior: This would be "Hosted App,Packaged App 1,Packaged App 2"
  // New behavior: Sorting order doesn't change.
  EXPECT_EQ(std::string(kDefaultApps), GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, InvalidOrdinal) {
  // Creates a no-ordinal case.
  extensions::AppSorting* sorting = service_->extension_prefs()->app_sorting();
  sorting->ClearOrdinals(kPackagedApp1Id);

  // Creates an corrupted ordinal case.
  extensions::ExtensionScopedPrefs* scoped_prefs = service_->extension_prefs();
  scoped_prefs->UpdateExtensionPref(
      kHostedAppId,
      "page_ordinal",
      base::Value::CreateStringValue("a corrupted ordinal"));

  // This should not assert or crash.
  ExtensionAppModelBuilder builder1(
      profile_.get(), model_.get(), controller_.get());
}

TEST_F(ExtensionAppModelBuilderTest, OrdinalConfilicts) {
  // Creates conflict ordinals for app1 and app2.
  syncer::StringOrdinal conflict_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();

  extensions::AppSorting* sorting = service_->extension_prefs()->app_sorting();
  sorting->SetPageOrdinal(kHostedAppId, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kHostedAppId, conflict_ordinal);

  sorting->SetPageOrdinal(kPackagedApp1Id, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kPackagedApp1Id, conflict_ordinal);

  sorting->SetPageOrdinal(kPackagedApp2Id, conflict_ordinal);
  sorting->SetAppLaunchOrdinal(kPackagedApp2Id, conflict_ordinal);

  // This should not assert or crash.
  ExtensionAppModelBuilder builder1(
      profile_.get(), model_.get(), controller_.get());

  // By default, conflicted items are sorted by their app ids (= order added).
  EXPECT_EQ(std::string("Hosted App,Packaged App 1,Packaged App 2"),
            GetModelContent(model_.get()));
}

TEST_F(ExtensionAppModelBuilderTest, SwitchProfile) {
  EXPECT_EQ(kDefaultAppCount, model_->item_list()->item_count());

  // Switch to a profile with no apps, ensure all apps are removed.
  ExtensionServiceInitParams params =
      CreateDefaultInitParamsInTempDir(&second_profile_temp_dir_);
  scoped_ptr<TestingProfile> profile2 = CreateTestingProfile(params);
  InitializeExtensionServiceForProfile(params, profile2.get());

  builder_->SwitchProfile(profile2.get());
  EXPECT_EQ(0u, model_->item_list()->item_count());

  // Switch back to the main profile, ensure apps are restored.
  builder_->SwitchProfile(profile_.get());
  EXPECT_EQ(kDefaultAppCount, model_->item_list()->item_count());
}
