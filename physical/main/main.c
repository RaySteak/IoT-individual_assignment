/*
 * echo serial
 *
 *  Created on: Dec 13, 2012
 *      	harter
 */

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>

#define MEASURE_RTT 1

#define DEBUG_PRINT_WINDOW 0

#define NUM_FFT_SAMPLES 256
#define INITIAL_SAMPLE_FREQ 1000
#define THRESHOLD_MULTIPLIER 0.1f

#define MAX_MQTT_SIZE 100
#define MQTT_QOS 2
#define RTT_AVERAGE_NUM 100
#define WINDOW_SIZE 5

// Use unions to be able to store imaginary numbers in both rectangular and polar coordinates
typedef struct
{
    union
    {
        float Re;
        float Mag;
    };
    union
    {
        float Im;
        float Ang;
    };
} complex;

const char *APP_TAG = "Individual assignment";
const char *WIFI_TAG = "WiFi";
const char *MQTT_TAG = "MQTT";

const char *ssid = "OnePlus Nord"; // WiFi credentials here
const char *pass = "ayylmao123";
SemaphoreHandle_t xSemaphoreWifiConnected = NULL;

esp_mqtt_client_handle_t client;
int mqtt_connected = 0;
char *mqtt_topic = "aggregate";
const char *mqtt_uri = "mqtts://192.168.58.26";
const int mqtt_port = 1883;
const char *root_ca_cert = "-----BEGIN CERTIFICATE-----\n"
                           "MIIEFTCCAv2gAwIBAgIUKH9dBq33UzvB80dnEQT5TZPBmLgwDQYJKoZIhvcNAQEL\n"
                           "BQAwgZkxCzAJBgNVBAYTAklUMQ4wDAYDVQQIDAVMYXppbzENMAsGA1UEBwwEUm9t\n"
                           "ZTERMA8GA1UECgwIU2FwaWVuemExDDAKBgNVBAsMA0lvVDEeMBwGA1UEAwwVaW5k\n"
                           "aXZpZHVhbF9hc3NpZ25tZW50MSowKAYJKoZIhvcNAQkBFhthZHJpYW5naGVvcmdo\n"
                           "aXUwMEBnbWFpbC5jb20wHhcNMjQwNTA5MTgwMzQ5WhcNMzQwNTA3MTgwMzQ5WjCB\n"
                           "mTELMAkGA1UEBhMCSVQxDjAMBgNVBAgMBUxhemlvMQ0wCwYDVQQHDARSb21lMREw\n"
                           "DwYDVQQKDAhTYXBpZW56YTEMMAoGA1UECwwDSW9UMR4wHAYDVQQDDBVpbmRpdmlk\n"
                           "dWFsX2Fzc2lnbm1lbnQxKjAoBgkqhkiG9w0BCQEWG2FkcmlhbmdoZW9yZ2hpdTAw\n"
                           "QGdtYWlsLmNvbTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKllRhiB\n"
                           "s4e3/UglkTK1qAEDYw+HeL6jVtvkY3hvRsQevTwaDD26dlwdAYHcctWh16fOcZCG\n"
                           "KWY67qYz0HwJn8d7DdjAIcmdV0OJcme4wBIpHY81/MHQpRNltS3fuAiE+AHqjmbM\n"
                           "uqFw678e3bA7TOQxF/nEkzMTk/y+Fnd60JuaaVc74w/cwIstpq0SWQC9MFjC5K7Z\n"
                           "vb21E+7D/JfjCE/X7uo12BNf/XF7pA3Oq2ifCxOF6CdLbGaEB8fAY4i8pbFI+DDm\n"
                           "oOfzz/tiChwLRS0YJ3xxLQhBCHMhbkrdV4JO3Hz9u711pfdZK/PWiVi7YB4FEK7l\n"
                           "/SrQj8q/Vy5i5sECAwEAAaNTMFEwHQYDVR0OBBYEFJhoFaxHXi5X3qKiD4Ksm20n\n"
                           "2HZ0MB8GA1UdIwQYMBaAFJhoFaxHXi5X3qKiD4Ksm20n2HZ0MA8GA1UdEwEB/wQF\n"
                           "MAMBAf8wDQYJKoZIhvcNAQELBQADggEBACtZWF8+SANUKcoq+xl852sgd72WWXdM\n"
                           "jKhUMrRJ5nJWyA03cMHPcypvRHByBoFgZhXFq+b2l0MCkGYtak2iHIgYT/v92veU\n"
                           "SoK+uiexohOzwo6wEvP64OdVeLi5erv4uIB5BEbNDHFLiNPvtEaKaRR2lJBt9QVS\n"
                           "lR3kkPEQvVSMwa4CSZ/144QfABwp5l4+KFWQVezqZm3vMLEskt4YyyhvjLxqYTjl\n"
                           "0GyzZJSnyRwrlbFFQH7G7D4iF1a4ObnxMc+zsz1aWZKVHQ7x5jAr7hKpXtgJuRHw\n"
                           "ImLk/X42HykSN3TTBxPijrcAQxUjspzjfiL8CjWqjDEva1e8y085ZhA=\n"
                           "-----END CERTIFICATE-----\n";
