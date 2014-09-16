/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <utils/waitgroup.h>
#include <utils/taskpool.h>
#include "sockbase.h"

int xsocket (int pf, int socktype)
{
	int fd = xalloc (pf, socktype);
	return fd;
}

int xbind (int fd, const char *addr)
{
	int rc;
	struct sockbase *sb = xget (fd);

	if (!sb) {
		errno = EBADF;
		return -1;
	}
	if (strlen (addr) >= PATH_MAX) {
		xput (fd);
		errno = EINVAL;
		return -1;
	}
	BUG_ON (!sb->vfptr);
	rc = sb->vfptr->bind (sb, addr);
	xput (fd);
	return rc;
}
