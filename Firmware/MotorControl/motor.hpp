#ifndef __MOTOR_HPP
#define __MOTOR_HPP

#ifndef __ODRIVE_MAIN_HPP
#error "This file should not be included directly. Include odrive_main.hpp instead."
#endif

#include "drv8301.h"

typedef enum {
    MOTOR_TYPE_HIGH_CURRENT = 0,
    // MOTOR_TYPE_LOW_CURRENT = 1, //Not yet implemented
    MOTOR_TYPE_GIMBAL = 2
} Motor_type_t;

typedef struct {
    float phB;
    float phC;
} Iph_BC_t;

typedef struct {
    float p_gain; // [V/A]
    float i_gain; // [V/As]
    float v_current_control_integral_d; // [V]
    float v_current_control_integral_q; // [V]
    float Ibus; // DC bus current [A]
    // Voltage applied at end of cycle:
    float final_v_alpha; // [V]
    float final_v_beta; // [V]
    float Iq_setpoint;
    float Iq_measured;
    float max_allowed_current;
} Current_control_t;

// NOTE: for gimbal motors, all units of A are instead V.
// example: vel_gain is [V/(count/s)] instead of [A/(count/s)]
// example: current_lim and calibration_current will instead determine the maximum voltage applied to the motor.
typedef struct {
    int32_t pole_pairs = 7; // This value is correct for N5065 motors and Turnigy SK3 series.
    float calibration_current = 10.0f;    // [A]
    float resistance_calib_max_voltage = 1.0f; // [V] - You may need to increase this if this voltage isn't sufficient to drive calibration_current through the motor.
    float phase_inductance = 0.0f;        // to be set by measure_phase_inductance
    float phase_resistance = 0.0f;        // to be set by measure_phase_resistance
    int32_t direction = 1;                // 1 or -1
    Motor_type_t motor_type = MOTOR_TYPE_HIGH_CURRENT;

    // Read out max_allowed_current to see max supported value for current_lim.
    // You can change DRV8301_ShuntAmpGain to get a different range.
    // float current_lim = 75.0f; //[A]
    float current_lim = 10.0f;  //[A]
} MotorConfig_t;

#define TIMING_LOG_SIZE 16

class Motor {
public:
    enum Error_t {
        ERROR_NO_ERROR,
        ERROR_PHASE_RESISTANCE_OUT_OF_RANGE,
        ERROR_PHASE_INDUCTANCE_OUT_OF_RANGE,
        ERROR_ADC_FAILED,
        ERROR_DRV_FAULT,
        ERROR_NOT_IMPLEMENTED_MOTOR_TYPE,
    };

    Motor(const MotorHardwareConfig_t& hw_config,
         const GateDriverHardwareConfig_t& gate_driver_config,
         MotorConfig_t& config);

    void arm();
    void disarm();
    void setup() {
        DRV8301_setup();
    }
    void DRV8301_setup();
    bool check_DRV_fault();
    bool do_checks();
    uint16_t check_timing();
    float phase_current_from_adcval(uint32_t ADCValue);
    bool measure_phase_resistance(float test_current, float max_voltage);
    bool measure_phase_inductance(float voltage_low, float voltage_high);
    bool run_calibration();
    void enqueue_modulation_timings(float mod_alpha, float mod_beta);
    void enqueue_voltage_timings(float v_alpha, float v_beta);
    bool FOC_voltage(float v_d, float v_q, float phase);
    bool FOC_current(float Id_des, float Iq_des, float phase);
    bool update(float current_setpoint, float phase);

    const MotorHardwareConfig_t& hw_config;
    const GateDriverHardwareConfig_t gate_driver_config;
    MotorConfig_t& config;
    Axis* axis = nullptr; // set by Axis constructor

//private:

    DRV8301_Obj gate_driver; // initialized in constructor
    uint16_t next_timings[3] = {
        TIM_1_8_PERIOD_CLOCKS / 2,
        TIM_1_8_PERIOD_CLOCKS / 2,
        TIM_1_8_PERIOD_CLOCKS / 2
    };
    uint16_t last_cpu_time = 0;
    int timing_log_index = 0;
    uint16_t timing_log[TIMING_LOG_SIZE] = { 0 };

