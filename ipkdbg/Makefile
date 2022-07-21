all: ipkdbg

ARCH_PPC64LE_BINS=bin/ppc64le/ubuntu/20.04/opkg
ARCH_X86_64_BINS=bin/x86_64/fedora/35/opkg \
		 bin/x86_64/fedora/36/opkg \
		 bin/x86_64/rhel/7.9/opkg \
		 bin/x86_64/rhel/8.6/opkg \
		 bin/x86_64/ubuntu/20.04/opkg \
		 bin/x86_64/ubuntu/21.10/opkg \
		 bin/x86_64/ubuntu/22.04/opkg \

SOURCE_DATE_EPOCH=$(shell git log --format='%at' -n1 -- `git ls-files bin`)

STRIPPED_ARCH_PPC64LE_BINS=$(patsubst %,stripped/%,$(ARCH_PPC64LE_BINS))
STRIPPED_ARCH_X86_64_BINS=$(patsubst %,stripped/%,$(ARCH_X86_64_BINS))

STRIPPED_BINS=$(STRIPPED_ARCH_PPC64LE_BINS) \
	      $(STRIPPED_ARCH_X86_64_BINS)

$(STRIPPED_ARCH_PPC64LE_BINS): stripped/%: %
	mkdir -p $(dir $@) && \
	powerpc64le-linux-gnu-strip --strip-all -o $@ $<

$(STRIPPED_ARCH_X86_64_BINS): stripped/%: %
	mkdir -p $(dir $@) && \
	x86_64-linux-gnu-strip --strip-all -o $@ $<

# https://reproducible-builds.org/docs/archives/
# https://www.gnu.org/software/tar/manual/html_section/create-options.html
bin.tar.gz: $(STRIPPED_BINS)
	tar \
		--sort=name \
		--clamp-mtime \
		--mtime="@${SOURCE_DATE_EPOCH}" \
		--owner=0 --group=0 --numeric-owner \
		--pax-option=exthdr.name=%d/PaxHeaders/%f,delete=atime,delete=ctime \
		-czf $@ \
		-C stripped $(patsubst stripped/%,%,$^)

bin.tar.gz.b64: bin.tar.gz
	base64 $^ > $@

ipkdbg: ipkdbg.in bin.tar.gz.b64
	cat $^ > $@
	chmod +x $@

.PHONY: clean
clean:
	$(RM) -r stripped
	$(RM) bin.tar.gz bin.tar.gz.b64 ipkdbg
