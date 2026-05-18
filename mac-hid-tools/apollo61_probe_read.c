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
    int write_enabled;
    uint8_t command[2];
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

static void usage(const char *argv0) {
    printf("Usage: %s --cmd HEX HEX [--write --i-understand-this-writes]\n", argv0);
    puts("");
    puts("Attempts to read a bulk payload from the keyboard.");
    puts("Sends a start (04 18), then the custom command prefix, then reads pages until closed.");
    puts("Example: --cmd 04 89");
    puts("");
    puts("Options:");
    puts("  --cmd HEX HEX                 The 2-byte command to trigger a read (e.g. 04 89)");
    puts("  --delay-ms N                  Wait after each write before reading, default 75");
    puts("  --write                       Actually send HID feature reports");
    puts("  --i-understand-this-writes    Required together with --write");
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
        fprintf(stderr, "Failed to open IOHIDManager\n");
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
    CFSetGetValues(devices, (const void **)device_list);
    *device_out = device_list[0];
    free(device_list);
    *manager_out = manager;
    *devices_out = devices;
    return 1;
}

static int send_report(IOHIDDeviceRef device, const char *name, const uint8_t *payload, int write_enabled) {
    printf("%s send=", name);
    print_hex_prefix(payload, 16);
    puts(" ...");

    if (!write_enabled) return 1;

    IOReturn set_result = IOHIDDeviceSetReport(device, kIOHIDReportTypeFeature, REPORT_ID, payload, REPORT_LEN);
    printf("%s set_result=0x%08x\n", name, set_result);
    return set_result == kIOReturnSuccess;
}

int main(int argc, char **argv) {
    Options options = {
        .vid = DEFAULT_VID,
        .pid = DEFAULT_PID,
        .usage_page = DEFAULT_USAGE_PAGE,
        .usage = DEFAULT_USAGE,
        .delay_ms = 75,
        .write_enabled = 0,
        .command = {0x04, 0x00}
    };
    int acknowledged = 0;
    int cmd_set = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cmd") == 0 && i + 2 < argc) {
            int b1, b2;
            if (!parse_hex_or_decimal(argv[++i], &b1) || !parse_hex_or_decimal(argv[++i], &b2)) {
                fprintf(stderr, "Invalid command bytes\n");
                return 2;
            }
            options.command[0] = (uint8_t)b1;
            options.command[1] = (uint8_t)b2;
            cmd_set = 1;
        } else if (strcmp(argv[i], "--write") == 0) {
            options.write_enabled = 1;
        } else if (strcmp(argv[i], "--i-understand-this-writes") == 0) {
            acknowledged = 1;
        } else if (strcmp(argv[i], "--delay-ms") == 0 && i + 1 < argc) {
            parse_hex_or_decimal(argv[++i], &options.delay_ms);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (!cmd_set) {
        usage(argv[0]);
        return 2;
    }

    if (options.write_enabled && !acknowledged) {
        fputs("Refusing to write: --write requires --i-understand-this-writes.\n", stderr);
        return 2;
    }

    IOHIDManagerRef manager = NULL;
    CFSetRef devices = NULL;
    IOHIDDeviceRef device = NULL;

    if (options.write_enabled) {
        if (!open_first_device(&options, &manager, &devices, &device)) return 1;
        char product[256];
        string_property(device, CFSTR(kIOHIDProductKey), product, sizeof(product));
        printf("Connected to %s\n", product);
    } else {
        puts("dry-run: no HID writes will be performed.");
    }

    uint8_t buf_start[REPORT_LEN] = {0x04, 0x18};
    send_report(device, "start", buf_start, options.write_enabled);
    if (options.delay_ms > 0) usleep(options.delay_ms * 1000);

    uint8_t buf_cmd[REPORT_LEN] = {0};
    buf_cmd[0] = options.command[0];
    buf_cmd[1] = options.command[1];
    send_report(device, "read_cmd", buf_cmd, options.write_enabled);
    if (options.delay_ms > 0) usleep(options.delay_ms * 1000);

    // Read loop
    if (options.write_enabled) {
        printf("Starting read loop...\n");
        for (int i = 0; i < 20; i++) {
            uint8_t response[REPORT_LEN] = {0};
            CFIndex length = REPORT_LEN;
            IOReturn get_result = IOHIDDeviceGetReport(device, kIOHIDReportTypeFeature, REPORT_ID, response, &length);
            
            if (get_result != kIOReturnSuccess) {
                printf("Read failed: 0x%08x\n", get_result);
                break;
            }
            
            printf("Read %d: ", i);
            print_hex_prefix(response, 16);
            printf(" ... status=0x%02x\n", response[3]);

            if (response[0] == 0x04 && response[1] == 0x02) {
                printf("End of transfer marker (04 02) received!\n");
                break;
            }
            if (response[3] == 0xff) {
                printf("Done marker received!\n");
                break;
            }
            usleep(50 * 1000);
        }
    }

    uint8_t buf_end[REPORT_LEN] = {0x04, 0x02};
    send_report(device, "end", buf_end, options.write_enabled);

    if (options.write_enabled) {
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
    }

    return 0;
}
