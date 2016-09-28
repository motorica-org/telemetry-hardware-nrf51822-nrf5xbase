/* ADC-test
 */

#include <stdbool.h>
#include <stdint.h>
#include "led.h"
#include "nordic_common.h"
#include "app_timer.h"
#include "softdevice_handler.h"
#include "nrf_drv_adc.h"
#include "app_util_platform.h"

#include "app_uart.h"
#include "app_error.h"


#define ADC_BUFFER_SIZE 10                                /**< Size of buffer for ADC samples.  */
static nrf_adc_value_t       adc_buffer[ADC_BUFFER_SIZE]; /**< ADC buffer. */
static nrf_drv_adc_channel_t m_channel_config = NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_7); /**< Channel instance. Default configuration used. */

#define LED 18

// Some constants about timers
#define ADC_TIMER_PRESCALER              0  // Value of the RTC1 PRESCALER register.
#define ADC_TIMER_OP_QUEUE_SIZE          4  // Size of timer operation queues.

// How long before the timer fires.
#define ADC_TIMER_RATE     APP_TIMER_TICKS(500, ADC_TIMER_PRESCALER) // Blink every 0.5 seconds

// Timer data structure
APP_TIMER_DEF(adc_timer);

/**
 * @brief ADC interrupt handler.
 */
static void adc_event_handler(nrf_drv_adc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_ADC_EVT_DONE)
    {
        uint32_t i;
        for (i = 0; i < p_event->data.done.size; i++)
        {
            printf("adc: %d\n", p_event->data.done.p_buffer[i]);
            if(p_event->data.done.p_buffer[i] > 512)
                led_on(LED);
            else
                led_off(LED);
            //NRF_LOG_PRINTF("Current sample value: %d\r\n", p_event->data.done.p_buffer[i]);
        }
    }
}

// Timer callback
static void timer_handler (void* p_context) {
    APP_ERROR_CHECK(nrf_drv_adc_buffer_convert(adc_buffer,ADC_BUFFER_SIZE));
    uint32_t i;
    for (i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        // manually trigger ADC conversion
        nrf_drv_adc_sample();
    }
}

/**
 * @brief ADC initialization.
 */
static void adc_config(void)
{
    ret_code_t ret_code;
    nrf_drv_adc_config_t config = NRF_DRV_ADC_DEFAULT_CONFIG;

    ret_code = nrf_drv_adc_init(&config, adc_event_handler);
    APP_ERROR_CHECK(ret_code);

    nrf_drv_adc_channel_enable(&m_channel_config);
}

// Setup timer
static void timer_init(void)
{
    uint32_t err_code;

    // Initialize timer module.
    APP_TIMER_INIT(ADC_TIMER_PRESCALER,
                   ADC_TIMER_OP_QUEUE_SIZE,
                   false);

    // Create a timer
    err_code = app_timer_create(&adc_timer,
                                APP_TIMER_MODE_REPEATED,
                                timer_handler);
    APP_ERROR_CHECK(err_code);
}

// Start the blink timer
static void timer_start(void) {
    uint32_t err_code;

    // Start application timers.
    err_code = app_timer_start(adc_timer, ADC_TIMER_RATE, NULL);
    APP_ERROR_CHECK(err_code);
}

void uart_error_handle (app_uart_evt_t * p_event) {
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    } else if (p_event->evt_type == APP_UART_FIFO_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

int main(void) {

    // Initialize.
    led_init(LED);
    led_off(LED);

    uint32_t err_code;
    const app_uart_comm_params_t comm_params = {
          1,
          2,
          0,
          0,
          APP_UART_FLOW_CONTROL_DISABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud115200
      };

    APP_UART_FIFO_INIT(&comm_params,
                         256,
                         256,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);
    APP_ERROR_CHECK(err_code);

    printf("Start: \n");

    adc_config();

    timer_init();
    timer_start();

    // Enter main loop.
    while (1) {
        sd_app_evt_wait();
    }
}
