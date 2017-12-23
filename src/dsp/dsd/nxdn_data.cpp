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


static const uint16_t start_state = 0x154;//0x154;//0xe4;
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
    unsigned char rawdata2[60];
    unsigned char deconv[36];
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
    unsigned char byte=0;


    // Initialize stuff
    lfsr_state = start_state;
    memset(rawdata,0,60);
    memset(deconv,0,36);


    for (i = 0; i < 60; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
        rawdata2[i] = a>>1;
        rawdata2[i+1] = a&1;
    }

/*
    printf("\noriginal ");
    for (i=0;i<60;i++)
    {
        byte |= (rawdata2[i]<<(7-(i%8)));
        if ((i%8)==7)
        {
            printf("[%02x]",byte);
            byte = 0;
        }
        printf("%02x ",rawdata2[i]);
    }
    printf("\n");

    printf("\ndescrambled ");
    byte = 0;
    for (i=0;i<60;i++)
    {
        byte |= (rawdata[i]<<(7-(i%8)));
        if ((i%8)==7)
        {
            printf("[%02x]",byte);
            byte = 0;
        }
        printf("%02x ",rawdata[i]);
    }
    printf("\n");
*/

    for (i=0;i<60;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(5);
    deinterleaver.set_cols(12);
    ioutput = deinterleaver.interleave(iinput);

//    cout << "AHOYdeinterleaved = " << ioutput << endl;
//    cout << "AHOYpreinterleaved = " << iinput << endl;

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

    encoded = modulated;// + sqrt(0.5)*randn(encoded.size());
//    cout << "AHOYBPSK = " << encoded << endl;
    conv_code.decode(encoded,decoded_bits);
//    cout << "AHOYfinal = " << decoded_bits << endl;
    decoded = to_ivec(decoded_bits);


    printf("SACCH = ");
    for (i=0;i<36;i++)
    {
        deconv[i] = decoded[i];
        printf("%02x ",deconv[i]);
    }
    printf("\n");

    char flags = (deconv[8]<<1)|(deconv[9]);
    char msgtype = deconv[15]|(deconv[14]<<1)|(deconv[13]<<2)|(deconv[12]<<3)|(deconv[11]<<4)|(deconv[10]<<5);
    printf("message type = %02x flags=%02x\n",msgtype,flags);


    //unsigned short crc = crc6(,3);
#endif
}




