/*
 * ESP32-S3 with USB serial and a USB network adapter at the same time, and a
 * small web server: a way to reach the board from an iPhone or iPad without
 * WiFi or Bluetooth, while computers and Android keep their serial port and
 * firmware uploads need no button. See README.md.
 * The USB to lwIP glue follows Espressif's sta2eth example (usb_ncm_iface.c),
 * the way back to download mode follows arduino-esp32 (esp32-hal-tinyusb.c).
 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_private/periph_ctrl.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/periph_defs.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/usb_serial_jtag_reg.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_net.h"
#if CONFIG_TINYUSB_CDC_ENABLED
#include "tinyusb_cdc_acm.h"
#endif

#define LED_GPIO GPIO_NUM_21     // XIAO ESP32-S3 user LED, active low
#define USB_DM_GPIO GPIO_NUM_19
#define USB_DP_GPIO GPIO_NUM_20
#define RESCUE_WINDOW_S 10

static const char *TAG = "usbnet";
static esp_netif_t *s_netif;

// Link history for /status. Written by the USB and lwIP tasks, read by the web server.
static volatile uint32_t s_attached, s_detached, s_suspended, s_resumed, s_leases, s_tx_failed;

// ---- Staying flashable without the BOOT button -----------------------------

/**
 * After power-up the chip's built-in USB serial port owns the connector until
 * tinyusb_driver_install() takes it over, and uploads through that port need no
 * code of ours. So this runs first: however broken the code below is, a new
 * firmware can go in during these seconds.
 */
static void rescue_window(void)
{
    for (int i = 0; i < RESCUE_WINDOW_S * 10; i++) {
        gpio_set_level(LED_GPIO, i % 2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

#if CONFIG_TINYUSB_CDC_ENABLED

/** Hands the connector back to the built-in USB serial port and restarts into the ROM bootloader. */
static void enter_download_mode(void)
{
    periph_module_reset(PERIPH_USB_MODULE);
    periph_module_disable(PERIPH_USB_MODULE);
    CLEAR_PERI_REG_MASK(RTC_CNTL_USB_CONF_REG,   // these survive a restart, so a plain esp_restart() is not enough
                        RTC_CNTL_SW_HW_USB_PHY_SEL | RTC_CNTL_SW_USB_PHY_SEL | RTC_CNTL_USB_PAD_ENABLE);
    CLEAR_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_PHY_SEL | USB_SERIAL_JTAG_USB_PAD_ENABLE);

    // Both data lines low: the host sees this device leave and enumerates the next one.
    for (int i = 0; i < 2; i++) {
        gpio_num_t pin = i == 0 ? USB_DM_GPIO : USB_DP_GPIO;
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
        gpio_set_level(pin, 0);
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    SET_PERI_REG_MASK(USB_SERIAL_JTAG_CONF0_REG, USB_SERIAL_JTAG_USB_PAD_ENABLE);
    vTaskDelay(pdMS_TO_TICKS(100));

    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}

/** PlatformIO's upload: opening the port at 1200 baud. */
static void on_line_coding(int itf, cdcacm_event_t *event)
{
    if (event->line_coding_changed_data.p_line_coding->bit_rate == 1200) {
        enter_download_mode();
    }
}

/** esptool and the web flasher: this DTR/RTS sequence. Anything else on the lines is ignored. */
static void on_line_state(int itf, cdcacm_event_t *event)
{
    static const int sequence[] = {0b01, 0b11, 0b10, 0b00};   // DTR, RTS
    static int last = -1, step;
    int lines = (event->line_state_changed_data.dtr << 1) | event->line_state_changed_data.rts;
    if (lines == last) {
        return;   // esptool repeats states
    }
    last = lines;
    step = lines == sequence[step] ? step + 1 : (lines == sequence[0] ? 1 : 0);
    if (step == 4) {
        enter_download_mode();
    }
}

// ---- USB serial: the start of the ODMR text protocol ------------------------

static void reply(const char *text)
{
    tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (const uint8_t *)text, strlen(text));
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
}

static void on_serial_line(const char *line)
{
    if (strcasecmp(line, "PING") == 0) {
        reply("PONG\n");
    } else if (strcasecmp(line, "VERSION") == 0) {
        reply("VERSION {\"version\":\"usb-test\",\"build_date\":\"" __DATE__ "\",\"git_hash\":\"-\"}\n");
    } else if (line[0] != '\0') {
        reply("ERR unknown command\n");
    }
}

static void on_serial_rx(int itf, cdcacm_event_t *event)
{
    static char line[64];
    static size_t len;
    uint8_t rx[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
    size_t n = 0;
    if (tinyusb_cdcacm_read(itf, rx, sizeof(rx), &n) != ESP_OK) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        if (rx[i] == '\n' || rx[i] == '\r') {
            line[len] = '\0';
            on_serial_line(line);
            len = 0;
        } else if (len < sizeof(line) - 1) {
            line[len++] = rx[i];
        }
    }
}

static void start_serial(void)
{
    const tinyusb_config_cdcacm_t serial = {
        .cdc_port = TINYUSB_CDC_ACM_0,
        .callback_rx = on_serial_rx,
        .callback_line_state_changed = on_line_state,
        .callback_line_coding_changed = on_line_coding,
    };
    ESP_ERROR_CHECK(tinyusb_cdcacm_init(&serial));
}

#endif // CONFIG_TINYUSB_CDC_ENABLED

// ---- USB <-> lwIP ---------------------------------------------------------

static esp_err_t netif_transmit(void *handle, void *buffer, size_t len)
{
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100)) != ESP_OK) {
        s_tx_failed++;   // lwIP has no use for the error: TCP retransmits
    }
    return ESP_OK;
}