    // variables exposed on protocol
    Error_t error = ERROR_NO_ERROR;
    Iph_BC_t current_meas = {0.0f, 0.0f};
    Iph_BC_t DC_calib = {0.0f, 0.0f};
    const float shunt_conductance = 1.0f / SHUNT_RESISTANCE;  //[S]
    float phase_current_rev_gain = 0.0f; // Reverse gain for ADC to Amps (to be set by DRV8301_setup)
    Current_control_t current_control = {
        .p_gain = 0.0f,        // [V/A] should be auto set after resistance and inductance measurement
        .i_gain = 0.0f,        // [V/As] should be auto set after resistance and inductance measurement
        .v_current_control_integral_d = 0.0f,
        .v_current_control_integral_q = 0.0f,
        .Ibus = 0.0f,
        .final_v_alpha = 0.0f,
        .final_v_beta = 0.0f,
        .Iq_setpoint = 0.0f,
        .Iq_measured = 0.0f,
        .max_allowed_current = 0.0f,
    };
    DRV8301_FaultType_e drv_fault = DRV8301_FaultType_NoFault;
    DRV_SPI_8301_Vars_t gate_driver_regs; //Local view of DRV registers (initialized by DRV8301_setup)

    // Communication protocol definitions
    auto make_protocol_definitions() {
        return make_protocol_member_list(
            make_protocol_property("error", reinterpret_cast<int32_t*>(&this->error)),
            make_protocol_ro_property("current_meas.phB", &this->current_meas.phB),
            make_protocol_ro_property("current_meas.phC", &this->current_meas.phC),
            make_protocol_property("DC_calib.phB", &this->DC_calib.phB),
            make_protocol_property("DC_calib.phC", &this->DC_calib.phC),
            make_protocol_property("shunt_conductance", &this->shunt_conductance),
            make_protocol_property("phase_current_rev_gain", &this->phase_current_rev_gain),
            make_protocol_object("current_control",
                make_protocol_property("p_gain", &this->current_control.p_gain),
                make_protocol_property("i_gain", &this->current_control.i_gain),
                make_protocol_property("v_current_control_integral_d", &this->current_control.v_current_control_integral_d),
                make_protocol_property("v_current_control_integral_q", &this->current_control.v_current_control_integral_q),
                make_protocol_property("Ibus", &this->current_control.Ibus),
                make_protocol_property("final_v_alpha", &this->current_control.final_v_alpha),
                make_protocol_property("final_v_beta", &this->current_control.final_v_beta),
                make_protocol_property("Iq_setpoint", &this->current_control.Iq_setpoint),
                make_protocol_property("Iq_measured", &this->current_control.Iq_measured),
                make_protocol_property("max_allowed_current", &this->current_control.max_allowed_current)
            ),
            make_protocol_object("gate_driver",
                make_protocol_ro_property("drv_fault", reinterpret_cast<int32_t*>(&this->drv_fault)),
                make_protocol_ro_property("status_reg_1", &this->gate_driver_regs.Stat_Reg_1_Value),
                make_protocol_ro_property("status_reg_2", &this->gate_driver_regs.Stat_Reg_2_Value),
                make_protocol_ro_property("ctrl_reg_1", &this->gate_driver_regs.Ctrl_Reg_1_Value),
                make_protocol_ro_property("ctrl_reg_2", &this->gate_driver_regs.Ctrl_Reg_2_Value)
            ),
            make_protocol_object("config",
                make_protocol_property("pole_pairs", &this->config.pole_pairs),
                make_protocol_property("calibration_current", &this->config.calibration_current),
                make_protocol_property("resistance_calib_max_voltage", &this->config.resistance_calib_max_voltage),
                make_protocol_property("phase_inductance", &this->config.phase_inductance),
                make_protocol_property("phase_resistance", &this->config.phase_resistance),
                make_protocol_property("direction", &this->config.direction),
                make_protocol_property("motor_type", reinterpret_cast<int32_t*>(&this->config.motor_type)),
                make_protocol_property("current_lim", &this->config.current_lim)
            )
        );
    }
};

#endif // __MOTOR_HPP