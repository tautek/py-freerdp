#ifndef _WIN32
#include <unistd.h>
#include <pthread.h>
#include <sys/select.h>
#else
#include <winsock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/utils/event.h>
#include <freerdp/client/file.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/client/cliprdr.h>
#include <freerdp/channels/channels.h>
#include <freerdp/locale/keyboard.h>
#include <winpr/crt.h>
#include <winpr/synch.h>

#include "freerdp.h"


#define MAX_CONNECTIONS 100
HANDLE g_sem = NULL;
static int g_thread_count = 0;
static freerdp* g_instances[MAX_CONNECTIONS] = { NULL };

/**
 * Additional context.
 */
struct context {
    rdpContext _p;
    BOOL shutdown;
    instance_callback_t onConnect; 
};
typedef struct context Context;

/**
 * Instance data for thread.
 */
struct thread_data {
    freerdp* instance;
};

/**
 * Init context.
 */
int fapi_context_new(freerdp* instance, rdpContext* context) {
    context->channels = freerdp_channels_new();
    return 0;
}

/**
 * Deinit context.
 */
void fapi_context_free(freerdp* instance, rdpContext* context) {
}

/**
 * Update paint.
 */
void fapi_begin_paint(rdpContext* context) {
    rdpGdi* gdi = context->gdi;
    gdi->primary->hdc->hwnd->invalid->null = 1;
}

/**
 * Paint updated.
 */
void fapi_end_paint(rdpContext* context) {
    rdpGdi* gdi = context->gdi;
    if (gdi->primary->hdc->hwnd->invalid->null)
        return;
}

/**
 * Updated channel data.
 */
int fapi_receive_channel_data(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size) {
    return freerdp_channels_data(instance, channelId, data, size, flags, total_size);
}

/**
 * Clipboard ready.
 */
void fapi_process_cb_monitor_ready_event(rdpChannels* channels, freerdp* instance) {
    wMessage* event;
    RDP_CB_FORMAT_LIST_EVENT* format_list_event;
    event = freerdp_event_new(CliprdrChannel_Class, CliprdrChannel_FormatList, NULL, NULL);
    format_list_event = (RDP_CB_FORMAT_LIST_EVENT*) event;
    format_list_event->num_formats = 0;
    freerdp_channels_send_event(channels, event);
}

/**
 * Channel event.
 */
void fapi_process_channel_event(rdpChannels* channels, freerdp* instance) {
    wMessage* event;
    event = freerdp_channels_pop_event(channels);
    if (event) {
        switch (GetMessageType(event->id)) {
            case CliprdrChannel_MonitorReady:
                fapi_process_cb_monitor_ready_event(channels, instance);
                break;
            default:
                fprintf(stderr, "fapi_process_channel_event: unknown event type %d\n", GetMessageType(event->id));
                break;
        }
        freerdp_event_free(event);
    }
}

/**
 * Pre connect.
 */
BOOL fapi_pre_connect(freerdp* instance) {
    rdpSettings* settings;
    settings = instance->settings;
    settings->OrderSupport[NEG_DSTBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_PATBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_SCRBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_OPAQUE_RECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_DRAWNINEGRID_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIDSTBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIPATBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTISCRBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTIOPAQUERECT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MULTI_DRAWNINEGRID_INDEX] = TRUE;
    settings->OrderSupport[NEG_LINETO_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYLINE_INDEX] = TRUE;
    settings->OrderSupport[NEG_MEMBLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_MEM3BLT_INDEX] = TRUE;
    settings->OrderSupport[NEG_SAVEBITMAP_INDEX] = TRUE;
    settings->OrderSupport[NEG_GLYPH_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_INDEX_INDEX] = TRUE;
    settings->OrderSupport[NEG_FAST_GLYPH_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYGON_SC_INDEX] = TRUE;
    settings->OrderSupport[NEG_POLYGON_CB_INDEX] = TRUE;
    settings->OrderSupport[NEG_ELLIPSE_SC_INDEX] = TRUE;
    settings->OrderSupport[NEG_ELLIPSE_CB_INDEX] = TRUE;
    freerdp_channels_pre_connect(instance->context->channels, instance);
    return TRUE;
}

