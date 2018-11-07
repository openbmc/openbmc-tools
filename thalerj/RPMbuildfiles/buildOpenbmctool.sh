#!/usr/bin/bash
echo "Version: ex 1.0"
read version
echo "Release: ex 4"
read release
mkdir -p /tmp/openbmctool-$version-$release
rm -rf /tmp/openbmctool-$version-$release/*
cp ../* /tmp/openbmctool-$version-$release
tar -cvzf /root/rpmbuild/SOURCES/openbmctool-$version-$release.tgz -C /tmp openbmctool-$version-$release
rpmbuild -ba --define "_version $version" --define "_release $release" /root/rpmbuild/SPECS/openbmctool.spec
cp /root/rpmbuild/RPMS/noarch/openbmctool-$version-$release.noarch.rpm /tmp/openbmctool-$version-$release/

