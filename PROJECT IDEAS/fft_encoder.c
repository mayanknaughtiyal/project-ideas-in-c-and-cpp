#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <complex.h>

// Compression parameters
#define MIN_FREQ 50.0
#define MAX_FREQ 16000.0
#define AMP_THRESHOLD 0.02
#define WINDOW_SMALL 512
#define WINDOW_LARGE 4096
#define M_Pi 3.1415926535
typedef struct {
    float amplitude;
    float frequency;
    int16_t phase_q10;  // Quantized phase (1024 = π radians)
} CompressedSine;

typedef struct {
    uint32_t chunk_id;
    uint32_t chunk_size;
    uint32_t format;
    uint32_t fmtchunk_id;
    uint32_t fmtchunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint32_t datachunk_id;
    uint32_t datachunk_size;
} WavHeader;

// Recursive Radix-2 FFT
static void fft_recursive(double complex *x, double complex *y, int n, int stride) {
    if (n == 1) {
        y[0] = x[0];
        return;
    }
    
    fft_recursive(x, y, n/2, 2*stride);
    fft_recursive(x+stride, y+n/2, n/2, 2*stride);
    
    for (int k = 0; k < n/2; k++) {
        double complex t = cexp(-2 * I * M_PI * k / n) * y[k + n/2];
        y[k + n/2] = y[k] - t;
        y[k] = y[k] + t;
    }
}

static int determine_window_size(double* samples, size_t pos, size_t sample_count) {
    int remaining = sample_count - pos;
    int window = (remaining < WINDOW_LARGE) ? remaining : WINDOW_LARGE;
    
    // Simple energy detection
    double energy = 0.0;
    for (int i = 0; i < window; i++) {
        energy += samples[pos + i] * samples[pos + i];
    }
    energy /= window;
    
    return (energy > 0.1) ? WINDOW_SMALL : window;
}

static void process_window(double* samples, int window_size,
                         CompressedSine** components, uint32_t* count,
                         int sample_rate) {
    
    double complex* fft_in = malloc(window_size * sizeof(double complex));
    double complex* fft_out = malloc(window_size * sizeof(double complex));
    
    // Apply Hann window
    for (int i = 0; i < window_size; i++) {
        double window = 0.5 * (1 - cos(2*M_PI*i/(window_size-1)));
        fft_in[i] = samples[i] * window;
    }
    
    // Perform FFT
    fft_recursive(fft_in, fft_out, window_size, 1);
    
    // Process FFT output
    for (int k = 0; k < window_size/2; k++) {
        double freq = (double)k * sample_rate / window_size;
        double amp = cabs(fft_out[k]);
        
        if (amp < AMP_THRESHOLD || freq < MIN_FREQ || freq > MAX_FREQ)
            continue;
            
        double phase = carg(fft_out[k]);
        
        // Store component
        *components = realloc(*components, (*count+1) * sizeof(CompressedSine));
        (*components)[*count] = (CompressedSine){
            .amplitude = (float)amp,
            .frequency = (float)freq,
            .phase_q10 = (int16_t)(phase * 1024.0 / M_PI)
        };
        (*count)++;
    }
    
    free(fft_in);
    free(fft_out);
}

static void write_ftae(const CompressedSine* components, uint32_t count,
                      int sample_rate, const char* filename) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        perror("Error creating output file");
        exit(EXIT_FAILURE);
    }
    
    // Write header (sample rate and component count)
    if (fwrite(&sample_rate, sizeof(uint16_t), 1, fp) != 1) {
        perror("Error writing header");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    if (fwrite(&count, sizeof(uint32_t), 1, fp) != 1) {
        perror("Error writing component count");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    // Write components
    if (fwrite(components, sizeof(CompressedSine), count, fp) != count) {
        perror("Error writing components");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    fclose(fp);
    printf("Successfully created %s (%u components)\n", filename, count);
}

int main(int argc, char** argv) {
    if (argc != 3 || strcmp(argv[2], "-ftae")) {
        fprintf(stderr, "Usage: %s <input.wav> -ftae\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    printf("Processing %s...\n", argv[1]);
    
    // Read WAV file
    FILE* wav = fopen(argv[1], "rb");
    if (!wav) {
        perror("Error opening WAV file");
        return EXIT_FAILURE;
    }
    
    WavHeader header;
    if (fread(&header, sizeof(WavHeader), 1, wav) != 1) {
        perror("Error reading WAV header");
        fclose(wav);
        return EXIT_FAILURE;
    }
    
    // Verify WAV format
    if (header.audio_format != 1 || header.bits_per_sample != 16) {
        fprintf(stderr, "Only 16-bit PCM WAV files are supported\n");
        fclose(wav);
        return EXIT_FAILURE;
    }
    
    size_t sample_count = header.datachunk_size / 2;  // 16-bit = 2 bytes per sample
    double* samples = malloc(sample_count * sizeof(double));
    if (!samples) {
        perror("Memory allocation failed");
        fclose(wav);
        return EXIT_FAILURE;
    }
    
    printf("Reading %zu samples...\n", sample_count);
    
    // Read samples
    for (size_t i = 0; i < sample_count; i++) {
        int16_t sample;
        if (fread(&sample, sizeof(int16_t), 1, wav) != 1) {
            perror("Error reading audio data");
            free(samples);
            fclose(wav);
            return EXIT_FAILURE;
        }
        samples[i] = sample / 32768.0;  // Normalize to [-1, 1]
    }
    fclose(wav);
    
    printf("Processing audio...\n");
    
    // Process audio
    CompressedSine* components = NULL;
    uint32_t component_count = 0;
    size_t pos = 0;
    
    while (pos < sample_count) {
        int window_size = determine_window_size(samples, pos, sample_count);
        process_window(samples + pos, window_size, &components, &component_count,
                      header.sample_rate);
        pos += window_size;
        
        // Progress indicator
        if (pos % (sample_count/10) == 0) {
            printf("Progress: %zu%%\r", (pos * 100) / sample_count);
            fflush(stdout);
        }
    }
    
    printf("\nFound %u frequency components\n", component_count);
    
    // Write output
    write_ftae(components, component_count, header.sample_rate, "output.ftae");
    
    free(components);
    free(samples);
    
    printf("Done! Check 'output.ftae' in the current directory.\n");
    return EXIT_SUCCESS;
}
