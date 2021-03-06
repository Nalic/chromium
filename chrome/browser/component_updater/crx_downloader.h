// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CRX_DOWNLOADER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CRX_DOWNLOADER_H_

#include <vector>
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "url/gurl.h"

namespace net {
class URLRequestContextGetter;
}

namespace component_updater {

// Defines a download interface for downloading components, with retrying on
// fallback urls in case of errors. This class implements a chain of
// responsibility design pattern. It can give successors in the chain a chance
// to handle a download request, until one of them succeeds, or there are no
// more urls or successors to try. A callback is always called at the end of
// the download, one time only.
// When multiple urls and downloaders exists, first all the urls are tried, in
// the order they are provided in the StartDownload function argument. After
// that, the download request is routed to the next downloader in the chain.
// The members of this class expect to be called from the UI thread only.
class CrxDownloader {
  // |error| contains the download error: 0 indicates success. |response|
  // contains the file path of the downloaded file, if the download was
  // successful.
  // The callback fires only once, regardless of how many urls are tried, and
  // how many successors in the chain of downloaders have handled the
  // download. The callback interface can be extended if needed to provide
  // more visibility into how the download has been handled, including
  // specific error codes and download metrics.
  typedef base::Callback<void(int error,
                              const base::FilePath& response)> DownloadCallback;

 public:
  // Factory method to create an instance of this class and build the
  // chain of responsibility.
  static CrxDownloader* Create(
      net::URLRequestContextGetter* context_getter,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const DownloadCallback& download_callback);
  virtual ~CrxDownloader();

  // Starts the download. One instance of the class handles one download only.
  void StartDownloadFromUrl(const GURL& url);
  void StartDownload(const std::vector<GURL>& urls);


 protected:
  CrxDownloader();

  // Derived class must call this function after each attempt at downloading
  // from the urls provided in the StartDownload function.
  void OnDownloadComplete(bool is_handled,
                          int error,
                          const base::FilePath& response);

 private:
  virtual void DoStartDownload(const GURL& url) = 0;

  std::vector<GURL> urls_;
  scoped_ptr<CrxDownloader> successor_;
  DownloadCallback download_callback_;

  size_t current_url_;

  DISALLOW_COPY_AND_ASSIGN(CrxDownloader);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CRX_DOWNLOADER_H_

