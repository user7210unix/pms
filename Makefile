include config.mk

pms: pms.c
	$(CC) $(CFLAGS) $(INCLUDES) pms.c -o $(TARGET) $(LDFLAGS) -lcjson -lcurl

clean:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(PREFIX)/bin/
