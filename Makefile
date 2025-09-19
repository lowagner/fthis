BUILD_DIR=build

$(BUILD_DIR)/wthis: src/main.c $(BUILD_DIR)
	$(CC) -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