/**
 * Post connect.
 */
BOOL fapi_post_connect(freerdp* instance) {
    //rdpGdi* gdi;
    gdi_init(instance, CLRCONV_ALPHA | CLRCONV_INVERT | CLRBUF_16BPP | CLRBUF_32BPP, NULL);
    //gdi = instance->context->gdi;
    instance->update->BeginPaint = fapi_begin_paint;
    instance->update->EndPaint = fapi_end_paint;
    freerdp_channels_post_connect(instance->context->channels, instance);
    return TRUE;
}

/**
 * Session thread.
 */
int fapi_run(freerdp* instance) {
    int i;
    int fds;
    int max_fds;
    int rcount;
    int wcount;
    void* rfds[32];
    void* wfds[32];
    fd_set rfds_set;
    fd_set wfds_set;
    rdpChannels* channels;
    ZeroMemory(rfds, sizeof(rfds));
    ZeroMemory(wfds, sizeof(wfds));
    channels = instance->context->channels;
    freerdp_connect(instance);
    Context* context = ((Context*)(instance->context));
    context->onConnect(instance);

    while (!context->shutdown)
    {
        rcount = 0;
        wcount = 0;
        if (freerdp_get_fds(instance, rfds, &rcount, wfds, &wcount) != TRUE) {
            fprintf(stderr, "Failed to get FreeRDP file descriptor\n");
            break;
        }
        if (freerdp_channels_get_fds(channels, instance, rfds, &rcount, wfds, &wcount) != TRUE) {
            fprintf(stderr, "Failed to get channel manager file descriptor\n");
            break;
        }

        max_fds = 0;
        FD_ZERO(&rfds_set);
        FD_ZERO(&wfds_set);

        for (i = 0; i < rcount; i++) {
            fds = (int)(long)(rfds[i]);
            if (fds > max_fds)
                max_fds = fds;
            FD_SET(fds, &rfds_set);
        }

        if (max_fds == 0)
            break;

        struct timeval timeout; timeout.tv_sec=1; timeout.tv_usec=0;
        BOOL timeoutBreak = FALSE;
        if (select(max_fds + 1, &rfds_set, &wfds_set, NULL, &timeout) == -1) {
            if (errno == ETIMEDOUT) { timeoutBreak = TRUE; }
            /* these are not really errors */ 
            else if (!((errno == EAGAIN) ||
	             (errno == EWOULDBLOCK) ||
		     (errno == EINPROGRESS) ||
                     (errno == EINTR))) /* signal occurred */
	    {
                fprintf(stderr, "fapi_run: select failed\n");
                break;
            }
        }

        if (!timeoutBreak && !((Context*)(instance->context))->shutdown) {
            if (freerdp_check_fds(instance) != TRUE) {
                fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
                break;
            }
            if (freerdp_channels_check_fds(channels, instance) != TRUE) {
                fprintf(stderr, "Failed to check channel manager file descriptor\n");
                break;
            }
            fapi_process_channel_event(channels, instance);
        }
    }
    freerdp_channels_close(channels, instance);
    freerdp_channels_free(channels);
    freerdp_free(instance);
    return 0;
}

/**
 * Thread definition.
 */
void* thread_func(void* param)
{
    struct thread_data* data;
    data = (struct thread_data*) param;
    fapi_run(data->instance);
    free(data);
    pthread_detach(pthread_self());
    g_thread_count--;
    if (g_thread_count < 1)
        ReleaseSemaphore(g_sem, 1, NULL);
    return NULL;
}

/**
 * Stop instance and remove tracking.
 */
void internal_stop(void* instance, int index) {
    g_instances[index] = NULL;
    ((Context*)(((freerdp*)instance)->context))->shutdown = TRUE;
    freerdp_disconnect((freerdp*)instance);
}

/**
 * Stop instance and disconnect.
 */
void stop(void* instance) {
    int index;
    for (index=0; index<MAX_CONNECTIONS; ++index) {
        if (g_instances[index] == instance) {
            internal_stop(g_instances[index], index);
            break;
        }
    }
}

