# pms - pack my sh*t
# See LICENSE file for copyright and license details.

include config.mk

pms: pms.c
	$(CC) $(CFLAGS) $(INCLUDES) pms.c -o $(TARGET) $(LDFLAGS) -lcjson -lcurl

options:
	@echo pms build options:
	@echo "CFLAGS      = $(CFLAGS)"
	@echo "LDFLAGS     = $(LDFLAGS)"
	@echo "CC          = $(CC)"

clean:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(TARGET)

install:
	cp $(TARGET) $(PREFIX)/bin/

release:
	git archive master --prefix=pms-$(VERSION)/ > pms-$(VERSION).tar
	@rm -rf pms-$(VERSION)
	tar -xf pms-$(VERSION).tar
	@rm pms-$(VERSION).tar
	cd pms-$(VERSION) && make clean
	tar -cf - pms-$(VERSION) | xz > pms-$(VERSION).src.tar.xz
	@rm -rf pms-$(VERSION)

release-clean:
	rm -f pms-$(VERSION).src.tar.xz
	rm -rf pms-$(VERSION)
