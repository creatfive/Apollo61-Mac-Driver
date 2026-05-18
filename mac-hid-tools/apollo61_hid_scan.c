#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_VID 0x05ac
#define DEFAULT_PID 0x024f

typedef struct Options {
    int scan_all;
    int verbose_elements;
    int vid;
    int pid;
} Options;

typedef struct ReportSummary {
    IOHIDElementType type;
    uint32_t report_id;
    uint32_t report_size;
    uint32_t report_count;
    uint32_t element_count;
} ReportSummary;

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

static const char *element_type_name(IOHIDElementType type) {
    switch (type) {
        case kIOHIDElementTypeInput_Misc: return "input-misc";
        case kIOHIDElementTypeInput_Button: return "input-button";
        case kIOHIDElementTypeInput_Axis: return "input-axis";
        case kIOHIDElementTypeInput_ScanCodes: return "input-scancodes";
        case kIOHIDElementTypeOutput: return "output";
        case kIOHIDElementTypeFeature: return "feature";
        case kIOHIDElementTypeCollection: return "collection";
        default: return "other";
    }
}

static void print_interesting_elements(IOHIDDeviceRef device) {
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
    if (!elements) {
        puts("  elements: unavailable");
        return;
    }

    CFIndex count = CFArrayGetCount(elements);
    printf("  elements: %ld\n", count);

    int printed = 0;
    for (CFIndex i = 0; i < count; i++) {
        IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        if (!element) {
            continue;
        }

        IOHIDElementType type = IOHIDElementGetType(element);
        if (type != kIOHIDElementTypeFeature &&
            type != kIOHIDElementTypeOutput &&
            type != kIOHIDElementTypeInput_Misc &&
            type != kIOHIDElementTypeInput_Button &&
            type != kIOHIDElementTypeInput_Axis &&
            type != kIOHIDElementTypeInput_ScanCodes) {
            continue;
        }

        uint32_t usage_page = IOHIDElementGetUsagePage(element);
        uint32_t usage = IOHIDElementGetUsage(element);
        uint32_t report_id = IOHIDElementGetReportID(element);
        uint32_t report_size = IOHIDElementGetReportSize(element);
        uint32_t report_count = IOHIDElementGetReportCount(element);

        printf("    %-15s usage=0x%04x:0x%04x report_id=%u bits=%u count=%u\n",
               element_type_name(type),
               usage_page,
               usage,
               report_id,
               report_size,
               report_count);

        printed++;
        if (printed >= 80) {
            printf("    ... truncated after %d interesting elements\n", printed);
            break;
        }
    }

    CFRelease(elements);
}

static void add_summary(ReportSummary *summaries,
                        size_t *summary_count,
                        size_t summary_capacity,
                        IOHIDElementType type,
                        uint32_t report_id,
                        uint32_t report_size,
                        uint32_t report_count) {
    for (size_t i = 0; i < *summary_count; i++) {
        if (summaries[i].type == type &&
            summaries[i].report_id == report_id &&
            summaries[i].report_size == report_size &&
            summaries[i].report_count == report_count) {
            summaries[i].element_count++;
            return;
        }
    }

    if (*summary_count >= summary_capacity) {
        return;
    }

    summaries[*summary_count] = (ReportSummary){
        .type = type,
        .report_id = report_id,
        .report_size = report_size,
        .report_count = report_count,
        .element_count = 1,
    };
    (*summary_count)++;
}

static void print_report_summary(IOHIDDeviceRef device) {
    CFArrayRef elements = IOHIDDeviceCopyMatchingElements(device, NULL, kIOHIDOptionsTypeNone);
    if (!elements) {
        puts("  report summary: unavailable");
        return;
    }

    ReportSummary summaries[256];
    size_t summary_count = 0;
    memset(summaries, 0, sizeof(summaries));

    CFIndex count = CFArrayGetCount(elements);
    for (CFIndex i = 0; i < count; i++) {
        IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        if (!element) {
            continue;
        }

        IOHIDElementType type = IOHIDElementGetType(element);
        if (type != kIOHIDElementTypeFeature &&
            type != kIOHIDElementTypeOutput &&
            type != kIOHIDElementTypeInput_Misc &&
            type != kIOHIDElementTypeInput_Button &&
            type != kIOHIDElementTypeInput_Axis &&
            type != kIOHIDElementTypeInput_ScanCodes) {
            continue;
        }

        add_summary(summaries,
                    &summary_count,
                    sizeof(summaries) / sizeof(summaries[0]),
                    type,
                    IOHIDElementGetReportID(element),
                    IOHIDElementGetReportSize(element),
                    IOHIDElementGetReportCount(element));
    }

    puts("  report summary:");
    for (size_t i = 0; i < summary_count; i++) {
        printf("    %-15s report_id=%u bits=%u count=%u elements=%u\n",
               element_type_name(summaries[i].type),
               summaries[i].report_id,
               summaries[i].report_size,
               summaries[i].report_count,
               summaries[i].element_count);
    }

    CFRelease(elements);
}