static void process_rtch_facch1(dsd_opts * opts, dsd_state * state)
{
#ifdef ITPP_FOUND
    unsigned char rawdata[144];
    unsigned char rawdata2[144];
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
    unsigned char byte=0;

    memset(rawdata,0,144);
    memset(deconv,0,96);

    for (i = 0; i < 144; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
        rawdata2[i] = a>>1;
        rawdata2[i+1] = a&1;
    }

/*
    printf("\noriginal ");
    for (i=0;i<144;i++)
    {
        byte |= (rawdata2[i]<<(7-(i%8)));
        if ((i%8)==7)
        {
            printf("[%02x]",byte);
            byte = 0;
        }
        printf("%02x ",rawdata2[i]);
    }
    printf("\n");

    printf("\ndescrambled ");
    byte = 0;
    for (i=0;i<144;i++)
    {
        byte |= (rawdata[i]<<(7-(i%8)));
        if ((i%8)==7)
        {
            printf("[%02x]",byte);
            byte = 0;
        }
        printf("%02x ",rawdata[i]);
    }
    printf("\n");
*/

    for (i=0;i<144;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(9);
    deinterleaver.set_cols(16);
    ioutput = deinterleaver.interleave(iinput);

//    cout << "AHOYdeinterleaved = " << ioutput << endl;
//    cout << "AHOYpreinterleaved = " << iinput << endl;

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

    encoded = modulated;// + sqrt(0.5)*randn(encoded.size());
//    cout << "AHOYBPSK = " << encoded << endl;
    conv_code.decode(encoded,decoded_bits);
//    cout << "AHOYfinal = " << decoded_bits << endl;
    decoded = to_ivec(decoded_bits);

    //printf("FACCH1 = ");
    for (i=0;i<96;i++)
    {
        deconv[i] = decoded[i];
        //printf("%02x ",deconv[i]);
    }
    //printf("\n");





    //unsigned short crc = crc6(,3);
#endif
}



static void process_rcch_cac1(dsd_opts * opts, dsd_state * state)
{
#ifdef ITPP_FOUND
    unsigned char rawdata[300];
    unsigned char rawdata2[300];
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
    unsigned char byte=0;

    lfsr_state = start_state;
    memset(rawdata,0,300);
    memset(deconv,0,175);

    for (i = 0; i < 300; i+=2)
    {
        char a = getDibit (opts, state);
        rawdata[i] = lfsr()^(a>>1);
        rawdata[i+1] = a&1;
        rawdata2[i] = a>>1;
        rawdata2[i+1] = a&1;
    }


    for (i=0;i<300;i++)
        iinput[i] = rawdata[i];

    deinterleaver.set_rows(25);
    deinterleaver.set_cols(12);
    ioutput = deinterleaver.interleave(iinput);

//    cout << "AHOYdeinterleaved = " << ioutput << endl;
//    cout << "AHOYpreinterleaved = " << iinput << endl;

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

    encoded = modulated;// + sqrt(0.5)*randn(encoded.size());
    conv_code.decode(encoded,decoded_bits);
    decoded = to_ivec(decoded_bits);

    for (i=0;i<175;i++)
    {
        deconv[i] = decoded[i];
    }
    

    byte = get_byte(deconv,0);
    int ran = byte & 0x3F;
    byte = get_byte(deconv,1) & 0x3F;
    // Site information
    if (byte==0x18)
    {
        printf("Site information - ");
        printf("Location ID: %02x%02x%02x ",get_byte(deconv,2),get_byte(deconv,3),get_byte(deconv,4));
        printf("Chan.Struct: %02x%02x ",get_byte(deconv,5),get_byte(deconv,6));
        printf("Serv.Info: %02x%02x\n",get_byte(deconv,7),get_byte(deconv,8));
    }
    // Service information
    else if (byte==0x19)
    {
        printf("Service information - ");
        printf("Location ID: %02x%02x%02x ",get_byte(deconv,2),get_byte(deconv,3),get_byte(deconv,4));
        printf("Serv. Info: %02x%02x\n",get_byte(deconv,5),get_byte(deconv,6));
    }
    // Digital Information
    else if (byte==0x17)
    {
        printf("Station ID information - ");
        byte = get_byte(deconv,2);
        printf("Option: %02x ",byte);
        printf("ID: %c%c%c%c%c%c\n",get_byte(deconv,3),get_byte(deconv,4),get_byte(deconv,5),get_byte(deconv,6),get_byte(deconv,7),get_byte(deconv,8));
    }
    else if (byte==0x17)
    {
        printf("Station ID information - ");
        byte = get_byte(deconv,2);
        printf("Option: %02x ",byte);
        printf("ID: %c%c%c%c%c%c\n",get_byte(deconv,3),get_byte(deconv,4),get_byte(deconv,5),get_byte(deconv,6),get_byte(deconv,7),get_byte(deconv,8));
    }
    else if (byte==0x1)
    {
        printf("Voice Call Response - ");
        printf("Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        printf("Dest. ID: %02x%02x ",get_byte(deconv,6),get_byte(deconv,7));
        printf("Cipher Type: %02x\n",get_byte(deconv,8)>>6);
    }
    else if (byte==0x2)
    {
        printf("Voice Call Reception Request - ");
        printf("Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        printf("Dest. ID: %02x%02x\n",get_byte(deconv,6),get_byte(deconv,7));
    }
    else if (byte==0x3)
    {
        printf("Voice Call Connection Response - ");
        printf("Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        printf("Dest. ID: %02x%02x\n",get_byte(deconv,6),get_byte(deconv,7));
    }
    else if ((byte==0x4)||(byte==0x5))
    {
        printf("Voice Call Assignment - ");
        printf("Source ID: %02x%02x ",get_byte(deconv,4),get_byte(deconv,5));
        printf("Dest. ID: %02x%02x ",get_byte(deconv,6),get_byte(deconv,7));
        printf("Channel: %d\n",(int)((get_byte(deconv,8)&3)<<8)|get_byte(deconv,9));
    }
    else if (byte==0x10)
        printf("Idle.\n");
    else if (byte==0x1a)
        printf("CC information\n");
    else if (byte==0x1b)
        printf("Adjacent site information\n");
    else if (byte==0x1c)
        printf("System failure notification\n");
    else printf("Unknown Message type=%02x\n",byte);
    
    

    //unsigned short crc = crc6(,3);
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
                        printf("Idle data\n");
                    else if(((lich[4] << 1) | lich[5])==2)
                        printf("Common data\n");
                    else
                        printf("Impossible (bad decode?!?)\n");
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