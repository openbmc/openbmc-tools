## Building an `opkg` binary for an OS

### libarchive
```
wget http://libarchive.org/downloads/libarchive-3.5.2.tar.gz
tar -xvf libarchive-3.5.2.tar.gz
./configure \
	--without-zlib \
	--without-bz2lib \
	--without-libb2 \
	--without-lz4 \
	--without-zstd \
	--without-lzo2 \
	--without-cng \
	--without-nettle \
	--without-xml2 \
	--without-expat \
	--disable-acl \
	--disable-xattr \
	--enable-posix-regex-lib=libc \
	--disable-rpath \
	--disable-bsdcat \
	--disable-bsdtar \
	--disable-bsdcpio \
	--with-pic
mkdir -p root && make -j$(nproc) install DESTDIR=$(realpath root)
```

### curl

```
wget https://curl.haxx.se/download/curl-7.79.1.tar.bz2
tar -xvf curl-7.79.1.tar.bz2
./configure --with-openssl
mkdir -p root && make -j$(nproc) install DESTDIR=$(realpath root)
```

### opkg

```
wget http://downloads.yoctoproject.org/releases/opkg/opkg-0.4.5.tar.gz
tar -xvf opkg-0.4.5.tar.gz

AR_FLAGS=Tcru \
PKG_CONFIG_PATH=$(realpath ../libarchive-3.5.2/root/usr/local/lib/pkgconfig/):$(realpath ../curl-7.79.1/root/usr/local/lib/pkgconfig/) \
CURL_CFLAGS=-I$(realpath ../curl-7.79.1/root/usr/local/include/) \
CURL_LIBS=$(realpath ../curl-7.79.1/root/usr/local/lib/libcurl.a) \
LIBARCHIVE_CFLAGS=-I$(realpath ../libarchive-3.5.2/root/usr/local/include/) \
LIBARCHIVE_LIBS=$(realpath ../libarchive-3.5.2/root/usr/local/lib/libarchive.a) \
LIBS="-llzma -lldap -llber -lz -pthread" \
./configure \
	--with-static-libopkg \
	--without-libsolv \
	--enable-curl \
	--enable-openssl \
	--disable-gpg \
	--disable-dependency-tracking
```