const char *cert = "-----BEGIN CERTIFICATE-----\n"
                   "MIIDwjCCAqoCFHWz5Im7cuGc5yXVurV4ruXGUXOTMA0GCSqGSIb3DQEBCwUAMIGZ\n"
                   "MQswCQYDVQQGEwJJVDEOMAwGA1UECAwFTGF6aW8xDTALBgNVBAcMBFJvbWUxETAP\n"
                   "BgNVBAoMCFNhcGllbnphMQwwCgYDVQQLDANJb1QxHjAcBgNVBAMMFWluZGl2aWR1\n"
                   "YWxfYXNzaWdubWVudDEqMCgGCSqGSIb3DQEJARYbYWRyaWFuZ2hlb3JnaGl1MDBA\n"
                   "Z21haWwuY29tMB4XDTI0MDUwOTE4MTA0MloXDTM0MDUwNzE4MTA0MlowgaAxCzAJ\n"
                   "BgNVBAYTAklUMQ4wDAYDVQQIDAVMYXppbzENMAsGA1UEBwwEUm9tZTERMA8GA1UE\n"
                   "CgwIU2FwaWVuemExDDAKBgNVBAsMA0lvVDElMCMGA1UEAwwcaW5kaXZpZHVhbF9h\n"
                   "c3NpZ25tZW50X2NsaWVudDEqMCgGCSqGSIb3DQEJARYbYWRyaWFuZ2hlb3JnaGl1\n"
                   "MDBAZ21haWwuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAoLi9\n"
                   "W/MYsqpOPA4BEmqR66LvX3QSYRoVKS1sbi6xtxSydUSwmFpAUGxC7ssNsnFjatY1\n"
                   "dZzaDb3mFFQyl5er23mjMVP0KDuxnhIsTURirInLf2l/UW4yKhDj6fCWeoAKDNh4\n"
                   "gqWH7ZNcdR1dWvgmkF/M6VIZhltUJ15bXN81dg6EUkSrRCYqYGrTkeLmQDrXPF49\n"
                   "8LR8S2Td4yXAPts9voes6evCX3bzpdsvEv+RNgzF8MAx6bVdUhM1XmWfp7XRp/vI\n"
                   "ZfV18/vY4LZ7phsyc/FvV0ZZ3T9T9+e+FhSFJIs7HZ5nzn6xlZfr7cxiWzIKh4Sb\n"
                   "9jiPEwO2Cr7yEl+X0QIDAQABMA0GCSqGSIb3DQEBCwUAA4IBAQCghph8s4UwkHii\n"
                   "OyJbUv3ifk7FTGSYka1vgPNDv5KSwNUAuNXdMynBOJNAp+TLwKlgQr+ugM8jk/Lq\n"
                   "UN+ts2G2ocoe+UVlm0V1ZAsXX18/mcdNMr9G1vZedb9e93suKK4npXqxTDnylJWT\n"
                   "AuaWdsaMexIhD8Kf6oelaAEJMeSP204xKvd5QBXFq8wsvfJqo/OtLSBQUAPWaE30\n"
                   "Gf9oHG5vU95yuwSDyjoa8QRN85VRDaq0l5maFSl4IPnAEXaAnH2b3V+iMnHwVer9\n"
                   "7bqI0e8xVlRrTnL1ZruXq7Jm3tigE29yaDMD+l8pzyIG12/YuUoDAnEZddVscKgB\n"
                   "+V1KwnIW\n"
                   "-----END CERTIFICATE-----\n";
