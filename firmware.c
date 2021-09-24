/*
 * File:   firmware.c
 * Author: vh
 *
 * Created on May 6, 2021, 11:42 AM
 */

#include <xc.h>

// Use External Crystal @ 8 MHz as PRIMARY OSCILLATOR
#pragma config POSCMOD = XT, FNOSC = PRI, FPBDIV = DIV_1

// Use Internal OSC @ 8 MHz:
/*
#pragma config FNOSC = FRCPLL // For Internal Fast RC oscillator (8 MHz) w/ PLL
#pragma config FPLLIDIV = DIV_2     // Divide FRC before PLL (now 4 MHz)
#pragma config FPLLMUL = MUL_20     // PLL Multiply (now 80 MHz)
#pragma config FPLLODIV = DIV_2     // Divide After PLL (now 40 MHz)
*/
                                    // see figure 8.1 in datasheet for more info
#pragma config FWDTEN = OFF         // Watchdog Timer Disabled
#pragma config ICESEL = ICS_PGx1    // ICE/ICD Comm Channel Select
#pragma config JTAGEN = OFF         // Disable JTAG
#pragma config FSOSCEN = OFF        // Disable Secondary Oscillator

// Defines
#define _XTAL_FREQ  8000000
#define DELAY_BASE  80000

// Note that 612Hz (PR2=0xffff) is the lowest pwm frequency with our configuration
// To get lower, use a timer prescaler or use the 32-bit timer mode
#define PWM_FREQ    4000//16000   // 16 kHz -> 1 oscillazione completa = 0.0000625 [s] = 62.5 [us]
                            // Con 60 [us] di ampiezza impulso ho 3V
                            // Quindi: con Duty Cycle = 10%, ho ampiezza impulso = 6.25 [us], quindi ~ 0.3V) 

#define DUTY_CYCLE  50      // Con Duty Cycle = 10% -> 0.3V
                            //                  20% -> 0.6V
                            //                  40% -> 1.2V
                            //                  50% -> 1.5V
                            //                  100% -> 3V

// Configuration
uint8_t config = -1;

// Controllable Variables
int thr_duty_cycle_rising = 0;
int thr_duty_cycle_falling = 0;

int dly_multiplier_rising = 2;
int flag_dly_mult_rising = 0;

int dly_multiplier_falling = 2;
int flag_dly_mult_falling = 0;

// Internal Variables
int current_delay_us = DELAY_BASE;
int duty_cycle = 0;
int delta_duty_cycle = 1;
uint8_t current_led = 0;

void delay_us(unsigned int us)
{
    // Convert microseconds us into how many clock ticks it will take
    us *= _XTAL_FREQ / 1000000 / 2; // Core Timer updates every 2 ticks

    _CP0_SET_COUNT(0); // Set Core Timer count to 0

    while (us > _CP0_GET_COUNT()); // Wait until Core Timer count reaches the number we calculated earlier
}

void select_LED(uint8_t index)
{
    switch (index)
    {
        case 0:
            PORTBbits.RB10 = 0;
            PORTAbits.RA4 = 0;
            PORTBbits.RB6 = 0;
            break;
        case 1:
            PORTBbits.RB10 = 1;
            PORTAbits.RA4 = 0;
            PORTBbits.RB6 = 0;
            break;
        case 2:
            PORTBbits.RB10 = 0;
            PORTAbits.RA4 = 1;
            PORTBbits.RB6 = 0;
            break;
        default:
            break;
    }
}

/* Configure using the CONFIG 4-POS DIP switch
 * Choose one of the following 16 operating modes:
 */
