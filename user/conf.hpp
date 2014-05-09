#ifndef _CONF_HPP_
#define _CONF_HPP_

#include "misc.hpp"
#include "units.hpp"


////////////
// debug flags


////////////
// buffer size


////////////
//stepper driver

static const uint32_t pulse_width_min_Tclk = 400;
static const uint32_t period_min_Tclk = 800; //min period (in ticks) <=> max pulse frequency
static const uint32_t period_max_Tclk = 1<<24; //max period (in ticks)

static const int Lpulse_Lstep = 1<<6; //stepper driver microsteps
static const int Lstep_Lrev = 200;
static const int Lmil_Lrev = 12;
static const float Lpulse_Lmil = 1. * Lpulse_Lstep * Lstep_Lrev / Lmil_Lrev;

static const int stepper_speed_max_Vmil = 60;

static const int stepper_travel_Lmil = 400;
static const uint32_t stepper_travel_Lpulse = stepper_travel_Lmil * Lpulse_Lmil;

//home switch marks: -1 => lo, +1 => hi
static const int stepper_home_dir = +1;


////////////
//ADCs

static const int adc_sample_rate = 400;
static const int ad7730_clk = 4915200;


////////////
//force sensor (Futek LSB200)

static const float force_FS_analog = 0.6781; // (mV/V)
static const float force_FS_digital = force_FS_analog * 5 / 10; // (mV/V) * (V) / (mV)
static const float force_FS_Lmil = 4; //deflection at FS

#endif//_CONF_HPP_
