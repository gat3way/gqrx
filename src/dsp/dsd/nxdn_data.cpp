extern "C" {
#include "dsd.h"
#include "nxdn_const.h"
#include <stdint.h>
#include <math.h>
}

#ifdef ITPP_FOUND
#include <itpp/itcomm.h>
using namespace itpp;
using std::cout;
using std::endl;
#endif


static const uint16_t start_state = 0x154;
static uint16_t lfsr_state;


char lfsr(void)
{
    uint16_t bit,bit2;

    bit  = (((lfsr_state >> 4)&1) ^ (lfsr_state&1))&1;
    bit2 = lfsr_state&1;
    lfsr_state = ((lfsr_state >> 1) | ((bit << 8))) & 0x1ff;

    return bit2;
}


unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

uint16_t crc12(uint8_t const* message, int nNibble)
{
    uint16_t  remainder = 0;
    int nibble;
    uint8_t bit, byte;
    int WIDTH=12;
    int TOPBIT=(1 << (WIDTH - 1));
    int POLYNOMIAL=0x180F;

    for (nibble = 0; nibble <nNibble; ++nibble)
    {
        byte = reverse(message[nibble / 2]);
        if (nibble % 2)
            remainder ^= ((byte & 0x0F) << (WIDTH - 4));
        else
            remainder ^= ((byte & 0xF0) << (WIDTH - 8));
        for (bit = 4; bit > 0; --bit)
        {
            if (remainder & TOPBIT)
                remainder = (remainder << 1) ^ POLYNOMIAL;
            else
                remainder = (remainder << 1);
        }
    }
    return (remainder & 0x0FFF);
}



uint16_t crc6(uint8_t* msg, int len) 
{
    uint16_t crc = 0x00;
    int pos,i;
    uint16_t poly = 0x67;

    for (pos = 0; pos < len; pos++) 
    {
        crc ^= msg[pos];
        for (i = 8; i != 0; i--) 
        {
            if ((crc & 0x0001) != 0) 
            {
                crc >>= 1;
                crc ^= poly;
            } else
                crc >>= 1;
        }
    }
    return crc;
}


unsigned char get_byte(unsigned char *stream, int byte)
{
    unsigned char t=0;
    int i = byte*8;

    t = (stream[i]<<7)|(stream[i+1]<<6)|(stream[i+2]<<5)|(stream[i+3]<<4);
    t |= (stream[i+4]<<3)|(stream[i+5]<<2)|(stream[i+6]<<1)|(stream[i+7]);
    return t;
}




