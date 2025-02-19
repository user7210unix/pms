# See LICENSE file for copyright and license details.

include config.mk

pms: pms.o
	$(CC) $(CFLAGS) $(INCLUDES) pms.o -o $(TARGET) $(LDFLAGS) -ltoml -lcurl
	@rm pms.o

config.h:
	cp config.def.h $@

pms.o: pms.c config.h
	$(CC) $(CFLAGS) -c pms.c

repo.o: repo.c repo.h config.h
	# $(CC) $(CFLAGS) -c repo.c
	@echo "repo.c is not implemented yet"

options:
	@echo pms build options:
	@echo "CFLAGS      = $(CFLAGS)"
	@echo "LDFLAGS     = $(LDFLAGS)"
	@echo "CC          = $(CC)"

clean:
	@rm -f $(TARGET)

install:
	cp $(TARGET) $(PREFIX)/bin/

release:
	@mkdir -p pms-$(VERSION)
	@cp -r *.c *.h Makefile config.mk LICENSE README.md pms-$(VERSION)/
	@tar -czf pms-$(VERSION).src.tar.xz pms-$(VERSION)
	@rm -rf pms-$(VERSION)

release-clean:
	@rm -f pms-$(VERSION).src.tar.xz
	@rm -rf pms-$(VERSION)
