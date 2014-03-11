#include <iostream>
#include <gtest/gtest.h>
extern "C" {
#include "bufio/bio.h"
}

extern int randstr(char *buf, int len);

static void bufio_test() {
    char buf1[PAGE_SIZE * 4] = {}, buf2[PAGE_SIZE * 4] = {};
    struct io ops = {};
    struct bio b = {};
    int i;
    int64_t rlen = rand() % PAGE_SIZE + 3 * PAGE_SIZE;

    bio_init(&b, &ops);
    randstr(buf1, sizeof(buf1));
    for (i = 0; i < rlen; i++)
	EXPECT_EQ(1, bio_write(&b, buf1 + i, 1));
    EXPECT_EQ(b.bsize, rlen);
    EXPECT_EQ(b.bsize, bio_readfull(&b, buf2, 4 * PAGE_SIZE));
    EXPECT_TRUE(memcmp(buf1, buf2, rlen) == 0);
    bio_destroy(&b);
}

TEST(bufio, bio) {
    bufio_test();
}
