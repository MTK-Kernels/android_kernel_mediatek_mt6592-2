// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_V8_H_
#define NET_PROXY_PROXY_RESOLVER_V8_H_
#pragma once

#include <utils/String16.h>

#include "proxy_resolver_js_bindings.h"

namespace net {

typedef void* RequestHandle;
typedef void* CompletionCallback;

#define OK 0
#define ERR_PAC_SCRIPT_FAILED -1
#define ERR_FAILED -2

class ProxyErrorListener {
protected:
  virtual ~ProxyErrorListener() {}
public:
  virtual void AlertMessage(android::String16 message) = 0;
  virtual void ErrorMessage(android::String16 error) = 0;
};

class ProxyResolverV8 {
 public:
  
  
  
  explicit ProxyResolverV8(ProxyResolverJSBindings* custom_js_bindings,
          ProxyErrorListener* error_listener);

  virtual ~ProxyResolverV8();

  ProxyResolverJSBindings* js_bindings() { return js_bindings_; }

  virtual int GetProxyForURL(const android::String16 spec, const android::String16 host,
                             android::String16* results);
  virtual void PurgeMemory();
  virtual int SetPacScript(const android::String16& script_data);

 private:
  
  
  
  class Context;
  Context* context_;

  ProxyResolverJSBindings* js_bindings_;
  ProxyErrorListener* error_listener_;
};

}  

#endif  
