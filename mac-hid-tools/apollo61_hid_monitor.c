#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VID 0x05ac
#define DEFAULT_PID 0x024f
#define DEFAULT_SECONDS 20

typedef struct MonitorContext {
    IOHIDManagerRef manager;
    uint8_t *buffer;
    CFIndex buffer_size;
    int report_filter;
    int seen_reports;
} MonitorContext;

static volatile sig_atomic_t g_should_stop = 0;

static void handle_signal(int signal_number) {
    (void)signal_number;
    g_should_stop = 1;
    CFRunLoopStop(CFRunLoopGetCurrent());
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

static CFMutableDictionaryRef matching_dictionary(int vid, int pid) {
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    if (!dict) {
        return NULL;
    }

    CFNumberRef vendor = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid);
    CFNumberRef product = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &pid);

    if (vendor) {
        CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey), vendor);
        CFRelease(vendor);
    }

    if (product) {
        CFDictionarySetValue(dict, CFSTR(kIOHIDProductIDKey), product);
        CFRelease(product);
    }

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

static void input_report_callback(void *context,
                                  IOReturn result,
                                  void *sender,
                                  IOHIDReportType type,
                                  uint32_t report_id,
                                  uint8_t *report,
                                  CFIndex report_length) {
    (void)sender;
    (void)type;

    MonitorContext *monitor = (MonitorContext *)context;
    if (result != kIOReturnSuccess) {
        fprintf(stderr, "input callback error: 0x%08x\n", result);
        return;
    }

    if (monitor->report_filter >= 0 && (int)report_id != monitor->report_filter) {
        return;
    }

    monitor->seen_reports++;
    printf("report_id=%u length=%ld data=", report_id, report_length);
    print_hex(report, report_length);
    putchar('\n');
    fflush(stdout);
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
    printf("Usage: %s [--vid HEX] [--pid HEX] [--report ID] [--seconds N]\n", argv0);
    puts("");
    puts("Default target: VID 0x05ac PID 0x024f, all report IDs, 20 seconds.");
}

int main(int argc, char **argv) {
    int vid = DEFAULT_VID;
    int pid = DEFAULT_PID;
    int seconds = DEFAULT_SECONDS;
    int report_filter = -1;

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
        } else if (strcmp(argv[i], "--report") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &report_filter)) {
                fprintf(stderr, "Invalid report ID: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            if (!parse_hex_or_decimal(argv[++i], &seconds) || seconds < 1 || seconds > 3600) {
                fprintf(stderr, "Invalid seconds: %s\n", argv[i]);
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

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        fprintf(stderr, "Failed to create IOHIDManager\n");
        return 2;
    }

    CFMutableDictionaryRef dict = matching_dictionary(vid, pid);
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
        printf("No matching HID device found for VID 0x%04x PID 0x%04x.\n", vid, pid);
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
    string_property(device, CFSTR(kIOHIDProductKey), product, sizeof(product));
    string_property(device, CFSTR(kIOHIDManufacturerKey), manufacturer, sizeof(manufacturer));

    long max_input = number_property(device, CFSTR(kIOHIDMaxInputReportSizeKey), 64);
    if (max_input < 1) {
        max_input = 64;
    }

    MonitorContext context = {
        .manager = manager,
        .buffer = calloc((size_t)max_input, sizeof(uint8_t)),
        .buffer_size = max_input,
        .report_filter = report_filter,
        .seen_reports = 0,
    };

    if (!context.buffer) {
        fprintf(stderr, "Out of memory\n");
        free(device_list);
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 2;
    }

    printf("Monitoring %s / %s at 0x%04x:0x%04x for %d second(s)",
           manufacturer,
           product,
           vid,
           pid,
           seconds);
    if (report_filter >= 0) {
        printf(", report_id=%d", report_filter);
    }
    puts(".");
    puts("Press keys or use keyboard controls now. Ctrl-C stops early.");
    fflush(stdout);

    IOHIDDeviceRegisterInputReportCallback(device,
                                           context.buffer,
                                           context.buffer_size,
                                           input_report_callback,
                                           &context);
    IOHIDManagerScheduleWithRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);

    if (!g_should_stop) {
        CFRunLoopStop(CFRunLoopGetCurrent());
    }

    printf("Done. Captured %d report(s).\n", context.seen_reports);

    IOHIDManagerUnscheduleFromRunLoop(manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    free(context.buffer);
    free(device_list);
    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return 0;
}
