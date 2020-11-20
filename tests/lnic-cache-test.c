#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lnic.h"

#define REPEAT_4(X) X X X X
#define REPEAT_8(X) REPEAT_4(X) REPEAT_4(X)
#define REPEAT_16(X) REPEAT_8(X) REPEAT_8(X)
#define REPEAT_32(X) REPEAT_16(X) REPEAT_16(X)
#define REPEAT_48(X) REPEAT_32(X) REPEAT_16(X)
#define REPEAT_56(X) REPEAT_48(X) REPEAT_8(X)
#define REPEAT_60(X) REPEAT_56(X) REPEAT_4(X)
#define REPEAT_62(X) REPEAT_60(X) X X
#define REPEAT_63(X) REPEAT_62(X) X
#define REPEAT_64(X) REPEAT_32(X) REPEAT_32(X)

#define MSG_SIZE_WORDS 64

// Every message will be 512 bytes, or 64 words
// We'll start off with a direct loop, and then have the first word be held for the end
// Then the first two words will be held, then the first four, then 8, then 16, then 32, then all 64

int main() {
    uint64_t app_hdr;
    uint64_t buffer[MSG_SIZE_WORDS];
    lnic_add_context(0, 0);

    // Direct loop
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    REPEAT_64(lnic_copy();)
    lnic_copy();
    lnic_msg_done();

    // Hold first word
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    REPEAT_63(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_copy();
    lnic_msg_done();

    // Hold first two words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    REPEAT_62(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_copy();
    lnic_msg_done();

    // Hold first four words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    buffer[2] = lnic_read();
    buffer[3] = lnic_read();
    REPEAT_60(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_write_r(buffer[2]);
    lnic_write_r(buffer[3]);
    lnic_copy();
    lnic_msg_done();

    // Hold first 8 words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    buffer[2] = lnic_read();
    buffer[3] = lnic_read();
    buffer[4] = lnic_read();
    buffer[5] = lnic_read();
    buffer[6] = lnic_read();
    buffer[7] = lnic_read();
    REPEAT_56(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_write_r(buffer[2]);
    lnic_write_r(buffer[3]);
    lnic_write_r(buffer[4]);
    lnic_write_r(buffer[5]);
    lnic_write_r(buffer[6]);
    lnic_write_r(buffer[7]);
    lnic_copy();
    lnic_msg_done();

    // Hold first 16 words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    buffer[2] = lnic_read();
    buffer[3] = lnic_read();
    buffer[4] = lnic_read();
    buffer[5] = lnic_read();
    buffer[6] = lnic_read();
    buffer[7] = lnic_read();
    buffer[8] = lnic_read();
    buffer[9] = lnic_read();
    buffer[10] = lnic_read();
    buffer[11] = lnic_read();
    buffer[12] = lnic_read();
    buffer[13] = lnic_read();
    buffer[14] = lnic_read();
    buffer[15] = lnic_read();
    REPEAT_48(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_write_r(buffer[2]);
    lnic_write_r(buffer[3]);
    lnic_write_r(buffer[4]);
    lnic_write_r(buffer[5]);
    lnic_write_r(buffer[6]);
    lnic_write_r(buffer[7]);
    lnic_write_r(buffer[8]);
    lnic_write_r(buffer[9]);
    lnic_write_r(buffer[10]);
    lnic_write_r(buffer[11]);
    lnic_write_r(buffer[12]);
    lnic_write_r(buffer[13]);
    lnic_write_r(buffer[14]);
    lnic_write_r(buffer[15]);
    lnic_copy();
    lnic_msg_done();

    // Hold first 32 words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    buffer[2] = lnic_read();
    buffer[3] = lnic_read();
    buffer[4] = lnic_read();
    buffer[5] = lnic_read();
    buffer[6] = lnic_read();
    buffer[7] = lnic_read();
    buffer[8] = lnic_read();
    buffer[9] = lnic_read();
    buffer[10] = lnic_read();
    buffer[11] = lnic_read();
    buffer[12] = lnic_read();
    buffer[13] = lnic_read();
    buffer[14] = lnic_read();
    buffer[15] = lnic_read();
    buffer[16] = lnic_read();
    buffer[17] = lnic_read();
    buffer[18] = lnic_read();
    buffer[19] = lnic_read();
    buffer[20] = lnic_read();
    buffer[21] = lnic_read();
    buffer[22] = lnic_read();
    buffer[23] = lnic_read();
    buffer[24] = lnic_read();
    buffer[25] = lnic_read();
    buffer[26] = lnic_read();
    buffer[27] = lnic_read();
    buffer[28] = lnic_read();
    buffer[29] = lnic_read();
    buffer[30] = lnic_read();
    buffer[31] = lnic_read();
    REPEAT_32(lnic_copy();)
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_write_r(buffer[2]);
    lnic_write_r(buffer[3]);
    lnic_write_r(buffer[4]);
    lnic_write_r(buffer[5]);
    lnic_write_r(buffer[6]);
    lnic_write_r(buffer[7]);
    lnic_write_r(buffer[8]);
    lnic_write_r(buffer[9]);
    lnic_write_r(buffer[10]);
    lnic_write_r(buffer[11]);
    lnic_write_r(buffer[12]);
    lnic_write_r(buffer[13]);
    lnic_write_r(buffer[14]);
    lnic_write_r(buffer[15]);
    lnic_write_r(buffer[16]);
    lnic_write_r(buffer[17]);
    lnic_write_r(buffer[18]);
    lnic_write_r(buffer[19]);
    lnic_write_r(buffer[20]);
    lnic_write_r(buffer[21]);
    lnic_write_r(buffer[22]);
    lnic_write_r(buffer[23]);
    lnic_write_r(buffer[24]);
    lnic_write_r(buffer[25]);
    lnic_write_r(buffer[26]);
    lnic_write_r(buffer[27]);
    lnic_write_r(buffer[28]);
    lnic_write_r(buffer[29]);
    lnic_write_r(buffer[30]);
    lnic_write_r(buffer[31]);
    lnic_copy();
    lnic_msg_done();

    // Hold all words
    lnic_wait();
    app_hdr = lnic_read();
    lnic_write_r(app_hdr);
    buffer[0] = lnic_read();
    buffer[1] = lnic_read();
    buffer[2] = lnic_read();
    buffer[3] = lnic_read();
    buffer[4] = lnic_read();
    buffer[5] = lnic_read();
    buffer[6] = lnic_read();
    buffer[7] = lnic_read();
    buffer[8] = lnic_read();
    buffer[9] = lnic_read();
    buffer[10] = lnic_read();
    buffer[11] = lnic_read();
    buffer[12] = lnic_read();
    buffer[13] = lnic_read();
    buffer[14] = lnic_read();
    buffer[15] = lnic_read();
    buffer[16] = lnic_read();
    buffer[17] = lnic_read();
    buffer[18] = lnic_read();
    buffer[19] = lnic_read();
    buffer[20] = lnic_read();
    buffer[21] = lnic_read();
    buffer[22] = lnic_read();
    buffer[23] = lnic_read();
    buffer[24] = lnic_read();
    buffer[25] = lnic_read();
    buffer[26] = lnic_read();
    buffer[27] = lnic_read();
    buffer[28] = lnic_read();
    buffer[29] = lnic_read();
    buffer[30] = lnic_read();
    buffer[31] = lnic_read();
    buffer[32] = lnic_read();
    buffer[33] = lnic_read();
    buffer[34] = lnic_read();
    buffer[35] = lnic_read();
    buffer[36] = lnic_read();
    buffer[37] = lnic_read();
    buffer[38] = lnic_read();
    buffer[39] = lnic_read();
    buffer[40] = lnic_read();
    buffer[41] = lnic_read();
    buffer[42] = lnic_read();
    buffer[43] = lnic_read();
    buffer[44] = lnic_read();
    buffer[45] = lnic_read();
    buffer[46] = lnic_read();
    buffer[47] = lnic_read();
    buffer[48] = lnic_read();
    buffer[49] = lnic_read();
    buffer[50] = lnic_read();
    buffer[51] = lnic_read();
    buffer[52] = lnic_read();
    buffer[53] = lnic_read();
    buffer[54] = lnic_read();
    buffer[55] = lnic_read();
    buffer[56] = lnic_read();
    buffer[57] = lnic_read();
    buffer[58] = lnic_read();
    buffer[59] = lnic_read();
    buffer[60] = lnic_read();
    buffer[61] = lnic_read();
    buffer[62] = lnic_read();
    buffer[63] = lnic_read();
    lnic_write_r(buffer[0]);
    lnic_write_r(buffer[1]);
    lnic_write_r(buffer[2]);
    lnic_write_r(buffer[3]);
    lnic_write_r(buffer[4]);
    lnic_write_r(buffer[5]);
    lnic_write_r(buffer[6]);
    lnic_write_r(buffer[7]);
    lnic_write_r(buffer[8]);
    lnic_write_r(buffer[9]);
    lnic_write_r(buffer[10]);
    lnic_write_r(buffer[11]);
    lnic_write_r(buffer[12]);
    lnic_write_r(buffer[13]);
    lnic_write_r(buffer[14]);
    lnic_write_r(buffer[15]);
    lnic_write_r(buffer[16]);
    lnic_write_r(buffer[17]);
    lnic_write_r(buffer[18]);
    lnic_write_r(buffer[19]);
    lnic_write_r(buffer[20]);
    lnic_write_r(buffer[21]);
    lnic_write_r(buffer[22]);
    lnic_write_r(buffer[23]);
    lnic_write_r(buffer[24]);
    lnic_write_r(buffer[25]);
    lnic_write_r(buffer[26]);
    lnic_write_r(buffer[27]);
    lnic_write_r(buffer[28]);
    lnic_write_r(buffer[29]);
    lnic_write_r(buffer[30]);
    lnic_write_r(buffer[31]);
    lnic_write_r(buffer[32]);
    lnic_write_r(buffer[33]);
    lnic_write_r(buffer[34]);
    lnic_write_r(buffer[35]);
    lnic_write_r(buffer[36]);
    lnic_write_r(buffer[37]);
    lnic_write_r(buffer[38]);
    lnic_write_r(buffer[39]);
    lnic_write_r(buffer[40]);
    lnic_write_r(buffer[41]);
    lnic_write_r(buffer[42]);
    lnic_write_r(buffer[43]);
    lnic_write_r(buffer[44]);
    lnic_write_r(buffer[45]);
    lnic_write_r(buffer[46]);
    lnic_write_r(buffer[47]);
    lnic_write_r(buffer[48]);
    lnic_write_r(buffer[49]);
    lnic_write_r(buffer[50]);
    lnic_write_r(buffer[51]);
    lnic_write_r(buffer[52]);
    lnic_write_r(buffer[53]);
    lnic_write_r(buffer[54]);
    lnic_write_r(buffer[55]);
    lnic_write_r(buffer[56]);
    lnic_write_r(buffer[57]);
    lnic_write_r(buffer[58]);
    lnic_write_r(buffer[59]);
    lnic_write_r(buffer[60]);
    lnic_write_r(buffer[61]);
    lnic_write_r(buffer[62]);
    lnic_write_r(buffer[63]);
    lnic_copy();
    lnic_msg_done();
    return 0;
}