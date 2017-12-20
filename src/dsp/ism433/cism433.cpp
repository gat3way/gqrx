/*
 *      cism433.cpp -- ISM433 demodulator class
 *
 *      Copyright (C) 2017 Milen Rangelov
 *      Significant portions of the code are used from xdemorse project 
 *      Which is Copyright (C) Neoklis Kyriazis (nkcyham at yahoo.com)
 *
 *      Copyright (C) 2011 Alexandru Csete (oz9aec at gmail.com)
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <QDebug>
#include <QTime>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include "cism433.h"
#include "includes/data.h"


extern "C" {
    static char message[65535];
    char tmp[1024];

    static float f2c(float fahrenheit)
    {
        return (fahrenheit - 32) / 1.8;
    }

    static void print_value(data_type_t type, void *value, char *format) {
        switch (type) {
            case DATA_DATA :
            case DATA_ARRAY :
            case DATA_FORMAT :
            case DATA_COUNT : {
                // ugh? no assert
            } break;
            //case DATA_DATA : {
            //    printer_ctx->printer->print_data(printer_ctx, value, format, file);
            //} break;
            case DATA_INT : {
                sprintf(tmp, format ? format : "%d", *(int*)value);
                strcat(message,tmp);
            } break;
            case DATA_DOUBLE : {
                sprintf(tmp, format ? format : "%.3f", *(double*)value);
                strcat(message,tmp);
            } break;
            case DATA_STRING : {
                snprintf(tmp, 100,format ? format : "%s", (char*)value);
                strcat(message,tmp);
            } break;
        }
    }


    void data_acquired_handler(data_t *data)
    {
        bool separator = false;
        bool was_labeled = false;
        bool written_title = false;


        for (data_t *d = data; d; d = d->next) {
            if ((d->type == DATA_DOUBLE) &&
                !strcmp(d->key, "temperature_F")) {
                    *(double*)d->value = f2c(*(double*)d->value);
                    free(d->key);
                    d->key = strdup("temperature_C");
                    char *pos;
                    if (d->format &&
                        (pos = strrchr(d->format, 'F'))) {
                        *pos = 'C';
                    }
            }
        }

        while (data)
        {
            bool labeled = data->pretty_key[0];
            if (separator) {
                if (labeled && !was_labeled && !written_title) {
                    strcat(message, "\n");
                    written_title = true;
                    separator = false;
                } else {
                    if (was_labeled)
                        strcat(message, "\n");
                    else
                        strcat(message, " ");
                }
            }
            if (!strcmp(data->key, "model"))
                strcat(message, ":\t");
            else if (!strcmp(data->key, "time"))
            {
            }
            else
            {
                if (strlen(data->pretty_key)>12)
                    sprintf(tmp, "  %s:\t", data->pretty_key);
                else
                    sprintf(tmp, "  %s:\t\t", data->pretty_key);
                strcat(message,tmp);
            }
            if (labeled)
                    strcat(message," ");
            if (!strcmp(data->key, "time"))
            {
                struct tm *tm_info;
                time_t etime = time(NULL);
                tm_info = localtime(&etime);
                strftime(tmp, 32, "%Y-%m-%d %H:%M:%S", tm_info);
                strcat(message,tmp);
            }
            else
                print_value(data->type, data->value, data->format);
            separator = true;
            was_labeled = labeled;
            data = data->next;
        }
        strcat(message,"\n");
        data_free(data);
    }

}




static int verbose_level = 2;


extern "C" {
void CIsm433::newmsg(char *str)
{
    QString msg;
    msg.append(str);
    emit newMessage(msg);
}
}

CIsm433::CIsm433(QObject *parent) :
    QObject(parent)
{
    reset();
}

CIsm433::~CIsm433()
{
    free(demodst);
}


void CIsm433::incLevel()
{
    level = demodst->level_limit;
    if (level<1000) {
        level++;
        demodst->level_limit = (uint32_t)level;
    }
    if ((level<10) && (level!=0))
        gain = (float)((10-level)/3)+1.0;
}


void CIsm433::decLevel()
{
    level = demodst->level_limit;
    if (level>0) {
        level--;
        demodst->level_limit = (uint32_t)level;
    }
    if ((level<10) && (level!=0))
        gain = (float)((10-level)/3)+1.0;
}


int CIsm433::getLevel()
{
    level = demodst->level_limit;
    return level;
}


void CIsm433::change_level(int newlevel)
{
    level = demodst->level_limit;
    if ((newlevel>=0)&&(newlevel<=1000)) {
        level=newlevel;
        demodst->level_limit = (uint32_t)level;
    }
    if ((level<10) && (level!=0))
        gain = (float)((10-level)/3)+1.0;
}





/*! \brief Reset the decoder. */
void CIsm433::reset()
{
    counter = 0;
    print = 1;
    print2 = 0;
    pulses_found = 0;
    prev_pulse_start = 0;
    pulse_start = 0;
    pulse_end = 0;
    pulse_avg = 0;
    signal_start = 0;
    signal_end = 0;
    memset(signal_pulse_data,0,4000*3*sizeof(int));
    memset(message,0,65535*sizeof(int));
    signal_pulse_counter = 0;
    register_all = 1;

    override_short = 0;
    override_long = 0;
    debug_output = 0;
    quiet_mode = 0;
    bufsize = 0;
    //sbuf = (float*)malloc(sizeof(float)*12000*2);

    demodst = (dm_state*)malloc(sizeof (struct dm_state));
    memset(demodst, 0, sizeof (struct dm_state));
    baseband_init();

    r_device devices[] = {
#define DECL(name) name,
            DEVICES
#undef DECL
            };
    num_r_devices = sizeof(devices)/sizeof(*devices);
    samp_rate = (uint32_t)48000;
    level = 0;
    gain = 1.0;
    demodst->level_limit = (uint32_t)level;
    demodst->debug_mode = 1;

    for (int i = 0; i < num_r_devices; i++) {
        register_protocol(demodst, &devices[i]);
        if(devices[i].modulation >= FSK_DEMOD_MIN_VAL) {
              demodst->enable_FM_demod = 1;
        }
    }


    zerostate.modulation = 0;
    zerostate.short_limit = 0;
    zerostate.long_limit = 0;
    zerostate.reset_limit = 0;
    zerostate.demod_arg = 1;

}