/**
 * Run a command string.
 */
char* run_command(void* void_instance, char* command) {
    freerdp* instance = (freerdp*)void_instance;
    freerdp_input_send_keyboard_event_ex(instance->input, TRUE, RDP_SCANCODE_LWIN);
    freerdp_input_send_keyboard_event_ex(instance->input, TRUE, RDP_SCANCODE_KEY_R);
    freerdp_input_send_keyboard_event_ex(instance->input, FALSE, RDP_SCANCODE_LWIN);
    freerdp_input_send_keyboard_event_ex(instance->input, FALSE, RDP_SCANCODE_KEY_R);
    sleep(1);

    char * raw;
    char val;
    DWORD code;
    BOOL isUpper;
    for (raw=command; *raw != '\0'; raw++) {
        isUpper = (*raw >= 'A' && *raw <= 'Z');
        val = isUpper ? tolower(*raw) : *raw;
        if (val=='a')      { code = RDP_SCANCODE_KEY_A; }
        else if (val=='b') { code = RDP_SCANCODE_KEY_B; }
        else if (val=='c') { code = RDP_SCANCODE_KEY_C; }
        else if (val=='d') { code = RDP_SCANCODE_KEY_D; }
        else if (val=='e') { code = RDP_SCANCODE_KEY_E; }
        else if (val=='f') { code = RDP_SCANCODE_KEY_F; }
        else if (val=='g') { code = RDP_SCANCODE_KEY_G; }
        else if (val=='h') { code = RDP_SCANCODE_KEY_H; }
        else if (val=='i') { code = RDP_SCANCODE_KEY_I; }
        else if (val=='j') { code = RDP_SCANCODE_KEY_J; }
        else if (val=='k') { code = RDP_SCANCODE_KEY_K; }
        else if (val=='l') { code = RDP_SCANCODE_KEY_L; }
        else if (val=='m') { code = RDP_SCANCODE_KEY_M; }
        else if (val=='n') { code = RDP_SCANCODE_KEY_N; }
        else if (val=='o') { code = RDP_SCANCODE_KEY_O; }
        else if (val=='p') { code = RDP_SCANCODE_KEY_P; }
        else if (val=='q') { code = RDP_SCANCODE_KEY_Q; }
        else if (val=='r') { code = RDP_SCANCODE_KEY_R; }
        else if (val=='s') { code = RDP_SCANCODE_KEY_S; }
        else if (val=='t') { code = RDP_SCANCODE_KEY_T; }
        else if (val=='u') { code = RDP_SCANCODE_KEY_U; }
        else if (val=='v') { code = RDP_SCANCODE_KEY_V; }
        else if (val=='w') { code = RDP_SCANCODE_KEY_W; }
        else if (val=='x') { code = RDP_SCANCODE_KEY_X; }
        else if (val=='y') { code = RDP_SCANCODE_KEY_Y; }
        else if (val=='z') { code = RDP_SCANCODE_KEY_Z; }
        else if (val=='1') { code = RDP_SCANCODE_KEY_1; }
        else if (val=='2') { code = RDP_SCANCODE_KEY_2; }
        else if (val=='3') { code = RDP_SCANCODE_KEY_3; }
        else if (val=='4') { code = RDP_SCANCODE_KEY_4; }
        else if (val=='5') { code = RDP_SCANCODE_KEY_5; }
        else if (val=='6') { code = RDP_SCANCODE_KEY_6; }
        else if (val=='7') { code = RDP_SCANCODE_KEY_7; }
        else if (val=='8') { code = RDP_SCANCODE_KEY_8; }
        else if (val=='9') { code = RDP_SCANCODE_KEY_9; }
        else if (val=='0') { code = RDP_SCANCODE_KEY_0; }
        else if (val=='-') { code = RDP_SCANCODE_OEM_MINUS; }
        else if (val=='+') { code = RDP_SCANCODE_OEM_PLUS; }
        else if (val=='[') { code = RDP_SCANCODE_OEM_4; }
        else if (val==']') { code = RDP_SCANCODE_OEM_6; }
        else if (val==';') { code = RDP_SCANCODE_OEM_1; }
        else if (val==''') { code = RDP_SCANCODE_OEM_7; }
        else if (val=='/') { code = RDP_SCANCODE_OEM_2; }
        else if (val=='\\'){ code = RDP_SCANCODE_OEM_102; }
        else if (val=='*'){ code = RDP_SCANCODE_MULTIPLY; }
        else if (val==' '){ code = RDP_SCANCODE_SPACE; }
        else if (val=='.') { code = RDP_SCANCODE_OEM_PERIOD; }
        else if (val==',') { code = RDP_SCANCODE_OEM_COMMA; }
        else { return "Bad code"; }
        if (isUpper) {
            freerdp_input_send_keyboard_event_ex(instance->input, TRUE, RDP_SCANCODE_LSHIFT);
            usleep(100);
        }
        freerdp_input_send_keyboard_event_ex(instance->input, TRUE, code); usleep(100);
        freerdp_input_send_keyboard_event_ex(instance->input, FALSE, code); usleep(100);
        if (isUpper) {
            freerdp_input_send_keyboard_event_ex(instance->input, FALSE, RDP_SCANCODE_LSHIFT);
            usleep(100);
        }
    }
    freerdp_input_send_keyboard_event_ex(instance->input, TRUE, RDP_SCANCODE_RETURN); usleep(100);
    freerdp_input_send_keyboard_event_ex(instance->input, FALSE, RDP_SCANCODE_RETURN);
    return "";
}

/**
 * Connect and start session.
 */
void* start(int argc, char* argv[], instance_callback_t onConnect) {
    if (g_sem == NULL) {
        g_sem = CreateSemaphore(NULL, 0, 1, NULL);
        freerdp_channels_global_init();
    }
    if (g_thread_count+1 >= MAX_CONNECTIONS) { return NULL; }
    
    int status;
    pthread_t thread;
    freerdp* instance;
    //rdpChannels* channels;
    struct thread_data* data;
    instance = freerdp_new();
    instance->PreConnect = fapi_pre_connect;
    instance->PostConnect = fapi_post_connect;
    instance->ReceiveChannelData = fapi_receive_channel_data;
    instance->ContextSize = sizeof(Context);
    instance->ContextNew = fapi_context_new;
    instance->ContextFree = fapi_context_free;
    freerdp_context_new(instance);
    Context* context;
    context = (Context*)instance->context;
    context->shutdown = FALSE;
    context->onConnect = onConnect;
    //channels = instance->context->channels;
    status = freerdp_client_parse_command_line_arguments(argc, argv, instance->settings);
    if (status < 0) {
        fprintf(stderr, "Bad start arguments");
        return NULL;
    }
    freerdp_client_load_addins(instance->context->channels, instance->settings);
    data = (struct thread_data*) malloc(sizeof(struct thread_data));
    ZeroMemory(data, sizeof(sizeof(struct thread_data)));
    data->instance = instance;
    g_thread_count++;
    int index;
    for (index=0; index<MAX_CONNECTIONS; ++index) {
        if (g_instances[index] == NULL) {
            g_instances[index] = instance;
            break;
        }
    }
    pthread_create(&thread, 0, thread_func, data);
    return instance;
}

/**
 * Stop all sessions.
 */
void destroy (int ms_timeout) {
    if (g_thread_count == 0) { return; }
    int index;
    for (index=0; index<MAX_CONNECTIONS; ++index) {
        if (g_instances[index] != NULL) {
            internal_stop(g_instances[index], index);
        }
    }
    int timeout = ms_timeout == 0 ? INFINITE : ms_timeout;
    WaitForSingleObject(g_sem, timeout);
    freerdp_channels_global_uninit();
    g_thread_count = 0;
}

void test_onConnect(void* instance) {
    fprintf(stderr, "Connected!\n");
}

int main(int argc, char* argv[])
{
    void * instance = start(argc, argv, test_onConnect);
    while (g_thread_count > 0)
    {
            WaitForSingleObject(g_sem, 10000);
            //run_command(instance, "calc");
            fprintf(stderr, "shutting down");
            stop(instance);
            destroy(0);
    }
    return 0;
}

