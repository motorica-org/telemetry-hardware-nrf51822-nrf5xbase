#include <stdbool.h>
#include <stdint.h>
#include "nordic_common.h"
#include "app_timer.h"
#include "softdevice_handler.h"
#include "nrf_drv_adc.h"
#include "app_util_platform.h"

#include "app_uart.h"
#include "app_error.h"

#include "simple_ble.h"
#include "simple_adv.h"

#define ADC_BUFFER_SIZE 1
static nrf_adc_value_t       adc_buffer[ADC_BUFFER_SIZE]; // Stores ADC samples
static nrf_drv_adc_channel_t m_channel_config = NRF_DRV_ADC_DEFAULT_CHANNEL(NRF_ADC_CONFIG_INPUT_7); // Configure ADC input

#define ADC_TIMER_PRESCALER              0  // Value of the RTC1 PRESCALER register.
#define ADC_TIMER_OP_QUEUE_SIZE          4  // Size of timer operation queues.
#define ADC_TIMER_RATE     APP_TIMER_TICKS(150, ADC_TIMER_PRESCALER) // Timer fires every %d

// Timer data structure
APP_TIMER_DEF(adc_timer);

// Intervals for advertising and connections
static simple_ble_config_t ble_config = {
    .platform_id       = 0x90,              // 4th octet in BLE address
    .device_id         = DEVICE_ID_DEFAULT,
    .adv_name          = "motorica.mech.протезик",       // used in advertisements if there is room
    .adv_interval      = MSEC_TO_UNITS(500, UNIT_0_625_MS),
    .min_conn_interval = MSEC_TO_UNITS(500, UNIT_1_25_MS),
    .max_conn_interval = MSEC_TO_UNITS(1000, UNIT_1_25_MS)
};

// service and characteristic handles
static simple_ble_service_t my_service = {
    // e35c8bac-a062-4e3f-856d-2cfa87f2f171
    .uuid128 = {{0x71, 0xf1, 0xf2, 0x87, 0xfa, 0x2c, 0x6d, 0x85, 0x3f, 0x4e, 0x62, 0xa0, 0x8b, 0xac, 0x5c, 0xe3}}
};
static simple_ble_char_t my_char = {.uuid16 = 0x8910};
static uint16_t my_value = 0;

// called automatically by simple_ble_init
void services_init (void) {
    simple_ble_add_service(&my_service);

    simple_ble_add_characteristic(1, 0, 1, 0, // read, write, notify, vlen
            1, (uint8_t*)&my_value,
            &my_service, &my_char);
}

bool flag;
static void adc_event_handler(nrf_drv_adc_evt_t const * p_event)
{
    if (p_event->type == NRF_DRV_ADC_EVT_DONE)
    {
        for (uint32_t i = 0; i < p_event->data.done.size; i++)
        {
            printf("adc: %d\n", p_event->data.done.p_buffer[i]);

            if (p_event->data.done.p_buffer[i] > 512) {
                    if (!flag) {
                            printf("NOTIF\n");
                            my_value = p_event->data.done.p_buffer[i];
                            simple_ble_notify_char(&my_char);
                    }
                    flag = true;
            }
            if (p_event->data.done.p_buffer[i] < 100) {
                    flag = false;
            }
            //NRF_LOG_PRINTF("Current sample value: %d\r\n", p_event->data.done.p_buffer[i]);
        }
    }
}

static void timer_handler(void* p_context) {
    APP_ERROR_CHECK(nrf_drv_adc_buffer_convert(adc_buffer, ADC_BUFFER_SIZE));
    for (uint32_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        // manually trigger ADC conversion
        nrf_drv_adc_sample();
    }
}

static void adc_config(void)
{
    nrf_drv_adc_config_t config = NRF_DRV_ADC_DEFAULT_CONFIG;

    APP_ERROR_CHECK(nrf_drv_adc_init(&config, adc_event_handler));

    nrf_drv_adc_channel_enable(&m_channel_config);
}

static void timer_init(void)
{
    APP_TIMER_INIT(ADC_TIMER_PRESCALER,
                   ADC_TIMER_OP_QUEUE_SIZE,
                   false);

    APP_ERROR_CHECK(app_timer_create(&adc_timer,
                                APP_TIMER_MODE_REPEATED,
                                timer_handler));
}

static void timer_start(void) {
    APP_ERROR_CHECK(app_timer_start(adc_timer, ADC_TIMER_RATE, NULL));
}

void uart_error_handle (app_uart_evt_t * p_event) {
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    } else if (p_event->evt_type == APP_UART_FIFO_ERROR) {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}

int main(void) {
    uint32_t err_code;
    const app_uart_comm_params_t comm_params = {
          1, // RX pin
          2, // TX pin
          0,
          0,
          APP_UART_FLOW_CONTROL_DISABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud115200
      };

    APP_UART_FIFO_INIT(&comm_params,
                         256, // RX buffer size
                         256, // TX buffer size
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);
    APP_ERROR_CHECK(err_code);

    printf("[INIT]: start\n");

    adc_config();

    // Setup BLE
    simple_ble_init(&ble_config);

    // Register advertisement
    simple_adv_only_name();

    timer_init();
    timer_start();

    printf("[INIT]: finish\n");

    // Enter main loop.
    while (1) {
        sd_app_evt_wait();
    }
}
