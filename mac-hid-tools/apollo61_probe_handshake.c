#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_VID 0x05ac
#define DEFAULT_PID 0x024f
#define DEFAULT_USAGE_PAGE 0x0001
#define DEFAULT_USAGE 0x0006
#define REPORT_ID 0
#define REPORT_LEN 64

typedef struct Options {
    int vid;
    int pid;
    int usage_page;
    int usage;
    int write_enabled;
    int delay_ms;
} Options;

typedef struct ProbeStep {
    const char *name;
    uint8_t payload[REPORT_LEN];
} ProbeStep;

static long number_property(IOHIDDeviceRef device, CFStringRef key, long fallback) {
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return fallback;
    }

    long result = fallback;
    CFNumberGetValue((CFNumberRef)value, kCFNumberLongType, &result);
    return result;
}

static void string_property(IOHIDDeviceRef device, CFStringRef key, char *buffer, size_t buffer_size) {
    if (buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (!value || CFGetTypeID(value) != CFStringGetTypeID()) {
        snprintf(buffer, buffer_size, "(none)");
        return;
    }

    if (!CFStringGetCString((CFStringRef)value, buffer, buffer_size, kCFStringEncodingUTF8)) {
        snprintf(buffer, buffer_size, "(unprintable)");
    }
}

static void set_number(CFMutableDictionaryRef dict, CFStringRef key, int value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    if (number) {
        CFDictionarySetValue(dict, key, number);
        CFRelease(number);
    }
}

static CFMutableDictionaryRef matching_dictionary(const Options *options) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    if (!dict) {
        return NULL;
    }

    set_number(dict, CFSTR(kIOHIDVendorIDKey), options->vid);
    set_number(dict, CFSTR(kIOHIDProductIDKey), options->pid);
    set_number(dict, CFSTR(kIOHIDPrimaryUsagePageKey), options->usage_page);
    set_number(dict, CFSTR(kIOHIDPrimaryUsageKey), options->usage);
    return dict;
}

static int parse_hex_or_decimal(const char *text, int *value) {
    char *end = NULL;
    long parsed = strtol(text, &end, 0);
    if (!text[0] || (end && *end != '\0') || parsed < 0 || parsed > 0xffff) {
        return 0;
    }
    *value = (int)parsed;
    return 1;
}

static void print_hex_prefix(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02x", data[i]);
        if (i + 1 < length) {
            putchar(' ');
        }
    }
}

static const char *status_name(uint8_t status) {
    switch (status) {
        case 0x01: return "ack";
        case 0xff: return "done/closed";
        case 0x00: return "zero";
        default: return "unknown";
    }
}

static void usage(const char *argv0) {
    printf("Usage: %s [--write --i-understand-this-writes] [options]\n", argv0);
    puts("");
    puts("Runs the known APOLLO61 handshake: 04 18, 04 17 ... 01, 04 02.");
    puts("Without --write this only prints the planned sequence.");
    puts("");
    puts("Options:");
    puts("  --write                       Actually send the three feature reports");
    puts("  --i-understand-this-writes    Required together with --write");
    puts("  --delay-ms N                  Wait after each write before reading, default 75");
    puts("  --vid HEX --pid HEX           Default 0x05ac:0x024f");
    puts("  --usage-page HEX --usage HEX  Default 0x0001:0x0006");
}

static int open_first_device(const Options *options,
                             IOHIDManagerRef *manager_out,
                             CFSetRef *devices_out,
                             IOHIDDeviceRef *device_out) {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        fprintf(stderr, "Failed to create IOHIDManager\n");
        return 0;
    }

    CFMutableDictionaryRef dict = matching_dictionary(options);
    if (!dict) {
        CFRelease(manager);
        fprintf(stderr, "Failed to create matching dictionary\n");
        return 0;
    }

    IOHIDManagerSetDeviceMatching(manager, dict);
    CFRelease(dict);

    IOReturn open_result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (open_result != kIOReturnSuccess) {
        fprintf(stderr, "Failed to open IOHIDManager: 0x%08x\n", open_result);
        CFRelease(manager);
        return 0;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices || CFSetGetCount(devices) == 0) {
        fprintf(stderr, "No matching HID device found.\n");
        if (devices) {
            CFRelease(devices);
        }
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 0;
    }

    IOHIDDeviceRef *device_list = calloc((size_t)CFSetGetCount(devices), sizeof(IOHIDDeviceRef));
    if (!device_list) {
        fprintf(stderr, "Out of memory\n");
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 0;
    }

    CFSetGetValues(devices, (const void **)device_list);
    *device_out = device_list[0];
    free(device_list);
    *manager_out = manager;
    *devices_out = devices;
    return 1;
}

