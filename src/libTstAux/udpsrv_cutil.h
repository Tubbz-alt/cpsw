/*
 //@C Copyright Notice
 //@C ================
 //@C This file is part of CPSW. It is subject to the license terms in the LICENSE.txt
 //@C file found in the top-level directory of this distribution and at
 //@C https://confluence.slac.stanford.edu/display/ppareg/LICENSE.html.
 //@C
 //@C No part of CPSW, including this file, may be copied, modified, propagated, or
 //@C distributed except according to the terms contained in the LICENSE.txt file.
*/

#ifndef UDPSRV_C_UTILS_H
#define UDPSRV_C_UTILS_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef  void *FrameNoAllocator;

	FrameNoAllocator udpsrvCreateFrameNoAllocator();

	unsigned udpsrvAllocFrameNo(FrameNoAllocator);

#ifdef __cplusplus
};
#endif

#endif

