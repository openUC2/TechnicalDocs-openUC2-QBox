// USB network (CDC-NCM) next to the USB serial port. See usb_net.h.
//
// Arduino core 3 ships TinyUSB with the NCM class compiled in and empty weak
// tud_network_* callbacks; this file adds the interface descriptor and the glue
// to lwIP. The glue follows esp_tinyusb's tinyusb_net.c and ESP-IDF's sta2eth
// example (examples/usb_network_s3 is the stand-alone version).
#include "usb_net.h"

#if HAS_USB_NET

#include "esp32-hal-tinyusb.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/semphr.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

static const char HOST_MAC[] = "020211223301";                         // host side of the link
static const uint8_t BOARD_MAC[6] = {0x02, 0x02, 0x11, 0x22, 0x33, 0x02}; // must differ from the host's

static esp_netif_t *s_netif;
static volatile unsigned long s_lastRx;   // millis() of the last packet from the host

// ---- USB descriptor --------------------------------------------------------

static uint16_t loadNcmDescriptor(uint8_t *dst, uint8_t *itf)
{
  uint8_t name = tinyusb_add_string_descriptor("ODMR USB network");
  uint8_t mac = tinyusb_add_string_descriptor(HOST_MAC);
  uint8_t epNotif = tinyusb_get_free_in_endpoint();
  uint8_t epIn = tinyusb_get_free_in_endpoint();
  uint8_t epOut = tinyusb_get_free_out_endpoint();
  if (!epNotif || !epIn || !epOut)
    return 0;
  uint8_t descriptor[TUD_CDC_NCM_DESC_LEN] = {
      TUD_CDC_NCM_DESCRIPTOR(*itf, name, mac, (uint8_t)(0x80 | epNotif), 64, epOut, (uint8_t)(0x80 | epIn), 64,
                             CFG_TUD_NET_MTU)};
  *itf += 2;
  memcpy(dst, descriptor, sizeof(descriptor));
  return sizeof(descriptor);
}

// The core starts USB before setup(), so the interface is registered here, at
// static initialisation, like the core's own USB classes do.
static struct RegisterNcm
{
  RegisterNcm() { tinyusb_enable_interface(USB_INTERFACE_CUSTOM, TUD_CDC_NCM_DESC_LEN, loadNcmDescriptor); }
} s_registerNcm;

// ---- Host -> board ---------------------------------------------------------

bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
  s_lastRx = millis();
  if (s_netif && size)
  {
    void *copy = malloc(size); // lwIP keeps the packet, USB reuses its buffer
    if (copy)
    {
      memcpy(copy, src, size);
      esp_netif_receive(s_netif, copy, size, NULL);
    }
  }
  tud_network_recv_renew();
  return true;
}

static void freeRxBuffer(void *handle, void *buffer)
{
  free(buffer);
}

// ---- Board -> host ---------------------------------------------------------
// lwIP sends from its own task, but TinyUSB must only be driven from the USB
// task: each packet is handed over and the sender waits for the outcome.

struct Packet
{
  const void *data;
  uint16_t len;
  bool sent;
};
static Packet s_packet; // one at a time: only lwIP's task sends
static SemaphoreHandle_t s_tried;

static void sendInUsbTask(void *arg)
{
  s_packet.sent = tud_network_can_xmit(s_packet.len);
  if (s_packet.sent)
    tud_network_xmit(&s_packet, s_packet.len); // copies via tud_network_xmit_cb
  xSemaphoreGive(s_tried);
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
  Packet *p = (Packet *)ref;
  memcpy(dst, p->data, p->len);
  return p->len;
}

static esp_err_t netifTransmit(void *handle, void *buffer, size_t len)
{
  if (!tud_mounted() || len > CFG_TUD_NET_MTU)
    return ESP_OK; // no host: drop, TCP retransmits
  s_packet = {buffer, (uint16_t)len, false};
  xSemaphoreTake(s_tried, 0); // forget a late answer to an earlier packet
  // The host may still be busy with earlier packets: retry briefly, then drop.
  for (int attempt = 0; attempt < 20 && !s_packet.sent; attempt++)
  {
    usbd_defer_func(sendInUsbTask, NULL, false);
    if (xSemaphoreTake(s_tried, pdMS_TO_TICKS(100)) != pdTRUE)
      break; // USB task not answering (host asleep): drop
    if (!s_packet.sent)
      vTaskDelay(1);
  }
  return ESP_OK;
}

// ---- Network ---------------------------------------------------------------

bool usbNetActive()
{
  return s_lastRx && millis() - s_lastRx < 5000;
}

static void onAddressAssigned(void *arg, esp_event_base_t base, int32_t id, void *data)
{
  ip_event_ap_staipassigned_t *e = (ip_event_ap_staipassigned_t *)data;
  if (e->esp_netif == s_netif)
    Serial.printf("USB network: host got " IPSTR "\n", IP2STR(&e->ip));
}

void usbNetBegin()
{
  s_tried = xSemaphoreCreateBinary();
  esp_netif_init();                 // both are no-ops if WiFi got there first
  esp_event_loop_create_default();

  static const esp_netif_ip_info_t ip = {
      .ip = {.addr = ESP_IP4TOADDR(192, 168, 7, 1)},
      .netmask = {.addr = ESP_IP4TOADDR(255, 255, 255, 0)},
      .gw = {.addr = ESP_IP4TOADDR(192, 168, 7, 1)},
  };
  esp_netif_inherent_config_t base = {};
  base.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
  base.ip_info = &ip;
  base.if_key = "usb_ncm";
  base.if_desc = "usb network";
  base.route_prio = 5; // below WiFi: the board never routes through the host

  esp_netif_driver_ifconfig_t driver = {};
  driver.handle = (void *)1; // USB is a singleton, but esp_netif needs a non-NULL handle
  driver.transmit = netifTransmit;
  driver.driver_free_rx_buffer = freeRxBuffer;

  esp_netif_config_t config = {};
  config.base = &base;
  config.driver = &driver;
  config.stack = ESP_NETIF_NETSTACK_DEFAULT_ETH; // to lwIP, USB is an Ethernet port

  s_netif = esp_netif_new(&config);
  if (!s_netif)
  {
    Serial.println("USB network: could not create the interface");
    return;
  }
  esp_netif_set_mac(s_netif, (uint8_t *)BOARD_MAC);
  // Offer no router, so the iPad keeps its own internet connection.
  uint8_t offerRouter = 0; // dhcps_offer_t
  esp_netif_dhcps_option(s_netif, ESP_NETIF_OP_SET, ESP_NETIF_ROUTER_SOLICITATION_ADDRESS, &offerRouter,
                         sizeof(offerRouter));
  esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, onAddressAssigned, NULL);
  esp_netif_action_start(s_netif, NULL, 0, NULL);
  Serial.println("USB network: http://192.168.7.1");
}

#endif // HAS_USB_NET