const char *privkey = "-----BEGIN PRIVATE KEY-----\n"
                      "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCguL1b8xiyqk48\n"
                      "DgESapHrou9fdBJhGhUpLWxuLrG3FLJ1RLCYWkBQbELuyw2ycWNq1jV1nNoNveYU\n"
                      "VDKXl6vbeaMxU/QoO7GeEixNRGKsict/aX9RbjIqEOPp8JZ6gAoM2HiCpYftk1x1\n"
                      "HV1a+CaQX8zpUhmGW1QnXltc3zV2DoRSRKtEJipgatOR4uZAOtc8Xj3wtHxLZN3j\n"
                      "JcA+2z2+h6zp68JfdvOl2y8S/5E2DMXwwDHptV1SEzVeZZ+ntdGn+8hl9XXz+9jg\n"
                      "tnumGzJz8W9XRlndP1P3574WFIUkizsdnmfOfrGVl+vtzGJbMgqHhJv2OI8TA7YK\n"
                      "vvISX5fRAgMBAAECggEAGuN1WKAFuoRYBN+R6dcW9Q1kDzvfxEuFDUfKa2+X70mA\n"
                      "5rIYelClEF9gXkttzfP+3jWaqYPVjjV+O9nvQeHP0G7b8Ml7IE0GVOv06fNeL7/4\n"
                      "4ebQNFsjfNqpCq8jubhYlhgUJ0VSxZog/n3sa6b26rIAWeuQliQK4vA8CEBWlRqq\n"
                      "HtfpqlEVOT2Kl51E8+BqQBOf0bLs/PQy+0B1KlvKIoRCaeFfsu2zXHahjAV2hvBy\n"
                      "E5DtzoPXPNtJZkzeGYsH61jJ4Mv89kU2At41KMC/ETsLxWyMD1Gm1htKYRn6Ryen\n"
                      "p425WQVEQL+zMbAW6GKdaVeeXWjxmRhVHdDrKlPA/QKBgQC91rh2bA/1aB93JmFb\n"
                      "vvLLsKwhsdNrBLtsBonLK/hg4jSMhjqi+0GwDSIipnXu4IkKPdqYrUQvCR8B7/k1\n"
                      "VFfnxXrhOKSsTb9GdSlkMtwYegz2tGMgGjFFy1omWCxOzqw3YNynfy2RqfFCpJ6L\n"
                      "ORGk1iCD1j2vT2/FGEBi9NyKbQKBgQDYvDX8Usuf6p+YVC8Z8xpoQ2HXGlO46QGZ\n"
                      "VW8Q0nHv9V/Y6QR1MkSJ+KaaVs+/tXjgL8PeX2W6bi7FT7m7ErSmE9KWtzivlhZG\n"
                      "DoB3F8TSungavZRNR1ZcxWbdEnwj4w5RR27TOOrFCSK1OzXA2P+IcUflXUfHQ7j3\n"
                      "l6BovHEkdQKBgB9EQDlw7ufycExFi/96Ya7euFsMWM/lhaDzKrrF0TDT0OfcK8gw\n"
                      "Lc97OkYOuJnRbYC8U0aWMwa0L+E3zwnTjG7l/akelTCz1W8bWOfh4JI/d+ciKrlc\n"
                      "1wSxy3VJTHLmY0LztyP3NGArZ0scpeg0TA2kHtLX9GztXnFN3zztK225AoGBAINc\n"
                      "J3hEEkhdYtdnhpi6wDGbTWya901mY+K0ZjmS4x2l4a7NJDeH/QSGoSuAHDA8ZAO0\n"
                      "z4ky1qxU5aIPPSnGH5ldAFD6wa+iTuOtHG2SCWgTPoIujvj7mLtnpX2uG3GAx4e5\n"
                      "vNDmeVxsug2P0neMzeu7hPDeRYffYvclfkKcnBKVAoGBAJE6xS6y46Esvo39Jrfu\n"
                      "oh7Ycy0iTkN7DgY8s5bgXrxpLBvzchpo1Y/pJ9Z0atmV7XZOpvz0+s4IWgJlZGrV\n"
                      "0FzOYzKKrj5QIWE7iBAClrOybCndRvQkNbo4Abe3r+sx68HzWmLwkm6/d69lB8Xu\n"
                      "X8+N0ICVA6eqWqb4ggyd8UZY\n"
                      "-----END PRIVATE KEY-----\n";
