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
#include "xgb.h"

int xcpu_alloc() {
    int cpu_no;

    mutex_lock(&xgb.lock);
    BUG_ON(xgb.ncpus >= XSOCK_MAX_CPUS);
    cpu_no = xgb.cpu_unused[xgb.ncpus++];
    mutex_unlock(&xgb.lock);
    return cpu_no;
}

int xcpu_choosed(int xd) {
    return xd % xgb.ncpus;
}

void xcpu_free(int cpu_no) {
    mutex_lock(&xgb.lock);
    xgb.cpu_unused[--xgb.ncpus] = cpu_no;
    mutex_unlock(&xgb.lock);
}

struct xcpu *xcpuget(int cpu_no) {
    return &xgb.cpus[cpu_no];
}


