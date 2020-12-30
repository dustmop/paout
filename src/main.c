#include <assert.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pulse/pulseaudio.h>
#include <wchar.h>
#include <locale.h>


static pa_mainloop *pa_ml = NULL;
static pa_mainloop_api *pa_mlapi = NULL;
static pa_context *context = NULL;
static pa_stream *stream = NULL;
static char* device_name = NULL;
static char* device_description = NULL;


static int opt_once = 0;
static int opt_stream = 0;
static int opt_bars = 0;


#define FULL_BLOCK_CHAR 0x2588
#define MAX_EST_AUDIO 0.02
#define MAX_BAR_UNITS 60
wchar_t bar_display[MAX_BAR_UNITS+1];


void init_bar_display() {
    setlocale(LC_CTYPE, "");
    for (int k = 0; k < MAX_BAR_UNITS; k++) {
        bar_display[k] = L'-';
    }
    bar_display[MAX_BAR_UNITS] = L'\0';
}


void stream_state_callback(pa_stream *s, void *data) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_UNCONNECTED:
        case PA_STREAM_CREATING:
            break;

        case PA_STREAM_READY:
            break;

        case PA_STREAM_FAILED:
            printf("ERROR: Connection failed\n");
            break;

        case PA_STREAM_TERMINATED:
            break;
    }
}


void stream_read_callback(pa_stream *s, size_t l, void *data) {
    const void *p;

    if (pa_stream_peek(s, &p, &l) < 0) {
        printf("ERROR: pa_stream_peek() failed: %s",
               pa_strerror(pa_context_errno(context)));
        return;
    }

    const float* fp = (const float*)p;

    if (pa_ml) {
        float app_result = fabs(*fp);
        if (opt_once) {
            printf("%.5f\n", app_result);
            // Return 1 if no audio
            pa_mainloop_quit(pa_ml, (app_result == 0.0));
        } else if (opt_bars) {
            int num_units = MAX_BAR_UNITS * app_result * (1 / MAX_EST_AUDIO);
            for (int k = 0; k < MAX_BAR_UNITS; k++) {
                bar_display[k] = (k < num_units) ? FULL_BLOCK_CHAR : L'-';
            }
            wprintf(L" %ls\r", bar_display);
            fflush(stdout);
        } else {
            printf(" %.5f   \r", app_result);
            fflush(stdout);
        }
    }

    pa_stream_drop(s);
}


void create_stream(const char *name, const char *description, const pa_sample_spec *ss, const pa_channel_map *cmap) {

    char t[256];
    pa_sample_spec nss;

    free(device_name);
    device_name = strdup(name);
    free(device_description);
    device_description = strdup(description);

    nss.format = PA_SAMPLE_FLOAT32;
    nss.rate = ss->rate;
    nss.channels = ss->channels;

    fprintf(stderr, "Using sample format: %s\n",
            pa_sample_spec_snprint(t, sizeof(t), &nss));
    fprintf(stderr, "Using channel map: %s\n",
            pa_channel_map_snprint(t, sizeof(t), cmap));

    stream = pa_stream_new(context, "PulseAudio Output Display", &nss, cmap);
    pa_stream_set_state_callback(stream, stream_state_callback, NULL);
    pa_stream_set_read_callback(stream, stream_read_callback, NULL);
    pa_stream_connect_record(stream, name, NULL, (enum pa_stream_flags)0);
}


static void context_sink_info_callback(pa_context *ctx, const pa_sink_info *si,
                                       int is_last, void *data) {
    assert(is_last >= 0);
    if (!si) {
        return;
    }

    create_stream(si->monitor_source_name, si->description, &si->sample_spec,
                  &si->channel_map);
}


void context_state_callback(pa_context *c, void *data) {
    pa_operation* op = NULL;
    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY:
            assert(!stream);
            op = pa_context_get_sink_info_by_name(c, device_name,
                                                  context_sink_info_callback,
                                                  NULL);
            pa_operation_unref(op);
            break;

        case PA_CONTEXT_FAILED:
            printf("ERROR: Connection failed\n");
            break;

        case PA_CONTEXT_TERMINATED:
            break;
    }
}


int output_display() {
    int retval = 0;

    init_bar_display();

    pa_ml = pa_mainloop_new();
    assert(pa_ml);

    pa_mlapi = pa_mainloop_get_api(pa_ml);
    assert(pa_mlapi);

    context = pa_context_new(pa_mlapi, "PulseAudio Output Display");
    assert(context);

    pa_context_set_state_callback(context, context_state_callback, NULL);
    pa_context_connect(context, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);

    pa_mainloop_run(pa_ml, NULL);
    retval = pa_mainloop_get_retval(pa_ml);

    if (stream) {
        pa_stream_unref(stream);
    }
    if (context) {
        pa_context_unref(context);
    }
    if (device_name) {
        free(device_name);
    }
    if (device_description) {
        free(device_description);
    }

    pa_mainloop_free(pa_ml);

    return retval;
}


int main(int argc, char *argv[]) {
    int c;
    while (1) {
        c = getopt(argc, argv, "1sb");
        if (c == -1) { break; }

        switch(c) {
        case '1':
            opt_once = 1;
            break;
        case 'b':
            opt_bars = 1;
            break;
        case 's':
            opt_stream = 1;
            break;
        }
    }

    if (!opt_once && !opt_stream && !opt_bars) {
        printf("paout displays pulseaudio output information\n");
        printf("\n");
        printf("usage: paout [-1 | -s | b]\n");
        printf("\n");
        printf("       -1  get one value then quit\n");
        printf("       -s  stream values continually to stdout\n");
        printf("       -b  bar graph drawn continually to stdout\n");
        printf("\n");
        printf("return value is 0 if there is audio, 1 if silent\n");
        exit(2);
    }
    if ((opt_once && opt_stream) ||
        (opt_once && opt_bars) ||
        (opt_bars && opt_stream)) {
        printf("paout: cannot set more than one of -1, -s, -b flags\n");
        exit(2);
    }

    return output_display();
}
