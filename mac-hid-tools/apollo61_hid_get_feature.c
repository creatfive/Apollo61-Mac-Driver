#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VID 0x05ac
#define DEFAULT_PID 0x024f
#define DEFAULT_USAGE_PAGE 0x0001
#define DEFAULT_USAGE 0x0006

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

static void print_hex(const uint8_t *data, CFIndex length) {
    for (CFIndex i = 0; i < length; i++) {
        printf("%02x", data[i]);
        if (i + 1 < length) {
            putchar(' ');
        }
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

static void usage(const char *argv0) {
    printf("Usage: %s [--vid HEX] [--pid HEX] [--usage-page HEX] [--usage HEX] [--report ID] [--length N]\n", argv0);
    puts("");
    puts("Default target: VID 0x05ac PID 0x024f usage 0x0001:0x0006 report 0.");
}

int main(int argc, char **argv) {
    int vid = DEFAULT_VID;
    int pid = DEFAULT_PID;
    int usage_page = DEFAULT_USAGE_PAGE;
    int usage_id = DEFAULT_USAGE;
    int report_id = 0;
    int requested_length = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--vid") == 0 && i + 1 < argc) {
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
            if (!parse_hex_or_decimal(argv[++i], &requested_length) || requested_length < 1 || requested_length > 4096) {
                fprintf(stderr, "Invalid length: %s\n", argv[i]);
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
        printf("No matching HID device found for VID 0x%04x PID 0x%04x usage 0x%04x:0x%04x.\n",
               vid,
               pid,
               usage_page,
               usage_id);
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
    CFIndex length = requested_length > 0 ? requested_length : max_feature;
    if (length < 1) {
        length = 64;
    }

    uint8_t *buffer = calloc((size_t)length, sizeof(uint8_t));
    if (!buffer) {
        fprintf(stderr, "Out of memory\n");
        free(device_list);
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 2;
    }

    CFIndex report_length = length;
    IOReturn result = IOHIDDeviceGetReport(device,
                                           kIOHIDReportTypeFeature,
                                           (CFIndex)report_id,
                                           buffer,
                                           &report_length);

    printf("device: %s / %s transport=%s usage=0x%04x:0x%04x max_feature=%ld\n",
           manufacturer,
           product,
           transport,
           usage_page,
           usage_id,
           max_feature);
    printf("get_feature report_id=%d requested=%ld result=0x%08x returned=%ld\n",
           report_id,
           length,
           result,
           report_length);

    if (result == kIOReturnSuccess) {
        printf("data=");
        print_hex(buffer, report_length);
        putchar('\n');
    }

    free(buffer);
    free(device_list);
    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return result == kIOReturnSuccess ? 0 : 1;
}
