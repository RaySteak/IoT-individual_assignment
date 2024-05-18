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

#include "platform.h"

#include "printf.h"
#include "soft_timer.h"
#include "soft_timer_delay.h"
#include <stdatomic.h>
#include <stdbool.h>

#include "queue.h"

#define SAMPLE_HARDCODED 1

#define NUM_FFT_SAMPLES 256
#define INITIAL_SAMPLE_FREQ 1000
#define THRESHOLD_MULTIPLIER 0.1f

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

static void app_task(void *);

// static void char_rx(handler_arg_t arg, uint8_t c);

#if SAMPLE_HARDCODED == 0
static float sensor_value;
#endif
static bool started_receiving = 0;

static void char_rx(handler_arg_t arg, uint8_t c)
{
#if SAMPLE_HARDCODED
    started_receiving = 1;
#else
    static int num_received = 0;
    static float intermediary_value;

    ((uint8_t *)&intermediary_value)[num_received++] = c;
    if (num_received < sizeof(float))
        return;

    started_receiving = true;
    num_received = 0;
    sensor_value = intermediary_value; // float load/store operations are atomic on ESP32
    intermediary_value = 0;
#endif
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

static inline float hann(int i, int N)
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
    return sine(1, 1, t);
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

#if SAMPLE_HARDCODED
    generate_signal(num_samples, sample_freq, signal);
    for (i = 0; i < num_samples; i++)
    {
        signal[i].Re *= hann_window[i];
        printf("%f ", signal[i].Re);
    }
#else
    for (i = 0; i < num_samples; i++)
    {
        signal[i].Re = sensor_value * hann_window[i];
        printf("%f ", signal[i].Re);
        signal[i].Im = 0.f;
        vTaskDelay(configTICK_RATE_HZ / sample_freq);
    }
#endif

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

void sample_for_seconds(float sample_freq, float seconds)
{
    const int window_size = 5;
    float sum = 0.f;
    int num_window = 0, cur_window_ind = 0;
    float *window = (float *)malloc(window_size * sizeof(float));
    float cur_seconds = 0.f;

    printf("SAMPLING SIGNAL FOR %f seconds at frequency %f...\n", seconds, sample_freq);

    while (cur_seconds < seconds)
    {
#if SAMPLE_HARDCODED
        float v = sample_signal(cur_seconds);
#else
        float v = sensor_value;
#endif
        sum += v;
        if (num_window < window_size)
            num_window++;
        else
            sum -= window[cur_window_ind];
        window[cur_window_ind] = v;
        cur_window_ind = (cur_window_ind + 1) % window_size;

        vTaskDelay(configTICK_RATE_HZ / sample_freq);
        cur_seconds += 1.f / sample_freq;
    }
    free(window);
    printf("DONE SAMPLING!\n");
}

static void app_task(void *param)
{
    float sample_freq = INITIAL_SAMPLE_FREQ;

    while (!started_receiving)
        vTaskDelay(0); // Otherwise compiler turns this into infinite loop it seems
    printf("STARTED RECEIVING SIGNAL\n");

    sample_for_seconds(sample_freq, 30);

    printf("INITIAL SAMPLING RATE IS %d\n", INITIAL_SAMPLE_FREQ);
    printf("CONFIG TICKRATE IS %d\n", configTICK_RATE_HZ);
    printf("\n");

    float max_frequency = sample_signal_get_max_freq(NUM_FFT_SAMPLES, INITIAL_SAMPLE_FREQ);
    printf("\n");
    sample_freq = 2.f * max_frequency;
    printf("MAX FREQUENCY IS ~%dHz, SAMPLE RATE SET TO ~%dHz\n", (int)max_frequency, (int)sample_freq);

    sample_for_seconds(sample_freq, 30);
    vTaskDelete(0);
}

int main()
{
    // Initialize the platform
    platform_init();

    uart_set_rx_handler(uart_print, char_rx, NULL);

    xTaskCreate(app_task, (const signed char *const)"app", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    // Run
    platform_run();
    return 0;
}
