/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "hardware/uart.h"

// #include "pico/binary_info.h"
// #include "hardware/spi.h"
#include "max31856.h"

/// \tag::pico_main[]

#define UART_ID uart0
#define BAUD_RATE 115200

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define TEMP_ONBOARD_ADC_CHAN 4U


static inline float convert_adc(uint16_t raw){
    return ((float)raw * 3.3f / (1 << 12));
}

uint16_t read_adc(uint8_t channel){
    /*
    Read the Pico ADC

    Select ADC input 0 (GPIO26) //TODO
    * Select an ADC input. 0...3 are GPIOs 26...29 respectively.
    * Input 4 is the onboard temperature sensor.
    */
    adc_select_input(channel);

    // 12-bit conversion, assume max value == ADC_VREF == 3.3 V
    uint16_t result = adc_read();

    //Return 12bit adc value from channel
    return result;
}



int main() {
    stdio_init_all();
    /* UART setup */
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    /* ADC setup */
    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);

    // Init max31856 sensor
    max31856_init();

    /* LED setup */
#ifndef PICO_DEFAULT_LED_PIN
#warning blink requires a board with a regular LED
#else
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
#endif

    uint32_t counter = 0;
    // Read temp forever
    while(1){

        /*
        Debug Print on USB to Host PC
        */
        printf("%d, ",counter);

        uint16_t temp_onboard_raw = read_adc(TEMP_ONBOARD_ADC_CHAN);
        //>>> 27 - (0.619556 - 0.706)/0.001721 == ~77.2289 degrees (double check this - slightly off)
        float temp_onboard_f = 27 - (convert_adc(temp_onboard_raw) - 0.706)/0.001721 - 10;// Added an additional -10 offset?? ;
        //printf("Temp_onboard: Celsius= %f : Fahrenheit= %f\n", (temp_onboard_f-32)*5/9, temp_onboard_f);
        printf("%f, %f, ", (temp_onboard_f-32)*5/9, temp_onboard_f);

        // Read Max31856 in C and F (F is just converted C)
        float temp_thermocouple = readCelsius();
        //printf("Thermocouple: Celsius= %f : ", temp_thermocouple);
        printf("%f, ", temp_thermocouple);
        temp_thermocouple = readFahrenheit();
        printf("%f \n", temp_thermocouple);

        /*
        Send chars over usb otg/serial uart
        */
        uart_putc_raw(UART_ID, (char)((counter & 0x000000FF) >> 0)); // fourth byte
        uart_putc_raw(UART_ID, (char)((counter & 0x0000FF00) >> 8)); // third byte 
        uart_putc_raw(UART_ID, (char)((counter & 0x00FF0000) >> 16)); // second byte
        uart_putc_raw(UART_ID, (char)((counter & 0xFF000000) >> 24)); // first byte 
        uart_puts(UART_ID, "$");
        /*
        Get raw bytes from onboard temp and send to device
        */
        uart_putc_raw(UART_ID, (char)((temp_onboard_raw & 0xFF00) >> 8)); // first byte 
        uart_putc_raw(UART_ID, (char)((temp_onboard_raw & 0x00FF) >> 0)); // second byte
        uart_puts(UART_ID, "$");
        /*
        Get f32 Fahrenheit and send bytes to device TODO
        */
        int index = 0;
        char outbox[4];
        unsigned long d = *(unsigned long *)&temp_thermocouple;
        outbox[index] = d & 0x00FF; index++;
        outbox[index] = (d & 0xFF00) >> 8; index++;
        outbox[index] = (d & 0xFF0000) >> 16; index++;
        outbox[index] = (d & 0xFF000000) >> 24; index++;
        while( index < 0){
            uart_putc_raw(UART_ID, outbox[index]); // byte
            index--;
        }
        uart_puts(UART_ID, "\n");

        sleep_ms(100);

        /*
        Toggle LED
        */

#ifndef PICO_DEFAULT_LED_PIN
#warning blink requires a board with a regular LED
#else
        gpio_put(LED_PIN, 1);
        sleep_ms(750);
        gpio_put(LED_PIN, 0);
        sleep_ms(750);
#endif
        // Increment counter
        counter++;

    }

}

/// \end::pico_main[]