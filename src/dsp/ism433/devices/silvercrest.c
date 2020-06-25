#include "../includes/rtl_433.h"

static int silvercrest_callback(bitbuffer_t *bitbuffer) {
    bitrow_t *bb = bitbuffer->bb;
    data_t *data;
    /* FIXME validate the received message better */
    if (bb[1][0] == 0xF8 &&
        bb[2][0] == 0xF8 &&
        bb[3][0] == 0xF8 &&
        bb[4][0] == 0xF8 &&
        bb[1][1] == 0x4d &&
        bb[2][1] == 0x4d &&
        bb[3][1] == 0x4d &&
        bb[4][1] == 0x4d) {
        /* Pretty sure this is a Silvercrest remote */
        fprintf(stdout, "Remote button event:\n");
        fprintf(stdout, "model = Silvercrest, %d bits\n",bitbuffer->bits_per_row[1]);
        fprintf(stdout, "%02x %02x %02x %02x %02x\n",bb[1][0],bb[0][1],bb[0][2],bb[0][3],bb[0][4]);

        data = data_make(
                        "model",        "",             DATA_STRING,    "Silvercrest remote button",
                        "byte1",        "Byte1",        DATA_INT,       bb[0][1],
                        "byte1",        "Byte2",        DATA_INT,       bb[0][2],
                        "byte1",        "Byte3",        DATA_INT,       bb[0][3],
                        "byte1",        "Byte4",        DATA_INT,       bb[0][4],
                        NULL);
        data_acquired_handler(data);


        return 1;
    }
    return 0;
}

r_device silvercrest = {
    .name           = "Silvercrest Remote Control",
    .modulation     = OOK_PULSE_PWM_RAW,
    .short_limit    = 600,
    .long_limit     = 5000,
    .reset_limit    = 15000,
    .json_callback  = &silvercrest_callback,
    .disabled       = 1,
    .demod_arg      = 1,	// Remove startbit
};