const char *client_id = "";

#if MEASURE_RTT
SemaphoreHandle_t xSemaphoreMQTTReceived = NULL;
#endif

char json_send[MAX_MQTT_SIZE];
char json_recv[MAX_MQTT_SIZE];

static void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retry_num = 0;

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(WIFI_TAG, "CONNECTING....\n");
        break;
    case WIFI_EVENT_STA_CONNECTED:
        ESP_LOGI(WIFI_TAG, "CONNECTED\n");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(WIFI_TAG, "LOST CONNECTION\n");
        if (retry_num < 5)
        {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(WIFI_TAG, "Retrying to Connect...\n");
        }
        break;
    case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(WIFI_TAG, "Got IP...\n\n");
        xSemaphoreGive(xSemaphoreWifiConnected);
        break;
    }
}

void wifi_init()
{
    // Wi-Fi config phase
    esp_netif_init();
    esp_event_loop_create_default();     // event loop
    esp_netif_create_default_wifi_sta(); // WiFi station
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration;
    memset(&wifi_configuration, 0, sizeof(wifi_configuration));
    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);
    esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_STA, &wifi_configuration);
    // Wi-Fi Start Phase
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    // Wi-Fi Connect Phase
    esp_wifi_connect();
    ESP_LOGI(WIFI_TAG, "initialization finished");
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, mqtt_topic, MQTT_QOS);
        mqtt_connected = 1;
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(MQTT_TAG, "MQTT_EVENT_DISCONNECTED");
        mqtt_connected = 0;
#if MEASURE_RTT
        // To prevent deadlock if the MQTT client disconnects while waiting for a response
        xSemaphoreGive(xSemaphoreMQTTReceived);
#endif
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(MQTT_TAG, "DATA RECEIVED");
        memcpy((uint8_t *)json_recv + event->current_data_offset, event->data, event->data_len);
#if MEASURE_RTT
        // If round trip has been completed, signal the aggregate task
        if (event->current_data_offset + event->data_len == event->total_data_len)
            xSemaphoreGive(xSemaphoreMQTTReceived);
#endif
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(MQTT_TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

static void mqtt_initialize(void)
{ /*Depending on your website or cloud there could be more parameters in mqtt_cfg.*/
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = mqtt_uri,
                .port = mqtt_port},
            .verification = {
                .certificate = root_ca_cert, // described above event handler
            }},
        .credentials = {.authentication = {.certificate = cert, .key = privkey}, .client_id = client_id}
        // .username = /*your username*/,                                 // your username
        // .password = /*your adafruit password*/,                        // your adafruit io password
    };
    client = esp_mqtt_client_init(&mqtt_cfg);                                           // sending struct as a parameter in init client function
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL); // registering event handler
    esp_mqtt_client_start(client);                                                      // starting the process
}

// Snatched from https://www.math.wustl.edu/~victor/mfmm/fourier/fft.c
/*
   fft(v,N):
   [0] If N==1 then return.
   [1] For k = 0 to N/2-1, let ve[k] = v[2*k]
   [2] Compute fft(ve, N/2);
   [3] For k = 0 to N/2-1, let vo[k] = v[2*k+1]
   [4] Compute fft(vo, N/2);
   [5] For m = 0 to N/2-1, do [6] through [9]
   [6]   Let w.re = cos(2*PI*m/N)
   [7]   Let w.im = -sin(2*PI*m/N)
   [8]   Let v[m] = ve[m] + w*vo[m]
   [9]   Let v[m+N/2] = ve[m] - w*vo[m]
 */
