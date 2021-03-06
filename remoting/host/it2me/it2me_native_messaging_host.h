// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_
#define REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/it2me/it2me_host.h"
#include "remoting/host/native_messaging/native_messaging_channel.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace remoting {

// Implementation of the native messaging host process.
class It2MeNativeMessagingHost : public NativeMessagingChannel::Delegate,
                                 public It2MeHost::Observer {
 public:
  typedef NativeMessagingChannel::SendMessageCallback SendMessageCallback;

  It2MeNativeMessagingHost(scoped_refptr<AutoThreadTaskRunner> task_runner,
                           scoped_ptr<It2MeHostFactory> factory);
  virtual ~It2MeNativeMessagingHost();

  // NativeMessagingChannel::Delegate interface.
  virtual void SetSendMessageCallback(const SendMessageCallback& send_message)
      OVERRIDE;
  virtual void ProcessMessage(scoped_ptr<base::DictionaryValue> message)
      OVERRIDE;

  // It2MeHost::Observer implementation
  virtual void OnClientAuthenticated(const std::string& client_username)
      OVERRIDE;
  virtual void OnStoreAccessCode(const std::string& access_code,
                                 base::TimeDelta access_code_lifetime) OVERRIDE;
  virtual void OnNatPolicyChanged(bool nat_traversal_enabled) OVERRIDE;
  virtual void OnStateChanged(It2MeHostState state) OVERRIDE;

  static void ShutDownHost(NativeMessagingChannel::Delegate* delegate) {
    It2MeNativeMessagingHost* host =
        static_cast<It2MeNativeMessagingHost*>(delegate);
    host->ShutDown();
  }

  static std::string HostStateToString(It2MeHostState host_state);

 private:
  // These "Process.." methods handle specific request types. The |response|
  // dictionary is pre-filled by ProcessMessage() with the parts of the
  // response already known ("id" and "type" fields).
  void ProcessHello(const base::DictionaryValue& message,
                    scoped_ptr<base::DictionaryValue> response);
  void ProcessConnect(const base::DictionaryValue& message,
                      scoped_ptr<base::DictionaryValue> response);
  void ProcessDisconnect(const base::DictionaryValue& message,
                         scoped_ptr<base::DictionaryValue> response);
  void SendErrorAndExit(scoped_ptr<base::DictionaryValue> response,
                        const std::string& description);

  scoped_refptr<AutoThreadTaskRunner> task_runner();

  void ShutDown();

  scoped_ptr<It2MeHostFactory> factory_;
  scoped_ptr<ChromotingHostContext> host_context_;
  scoped_refptr<It2MeHost> it2me_host_;
  SendMessageCallback send_message_;

  // Cached, read-only copies of |it2me_host_| session state.
  It2MeHostState state_;
  std::string access_code_;
  base::TimeDelta access_code_lifetime_;
  std::string client_username_;

  // IT2Me Talk server configuration used by |it2me_host_| to connect.
  XmppSignalStrategy::XmppServerConfig xmpp_server_config_;

  // Chromoting Bot JID used by |it2me_host_| to register the host.
  std::string directory_bot_jid_;

  base::WeakPtr<It2MeNativeMessagingHost> weak_ptr_;
  base::WeakPtrFactory<It2MeNativeMessagingHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(It2MeNativeMessagingHost);
};

}  // namespace remoting

#endif  // REMOTING_HOST_IT2ME_IT2ME_NATIVE_MESSAGING_HOST_H_
