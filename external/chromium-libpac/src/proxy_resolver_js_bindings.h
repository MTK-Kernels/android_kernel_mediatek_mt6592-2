// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_PROXY_PROXY_RESOLVER_JS_BINDINGS_H_
#define NET_PROXY_PROXY_RESOLVER_JS_BINDINGS_H_
#pragma once

#include <utils/String16.h>
#include <string>

namespace net {

class ProxyErrorListener;

class ProxyResolverJSBindings {
 public:
  ProxyResolverJSBindings() {}

  virtual ~ProxyResolverJSBindings() {}

  
  
  virtual bool MyIpAddress(std::string* first_ip_address) = 0;

  
  
  
  
  

  virtual bool MyIpAddressEx(std::string* ip_address_list) = 0;

  
  
  virtual bool DnsResolve(const std::string& host,
                          std::string* first_ip_address) = 0;

  
  
  
  
  
  virtual bool DnsResolveEx(const std::string& host,
                            std::string* ip_address_list) = 0;

  
  
  
  
  static ProxyResolverJSBindings* CreateDefault();

 private:
};

}  

#endif  