static int inspect_devices(Options options) {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) {
        fprintf(stderr, "Failed to create IOHIDManager\n");
        return 2;
    }

    if (options.scan_all) {
        IOHIDManagerSetDeviceMatching(manager, NULL);
    } else {
        CFMutableDictionaryRef dict = matching_dictionary(options.vid, options.pid);
        if (!dict) {
            CFRelease(manager);
            fprintf(stderr, "Failed to create matching dictionary\n");
            return 2;
        }
        IOHIDManagerSetDeviceMatching(manager, dict);
        CFRelease(dict);
    }

    IOReturn open_result = IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (open_result != kIOReturnSuccess) {
        fprintf(stderr, "Failed to open IOHIDManager: 0x%08x\n", open_result);
        CFRelease(manager);
        return 2;
    }

    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices || CFSetGetCount(devices) == 0) {
        if (options.scan_all) {
            puts("No HID devices found.");
        } else {
            printf("No matching HID device found for VID 0x%04x PID 0x%04x.\n",
                   options.vid,
                   options.pid);
            puts("Try: ./apollo61_hid_scan --all");
        }

        if (devices) {
            CFRelease(devices);
        }
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 1;
    }

    CFIndex count = CFSetGetCount(devices);
    IOHIDDeviceRef *device_list = calloc((size_t)count, sizeof(IOHIDDeviceRef));
    if (!device_list) {
        fprintf(stderr, "Out of memory\n");
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
        return 2;
    }

    CFSetGetValues(devices, (const void **)device_list);
    printf("Found %ld HID device(s)%s.\n", count, options.scan_all ? "" : " matching filter");

    for (CFIndex i = 0; i < count; i++) {
        IOHIDDeviceRef device = device_list[i];
        char product[256];
        char manufacturer[256];
        char serial[256];
        char transport[256];

        string_property(device, CFSTR(kIOHIDProductKey), product, sizeof(product));
        string_property(device, CFSTR(kIOHIDManufacturerKey), manufacturer, sizeof(manufacturer));
        string_property(device, CFSTR(kIOHIDSerialNumberKey), serial, sizeof(serial));
        string_property(device, CFSTR(kIOHIDTransportKey), transport, sizeof(transport));

        long vid = number_property(device, CFSTR(kIOHIDVendorIDKey), -1);
        long pid = number_property(device, CFSTR(kIOHIDProductIDKey), -1);
        long version = number_property(device, CFSTR(kIOHIDVersionNumberKey), -1);
        long primary_usage_page = number_property(device, CFSTR(kIOHIDPrimaryUsagePageKey), -1);
        long primary_usage = number_property(device, CFSTR(kIOHIDPrimaryUsageKey), -1);
        long max_input = number_property(device, CFSTR(kIOHIDMaxInputReportSizeKey), -1);
        long max_output = number_property(device, CFSTR(kIOHIDMaxOutputReportSizeKey), -1);
        long max_feature = number_property(device, CFSTR(kIOHIDMaxFeatureReportSizeKey), -1);

        printf("\n[%ld]\n", i + 1);
        printf("  product:      %s\n", product);
        printf("  manufacturer: %s\n", manufacturer);
        printf("  serial:       %s\n", serial);
        printf("  transport:    %s\n", transport);
        printf("  vid:pid:      0x%04lx:0x%04lx\n", vid, pid);
        printf("  version:      0x%04lx\n", version);
        printf("  primary:      usage=0x%04lx:0x%04lx\n", primary_usage_page, primary_usage);
        printf("  report sizes: input=%ld output=%ld feature=%ld bytes\n",
               max_input,
               max_output,
               max_feature);

        print_report_summary(device);
        if (options.verbose_elements) {
            print_interesting_elements(device);
        }
    }

    free(device_list);
    CFRelease(devices);
    IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
    CFRelease(manager);
    return 0;
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
    printf("Usage: %s [--all] [--elements] [--vid HEX] [--pid HEX]\n", argv0);
    puts("");
    puts("Default filter: --vid 0x05ac --pid 0x024f");
}

int main(int argc, char **argv) {
    Options options = {
        .scan_all = 0,
        .verbose_elements = 0,
        .vid = DEFAULT_VID,
        .pid = DEFAULT_PID,
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--all") == 0) {
            options.scan_all = 1;
        } else if (strcmp(argv[i], "--elements") == 0) {
            options.verbose_elements = 1;
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
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    return inspect_devices(options);
}