static void process_rtch_sacch(dsd_opts * opts, dsd_state * state)
{
#ifdef ITPP_FOUND
    unsigned char rawdata[60];
    unsigned char deconv[36];
    char msg[1024];
    int i;
    // ITPP stuff
    bvec iinput(60);
    bvec ioutput(60);
    Block_Interleaver<bin> deinterleaver;
    BPSK bpsk;
    int constraint_length;
    ivec generators(2);
    bvec decoded_bits(40);
    Punctured_Convolutional_Code conv_code;
    vec encoded(72), modulated(72);
    ivec decoded(36);

    // Initialize stuff
    msg[0] = 0;
    lfsr_state = start_state;
    memset(rawdata,0,60);
    memset(deconv,0,36);


    for (i = 0; i < 60; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
    }

    for (i=0;i<60;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(5);
    deinterleaver.set_cols(12);
    ioutput = deinterleaver.interleave(iinput);

    // Octal
    generators.set_size(2, false);
    generators(0) = 023;//031;
    generators(1) = 035;//027;
    constraint_length = 5;
    conv_code.set_generator_polynomials(generators, constraint_length);
    bmat puncture_matrix = "1 1 1 1 1 1;1 1 0 1 1 0";

    conv_code.set_puncture_matrix(puncture_matrix);
    conv_code.set_truncation_length(32);
    modulated = bpsk.modulate_bits(ioutput);

    encoded = modulated;
    conv_code.decode(encoded,decoded_bits);
    decoded = to_ivec(decoded_bits);

    char flags = (deconv[8]<<1)|(deconv[9]);
    char msgtype = deconv[15]|(deconv[14]<<1)|(deconv[13]<<2)|(deconv[12]<<3)|(deconv[11]<<4)|(deconv[10]<<5);
    sprintf(msg,"message type = %02x flags=%02x\n",msgtype,flags);
    strcat(state->msgbuf,msg);


#endif
}




static void process_rtch_facch1(dsd_opts * opts, dsd_state * state)
{
#ifdef ITPP_FOUND
    unsigned char rawdata[144];
    unsigned char deconv[96];
    int i;
    // ITPP stuff
    bvec iinput(144);
    bvec ioutput(144);
    Block_Interleaver<bin> deinterleaver;
    BPSK bpsk;
    int constraint_length;
    ivec generators(2);
    bvec decoded_bits(96);
    Punctured_Convolutional_Code conv_code;
    vec encoded(144), modulated(144);
    ivec decoded(96);

    memset(rawdata,0,144);
    memset(deconv,0,96);

    for (i = 0; i < 144; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
    }

    for (i=0;i<144;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(9);
    deinterleaver.set_cols(16);
    ioutput = deinterleaver.interleave(iinput);

    // Octal
    generators.set_size(2, false);
    generators(0) = 023;//031;
    generators(1) = 035;//027;
    constraint_length = 5;
    conv_code.set_generator_polynomials(generators, constraint_length);
    bmat puncture_matrix = "1 1;0 1";

    conv_code.set_puncture_matrix(puncture_matrix);
    conv_code.set_truncation_length(32);
    modulated = bpsk.modulate_bits(ioutput);

    encoded = modulated;
    conv_code.decode(encoded,decoded_bits);
    decoded = to_ivec(decoded_bits);

    for (i=0;i<96;i++)
    {
        deconv[i] = decoded[i];
    }


#endif
}



static void process_rcch_cac1(dsd_opts * opts, dsd_state * state)
{
#ifdef ITPP_FOUND
    unsigned char rawdata[300];
    unsigned char deconv[175];
    int i;
    // ITPP stuff
    bvec iinput(300);
    bvec ioutput(300);
    Block_Interleaver<bin> deinterleaver;
    BPSK bpsk;
    int constraint_length;
    ivec generators(2);
    bvec decoded_bits(175);
    Punctured_Convolutional_Code conv_code;
    vec encoded(300), modulated(300);
    ivec decoded(175);
    unsigned char byte;
    char msg[1024];

    msg[0] = 0;
    lfsr_state = start_state;
    memset(rawdata,0,300);
    memset(deconv,0,175);

    state->trunkdata.newdata = 0;
    state->trunkdata.trunkid = 0;
    state->trunkdata.nxdntype = 0;
    state->trunkdata.fixedchan = -1;
    state->trunkdata.fixedbase = -1;
    state->trunkdata.callchan = -1;
    state->trunkdata.callsrc = -1;
    state->trunkdata.calldst = -1;
    state->trunkdata.regunit = -1;
    state->trunkdata.regdst = -1;

    for (i = 0; i < 300; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
    }


    for (i=0;i<300;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(25);
    deinterleaver.set_cols(12);
    ioutput = deinterleaver.interleave(iinput);

    // Octal
    generators.set_size(2, false);
    generators(0) = 023;//031;
    generators(1) = 035;//027;
    constraint_length = 5;
    conv_code.set_generator_polynomials(generators, constraint_length);
    bmat puncture_matrix = "1 1 1 1 1 1 1;1 0 1 1 1 0 1";

    conv_code.set_puncture_matrix(puncture_matrix);
    conv_code.set_truncation_length(32);
    modulated = bpsk.modulate_bits(ioutput);

    encoded = modulated;
    conv_code.decode(encoded,decoded_bits);
    decoded = to_ivec(decoded_bits);

    for (i=0;i<175;i++)
    {
        deconv[i] = decoded[i];
    }
    

    //byte = get_byte(deconv,0);
    //int ran = byte & 0x3F;
    byte = get_byte(deconv,1) & 0x3F;

    // Site information
    if (byte==0x18)
    {
        sprintf(msg,"Site information - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Location ID: %02x%02x%02x ",get_byte(deconv,2),get_byte(deconv,3),get_byte(deconv,4));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Chan.Access: %02x ",get_byte(deconv,12));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Serv.Info: %02x%02x\n",get_byte(deconv,7),get_byte(deconv,8));
        strcat(state->msgbuf,msg);
        state->trunkdata.trunkid = (get_byte(deconv,2)<<16|get_byte(deconv,3)<<8|get_byte(deconv,4));
        if (get_byte(deconv,12)>>7)
        {
            state->trunkdata.fixedchan = 1;
            state->trunkdata.fixedsteps = (get_byte(deconv,12)>>6)&3;
            state->trunkdata.fixedbase = (get_byte(deconv,12)>>5)&7;
        }
        else 
        {
            state->trunkdata.fixedchan = 0;
            state->trunkdata.fixedbase = 0;
            state->trunkdata.fixedsteps = 0;
        }
        state->trunkdata.trunkid = ((get_byte(deconv,2)<<16)|(get_byte(deconv,3)<<8)|get_byte(deconv,4));
        state->trunkdata.newdata = 1;
    }
    // Service information
    else if (byte==0x19)
    {
        sprintf(msg,"Service information - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Location ID: %02x%02x%02x ",get_byte(deconv,2),get_byte(deconv,3),get_byte(deconv,4));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Serv. Info: %02x%02x\n",get_byte(deconv,5),get_byte(deconv,6));
        strcat(state->msgbuf,msg);
        state->trunkdata.trunkid = ((get_byte(deconv,2)<<16)|(get_byte(deconv,3)<<8)|get_byte(deconv,4));
        state->trunkdata.newdata = 1;
    }
    // Digital Information
    else if (byte==0x17)
    {
        sprintf(msg,"Station ID information - ");
        strcat(state->msgbuf,msg);
        byte = get_byte(deconv,2);
        sprintf(msg,"Option: %02x ",byte);
        strcat(state->msgbuf,msg);
        sprintf(msg,"ID: %c%c%c%c%c%c\n",get_byte(deconv,3),get_byte(deconv,4),get_byte(deconv,5),get_byte(deconv,6),get_byte(deconv,7),get_byte(deconv,8));
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x20)
    {
        sprintf(msg,"Registration response - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Destination unit: %02x%02x ",get_byte(deconv,5),get_byte(deconv,6));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Group: %02x%02x\n",get_byte(deconv,7),get_byte(deconv,8));
        strcat(state->msgbuf,msg);
        state->trunkdata.regunit = ((get_byte(deconv,5)<<8)|get_byte(deconv,6));
        state->trunkdata.regdst = ((get_byte(deconv,7)<<8)|get_byte(deconv,8));
        state->trunkdata.newdata = 1;
    }

    else if (byte==0x1)
    {
        sprintf(msg,"Voice Call Response - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Dest. ID: %02x%02x ",get_byte(deconv,6),get_byte(deconv,7));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Cipher Type: %02x\n",get_byte(deconv,8)>>6);
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x2)
    {
        sprintf(msg,"Voice Call Reception Request - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Dest. ID: %02x%02x\n",get_byte(deconv,6),get_byte(deconv,7));
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x3)
    {
        sprintf(msg,"Voice Call Connection Response - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Dest. ID: %02x%02x\n",get_byte(deconv,6),get_byte(deconv,7));
        strcat(state->msgbuf,msg);
    }
    else if ((byte==0x4)||(byte==0x5))
    {
        sprintf(msg,"Voice Call Assignment - ");
        strcat(state->msgbuf,msg);
        sprintf(msg,"Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Dest. ID: %02x%02x ",get_byte(deconv,6),get_byte(deconv,7));
        strcat(state->msgbuf,msg);
        sprintf(msg,"Channel: %d\n",(int)((get_byte(deconv,8)&3)<<8)|get_byte(deconv,9));
        strcat(state->msgbuf,msg);
        state->trunkdata.callsrc = ((get_byte(deconv,4)<<8)|get_byte(deconv,5));
        state->trunkdata.calldst = ((get_byte(deconv,6)<<8)|get_byte(deconv,7));
        state->trunkdata.callchan = (int)(((get_byte(deconv,8)&3)<<8)|get_byte(deconv,9));
        state->trunkdata.newdata = 1;
    }
    else if (byte==0x10)
    {
        sprintf(msg,"Idle.\n");
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x1a)
    {
        sprintf(msg,"CC information\n");
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x1b)
    {
        sprintf(msg,"Adjacent site information\n");
        strcat(state->msgbuf,msg);
    }
    else if (byte==0x1c)
    {
        sprintf(msg,"System failure notification\n");
        strcat(state->msgbuf,msg);
    }
    else 
    {
        sprintf(msg,"Unknown Message type=%02x\n",byte);
        strcat(state->msgbuf,msg);
    }

#endif
}









extern "C" {
void
processNXDNData (dsd_opts * opts, dsd_state * state)
{
    int i, dibit;
    int *dibit_p;
    char lich[9];
    char lich_scram[9] = { 0, 0, 1, 0, 0, 1, 1, 1, 0 };
    char msg[1024];

    msg[0] = 0;

    dibit_p = state->dibit_buf_p - 8;

    for (i = 0; i < 8; i++)
    {
        dibit = *dibit_p;
        dibit_p++;
        if(lich_scram[i] ^ (state->lastsynctype & 0x1))
            dibit = (dibit ^ 2);
        lich[i] = 1 & (dibit >> 1);
    }

    if (opts->errorbars == 1)
    {
        switch((lich[0] << 1) | lich[1])
        {
            case 0:
                if (((lich[2] << 1) | lich[3])==0)
                {
                    sprintf(msg," Trunk-C Control Ch ");
                    strcat(state->msgbuf,msg);
                    if (((lich[4] << 1) | lich[5])==0)
                        process_rcch_cac1(opts,state);
                    else if(((lich[4] << 1) | lich[5])==1)
                    {
                        sprintf(msg,"Idle data\n");
                        strcat(state->msgbuf,msg);
                    }
                    else if(((lich[4] << 1) | lich[5])==2)
                    {
                        sprintf(msg,"Common data\n");
                        strcat(state->msgbuf,msg);
                    }
                    else
                    {
                        sprintf(msg,"Impossible (bad decode?!?)\n");
                        strcat(state->msgbuf,msg);
                    }
                }
                break;
            case 1:
                sprintf(msg," Trunk-C Traffic Ch ");
                strcat(state->msgbuf,msg);
                process_rtch_sacch(opts, state);
                process_rtch_facch1(opts, state);
                process_rtch_facch1(opts, state);
                break;
            case 2:
                if(lich[6]) 
                {
                    sprintf (msg, " Repeater Ch ");
                    strcat(state->msgbuf,msg);
                }
                else
                {
                    sprintf(msg," Mobile Direct Ch ");
                    strcat(state->msgbuf,msg);
                }
                break;
            case 3:
                sprintf(msg," Trunk-D Composite Ch ");
                strcat(state->msgbuf,msg);
                break;
        }
        sprintf (msg,"DATA    ");
        strcat(state->msgbuf,msg);
    }


    if (opts->errorbars == 1)
    {
        sprintf (msg,"\n");
        strcat(state->msgbuf,msg);
    }
}
}