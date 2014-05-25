#include "stdafx.h"
#pragma hdrstop
using namespace std;

#include <iostream>
#include <sstream>
#include <string>

#include "conf.hpp"
#include "pinout.hpp"
#include "misc.hpp"
#include "math.hpp"

#include "stepper.hpp"
#include "adc.hpp"
#include "dac_ramp.hpp"


#include "spi.hpp"



////////////
// 4-20 mA pressure transducer

static float gage_LRV_Pkpa = 0;
static float gage_URV_Pkpa = 5;
static uint16_t gage_LRV_adc = 0x3139;
static uint16_t gage_URV_adc = 0xf63e;
static const size_t gage_calib_n = adc_sample_rate * 5;

static float gage_from_ad7686(uint32_t ad7686) {
    return gage_LRV_Pkpa +
            (int32_t(ad7686) - gage_LRV_adc)*(gage_URV_Pkpa - gage_LRV_Pkpa)
                                            /(gage_URV_adc  - gage_LRV_adc);
}

template <typename T>
static void adc_dump_n(T* src, T* buf, size_t n) {
    os_evt_clr(1, os_tsk_self());
    T* end = buf + n;
    while (buf != end) {
        os_evt_wait_or(1, FOREVER);
        os_evt_clr(1, os_tsk_self());
        *buf++ = *src;
    }
}

//ADC value to/from relative force
static inline uint32_t ad7730_from_rel(float rel) {
    return (1+force_FS_out_rel*rel) * 0x8000;
}
static inline float rel_from_ad7730(uint32_t ad7730) {
    return (int32_t(ad7730) - 0x8000) / (force_FS_out_rel * float(0x8000));
}

//absolute force from ADC value
static inline float force_from_ad7730(uint32_t ad7730) {
    return rel_from_ad7730(ad7730) * force_FS_Fmn;
}


static void home() {
    printf("### moving probe to home...\r\n");
    stepper_home(Vmil_nominal, Vmil_nominal / 4);
    printf("    done\r\n");
}

static void touchdown() {
    printf("### touchdown... (thres=%04x, limit=%d)\r\n",
           ad7730_from_rel(force_touchdown_thres),
           stepper_limit_lo_Lpulse);

    stepper_run(-Vmil_nominal / 2);
    while (stepper_running) {
        os_evt_wait_or(1, FOREVER);
        os_evt_clr(1, os_tsk_self());
        if (ad7730_data <= ad7730_from_rel(force_touchdown_thres)) {
            break;
        }
        printf("%04x\r\n", ad7730_data);
    }
    if (stepper_running) {
        stepper_stop();
        printf("    done\r\n");
    } else {
        printf("    error: hit limit\r\n");
    }
}

static void calib() {
    uint32_t* buf = new uint32_t[gage_calib_n];
    uint32_t sum;

    printf("### set AO to  4mA and enter LRV (kPa)... "); fflush(stdout);
    scanf("%f", &gage_LRV_Pkpa);
    adc_dump_n(&ad7686_data, buf, gage_calib_n);
    sum = 0;
    for (int i = 0 ; i < gage_calib_n ; ++i) sum += buf[i];
    gage_LRV_adc = uint16_t(sum / gage_calib_n);
    printf("<<< LRV ADC value : 0x%04x\r\n\r\n", gage_LRV_adc);

    printf("### set AO to 20mA and enter URV (kPa)... "); fflush(stdout);
    scanf("%f", &gage_URV_Pkpa);
    adc_dump_n(&ad7686_data, buf, gage_calib_n);
    sum = 0;
    for (int i = 0 ; i < gage_calib_n ; ++i) sum += buf[i];
    gage_URV_adc = uint16_t(sum / gage_calib_n);
    printf("<<< URV ADC value : 0x%04x\r\n\r\n", gage_URV_adc);

    printf("### done\r\n\r\n");

    delete buf;
}

static void run() {
    /*
    printf("??? ramp time (s) : "); fflush(stdout);
    float ramp_Ts; cin >> ramp_Ts;
    printf("\r\n");

    printf("??? max pressure (kPa) : "); fflush(stdout);
    float max_Pkpa; cin >> max_Pkpa;
    printf("\r\n");
    */

    printf(
        "### test start\r\n\r\n"
        "ad7730_data, ad7686_data\r\n"
    );

    O_VALVE = 1;
    /*
    dac_ramp_start
        ( (reg_min_Pkpa/reg_FS_Pkpa)*4095
        , (max_Pkpa/reg_FS_Pkpa)*4095
        , ramp_Ts*1000
        );
    */

    while (1/*dac_ramp_running*/) {
        os_evt_wait_or(1, FOREVER);
        os_evt_clr(1, os_tsk_self());
        if (ad7730_data <= ad7730_from_rel(force_bailout_thres)) {
            break;
        }
        printf("%.4f,%.4f\r\n",
               -force_from_ad7730(ad7730_data), //compression as positive
               gage_from_ad7686(ad7686_data));
    }

    O_VALVE = 0; //shut off pressure first!
    /*
    dac_ramp_stop();
    *dac_ramp_data = 0;
    */

    printf(
        "\r\n"
        "### test end\r\n"
    );
}

static OS_TID main_tid;
static __task void main_task() {
    main_tid = os_tsk_self();
    printf(
        "\r\n\r\n"
        "### e-chuck-ctrl\t%s %s\r\n"
        "\r\n",
        __DATE__, __TIME__
    );


    printf("### initializing ADCs..."); fflush(stdout);
    adc_init();
    adc_start();
    printf("    done\r\n");


    printf("### CLI\r\n");

    string cmd = "\0";
    while (1) {
        //first run: print usage
        if (0) {
        } else if (cmd[0] == 'h') {
            home();
        } else if (cmd[0] == 't') {
            touchdown();
        } else if (cmd[0] == 'c') {
            calib();
        } else if (cmd[0] == 'a') {
            printf("### adc test\r\n");
            os_evt_clr(1, os_tsk_self());
            for (int i = 0 ; i < adc_sample_rate ; ++i) {
                os_evt_wait_or(1, FOREVER);
                os_evt_clr(1, os_tsk_self());
                printf("%04x,%04x\r\n", ad7730_data, ad7686_data);
            }
            printf("    done\r\n");
        } else if (cmd[0] == 'r') {
            run();
        } else {
            printf(
                "### commands:\r\n"
                "    h(ome), t(ouchdown), c(alib), a(dc), r(un)\r\n"
            );
        }
        cin >> cmd;
    }
}

void adc_sample_handler() {
    os_evt_set(1, main_tid);
}


int main(){
    os_sys_init_prio(main_task, 0x80);
}