static void free_rx_buffer(void *handle, void *buffer)
{
    free(buffer);
}

static esp_err_t usb_rx(void *buffer, uint16_t len, void *ctx)
{
    if (s_netif == NULL) {
        return ESP_OK;
    }
    void *copy = malloc(len);   // lwIP keeps the packet; USB reuses its buffer
    if (copy == NULL) {
        return ESP_ERR_NO_MEM;
    }
    memcpy(copy, buffer, len);
    return esp_netif_receive(s_netif, copy, len, NULL);
}

static void on_usb_event(tinyusb_event_t *event, void *arg)
{
    switch (event->id) {
    case TINYUSB_EVENT_ATTACHED:  s_attached++;  break;
    case TINYUSB_EVENT_DETACHED:  s_detached++;  break;
    case TINYUSB_EVENT_SUSPENDED: s_suspended++; break;
    case TINYUSB_EVENT_RESUMED:   s_resumed++;   break;
    }
}

static void on_address_assigned(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    s_leases++;
    ESP_LOGI(TAG, "host has an address");
}

/** The board is 192.168.7.1 and hands the host an address by DHCP. */
static void start_network(void)
{
    static const esp_netif_ip_info_t ip = {
        .ip = {.addr = ESP_IP4TOADDR(192, 168, 7, 1)},
        .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
        .gw = {.addr = ESP_IP4TOADDR(192, 168, 7, 1)},
    };
    const esp_netif_inherent_config_t base = {
        .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
        .ip_info = &ip,
        .if_key = "usb",
        .if_desc = "usb network",
        .route_prio = 10,
    };
    const esp_netif_driver_ifconfig_t driver = {
        .handle = (void *)1,   // USB is a singleton, but esp_netif needs a non-NULL handle
        .transmit = netif_transmit,
        .driver_free_rx_buffer = free_rx_buffer,
    };
    const esp_netif_config_t config = {
        .base = &base,
        .driver = &driver,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,   // to lwIP, USB is an Ethernet port
    };
    s_netif = esp_netif_new(&config);
    ESP_ERROR_CHECK(s_netif == NULL ? ESP_FAIL : ESP_OK);

    // The board's own MAC; must differ from the host side's in tinyusb_net_init().
    const uint8_t mac[6] = {0x02, 0x02, 0x11, 0x22, 0x33, 0x02};
    ESP_ERROR_CHECK(esp_netif_set_mac(s_netif, (uint8_t *)mac));

    // Offer no router, so the phone keeps its normal internet connection.
    uint8_t offer_router = 0;   // dhcps_offer_t
    ESP_ERROR_CHECK(esp_netif_dhcps_option(s_netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS,
                                           &offer_router, sizeof(offer_router)));
    esp_netif_action_start(s_netif, NULL, 0, NULL);
}

// ---- Web server -----------------------------------------------------------

extern const char index_html_start[] asm("_binary_index_html_start");

static esp_err_t get_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, index_html_start);
}

static esp_err_t get_status(httpd_req_t *req)
{
    char json[160];
    snprintf(json, sizeof(json),
             "{\"uptime_s\":%" PRIu32 ",\"attached\":%" PRIu32 ",\"detached\":%" PRIu32 ",\"suspended\":%" PRIu32
             ",\"resumed\":%" PRIu32 ",\"leases\":%" PRIu32 ",\"tx_failed\":%" PRIu32 "}",
             (uint32_t)(esp_timer_get_time() / 1000000), s_attached, s_detached, s_suspended, s_resumed,
             s_leases, s_tx_failed);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

static void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;   // a sleeping phone leaves dead sockets behind
    httpd_handle_t server;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t index = {.uri = "/", .method = HTTP_GET, .handler = get_index};
    const httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = get_status};
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &index));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &status));
}

void app_main(void)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    rescue_window();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, on_address_assigned, NULL));

    const tinyusb_config_t usb = TINYUSB_DEFAULT_CONFIG(on_usb_event);
    ESP_ERROR_CHECK(tinyusb_driver_install(&usb));
#if CONFIG_TINYUSB_CDC_ENABLED
    start_serial();
#endif
    const tinyusb_net_config_t net = {
        .mac_addr = {0x02, 0x02, 0x11, 0x22, 0x33, 0x01},   // locally administered: the host side of the link
        .on_recv_callback = usb_rx,
    };
    ESP_ERROR_CHECK(tinyusb_net_init(&net));

    start_network();
    start_web_server();
    ESP_LOGI(TAG, "open http://192.168.7.1");

    // LED blinks until the host has an address, then stays on.
    for (bool on = false;; on = !on) {
        gpio_set_level(LED_GPIO, s_leases > 0 ? 0 : on);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}
