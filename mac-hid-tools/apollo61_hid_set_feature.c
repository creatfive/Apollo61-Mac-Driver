#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VID 0x05ac
#define DEFAULT_PID 0x024f
#define DEFAULT_USAGE_PAGE 0x0001
#define DEFAULT_USAGE 0x0006
#define DEFAULT_REPORT_ID 0
#define DEFAULT_LENGTH 64

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

static CFMutableDictionaryRef matching_dictionary(int vid, int pid, int usage_page, int usage) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    if (!dict) {
        return NULL;
    }

    set_number(dict, CFSTR(kIOHIDVendorIDKey), vid);
    set_number(dict, CFSTR(kIOHIDProductIDKey), pid);
    set_number(dict, CFSTR(kIOHIDPrimaryUsagePageKey), usage_page);
    set_number(dict, CFSTR(kIOHIDPrimaryUsageKey), usage);
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

static void print_hex(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02x", data[i]);
        if (i + 1 < length) {
            putchar(' ');
        }
    }
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

        if (*p == 'x' || *p == 'X') {
            if (p > text && p[-1] == '0' && high == 0) {
                high = -1;
                continue;
            }
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

static void usage(const char *argv0) {
    printf("Usage: %s --payload HEX [options]\n", argv0);
    puts("");
    puts("Options:");
    puts("  --payload HEX                 Payload bytes, for example: \"04 18\"");
    puts("  --length N                    Feature payload length, default 64");
    puts("  --report ID                   Feature report ID, default 0");
    puts("  --pad BYTE                    Pad byte for remaining payload, default 0");
    puts("  --write                       Actually send the feature report");
    puts("  --i-understand-this-writes    Required together with --write");
    puts("  --vid HEX --pid HEX           Default 0x05ac:0x024f");
    puts("  --usage-page HEX --usage HEX  Default 0x0001:0x0006");
    puts("");
    puts("Without --write this only prints the packet that would be sent.");
}

int main(int argc, char **argv) {
    int vid = DEFAULT_VID;
    int pid = DEFAULT_PID;
    int usage_page = DEFAULT_USAGE_PAGE;
    int usage_id = DEFAULT_USAGE;
    int report_id = DEFAULT_REPORT_ID;
    int requested_length = DEFAULT_LENGTH;
    int pad_byte = 0;
    int write_enabled = 0;
    int acknowledged = 0;
    uint8_t payload[4096];
    size_t payload_length = 0;

    memset(payload, 0, sizeof(payload));

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--payload") == 0 && i + 1 < argc) {
            if (!append_hex_bytes(argv[++i], payload, sizeof(payload), &payload_length)) {
                fprintf(stderr, "Invalid payload hex: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--vid") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &vid)) {
                fprintf(stderr, "Invalid VID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--pid") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &pid)) {
                fprintf(stderr, "Invalid PID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--usage-page") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &usage_page)) {
                fprintf(stderr, "Invalid usage page: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--usage") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &usage_id)) {
                fprintf(stderr, "Invalid usage: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &report_id)) {
                fprintf(stderr, "Invalid report ID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--length") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &requested_length) || requested_length < 1 || requested_length > (int)sizeof(payload)) {
                fprintf(stderr, "Invalid length: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--pad") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &pad_byte) || pad_byte > 0xff) {
                fprintf(stderr, "Invalid pad byte: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--write") == 0) {
            write_enabled = 1;
        } else if (strcmp(argv[i], "--i-understand-this-writes") == 0) {
            acknowledged = 1;
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

    if (payload_length > (size_t)requested_length) {
        fprintf(stderr, "Payload has %zu bytes but --length is %d.\n", payload_length, requested_length);
        return 2;
    }

    memset(payload + payload_length, pad_byte, (size_t)requested_length - payload_length);

    printf("target: vid=0x%04x pid=0x%04x usage=0x%04x:0x%04x report_id=%d length=%d\n",
           vid,
           pid,
           usage_page,
           usage_id,
           report_id,
           requested_length);
    printf("payload=");
    print_hex(payload, (size_t)requested_length);
    putchar('\n');

    if (!write_enabled) {
        puts("dry-run: no HID write was performed. Add --write --i-understand-this-writes to send it.");
        return 0;
    }

    if (!acknowledged) {
        fputs("Refusing to write: --write requires --i-understand-this-writes.\n", stderr);
        return 2;
    }

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        fprintf(stderr, "Failed to create IOHIDManager\n");
        return 2;
    }

    CFMutableDictionaryRef dict = matching_dictionary(vid, pid, usage_page, usage_id);
    if (!dict) {
        CFRelease(manager);
        fprintf(stderr, "Failed to create matching dictionary\n");
        return 2;
    }

    IOHIDManagerSetDeviceMatching(manager, dict);
    CFRelease(dict);

    IOReturn open_result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (open_result != kIOReturnSuccess) {
        fprintf(stderr, "Failed to open IOHIDManager: 0x%08x\n", open_result);
        CFRelease(manager);
        return 2;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices || CFSetGetCount(devices) == 0) {
        printf("No matching HID device found.\n");
        if (devices) {
            CFRelease(devices);
        }
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 1;
    }

    CFIndex device_count = CFSetGetCount(devices);
    IOHIDDeviceRef *device_list = calloc((size_t)device_count, sizeof(IOHIDDeviceRef));
    if (!device_list) {
        fprintf(stderr, "Out of memory\n");
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 2;
    }

    CFSetGetValues(devices, (const void **)device_list);
    IOHIDDeviceRef device = device_list[0];

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

    IOReturn result = IOHIDDeviceSetReport(device,
                                           kIOHIDReportTypeFeature,
                                           (CFIndex)report_id,
                                           payload,
                                           (CFIndex)requested_length);

    printf("set_feature result=0x%08x\n", result);

    free(device_list);
    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return result == kIOReturnSuccess ? 0 : 1;
}
