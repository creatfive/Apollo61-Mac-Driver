#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <ctype.h>
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
    int delay_ms;
    int do_start;
    int do_end;
    int write_enabled;
} Options;

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

static long number_property(IOHIDDeviceRef device, CFStringRef key, long fallback) {
    CFTypeRef value = IOHIDDeviceGetProperty(device, key);
    if (!value || CFGetTypeID(value) != CFNumberGetTypeID()) {
        return fallback;
    }

    long result = fallback;
    CFNumberGetValue((CFNumberRef)value, kCFNumberLongType, &result);
    return result;
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

static int hex_nibble(int c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static int append_hex_bytes(const char *text, uint8_t *buffer, size_t capacity, size_t *length) {
    int high = -1;

    for (const char *p = text; *p; p++) {
        if (isspace((unsigned char)*p) || *p == ':' || *p == '-' || *p == ',') {
            continue;
        }

        if ((*p == 'x' || *p == 'X') && p > text && p[-1] == '0' && high == 0) {
            high = -1;
            continue;
        }

        int nibble = hex_nibble((unsigned char)*p);
        if (nibble < 0) {
            return 0;
        }

        if (high < 0) {
            high = nibble;
        } else {
            if (*length >= capacity) {
                return 0;
            }
            buffer[*length] = (uint8_t)((high << 4) | nibble);
            (*length)++;
            high = -1;
        }
    }

    return high < 0;
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
    printf("Usage: %s --payload HEX [--start] [--end] [--write --i-understand-this-writes]\n", argv0);
    puts("");
    puts("Dry-run by default. Sends one small 64-byte APOLLO61 feature report when --write is provided.");
    puts("");
    puts("Options:");
    puts("  --payload HEX                 Payload bytes, padded with zeroes to 64 bytes");
    puts("  --start                       Send 04 18 before payload");
    puts("  --end                         Send 04 02 after payload");
    puts("  --delay-ms N                  Wait after each write before reading, default 75");
    puts("  --write                       Actually send HID feature reports");
    puts("  --i-understand-this-writes    Required together with --write");
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

static int send_and_read(IOHIDDeviceRef device, const char *name, const uint8_t *payload, int delay_ms) {
    uint8_t response[REPORT_LEN];
    CFIndex report_length = REPORT_LEN;

    printf("%s send=", name);
    print_hex_prefix(payload, 16);
    puts(" ...");

    IOReturn set_result = IOHIDDeviceSetReport(device,
                                               kIOHIDReportTypeFeature,
                                               REPORT_ID,
                                               payload,
                                               REPORT_LEN);
    printf("%s set_result=0x%08x\n", name, set_result);
    if (set_result != kIOReturnSuccess) {
        return 0;
    }

    if (delay_ms > 0) {
        usleep((useconds_t)delay_ms * 1000);
    }

    memset(response, 0, sizeof(response));
    IOReturn get_result = IOHIDDeviceGetReport(device,
                                               kIOHIDReportTypeFeature,
                                               REPORT_ID,
                                               response,
                                               &report_length);
    printf("%s get_result=0x%08x response=", name, get_result);
    print_hex_prefix(response, 16);
    printf(" ... status=0x%02x (%s)\n", response[3], status_name(response[3]));
    return get_result == kIOReturnSuccess;
}

int main(int argc, char **argv) {
    Options options = {
        .vid = DEFAULT_VID,
        .pid = DEFAULT_PID,
        .usage_page = DEFAULT_USAGE_PAGE,
        .usage = DEFAULT_USAGE,
        .delay_ms = 75,
        .do_start = 0,
        .do_end = 0,
        .write_enabled = 0,
    };
    int acknowledged = 0;
    uint8_t payload[REPORT_LEN];
    size_t payload_length = 0;

    memset(payload, 0, sizeof(payload));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
            if (!append_hex_bytes(argv[++i], payload, sizeof(payload), &payload_length)) {
                fprintf(stderr, "Invalid payload hex: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--start") == 0) {
            options.do_start = 1;
        } else if (strcmp(argv[i], "--end") == 0) {
            options.do_end = 1;
        } else if (strcmp(argv[i], "--write") == 0) {
            options.write_enabled = 1;
        } else if (strcmp(argv[i], "--i-understand-this-writes") == 0) {
            acknowledged = 1;
        } else if (strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &options.delay_ms) || options.delay_ms > 5000) {
                fprintf(stderr, "Invalid delay: %s\n", argv[i]);
                return 2;
            }
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
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (payload_length == 0) {
        fprintf(stderr, "Missing --payload.\n");
        usage(argv[0]);
        return 2;
    }

    uint8_t start[REPORT_LEN] = {0x04, 0x18};
    uint8_t end[REPORT_LEN] = {0x04, 0x02};

    printf("target: vid=0x%04x pid=0x%04x usage=0x%04x:0x%04x report_id=%d length=%d delay_ms=%d\n",
           options.vid,
           options.pid,
           options.usage_page,
           options.usage,
           REPORT_ID,
           REPORT_LEN,
           options.delay_ms);

    if (!options.write_enabled) {
        if (options.do_start) {
            printf("start planned=");
            print_hex_prefix(start, 16);
            puts(" ...");
        }
        printf("payload planned=");
        print_hex_prefix(payload, 16);
        puts(" ...");
        if (options.do_end) {
            printf("end planned=");
            print_hex_prefix(end, 16);
            puts(" ...");
        }
        puts("dry-run: no HID writes were performed. Add --write --i-understand-this-writes to send.");
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
    if (options.do_start && !send_and_read(device, "start", start, options.delay_ms)) {
        ok = 0;
    }
    if (ok && !send_and_read(device, "payload", payload, options.delay_ms)) {
        ok = 0;
    }
    if (ok && options.do_end && !send_and_read(device, "end", end, options.delay_ms)) {
        ok = 0;
    }

    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return ok ? 0 : 1;
}
