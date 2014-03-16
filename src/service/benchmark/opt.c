#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "os/timesz.h"
#include "opt.h"

static char __usage[] = "\n\
NAME\n\
    pio_benchmark - pio benchmark program\n\
\n\
SYNOPSIS\n\
    pio -r time -c comsumers -p producers -h host -m mode\n\
\n\
OPTIONS\n\
    -r benchmark running time. default forever\n\
    -c comsumer number\n\
    -p producer number\n\
    -h proxyio host addr\n\
    -x proxyname\n\
    -m benchmark mode\n\
\n\
EXAMPLE:\n\
    pio -r 60 -c 20 -p 20 -h 127.0.0.1:4478 -m mode\n\n";


static inline void usage() {
    system("clear");
    printf("%s", __usage);
}

int getoption(int argc, char* argv[], struct bc_opt *cf) {
    int rc;

    cf->deadline = 0xffffffff;
    while ( (rc = getopt(argc, argv, "r:c:p:x:h:m:s:z")) != -1 ) {
        switch(rc) {
	case 'r':
	    cf->deadline = rt_mstime() + atoi(optarg) * 1E3;
	    break;
        case 'c':
	    cf->comsumer_num = atoi(optarg);
            break;
	case 'p':
	    cf->producer_num = atoi(optarg);
	    break;
	case 'x':
	    cf->proxyname = optarg;
	    break;
	case 'h':
	    cf->host = optarg;
	    break;
	case 'm':
	    cf->mode = atoi(optarg);
	    break;
	case 's':
	    cf->size = atoi(optarg);
	    break;
	default:
	    usage();
	    return -1;
        }
    }
    return 0;
}
