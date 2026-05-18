CC ?= clang
CFLAGS ?= -Wall -Wextra -O2
FRAMEWORKS = -framework IOKit -framework CoreFoundation

BIN = apollo61_hid_scan apollo61_hid_monitor apollo61_hid_get_feature apollo61_hid_set_feature apollo61_probe_handshake apollo61_probe_command apollo61_probe_bulk apollo61_probe_read

.PHONY: all clean

all: $(BIN)

apollo61_hid_scan: apollo61_hid_scan.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_hid_monitor: apollo61_hid_monitor.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_hid_get_feature: apollo61_hid_get_feature.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_hid_set_feature: apollo61_hid_set_feature.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_probe_handshake: apollo61_probe_handshake.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_probe_command: apollo61_probe_command.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_probe_bulk: apollo61_probe_bulk.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

apollo61_probe_read: apollo61_probe_read.c
	$(CC) $(CFLAGS) $< -o $@ $(FRAMEWORKS)

clean:
	rm -f $(BIN)