void configure()
{
    uint8_t hw_config = (PORTBbits.RB8 << 2) | (PORTBbits.RB9 << 1) | (PORTBbits.RB11 << 0);
    
    // If the configuration changed according to the hw CONFIG switch, reconfigure and reset
    if (config != hw_config) {
        config = hw_config;
        
        switch (config)
        {
            case 0:
                thr_duty_cycle_rising = 100;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 1;
                dly_multiplier_falling = 1;                
                break;
            case 1:                
                thr_duty_cycle_rising = 100;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 2;
                dly_multiplier_falling = 2;
                break;
            case 2:                
                thr_duty_cycle_rising = 45;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 1;
                dly_multiplier_falling = 1;
                break;
            case 3:                
                thr_duty_cycle_rising = 50;
                thr_duty_cycle_falling = 45;
                dly_multiplier_rising = 1;
                dly_multiplier_falling = 2;
                break;
            case 4:                
                thr_duty_cycle_rising = 100;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 3;
                dly_multiplier_falling = 4;
                break;
            case 5:                
                thr_duty_cycle_rising = 50;
                thr_duty_cycle_falling = 45;
                dly_multiplier_rising = 2;
                dly_multiplier_falling = 3;
                break;
            case 6:                
                thr_duty_cycle_rising = 70;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 3;
                dly_multiplier_falling = 1;
                break;                
            case 7:                
                thr_duty_cycle_rising = 70;
                thr_duty_cycle_falling = 30;
                dly_multiplier_rising = 2;
                dly_multiplier_falling = 3;
                break; 
            default:
                break;
        }
        
        duty_cycle = thr_duty_cycle_falling + 1;
        select_LED(0);
        current_led = 1;
    }
}

void main(void) {
    // Set OC1 to pin RB7 with peripheral pin select
    RPB7Rbits.RPB7R = 0x0005;
 
    // Configure standard PWM mode for output compare module 1
    OC1CON = 0x0006; 
 
    // A write to PRy configures the PWM frequency
    // PR = [FPB / (PWM Frequency * TMR Prescale Value)] ? 1
    // : note the TMR Prescaler is 1 and is thus ignored
    PR2 = (_XTAL_FREQ / PWM_FREQ) - 1;
 
    // A write to OCxRS configures the duty cycle
    // : OCxRS / PRy = duty cycle
    //OC1RS = (PR2 + 1) * ((float)DUTY_CYCLE / 100);
 
    T2CONSET = 0x8000;      // Enable Timer2, prescaler 1:1
    OC1CONSET = 0x8000;     // Enable Output Compare Module 1
 
    // DEMUX Addresses:
    //  RB10    RA4    RB6
    //  A0      A1      A2          SW
    //  0       0       0       =   1
    //  1       0       0       =   2
    //  0       1       0       =   3
    TRISBbits.TRISB10 = 0;  // A0 -> outputted on RB10
    TRISAbits.TRISA4 = 0;   // A1 -> outputted on RA4
    TRISBbits.TRISB6 = 0;   // A2 -> outputted on RB6
    
    // Config Switch:
    // CFG1 = RB8 (pin 17)
    // CFG2 = RB9 (pin 18)
    // CFG3 = RB11 (pin 22)
    TRISBbits.TRISB8 = 1;
    TRISBbits.TRISB9 = 1;
    TRISBbits.TRISB11 = 1;

    // *** Variables Initialization ***
    duty_cycle = thr_duty_cycle_falling + 1;
    select_LED(0);
    current_led = 1;
    
    while(1) {
        configure();
        
        // LED intensity reached maximum or minimum
        if (duty_cycle <= thr_duty_cycle_falling || duty_cycle >= thr_duty_cycle_rising) {
            delta_duty_cycle = -delta_duty_cycle;
            
            // If delta changed, and now is raising again, change LED
            if (delta_duty_cycle > 0) {
                
                // Reset delay to normal speed
                current_delay_us = DELAY_BASE;
                flag_dly_mult_rising = 0;
                flag_dly_mult_falling = 0;
                
                // Switch to next LED
                select_LED(current_led);
                current_led = (current_led + 1) % 3;
            }
        }
        
        // LED intensity in transition
        else
        {
            // If the LED's intensity is in transition, regulate current delay:
            // Falling -> multiply by Falling Multiplier
            // Rising -> multiply by Rising Multiplier

            // Rising Transition: when delta > 0
            if (delta_duty_cycle > 0 && !flag_dly_mult_rising)
            {
                flag_dly_mult_rising = 1;
                current_delay_us *= dly_multiplier_rising;
            }
            else if (delta_duty_cycle < 0 && !flag_dly_mult_falling)
            {
                flag_dly_mult_falling = 1;
                current_delay_us *= dly_multiplier_falling;
            }
        }
        
        // Regulate PWM signal
        duty_cycle += delta_duty_cycle;
        OC1RS = (PR2 + 1) * ((float)duty_cycle / 100);
        
        delay_us(current_delay_us);
    }
}