void fft(complex *v, int n, complex *tmp)
{
    if (n > 1)
    { /* otherwise, do nothing and return */
        int k, m;
        complex z, w, *vo, *ve;
        ve = tmp;
        vo = tmp + n / 2;
        for (k = 0; k < n / 2; k++)
        {
            ve[k] = v[2 * k];
            vo[k] = v[2 * k + 1];
        }
        fft(ve, n / 2, v); /* FFT on even-indexed elements of v[] */
        fft(vo, n / 2, v); /* FFT on odd-indexed elements of v[] */
        for (m = 0; m < n / 2; m++)
        {
            w.Re = cosf(2 * M_PI * m / (float)n);
            w.Im = -sinf(2 * M_PI * m / (float)n);
            z.Re = w.Re * vo[m].Re - w.Im * vo[m].Im; /* Re(w*vo[m]) */
            z.Im = w.Re * vo[m].Im + w.Im * vo[m].Re; /* Im(w*vo[m]) */
            v[m].Re = ve[m].Re + z.Re;
            v[m].Im = ve[m].Im + z.Im;
            v[m + n / 2].Re = ve[m].Re - z.Re;
            v[m + n / 2].Im = ve[m].Im - z.Im;
        }
    }
    return;
}

float hann(int i, int N)
{
    return 0.5f * (1.f - cos(2 * M_PI * i / (N - 1)));
}

float *generate_hann_window(int N)
{
    int i;
    float *hann_window = (float *)malloc(N * sizeof(float));
    if (!hann_window)
    {
        printf("Failed to allocate memory for window\n");
        return NULL;
    }

    for (i = 0; i < N; i++)
        hann_window[i] = hann(i, N);
    return hann_window;
}

static inline float sine(float A, float f, float t)
{
    // To make the sin function harder and overall more uniform to compute
    // across the input variables
    return A * sinf((2 * M_PI * INITIAL_SAMPLE_FREQ) + 2 * M_PI * f * t);
}

static inline float sample_signal(float t)
{
    // !!! Change this to change signal if using hardcoded mode
    return sine(1, 100, t);
}

void generate_signal(int N, float sample_freq, complex *signal)
{
    int i;
    for (i = 0; i < N; i++)
    {
        float t = i / sample_freq;
        signal[i].Re = sample_signal(t);
        //
        signal[i].Im = 0.f;
    }
}

float sample_signal_get_max_freq(int num_samples, float sample_freq)
{
    int i;
    complex *signal = malloc(num_samples * sizeof(complex));
    complex *aux = malloc(num_samples * sizeof(complex));
    float *hann_window = generate_hann_window(num_samples);

    generate_signal(num_samples, sample_freq, signal);
    for (i = 0; i < num_samples; i++)
    {
        signal[i].Re *= hann_window[i];
        printf("%f ", signal[i].Re);
        // vTaskDelay(configTICK_RATE_HZ / sample_freq);
    }

    printf("\n\n");

    printf("SAMPLED SIGNAL FOR %d STEPS, COMPUTING FFT...\n\n", num_samples);

    fft(signal, num_samples, aux);

    float max_mag = 0.f;
    int max_mag_ind = -1;
    for (i = 0; i < num_samples; i++)
    {
        signal[i].Mag = sqrtf(signal[i].Re * signal[i].Re + signal[i].Im * signal[i].Im);
        // Consider only first half of FFT (positive frequencies, before Nyquist) since
        // the second half is just a mirror of the first half (negative frequency responses)
        if (i < num_samples / 2 && signal[i].Mag > max_mag)
        {
            max_mag = signal[i].Mag;
            max_mag_ind = i;
        }
        printf("%f ", signal[i].Mag);
    }
    printf("\n");
    int max_freq_ind = 0;
    for (i = 0; i < num_samples / 2; i++)
    {
        if (signal[i].Mag > max_mag * THRESHOLD_MULTIPLIER)
            max_freq_ind = i;
    }

    printf("\nFUNDAMENTAL FREQUENCY IN BIN %d, MAX FREQUENCY IN BIN %d\n", max_mag_ind, max_freq_ind);

    free(signal);
    free(aux);
    free(hann_window);

    return (sample_freq / (float)num_samples) * max_freq_ind;
}

