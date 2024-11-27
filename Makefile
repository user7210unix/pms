include config.mk

pms: pms.c
	$(CC) $(CFLAGS) $(INCLUDES) pms.c -o pms $(LDFLAGS) -lcjson

clean:
	rm -f pms
