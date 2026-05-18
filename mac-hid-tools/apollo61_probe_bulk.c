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
    const char *filename;
    int cmd;
    int do_start;
    int do_f0;
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
    if (buffer_size == 0) return;
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

static void print_hex_prefix(const uint8_t *data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        printf("%02x", data[i]);
        if (i + 1 < length) putchar(' ');
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
    printf("Usage: %s --file FILENAME --cmd HEX [--start] [--f0] [--write --i-understand-this-writes]\n", argv0);
    puts("");
    puts("Sends bulk APOLLO61 feature reports.");
    puts("Options:");
    puts("  --file FILENAME               Binary payload file to send");
    puts("  --cmd HEX                     The command byte (e.g. 0x11, 0x13, 0x15, 0x17)");
    puts("  --start                       Prepend '04 18' (needed for 11, 13, 17)");
    puts("  --f0                          Append '04 f0' (needed for 11, 13)");
    puts("  --delay-ms N                  Wait after each write before reading, default 75");
    puts("  --write                       Actually send HID feature reports");
    puts("  --i-understand-this-writes    Required together with --write");
}

static int open_first_device(const Options *options, IOHIDManagerRef *manager_out, CFSetRef *devices_out, IOHIDDeviceRef *device_out) {
    IOHIDManagerRef manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (!manager) return 0;
    CFMutableDictionaryRef dict = matching_dictionary(options);
    if (!dict) { CFRelease(manager); return 0; }
    IOHIDManagerSetDeviceMatching(manager, dict);
    CFRelease(dict);
    if (IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) { CFRelease(manager); return 0; }
    CFSetRef devices = IOHIDManagerCopyDevices(manager);
    if (!devices || CFSetGetCount(devices) == 0) {
        if (devices) CFRelease(devices);
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

static int send_and_read(IOHIDDeviceRef device, const char *name, const uint8_t *payload, int delay_ms, int write_enabled) {
    uint8_t response[REPORT_LEN];
    CFIndex report_length = REPORT_LEN;
    printf("%s send=", name);
    print_hex_prefix(payload, 16);
    puts(" ...");

    if (!write_enabled) return 1;

    IOReturn set_result = IOHIDDeviceSetReport(device, kIOHIDReportTypeFeature, REPORT_ID, payload, REPORT_LEN);
    printf("%s set_result=0x%08x\n", name, set_result);
    if (set_result != kIOReturnSuccess) return 0;

    if (delay_ms > 0) usleep((useconds_t)delay_ms * 1000);

    memset(response, 0, sizeof(response));
    IOReturn get_result = IOHIDDeviceGetReport(device, kIOHIDReportTypeFeature, REPORT_ID, response, &report_length);
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
        .write_enabled = 0,
        .filename = NULL,
        .cmd = 0,
        .do_start = 0,
        .do_f0 = 0
    };
    int acknowledged = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            options.filename = argv[++i];
        } else if (strcmp(argv[i], "--cmd") == 0 && i + 1 < argc) {
            parse_hex_or_decimal(argv[++i], &options.cmd);
        } else if (strcmp(argv[i], "--start") == 0) {
            options.do_start = 1;
        } else if (strcmp(argv[i], "--f0") == 0) {
            options.do_f0 = 1;
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

    if (!options.filename || !options.cmd) {
        usage(argv[0]);
        return 2;
    }
    if (options.write_enabled && !acknowledged) {
        fputs("Refusing to write: --write requires --i-understand-this-writes.\n", stderr);
        return 2;
    }

    FILE *f = fopen(options.filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file\n");
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *file_data = malloc((size_t)file_size);
    if (fread(file_data, 1, (size_t)file_size, f) != (size_t)file_size) {
        free(file_data); fclose(f); return 2;
    }
    fclose(f);

    size_t page_count = (file_size + REPORT_LEN - 1) / REPORT_LEN;
    if (page_count > 0xff) {
        fprintf(stderr, "File too large\n");
        free(file_data); return 2;
    }

    printf("file: size=%ld page_count=%zu cmd=0x%02x\n", file_size, page_count, options.cmd);

    IOHIDManagerRef manager = NULL; CFSetRef devices = NULL; IOHIDDeviceRef device = NULL;
    if (options.write_enabled) {
        if (!open_first_device(&options, &manager, &devices, &device)) { free(file_data); return 1; }
    } else {
        puts("dry-run: no HID writes will be performed.");
    }

    int ok = 1;
    
    // 1. Optional 04 18 start
    if (options.do_start && ok) {
        uint8_t buf_start[REPORT_LEN] = {0x04, 0x18};
        ok = send_and_read(device, "start (04 18)", buf_start, options.delay_ms, options.write_enabled);
    }
    
    // 2. Command with page count at byte 8 (if not 04 19)
    if (options.cmd == 0x15) {
        // The original 04 19 + 04 15 logic
        uint8_t buf_19[REPORT_LEN] = {0x04, 0x19};
        if (ok) ok = send_and_read(device, "setup (04 19)", buf_19, options.delay_ms, options.write_enabled);
        
        uint8_t buf_cmd[REPORT_LEN] = {0x04, 0x15};
        buf_cmd[8] = (uint8_t)page_count;
        if (ok) ok = send_and_read(device, "page_count (04 15)", buf_cmd, options.delay_ms, options.write_enabled);
    } else {
        // The newer 04 11 / 04 13 / 04 17 logic
        uint8_t buf_cmd[REPORT_LEN] = {0x04, (uint8_t)options.cmd};
        buf_cmd[8] = (uint8_t)page_count;
        if (ok) ok = send_and_read(device, "cmd_header", buf_cmd, options.delay_ms, options.write_enabled);
    }

    // 3. Pages
    for (size_t i = 0; i < page_count && ok; i++) {
        uint8_t buf_page[REPORT_LEN] = {0};
        size_t offset = i * REPORT_LEN;
        size_t len = file_size - offset;
        if (len > REPORT_LEN) len = REPORT_LEN;
        memcpy(buf_page, file_data + offset, len);
        char name[64]; snprintf(name, sizeof(name), "page %zu/%zu", i + 1, page_count);
        ok = send_and_read(device, name, buf_page, options.delay_ms, options.write_enabled);
    }

    // 4. End 04 02
    if (ok) {
        uint8_t buf_end[REPORT_LEN] = {0x04, 0x02};
        ok = send_and_read(device, "end (04 02)", buf_end, options.delay_ms, options.write_enabled);
    }
    
    // 5. Optional 04 f0 end
    if (options.do_f0 && ok) {
        uint8_t buf_f0[REPORT_LEN] = {0x04, 0xf0};
        ok = send_and_read(device, "f0_commit (04 f0)", buf_f0, options.delay_ms, options.write_enabled);
    }

    if (options.write_enabled) {
        CFRelease(devices);
        IOHIDManagerClose(manager, kIOHIDOptionsTypeNone);
        CFRelease(manager);
    }

    free(file_data);
    return ok ? 0 : 1;
}
