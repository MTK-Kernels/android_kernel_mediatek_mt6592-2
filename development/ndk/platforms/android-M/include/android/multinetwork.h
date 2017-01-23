/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_MULTINETWORK_H
#define ANDROID_MULTINETWORK_H

#include <netdb.h>
#include <stdlib.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

typedef uint64_t net_handle_t;

#define NETWORK_UNSPECIFIED  ((net_handle_t)0)




int android_setsocknetwork(net_handle_t network, int fd);


int android_setprocnetwork(net_handle_t network);


int android_getaddrinfofornetwork(net_handle_t network,
        const char *node, const char *service,
        const struct addrinfo *hints, struct addrinfo **res);

__END_DECLS

#endif  
