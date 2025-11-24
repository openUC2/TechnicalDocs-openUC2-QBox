#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_camera.h"

// -----------------------------
// Camera pins for XIAO ESP32S3
// -----------------------------
#define XIAO_PWDN -1
#define XIAO_RESET -1
#define XIAO_XCLK 10
#define XIAO_SIOD 40
#define XIAO_SIOC 39

#define XIAO_D0 48
#define XIAO_D1 11
#define XIAO_D2 12
#define XIAO_D3 14
#define XIAO_D4 16
#define XIAO_D5 18
#define XIAO_D6 17
#define XIAO_D7 15

#define XIAO_VSYNC 38
#define XIAO_HREF 47
#define XIAO_PCLK 13

// ===================================================================================
// Helper: Configure camera
// ===================================================================================
bool init_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = XIAO_D0;
    config.pin_d1 = XIAO_D1;
    config.pin_d2 = XIAO_D2;
    config.pin_d3 = XIAO_D3;
    config.pin_d4 = XIAO_D4;
    config.pin_d5 = XIAO_D5;
    config.pin_d6 = XIAO_D6;
    config.pin_d7 = XIAO_D7;
    config.pin_xclk = XIAO_XCLK;
    config.pin_pclk = XIAO_PCLK;
    config.pin_vsync = XIAO_VSYNC;
    config.pin_href = XIAO_HREF;
    config.pin_sccb_sda = XIAO_SIOD;
    config.pin_sccb_scl = XIAO_SIOC;
    config.pin_pwdn = XIAO_PWDN;
    config.pin_reset = XIAO_RESET;

    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_YUV422;     // better than grayscale
    config.frame_size = FRAMESIZE_QQVGA;
    config.fb_count = 2;

    if (esp_camera_init(&config) != ESP_OK)
        return false;

    sensor_t *s = esp_camera_sensor_get();

    // --- TRUE MANUAL MODE ---
    s->set_exposure_ctrl(s, 0);   // disable AEC
    s->set_aec2(s, 0);
    s->set_aec_value(s, 120);     // long enough exposure

    s->set_gain_ctrl(s, 0);       // disable AGC
    s->set_agc_gain(s, 4);        // stable, low-noise gain

    s->set_whitebal(s, 0);        // disable AWB
    s->set_awb_gain(s, 0);

    // optional cleanup
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_raw_gma(s, 0);

    return true;
}

// ===================================================================================
// Helper: compute statistics
// ===================================================================================
void compute_mean_and_projections(const camera_fb_t *fb,
                                  float &mean,
                                  std::vector<uint32_t> &projX,
                                  std::vector<uint32_t> &projY)
{
    int w = fb->width;
    int h = fb->height;
    const uint8_t *img = fb->buf;

    projX.assign(w, 0);
    projY.assign(h, 0);

    uint64_t sum = 0;

    int idx = 0;
    for (int y = 0; y < h; y++)
    {
        uint32_t rowSum = 0;
        for (int x = 0; x < w; x++)
        {
            uint8_t v = img[idx++];
            rowSum += v;
            projX[x] += v;
        }
        projY[y] = rowSum;
        sum += rowSum;
    }

    mean = float(sum) / float(w * h);
}

// ===================================================================================
// JSON command interpreter
// ===================================================================================
StaticJsonDocument<2048> json_in;
StaticJsonDocument<4096> json_out;

void process_command(JsonDocument &cmd)
{
    json_out.clear();

    const char *c = cmd["cmd"] | "";

    if (strcmp(c, "set") == 0)
    {
        // Camera parameter updates
        sensor_t *s = esp_camera_sensor_get();

        if (cmd.containsKey("exposure"))
            s->set_exposure_ctrl(s, 0), s->set_aec_value(s, cmd["exposure"]);

        if (cmd.containsKey("gain"))
            s->set_gain_ctrl(s, 0), s->set_agc_gain(s, cmd["gain"]);

        if (cmd.containsKey("wb"))
            s->set_whitebal(s, 1), s->set_awb_gain(s, cmd["wb"]);

        json_out["status"] = "ok";
        return;
    }

    if (strcmp(c, "get_frame") == 0)
    {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb)
        {
            json_out["error"] = "no_frame";
            return;
        }

        // compute statistics
        float mean;
        std::vector<uint32_t> projX, projY;
        compute_mean_and_projections(fb, mean, projX, projY);

        json_out["mean"] = mean;

        if (cmd["projections"] == true)
        {
            JsonArray arrX = json_out["projX"].to<JsonArray>();
            JsonArray arrY = json_out["projY"].to<JsonArray>();

            for (auto v : projX)
                arrX.add(v);
            for (auto v : projY)
                arrY.add(v);
        }

        esp_camera_fb_return(fb);
        return;
    }

    json_out["error"] = "unknown_cmd";
}

// ===================================================================================
// Serial input buffer
// ===================================================================================
String serial_buffer;

void handle_serial()
{
    while (Serial.available())
    {
        char c = Serial.read();
        if (c == '\n')
        {
            DeserializationError err = deserializeJson(json_in, serial_buffer);
            serial_buffer = "";

            if (!err)
            {
                process_command(json_in);
                serializeJson(json_out, Serial);
                Serial.println();
            }
            else
            {
                json_out.clear();
                json_out["error"] = "json_parse";
                serializeJson(json_out, Serial);
                Serial.println();
            }
        }
        else
        {
            serial_buffer += c;
        }
    }
}

// ===================================================================================
void setup()
{
    Serial.begin(115200);
    init_camera();
}

void loop()
{
    handle_serial();
    // compute and return mean always
    json_out.clear();

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb)
    {
        json_out["error"] = "no_frame";
        return;
    }

    // compute statistics
    float mean;
    std::vector<uint32_t> projX, projY;
    compute_mean_and_projections(fb, mean, projX, projY);

    json_out["mean"] = mean;
    esp_camera_fb_return(fb);
    serializeJson(json_out, Serial);
    Serial.println();
}