void CIsm433::demod(float *buffer, int length)
{
    int package_type = 1;
    int p_events = 0;

    for (int i=0;i<length;i++)
    {
        if (buffer[i] < 0)
            buffer[i] = std::max(buffer[i]*gain, -1.0f);
        else if (buffer[i] > 0)
            buffer[i] = std::min(buffer[i]*gain, 1.0f);
        demodst->am_buf[i] = (short int)(buffer[i]*32768.0*0.999);
        demodst->fm_buf[i] = demodst->am_buf[i];
    }

    demodst->analyze_pulses = 1;


    while(package_type) {
        package_type = pulse_detect_package(demodst->am_buf, demodst->fm_buf, length, demodst->level_limit, samp_rate, &demodst->pulse_data, &demodst->fsk_pulse_data);
        if (package_type == 1) {
            for (int i = 0; i < demodst->r_dev_num; i++) {
                switch (demodst->r_devs[i]->modulation) {
                    case OOK_PULSE_PCM_RZ:
                        p_events += pulse_demod_pcm(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_PPM_RAW:
                        p_events += pulse_demod_ppm(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_PWM_PRECISE:
                        p_events += pulse_demod_pwm_precise(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_PWM_RAW:
                        p_events += pulse_demod_pwm(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_PWM_TERNARY:
                        p_events += pulse_demod_pwm_ternary(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_MANCHESTER_ZEROBIT:
                        p_events += pulse_demod_manchester_zerobit(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_CLOCK_BITS:
                        p_events += pulse_demod_clock_bits(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    case OOK_PULSE_PWM_OSV1:
                        p_events += pulse_demod_osv1(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    // FSK decoders
                    case FSK_PULSE_PCM:
                    case FSK_PULSE_PWM_RAW:
                        break;
                    case FSK_PULSE_MANCHESTER_ZEROBIT:
                        p_events += pulse_demod_manchester_zerobit(&demodst->pulse_data, demodst->r_devs[i]);
                        break;
                    default:
                       fprintf(stderr, "Unknown modulation %d in protocol!\n", demodst->r_devs[i]->modulation);
                }
            } // for demodulators
            if(demodst->analyze_pulses) {
                pulse_analyzer(&demodst->pulse_data, samp_rate);
            }
            printf("envelope decoded events=%d\n",p_events);
        } else if (package_type == 2) {
            //if(demodst->analyze_pulses) fprintf(stderr, "Detected FSK package\t@ %s\n", local_time_str(0, time_str));
            for (int i = 0; i < demodst->r_dev_num; i++) {
                switch (demodst->r_devs[i]->modulation) {
                    // OOK decoders
                    case OOK_PULSE_PCM_RZ:
                    case OOK_PULSE_PPM_RAW:
                    case OOK_PULSE_PWM_PRECISE:
                    case OOK_PULSE_PWM_RAW:
                    case OOK_PULSE_PWM_TERNARY:
                    case OOK_PULSE_MANCHESTER_ZEROBIT:
                    case OOK_PULSE_CLOCK_BITS:
                    case OOK_PULSE_PWM_OSV1:
                        break;
                    case FSK_PULSE_PCM:
                        p_events += pulse_demod_pcm(&demodst->fsk_pulse_data, demodst->r_devs[i]);
                        break;
                    case FSK_PULSE_PWM_RAW:
                        p_events += pulse_demod_pwm(&demodst->fsk_pulse_data, demodst->r_devs[i]);
                        break;
                    case FSK_PULSE_MANCHESTER_ZEROBIT:
                        p_events += pulse_demod_manchester_zerobit(&demodst->fsk_pulse_data, demodst->r_devs[i]);
                        break;
                    default:
                        fprintf(stderr, "Unknown modulation %d in protocol!\n", demodst->r_devs[i]->modulation);
                }

            } // for demodulators
            //if(debug_output > 1) pulse_data_print(&demodst->fsk_pulse_data);
            if(demodst->analyze_pulses) {
                pulse_analyzer(&demodst->fsk_pulse_data, samp_rate);
            }
            printf("FSK decoded events=%d\n",p_events);
        } // if (package_type == ...
        if (message[0]!=0)
        {
            newmsg(message);
            memset(message,0,65535);
        }

    } // while(package_type)...
/*
    memmove(sbuf,&sbuf[12000],bufsize-12000);
    bufsize -= 12000;
*/
}




void CIsm433::register_protocol(struct dm_state *demods, r_device *t_dev) {
    struct protocol_state *p = (protocol_state*)calloc(1, sizeof (struct protocol_state));
    p->short_limit = (float) t_dev->short_limit / ((float) 1000000 / (float) samp_rate);
    p->long_limit = (float) t_dev->long_limit / ((float) 1000000 / (float) samp_rate);
    p->reset_limit = (float) t_dev->reset_limit / ((float) 1000000 / (float) samp_rate);
    p->modulation = t_dev->modulation;
    p->callback = t_dev->json_callback;
    p->name = t_dev->name;
    p->demod_arg = t_dev->demod_arg;
    if (p->modulation == OOK_PULSE_PWM_PRECISE || p->modulation == OOK_PULSE_CLOCK_BITS) {
        PWM_Precise_Parameters *pwm_precise_parameters = (PWM_Precise_Parameters *)p->demod_arg;
        pwm_precise_parameters->pulse_tolerance = (float)pwm_precise_parameters->pulse_tolerance / ((float)1000000 / (float)samp_rate);
    }
    bitbuffer_clear(&p->bits);

    demods->r_devs[demodst->r_dev_num] = p;
    demods->r_dev_num++;

    if (!quiet_mode) {
        fprintf(stderr, "Registering protocol [%d] \"%s\"\n", demods->r_dev_num, t_dev->name);
    }

    if (demodst->r_dev_num > MAX_PROTOCOLS) {
        fprintf(stderr, "\n\nMax number of protocols reached %d\n", MAX_PROTOCOLS);
        fprintf(stderr, "Increase MAX_PROTOCOLS and recompile\n");
        exit(-1);
    }
}

void CIsm433::classify_signal()
{
    unsigned int i, k, max = 0, min = 1000000, t;
    unsigned int delta, count_min, count_max, min_new, max_new, p_limit;
    unsigned int a[3], b[2], a_cnt[3], a_new[3];
    unsigned int signal_distance_data[4000] = {0};
    struct protocol_state p = zerostate;
    unsigned int signal_type;

    if (!signal_pulse_data[0][0])
        return;

    for (i = 0; i < 1000; i++) {
        if (signal_pulse_data[i][0] > 0) {
            //fprintf(stderr, "[%03d] s: %d\t  e:\t %d\t l:%d\n",
            //i, signal_pulse_data[i][0], signal_pulse_data[i][1],
            //signal_pulse_data[i][2]);
            if (signal_pulse_data[i][2] > max)
                max = signal_pulse_data[i][2];
            if (signal_pulse_data[i][2] <= min)
                min = signal_pulse_data[i][2];
        }
    }
    t = (max + min) / 2;
    //fprintf(stderr, "\n\nMax: %d, Min: %d  t:%d\n", max, min, t);

    delta = (max - min)*(max - min);

    //TODO use Lloyd-Max quantizer instead
    k = 1;
    while ((k < 10) && (delta > 0)) {
        min_new = 0;
        count_min = 0;
        max_new = 0;
        count_max = 0;
       for (i = 0; i < 1000; i++) {
            if (signal_pulse_data[i][0] > 0) {
                if (signal_pulse_data[i][2] < t) {
                    min_new = min_new + signal_pulse_data[i][2];
                    count_min++;
                } else {
                    max_new = max_new + signal_pulse_data[i][2];
                    count_max++;
                }
            }
        }
        if (count_min != 0 && count_max != 0) {
            min_new = min_new / count_min;
            max_new = max_new / count_max;
        }

        delta = (min - min_new)*(min - min_new) + (max - max_new)*(max - max_new);
        min = min_new;
        max = max_new;
        t = (min + max) / 2;

        fprintf(stderr, "Iteration %d. t: %d    min: %d (%d)    max: %d (%d)    delta %d\n", k, t, min, count_min, max, count_max, delta);
        k++;
    }

    for (i = 0; i < 1000; i++) {
        if (signal_pulse_data[i][0] > 0) {
            //fprintf(stderr, "%d\n", signal_pulse_data[i][1]);
        }
    }
    /* 50% decision limit */
    if (min != 0 && max / min > 1) {
        fprintf(stderr, "Pulse coding: Short pulse length %d - Long pulse length %d\n", min, max);
        signal_type = 2;
    } else {
        fprintf(stderr, "Distance coding: Pulse length %d\n", (min + max) / 2);
        signal_type = 1;
    }
    p_limit = (max + min) / 2;
    /* Initial guesses */
    a[0] = 1000000;
    a[2] = 0;
    for (i = 1; i < 1000; i++) {
        if (signal_pulse_data[i][0] > 0) {
            //               fprintf(stderr, "[%03d] s: %d\t  e:\t %d\t l:%d\t  d:%d\n",
            //               i, signal_pulse_data[i][0], signal_pulse_data[i][1],
            //               signal_pulse_data[i][2], signal_pulse_data[i][0]-signal_pulse_data[i-1][1]);
            signal_distance_data[i - 1] = signal_pulse_data[i][0] - signal_pulse_data[i - 1][1];
            if (signal_distance_data[i - 1] > a[2])
                a[2] = signal_distance_data[i - 1];
            if (signal_distance_data[i - 1] <= a[0])
                a[0] = signal_distance_data[i - 1];
        }
    }
    min = a[0];
    max = a[2];
    a[1] = (a[0] + a[2]) / 2;
    //    for (i=0 ; i<1 ; i++) {
    //        b[i] = (a[i]+a[i+1])/2;
    //    }
    b[0] = (a[0] + a[1]) / 2;
    b[1] = (a[1] + a[2]) / 2;
    //     fprintf(stderr, "a[0]: %d\t a[1]: %d\t a[2]: %d\t\n",a[0],a[1],a[2]);
    //     fprintf(stderr, "b[0]: %d\t b[1]: %d\n",b[0],b[1]);

    k = 1;
    delta = 10000000;
    while ((k < 10) && (delta > 0)) {
        for (i = 0; i < 3; i++) {
            a_new[i] = 0;
            a_cnt[i] = 0;
        }
       for (i = 0; i < 1000; i++) {
            if (signal_distance_data[i] > 0) {
                if (signal_distance_data[i] < b[0]) {
                    a_new[0] += signal_distance_data[i];
                    a_cnt[0]++;
                } else if (signal_distance_data[i] < b[1] && signal_distance_data[i] >= b[0]) {
                    a_new[1] += signal_distance_data[i];
                    a_cnt[1]++;
                } else if (signal_distance_data[i] >= b[1]) {
                    a_new[2] += signal_distance_data[i];
                    a_cnt[2]++;
                }
            }
        }

        //         fprintf(stderr, "Iteration %d.", k);
        delta = 0;
        for (i = 0; i < 3; i++) {
            if (a_cnt[i])
                a_new[i] /= a_cnt[i];
            delta += (a[i] - a_new[i])*(a[i] - a_new[i]);
            //             fprintf(stderr, "\ta[%d]: %d (%d)", i, a_new[i], a[i]);
            a[i] = a_new[i];
        }
        //         fprintf(stderr, " delta %d\n", delta);

        if (a[0] < min) {
            a[0] = min;
            //             fprintf(stderr, "Fixing a[0] = %d\n", min);
        }
        if (a[2] > max) {
            a[0] = max;
            //             fprintf(stderr, "Fixing a[2] = %d\n", max);
        }
        //         if (a[1] == 0) {
        //             a[1] = (a[2]+a[0])/2;
        //             fprintf(stderr, "Fixing a[1] = %d\n", a[1]);
        //         }

        //         fprintf(stderr, "Iteration %d.", k);
       for (i = 0; i < 2; i++) {
            //             fprintf(stderr, "\tb[%d]: (%d) ", i, b[i]);
            b[i] = (a[i] + a[i + 1]) / 2;
            //             fprintf(stderr, "%d  ", b[i]);
        }
        //         fprintf(stderr, "\n");
        k++;
    }

    if (override_short) {
        p_limit = override_short;
        a[0] = override_short;
    }

    if (override_long) {
        a[1] = override_long;
    }

    fprintf(stderr, "\nShort distance: %d, long distance: %d, packet distance: %d\n", a[0], a[1], a[2]);
    fprintf(stderr, "\np_limit: %d\n", p_limit);

    bitbuffer_clear(&p.bits);
    if (signal_type == 1) {
        for (i = 0; i < 1000; i++) {
            if (signal_distance_data[i] > 0) {
                if (signal_distance_data[i] < (a[0] + a[1]) / 2) {
                    //                     fprintf(stderr, "0 [%d] %d < %d\n",i, signal_distance_data[i], (a[0]+a[1])/2);
                    bitbuffer_add_bit(&p.bits, 0);
                } else if ((signal_distance_data[i] > (a[0] + a[1]) / 2) && (signal_distance_data[i] < (a[1] + a[2]) / 2)) {
                    //                     fprintf(stderr, "0 [%d] %d > %d\n",i, signal_distance_data[i], (a[0]+a[1])/2);
                    bitbuffer_add_bit(&p.bits, 1);
                } else if (signal_distance_data[i] > (a[1] + a[2]) / 2) {
                    //                     fprintf(stderr, "0 [%d] %d > %d\n",i, signal_distance_data[i], (a[1]+a[2])/2);
                    bitbuffer_add_row(&p.bits);
                }

            }

        }
        bitbuffer_print(&p.bits);
    }
    if (signal_type == 2) {
        for (i = 0; i < 1000; i++) {
            if (signal_pulse_data[i][2] > 0) {
                if (signal_pulse_data[i][2] < p_limit) {
                    //                     fprintf(stderr, "0 [%d] %d < %d\n",i, signal_pulse_data[i][2], p_limit);
                    bitbuffer_add_bit(&p.bits, 0);
                } else {
                    //                     fprintf(stderr, "1 [%d] %d > %d\n",i, signal_pulse_data[i][2], p_limit);
                    bitbuffer_add_bit(&p.bits, 1);
                }
                if ((signal_distance_data[i] >= (a[1] + a[2]) / 2)) {
                    //                     fprintf(stderr, "\\n [%d] %d > %d\n",i, signal_distance_data[i], (a[1]+a[2])/2);
                    bitbuffer_add_row(&p.bits);
                }


            }
        }
        bitbuffer_print(&p.bits);
    }

    for (i = 0; i < 1000; i++) {
        signal_pulse_data[i][0] = 0;
        signal_pulse_data[i][1] = 0;
        signal_pulse_data[i][2] = 0;
        signal_distance_data[i] = 0;
    }
}


void CIsm433::pwm_analyze(struct dm_state *demodst, int16_t *buf, uint32_t len)
{
    unsigned int i;
    int32_t threshold = (demodst->level_limit ? demodst->level_limit : 8000);  // Does not support auto level. Use old default instead.

    for (i = 0; i < len; i++) {
        if (buf[i] > threshold) {
            if (!signal_start)
                signal_start = counter;
            if (print) {
                pulses_found++;
                pulse_start = counter;
                signal_pulse_data[signal_pulse_counter][0] = counter;
                signal_pulse_data[signal_pulse_counter][1] = -1;
                signal_pulse_data[signal_pulse_counter][2] = -1;
                if (debug_output) fprintf(stderr, "pulse_distance %d\n", counter - pulse_end);
                if (debug_output) fprintf(stderr, "pulse_start distance %d\n", pulse_start - prev_pulse_start);
                if (debug_output) fprintf(stderr, "pulse_start[%d] found at sample %d, value = %d\n", pulses_found, counter, buf[i]);
                prev_pulse_start = pulse_start;
                print = 0;
                print2 = 1;
            }
        }
        counter++;
        if (buf[i] < threshold) {
            if (print2) {
                pulse_avg += counter - pulse_start;
                if (debug_output) fprintf(stderr, "pulse_end  [%d] found at sample %d, pulse length = %d, pulse avg length = %d\n",
                        pulses_found, counter, counter - pulse_start, pulse_avg / pulses_found);
                pulse_end = counter;
                print2 = 0;
                signal_pulse_data[signal_pulse_counter][1] = counter;
                signal_pulse_data[signal_pulse_counter][2] = counter - pulse_start;
                signal_pulse_counter++;
                if (signal_pulse_counter >= 4000) {
                    signal_pulse_counter = 0;
                    goto err;
                }
            }
            print = 1;
            if (signal_start && (pulse_end + 50000 < counter)) {
                signal_end = counter - 40000;
                fprintf(stderr, "*** signal_start = %d, signal_end = %d\n", signal_start - 10000, signal_end);
                fprintf(stderr, "signal_len = %d,  pulses = %d\n", signal_end - (signal_start - 10000), pulses_found);
                pulses_found = 0;
                classify_signal();

                signal_pulse_counter = 0;
                if (demodst->sg_buf) {
                    int start_pos, signal_bszie, wlen, wrest = 0, sg_idx, idx;
                    char sgf_name[256] = {0};
                    FILE *sgfp;

            while (1) {
            sprintf(sgf_name, "gfile%03d.data", demodst->signal_grabber);
            demodst->signal_grabber++;
            if (access(sgf_name, F_OK) == -1 || overwrite_mode) {
                break;
            }
            }

                    signal_bszie = 2 * (signal_end - (signal_start - 10000));
                    signal_bszie = (131072 - (signal_bszie % 131072)) + signal_bszie;
                    sg_idx = demodst->sg_index - demodst->sg_len;
                    if (sg_idx < 0)
                        sg_idx = SIGNAL_GRABBER_BUFFER - demodst->sg_len;
                    idx = (i - 40000)*2;
                    start_pos = sg_idx + idx - signal_bszie;
                    fprintf(stderr, "signal_bszie = %d  -      sg_index = %d\n", signal_bszie, demodst->sg_index);
                    fprintf(stderr, "start_pos    = %d  -   buffer_size = %d\n", start_pos, SIGNAL_GRABBER_BUFFER);
                    if (signal_bszie > SIGNAL_GRABBER_BUFFER)
                        fprintf(stderr, "Signal bigger then buffer, signal = %d > buffer %d !!\n", signal_bszie, SIGNAL_GRABBER_BUFFER);

                    if (start_pos < 0) {
                        start_pos = SIGNAL_GRABBER_BUFFER + start_pos;
                        fprintf(stderr, "restart_pos = %d\n", start_pos);
                    }

                    fprintf(stderr, "*** Saving signal to file %s\n", sgf_name);
                    sgfp = fopen(sgf_name, "wb");
                    if (!sgfp) {
                        fprintf(stderr, "Failed to open %s\n", sgf_name);
                    }
                   wlen = signal_bszie;
                    if (start_pos + signal_bszie > SIGNAL_GRABBER_BUFFER) {
                        wlen = SIGNAL_GRABBER_BUFFER - start_pos;
                        wrest = signal_bszie - wlen;
                    }
                    fprintf(stderr, "*** Writing data from %d, len %d\n", start_pos, wlen);
                    fwrite(&demodst->sg_buf[start_pos], 1, wlen, sgfp);

                    if (wrest) {
                        fprintf(stderr, "*** Writing data from %d, len %d\n", 0, wrest);
                        fwrite(&demodst->sg_buf[0], 1, wrest, sgfp);
                    }

                    fclose(sgfp);
                }
                signal_start = 0;
            }
        }


    }
    return;

err:
    fprintf(stderr, "To many pulses detected, probably bad input data or input parameters\n");
    return;
}





void CIsm433::verbprintf(int verb_level, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    if (verb_level <= verbose_level) {
        vfprintf(stdout, fmt, args);
        fflush(stdout);
    }
    va_end(args);
}



