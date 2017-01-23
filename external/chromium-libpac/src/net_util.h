// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_UTIL_H_
#define NET_BASE_NET_UTIL_H_
#pragma once

#if defined(OS_WIN)
#include <windows.h>
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <sys/socket.h>
#endif

#include <list>
#include <string>
#include <set>
#include <vector>

#include <cstdint>

namespace net {

typedef std::vector<unsigned char> IPAddressNumber;
typedef std::vector<IPAddressNumber> IPAddressList;

bool ParseIPLiteralToNumber(const std::string& ip_literal,
                            IPAddressNumber* ip_number);

bool ParseCIDRBlock(const std::string& cidr_literal,
                    IPAddressNumber* ip_number,
                    size_t* prefix_length_in_bits);

bool IPNumberMatchesPrefix(const IPAddressNumber& ip_number,
                           const IPAddressNumber& ip_prefix,
                           size_t prefix_length_in_bits);

}  

#endif  