void sample_and_send_averages(float sample_freq, int window_size)
{
    float sum = 0.f;
    int num_window = 0, cur_window_ind = 0;
    float *window = (float *)malloc(window_size * sizeof(float));
#if MEASURE_RTT
    float rtt_avg_seconds = 0.f;
    int rtt_avg_count = 0;
#endif
    // Assume that the aggregate value has at most 10 characters:
    // 6 for the decimal part, 1 for the sign, 1 for the dot, 2 for the integer part
    // Could be more than 10 characters if aggregate value is 1000 or above
    // or -100 or below
    const char json_format_string[] = "{\"average\": %10.6f}"; // pad to 10 characters, 6 decimal places
    const int float_print_size = 10;
    int json_overhead = strlen(json_format_string) - 6; // subtract the format specifier length
    int total_message_length = float_print_size + json_overhead;

    float initial_volume_bps = INITIAL_SAMPLE_FREQ * total_message_length * 8;
    float adjusted_volume_bps = sample_freq * total_message_length * 8;
    printf("WITH INITIAL FREQUENCY, DATA VOLUME TRANSMITTED OVER NETWORK IS %fb/s\n", initial_volume_bps);
    printf("WITH ADJUSTED FREQUENCY, DATA VOLUME TRANSMITTED OVER NETWORK IS %fb/s\n", adjusted_volume_bps);
    printf("WITH ADJUSTED FREQUENCY, DATA VOLUME IS %f times smaller\n", initial_volume_bps / adjusted_volume_bps);

    float cur_time = 0.f;

    while (1)
    {
        float v = sample_signal(cur_time);
        sum += v;
        if (num_window < window_size)
            num_window++;
        else
            sum -= window[cur_window_ind];
        window[cur_window_ind] = v;
        cur_window_ind = (cur_window_ind + 1) % window_size;

#if DEBUG_PRINT_WINDOW
        printf("Window: [ ");
        for (int i = 0; i < num_window; i++)
            printf("%f ", window[i]);
        printf("]\n");
        printf("Average: %f\n", sum / num_window);
#endif

        // Send average to MQTT
        if (num_window == window_size && mqtt_connected)
        {
            float avg = sum / window_size;
            snprintf(json_send, MAX_MQTT_SIZE, json_format_string, avg);
            int64_t t = esp_timer_get_time();
            int msg_id = esp_mqtt_client_publish(client, mqtt_topic, json_send, 0, MQTT_QOS, 0);
#if MEASURE_RTT
            xSemaphoreTake(xSemaphoreMQTTReceived, portMAX_DELAY);
            int64_t rtt = esp_timer_get_time() - t;
            ESP_LOGI(MQTT_TAG, "MSG %d, round trip time=%fs, delay=%fs", msg_id, rtt / 1e6f, rtt / 2e6f);
            rtt_avg_seconds += rtt / 1e6f;
            rtt_avg_count++;
            if (rtt_avg_count == RTT_AVERAGE_NUM)
            {
                rtt_avg_seconds /= RTT_AVERAGE_NUM;
                ESP_LOGW(MQTT_TAG, "AVERAGE DELAY OVER %d SENDS: %fs", rtt_avg_count, rtt_avg_seconds / 2.f);
                rtt_avg_seconds = 0.f;
                rtt_avg_count = 0;
            }
#else
            ESP_LOGI(APP_TAG, "MSG %d", msg_id);
#endif
        }

        vTaskDelay(configTICK_RATE_HZ / sample_freq);
        cur_time += 1.f / sample_freq;
    }
}

static void individual_assignment(void *param)
{
    float sample_freq = INITIAL_SAMPLE_FREQ;

    printf("INITIAL SAMPLING RATE IS %d\n", INITIAL_SAMPLE_FREQ);
    printf("CONFIG TICKRATE IS %d\n", configTICK_RATE_HZ);
    printf("\n");

    float max_frequency = sample_signal_get_max_freq(NUM_FFT_SAMPLES, INITIAL_SAMPLE_FREQ);
    printf("\n");
    sample_freq = 2.f * max_frequency;
    printf("MAX FREQUENCY IS %fHz, SAMPLE RATE SET TO %fHz\n", max_frequency, sample_freq);
    printf("\n");

    sample_and_send_averages(sample_freq, WINDOW_SIZE);
}

void app_main(void)
{
    xSemaphoreWifiConnected = xSemaphoreCreateBinary();
#if MEASURE_RTT
    xSemaphoreMQTTReceived = xSemaphoreCreateBinary();
#endif
    // WiFi
    nvs_flash_init();
    wifi_init();
    xSemaphoreTake(xSemaphoreWifiConnected, portMAX_DELAY);
    mqtt_initialize();

    xTaskCreate(individual_assignment, "individual_assignment", configMINIMAL_STACK_SIZE + 2000, NULL, 1, NULL);
}