static IOReturn read_response(IOHIDDeviceRef device, uint8_t *response) {
    CFIndex report_length = REPORT_LEN;
    memset(response, 0, REPORT_LEN);
    return IOHIDDeviceGetReport(device,
                                kIOHIDReportTypeFeature,
                                REPORT_ID,
                                response,
                                &report_length);
}

static int run_step(IOHIDDeviceRef device, const ProbeStep *step, int delay_ms) {
    uint8_t response[REPORT_LEN];

    printf("%s send=", step->name);
    print_hex_prefix(step->payload, 16);
    puts(" ...");

    IOReturn set_result = IOHIDDeviceSetReport(device,
                                               kIOHIDReportTypeFeature,
                                               REPORT_ID,
                                               step->payload,
                                               REPORT_LEN);
    printf("%s set_result=0x%08x\n", step->name, set_result);
    if (set_result != kIOReturnSuccess) {
        return 0;
    }

    if (delay_ms > 0) {
        usleep((useconds_t)delay_ms * 1000);
    }

    IOReturn get_result = read_response(device, response);
    printf("%s get_result=0x%08x response=", step->name, get_result);
    print_hex_prefix(response, 8);
    printf(" ... status=0x%02x (%s)\n", response[3], status_name(response[3]));

    if (response[0] != step->payload[0] || response[1] != step->payload[1]) {
        printf("%s warning: response command does not echo request\n", step->name);
    }

    return get_result == kIOReturnSuccess;
}

int main(int argc, char **argv) {
    Options options = {
        .vid = DEFAULT_VID,
        .pid = DEFAULT_PID,
        .usage_page = DEFAULT_USAGE_PAGE,
        .usage = DEFAULT_USAGE,
        .write_enabled = 0,
        .delay_ms = 75,
    };
    int acknowledged = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--write") == 0) {
            options.write_enabled = 1;
        } else if (strcmp(argv[i], "--i-understand-this-writes") == 0) {
            acknowledged = 1;
        } else if (strcmp(argv[i], "--vid") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.vid)) {
                fprintf(stderr, "Invalid VID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.pid)) {
                fprintf(stderr, "Invalid PID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--usage-page") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.usage_page)) {
                fprintf(stderr, "Invalid usage page: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--usage") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.usage)) {
                fprintf(stderr, "Invalid usage: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.delay_ms) || options.delay_ms > 5000) {
                fprintf(stderr, "Invalid delay: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    ProbeStep steps[] = {
        {.name = "start", .payload = {0x04, 0x18}},
        {.name = "probe", .payload = {0x04, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
        {.name = "end", .payload = {0x04, 0x02}},
    };

    printf("target: vid=0x%04x pid=0x%04x usage=0x%04x:0x%04x report_id=%d length=%d\n",
           options.vid,
           options.pid,
           options.usage_page,
           options.usage,
           REPORT_ID,
           REPORT_LEN);
    printf("delay_ms=%d\n", options.delay_ms);

    if (!options.write_enabled) {
        for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
            printf("%s planned=", steps[i].name);
            print_hex_prefix(steps[i].payload, 16);
            puts(" ...");
        }
        puts("dry-run: no HID writes were performed. Add --write --i-understand-this-writes to run the handshake.");
        return 0;
    }

    if (!acknowledged) {
        fputs("Refusing to write: --write requires --i-understand-this-writes.\n", stderr);
        return 2;
    }

    IOHIDManagerRef manager = NULL;
    CFSetRef devices = NULL;
    IOHIDDeviceRef device = NULL;
    if (!open_first_device(&options, &manager, &devices, &device)) {
        return 1;
    }

    char product[256];
    char manufacturer[256];
    char transport[256];
    string_property(device, CFSTR(kIOHIDProductKey), product, sizeof(product));
    string_property(device, CFSTR(kIOHIDManufacturerKey), manufacturer, sizeof(manufacturer));
    string_property(device, CFSTR(kIOHIDTransportKey), transport, sizeof(transport));
    long max_feature = number_property(device, CFSTR(kIOHIDMaxFeatureReportSizeKey), 0);

    printf("device: %s / %s transport=%s max_feature=%ld\n",
           manufacturer,
           product,
           transport,
           max_feature);

    int ok = 1;
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        if (!run_step(device, &steps[i], options.delay_ms)) {
            ok = 0;
            break;
        }
    }

    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return ok ? 0 : 1;
}
