#!/usr/bin/env python3

import base64
import hashlib
import importlib.util
import io
import json
import os
import shutil
import subprocess
import sys
import tarfile
import tempfile
import types
import unittest
from unittest import mock

import webui_vue_npm_recipes as recipes

MIT_TEXT = b"Permission is hereby granted, for the test.\n"

AUTOBUMP = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "openbmc-autobump.py"
)


def integrity_for(payload, algo="sha512"):
    digest = hashlib.new(algo, payload).digest()
    return algo + "-" + base64.b64encode(digest).decode("ascii")


def make_tarball(files):
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w:gz") as archive:
        for name, data in files.items():
            info = tarfile.TarInfo(name)
            info.size = len(data)
            archive.addfile(info, io.BytesIO(data))
    return buffer.getvalue()


def package_tarball(
    name, license_name="MIT", license_file=True, root="package"
):
    files = {
        root
        + "/package.json": json.dumps({"license": license_name}).encode(
            "utf-8"
        ),
    }
    if license_file:
        files[root + "/LICENSE"] = MIT_TEXT
    return make_tarball(files)


def git(cwd, *args):
    """Run git in a test repository without the user's hooks or signing."""
    command = ["git"]
    for setting in (
        "user.name=Test",
        "user.email=test@example.test",
        "commit.gpgsign=false",
        "core.hooksPath=/dev/null",
    ):
        command += ["-c", setting]
    result = subprocess.run(
        command + list(args), cwd=cwd, check=True, capture_output=True
    )
    return result.stdout.decode("utf-8")


def load_autobump():
    """Import openbmc-autobump.py with a stand-in for the sh module."""

    class Command:
        def bake(self, *args, **kwargs):
            return self

        def __call__(self, *args, **kwargs):
            return FakeResult()

    stub = types.ModuleType("sh")
    stub.git = Command()
    stub.ssh = Command()
    stub.echo = str
    stub.ErrorReturnCode = type("ErrorReturnCode", (Exception,), {})
    stub.ErrorReturnCode_1 = type(
        "ErrorReturnCode_1", (stub.ErrorReturnCode,), {}
    )
    spec = importlib.util.spec_from_file_location("openbmc_autobump", AUTOBUMP)
    module = importlib.util.module_from_spec(spec)
    with mock.patch.dict(sys.modules, {"sh": stub}):
        spec.loader.exec_module(module)
    return module


class WebuiVueNpmRecipesTest(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()
        self.licenses = os.path.join(self.tmpdir, "common-licenses")
        os.makedirs(self.licenses)
        with open(os.path.join(self.licenses, "MIT"), "wb") as handle:
            handle.write(MIT_TEXT)
        self.outdir = os.path.join(self.tmpdir, "out")
        self.payloads = {}

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def fetch(self, url):
        if url not in self.payloads:
            raise AssertionError("unexpected fetch {}".format(url))
        return self.payloads[url]

    def add(self, url, payload):
        self.payloads[url] = payload
        return integrity_for(payload)

    def write_lock(self, packages, root=None):
        path = os.path.join(self.tmpdir, "package-lock.json")
        document = {
            "lockfileVersion": 3,
            "packages": {
                "": root or {"dependencies": {}, "devDependencies": {}}
            },
        }
        document["packages"].update(packages)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(document, handle)
        return path

    def generate(self, packages, root=None, vite_config=None):
        lock = self.write_lock(packages, root)
        return recipes.generate_recipes(
            lock,
            self.outdir,
            self.licenses,
            fetch=self.fetch,
            vite_config=vite_config,
        )

    def read(self, name):
        with open(os.path.join(self.outdir, name), encoding="utf-8") as handle:
            return handle.read()

    def entry(self, version, url, integrity, **extra):
        meta = {
            "version": version,
            "resolved": url,
            "integrity": integrity,
        }
        meta.update(extra)
        return meta

    def test_dev_and_devoptional_are_omitted_from_shipped(self):
        lodash = package_tarball("lodash")
        lodash_url = "https://example.test/lodash.tgz"
        vue = package_tarball("@vue/runtime-core")
        vue_url = "https://example.test/runtime-core.tgz"
        typescript = package_tarball("typescript")
        ts_url = "https://example.test/typescript.tgz"
        self.add(lodash_url, lodash)
        self.add(vue_url, vue)
        root = {
            "dependencies": {
                "lodash": "4.17.21",
                "@vue/runtime-core": "3.5.24",
            },
            "devDependencies": {"typescript": "5.9.3"},
        }
        written = self.generate(
            {
                "node_modules/lodash": self.entry(
                    "4.17.21",
                    lodash_url,
                    integrity_for(lodash),
                    dependencies={"typescript": "5.9.3"},
                ),
                "node_modules/@vue/runtime-core": self.entry(
                    "3.5.24", vue_url, integrity_for(vue)
                ),
                "node_modules/typescript": self.entry(
                    "5.9.3",
                    ts_url,
                    integrity_for(typescript),
                    devOptional=True,
                ),
                "node_modules/jest": self.entry(
                    "27.5.1",
                    "https://example.test/jest.tgz",
                    integrity_for(b"unused"),
                    dev=True,
                ),
            },
            root,
        )
        names = sorted(package["pn"] for package in written)
        self.assertEqual(
            names,
            ["webui-vue-npm-lodash", "webui-vue-npm-vue-runtime-core"],
        )
        lodash_text = self.read("webui-vue-npm-lodash_4.17.21.bb")
        self.assertIn('LICENSE = "MIT"', lodash_text)
        self.assertIn("file://LICENSE;md5=", lodash_text)
        self.assertIn('S = "${UNPACKDIR}/${BPN}"', lodash_text)
        self.assertIn("striplevel=1", lodash_text)
        self.assertIn("/sysroot-only/webui-vue/", lodash_text)
        self.assertNotIn("${WORKDIR}", lodash_text)
        self.assertIn("pkg:npm/lodash@4.17.21", lodash_text)
        self.assertIn('WEBUI_VUE_NPM_ROLE = "shipped"', lodash_text)
        self.assertIn(
            'SPDX_CUSTOM_ANNOTATION_VARS:append = " WEBUI_VUE_NPM_ROLE"',
            lodash_text,
        )
        self.assertIn('INHIBIT_SYSROOT_STRIP = "1"', lodash_text)
        self.assertNotIn("ALLOW_EMPTY", lodash_text)
        self.assertNotIn("jest", lodash_text)
        scoped = self.read("webui-vue-npm-vue-runtime-core_3.5.24.bb")
        self.assertIn('NPM_PACKAGE = "@vue/runtime-core"', scoped)
        self.assertIn("pkg:npm/%40vue/runtime-core@3.5.24", scoped)
        inc = self.read("webui-vue-npm.inc")
        self.assertIn("WEBUI_VUE_NPM_PACKAGES", inc)
        self.assertIn("WEBUI_VUE_NPM_LOCK_SHA256", inc)
        self.assertIn("webui-vue-npm-lodash", inc)
        self.assertNotIn("typescript", inc)
        self.assertNotIn("WEB_UI_VUE_NPM_PACKAGES", inc)

    def test_toolchain_keeps_host_binary_only(self):
        vite = package_tarball("vite")
        linux = package_tarball("@esbuild/linux-x64", root="package")
        darwin = package_tarball("@esbuild/darwin-arm64")
        vite_url = "https://example.test/vite.tgz"
        linux_url = "https://example.test/linux.tgz"
        darwin_url = "https://example.test/darwin.tgz"
        self.add(vite_url, vite)
        self.add(linux_url, linux)
        root = {"dependencies": {}, "devDependencies": {"vite": "6.4.1"}}
        written = self.generate(
            {
                "node_modules/vite": self.entry(
                    "6.4.1",
                    vite_url,
                    integrity_for(vite),
                    dev=True,
                    dependencies={"esbuild": "0.25.12"},
                ),
                "node_modules/esbuild": self.entry(
                    "0.25.12",
                    vite_url,
                    integrity_for(vite),
                    dev=True,
                    optionalDependencies={
                        "@esbuild/linux-x64": "0.25.12",
                        "@esbuild/darwin-arm64": "0.25.12",
                    },
                ),
                "node_modules/@esbuild/linux-x64": self.entry(
                    "0.25.12",
                    linux_url,
                    integrity_for(linux),
                    dev=True,
                    os=["linux"],
                    cpu=["x64"],
                ),
                "node_modules/@esbuild/darwin-arm64": self.entry(
                    "0.25.12",
                    darwin_url,
                    integrity_for(darwin),
                    dev=True,
                    os=["darwin"],
                    cpu=["arm64"],
                ),
            },
            root,
        )
        names = sorted(package["pn"] for package in written)
        self.assertIn("webui-vue-npm-esbuild-linux-x64", names)
        self.assertNotIn("webui-vue-npm-esbuild-darwin-arm64", names)
        roles = {package["pn"]: package["role"] for package in written}
        self.assertEqual(roles["webui-vue-npm-vite"], "build")
        self.assertIn(
            'WEBUI_VUE_NPM_ROLE = "build"',
            self.read("webui-vue-npm-vite_6.4.1.bb"),
        )
        inc = self.read("webui-vue-npm.inc")
        self.assertIn("WEBUI_VUE_NPM_BUILD_PACKAGES", inc)
        self.assertIn('WEBUI_VUE_NPM_BUILD_HOST = "linux-x64-gnu"', inc)

    def test_optional_peers_are_not_followed(self):
        vite = package_tarball("vite")
        postcss = package_tarball("postcss")
        self.add("https://example.test/vite.tgz", vite)
        self.add("https://example.test/postcss.tgz", postcss)
        root = {"dependencies": {}, "devDependencies": {"vite": "6.4.1"}}
        written = self.generate(
            {
                "node_modules/vite": self.entry(
                    "6.4.1",
                    "https://example.test/vite.tgz",
                    integrity_for(vite),
                    dev=True,
                    peerDependencies={"postcss": "^8", "stylus": "*"},
                    peerDependenciesMeta={"stylus": {"optional": True}},
                ),
                "node_modules/postcss": self.entry(
                    "8.5.6",
                    "https://example.test/postcss.tgz",
                    integrity_for(postcss),
                    dev=True,
                ),
                "node_modules/stylus": self.entry(
                    "0.54.8",
                    "https://example.test/stylus.tgz",
                    integrity_for(b"unused"),
                    dev=True,
                ),
            },
            root,
        )
        self.assertEqual(
            sorted(package["pn"] for package in written),
            ["webui-vue-npm-postcss", "webui-vue-npm-vite"],
        )

    def test_host_filter_trusts_os_and_cpu(self):
        def matches(meta, name, cpu="x64", libc="gnu"):
            return recipes.matches_build_host(meta, name, "linux", cpu, libc)

        arm64 = {"os": ["linux"], "cpu": ["arm64"]}
        self.assertTrue(matches(arm64, "@esbuild/linux-arm64", cpu="arm64"))
        self.assertTrue(
            matches(arm64, "@rollup/rollup-linux-arm64-gnu", cpu="arm64")
        )
        self.assertFalse(
            matches(arm64, "@rollup/rollup-linux-arm64-musl", cpu="arm64")
        )
        self.assertFalse(matches(arm64, "@esbuild/linux-arm64"))
        self.assertTrue(matches({}, "is-windows"))
        self.assertTrue(matches({"os": ["!win32"]}, "watcher"))
        self.assertFalse(matches({"os": ["!linux"]}, "watcher"))
        musl_only = {"os": ["linux"], "cpu": ["x64"], "libc": ["musl"]}
        self.assertFalse(matches(musl_only, "native"))
        self.assertTrue(matches(musl_only, "native", libc="musl"))

    def test_config_imports(self):
        config = "\n".join(
            [
                '/// <reference types="vitest" />',
                "/** @type {import('vitest/config').UserConfig} */",
                "import { defineConfig } from 'vite';",
                "import vue from '@vitejs/plugin-vue';",
                "import {",
                "  first,",
                "  second,",
                '} from "@scope/multi/sub";',
                "import 'side-effect';",
                "import path from 'node:path';",
                "import fs from 'fs';",
                "import local from './local.js';",
                "// import commented from 'commented-out';",
                "const lazy = () => import('lazy-plugin');",
                "const legacy = require('legacy-plugin');",
                'const scss = `@import "@/assets/styles"; @import "partial";`;',
                "const assets = ['**/*.ico', 'text/html,*/*;q=0.8'];",
            ]
        )
        self.assertEqual(
            recipes.config_imports(config),
            [
                "@scope/multi",
                "@vitejs/plugin-vue",
                "lazy-plugin",
                "legacy-plugin",
                "side-effect",
                "vite",
            ],
        )

    def test_config_imports_join_the_toolchain(self):
        plugin = package_tarball("custom-plugin")
        self.add("https://example.test/plugin.tgz", plugin)
        root = {
            "dependencies": {},
            "devDependencies": {"custom-plugin": "1.0.0"},
        }
        entries = {
            "node_modules/custom-plugin": self.entry(
                "1.0.0",
                "https://example.test/plugin.tgz",
                integrity_for(plugin),
                dev=True,
            )
        }
        written = self.generate(
            entries, root, vite_config="import plugin from 'custom-plugin';\n"
        )
        self.assertEqual(
            [package["pn"] for package in written],
            ["webui-vue-npm-custom-plugin"],
        )
        roots = [
            line
            for line in self.read("webui-vue-npm.inc").splitlines()
            if line.startswith("WEBUI_VUE_NPM_TOOLCHAIN_ROOTS")
        ]
        self.assertIn("custom-plugin", roots[0])
        with self.assertRaises(ValueError):
            self.generate(
                entries, root, vite_config="import x from 'not-installed';\n"
            )

    def test_inc_lists_are_sorted(self):
        zeta = package_tarball("zeta")
        alpha = package_tarball("alpha")
        self.add("https://example.test/zeta.tgz", zeta)
        self.add("https://example.test/alpha.tgz", alpha)
        self.generate(
            {
                "node_modules/zeta": self.entry(
                    "1.0.0",
                    "https://example.test/zeta.tgz",
                    integrity_for(zeta),
                ),
                "node_modules/alpha": self.entry(
                    "1.0.0",
                    "https://example.test/alpha.tgz",
                    integrity_for(alpha),
                ),
            },
            {"dependencies": {"zeta": "1.0.0", "alpha": "1.0.0"}},
        )
        inc = self.read("webui-vue-npm.inc")
        self.assertLess(
            inc.index("webui-vue-npm-alpha"), inc.index("webui-vue-npm-zeta")
        )

    def test_recipe_names_have_no_underscores(self):
        self.assertEqual(
            recipes.recipe_pn("@types/babel__core"),
            "webui-vue-npm-types-babel--core",
        )

    def test_purl_encodes_the_scope(self):
        self.assertEqual(
            recipes.purl("@vue/shared", "3.5.24"),
            "pkg:npm/%40vue/shared@3.5.24",
        )
        self.assertEqual(
            recipes.purl("lodash", "4.17.21"), "pkg:npm/lodash@4.17.21"
        )

    def test_find_common_licenses_stays_inside_the_checkout(self):
        outer = os.path.join(self.tmpdir, "outer")
        os.makedirs(os.path.join(outer, recipes.COMMON_LICENSE_DIRS[1]))
        checkout = os.path.join(outer, "openbmc")
        os.makedirs(checkout)
        with self.assertRaises(ValueError):
            recipes.find_common_licenses(checkout)
        inside = os.path.join(checkout, recipes.COMMON_LICENSE_DIRS[0])
        os.makedirs(inside)
        self.assertEqual(recipes.find_common_licenses(checkout), inside)

    def test_default_fetch_has_a_timeout(self):
        url = "https://example.test/x.tgz"
        with mock.patch.object(
            recipes.urllib.request,
            "urlopen",
            return_value=io.BytesIO(b"payload"),
        ) as urlopen:
            self.assertEqual(recipes.default_fetch(url), b"payload")
        urlopen.assert_called_once_with(
            url, timeout=recipes.FETCH_TIMEOUT_SECONDS
        )

    def test_top_level_copy_keeps_the_bare_name(self):
        old = package_tarball("vue-demi")
        new = package_tarball("vue-demi")
        core = package_tarball("@vuelidate/core")
        old_url = "https://example.test/demi-old.tgz"
        new_url = "https://example.test/demi-new.tgz"
        core_url = "https://example.test/core.tgz"
        self.add(old_url, old)
        self.add(new_url, new)
        self.add(core_url, core)
        root = {
            "dependencies": {
                "vue-demi": "0.14.10",
                "@vuelidate/core": "2.0.0",
            }
        }
        written = self.generate(
            {
                "node_modules/vue-demi": self.entry(
                    "0.14.10", new_url, integrity_for(new)
                ),
                "node_modules/@vuelidate/core": self.entry(
                    "2.0.0",
                    core_url,
                    integrity_for(core),
                    dependencies={"vue-demi": "0.13.11"},
                ),
                "node_modules/@vuelidate/core/node_modules/vue-demi": self.entry(
                    "0.13.11", old_url, integrity_for(old)
                ),
            },
            root,
        )
        names = sorted(
            package["pn"] for package in written if "vue-demi" in package["pn"]
        )
        self.assertEqual(
            names,
            [
                "webui-vue-npm-vue-demi",
                "webui-vue-npm-vue-demi-0.13.11",
            ],
        )

    def test_nested_same_version_is_one_recipe(self):
        shared = package_tarball("@vue/shared")
        url = "https://example.test/shared.tgz"
        digest = integrity_for(shared)
        self.add(url, shared)
        root = {
            "dependencies": {
                "vue": "3.5.24",
                "@vue/runtime-core": "3.5.24",
            }
        }
        self.generate(
            {
                "node_modules/vue": self.entry(
                    "3.5.24",
                    url,
                    digest,
                    dependencies={"@vue/shared": "3.5.24"},
                ),
                "node_modules/@vue/runtime-core": self.entry(
                    "3.5.24",
                    url,
                    digest,
                    dependencies={"@vue/shared": "3.5.24"},
                ),
                "node_modules/vue/node_modules/@vue/shared": self.entry(
                    "3.5.24", url, digest
                ),
                "node_modules/@vue/runtime-core/node_modules/@vue/shared": (
                    self.entry("3.5.24", url, digest)
                ),
            },
            root,
        )
        text = self.read("webui-vue-npm-vue-shared_3.5.24.bb")
        self.assertIn("node_modules/vue/node_modules/@vue/shared", text)
        self.assertIn(
            "node_modules/@vue/runtime-core/node_modules/@vue/shared", text
        )

    def test_mismatched_duplicate_entries_raise(self):
        first = package_tarball("lodash")
        second = package_tarball("lodash")
        vue = package_tarball("vue")
        self.add("https://example.test/a.tgz", first)
        self.add("https://example.test/b.tgz", second)
        self.add("https://example.test/vue.tgz", vue)
        root = {"dependencies": {"lodash": "4.17.21", "vue": "3.5.24"}}
        with self.assertRaises(ValueError):
            self.generate(
                {
                    "node_modules/lodash": self.entry(
                        "4.17.21",
                        "https://example.test/a.tgz",
                        integrity_for(first),
                    ),
                    "node_modules/vue": self.entry(
                        "3.5.24",
                        "https://example.test/vue.tgz",
                        integrity_for(vue),
                        dependencies={"lodash": "4.17.21"},
                    ),
                    "node_modules/vue/node_modules/lodash": self.entry(
                        "4.17.21",
                        "https://example.test/b.tgz",
                        integrity_for(second),
                    ),
                },
                root,
            )

    def test_recipe_name_collision_raises(self):
        foo = package_tarball("foo-bar")
        scoped = package_tarball("@foo/bar")
        foo_url = "https://example.test/foo.tgz"
        scoped_url = "https://example.test/scoped.tgz"
        self.add(foo_url, foo)
        self.add(scoped_url, scoped)
        root = {"dependencies": {"foo-bar": "1.0.0", "@foo/bar": "1.0.0"}}
        with self.assertRaises(ValueError):
            self.generate(
                {
                    "node_modules/foo-bar": self.entry(
                        "1.0.0", foo_url, integrity_for(foo)
                    ),
                    "node_modules/@foo/bar": self.entry(
                        "1.0.0", scoped_url, integrity_for(scoped)
                    ),
                },
                root,
            )
        self.assertFalse(os.path.exists(self.outdir))

    def test_failed_render_leaves_output_unchanged(self):
        os.makedirs(self.outdir)
        kept = os.path.join(self.outdir, "webui-vue-npm-keep_1.0.0.bb")
        original = recipes.HEADER + 'SUMMARY = "keep"\n'
        with open(kept, "w", encoding="utf-8") as handle:
            handle.write(original)
        payload = package_tarball(
            "example", license_name="NOT-A-LICENSE", license_file=False
        )
        url = "https://example.test/example.tgz"
        self.add(url, payload)
        root = {"dependencies": {"example": "1.2.3"}}
        with self.assertRaises(ValueError):
            self.generate(
                {
                    "node_modules/example": self.entry(
                        "1.2.3", url, integrity_for(payload)
                    )
                },
                root,
            )
        with open(kept, encoding="utf-8") as handle:
            self.assertEqual(handle.read(), original)
        self.assertFalse(
            os.path.exists(
                os.path.join(self.outdir, "webui-vue-npm-example_1.2.3.bb")
            )
        )

    def test_with_and_and_licenses(self):
        with open(
            os.path.join(self.licenses, "Apache-2.0-with-LLVM-exception"), "wb"
        ) as handle:
            handle.write(b"Apache with LLVM\n")
        with open(os.path.join(self.licenses, "Apache-2.0"), "wb") as handle:
            handle.write(b"Apache\n")
        with_pkg = package_tarball(
            "with-pkg",
            license_name="Apache-2.0 WITH LLVM-exception",
            license_file=False,
        )
        and_pkg = package_tarball(
            "and-pkg", license_name="MIT AND Apache-2.0", license_file=True
        )
        with_url = "https://example.test/with.tgz"
        and_url = "https://example.test/and.tgz"
        self.add(with_url, with_pkg)
        self.add(and_url, and_pkg)
        root = {"dependencies": {"with-pkg": "1.0.0", "and-pkg": "1.0.0"}}
        self.generate(
            {
                "node_modules/with-pkg": self.entry(
                    "1.0.0", with_url, integrity_for(with_pkg)
                ),
                "node_modules/and-pkg": self.entry(
                    "1.0.0", and_url, integrity_for(and_pkg)
                ),
            },
            root,
        )
        with_text = self.read("webui-vue-npm-with-pkg_1.0.0.bb")
        self.assertIn('LICENSE = "Apache-2.0-with-LLVM-exception"', with_text)
        self.assertIn("Apache-2.0-with-LLVM-exception;md5=", with_text)
        and_text = self.read("webui-vue-npm-and-pkg_1.0.0.bb")
        self.assertIn('LICENSE = "MIT & Apache-2.0"', and_text)
        self.assertIn("file://LICENSE;md5=", and_text)

    def test_sha1_and_multi_hash_integrity(self):
        payload = package_tarball("sha")
        sha1 = integrity_for(payload, "sha1")
        sha512 = integrity_for(payload, "sha512")
        url = "https://example.test/sha.tgz"
        self.add(url, payload)
        root = {"dependencies": {"sha": "1.0.0"}}
        self.generate(
            {
                "node_modules/sha": self.entry(
                    "1.0.0", url, sha512 + " " + sha1
                )
            },
            root,
        )
        text = self.read("webui-vue-npm-sha_1.0.0.bb")
        self.assertIn("SRC_URI[sha512sum]", text)
        self.assertNotIn("SRC_URI[sha1sum]", text)

        outdir = os.path.join(self.tmpdir, "sha1-out")
        lock = self.write_lock(
            {"node_modules/sha": self.entry("1.0.0", url, sha1)}, root
        )
        recipes.generate_recipes(lock, outdir, self.licenses, fetch=self.fetch)
        with open(
            os.path.join(outdir, "webui-vue-npm-sha_1.0.0.bb"),
            encoding="utf-8",
        ) as handle:
            sha1_text = handle.read()
        self.assertIn("SRC_URI[sha1sum]", sha1_text)

    def test_non_package_root_and_stale_removal(self):
        payload = package_tarball("js-cookie", root="js-cookie")
        url = "https://example.test/js-cookie.tgz"
        self.add(url, payload)
        os.makedirs(self.outdir)
        stale = os.path.join(self.outdir, "webui-vue-npm-old_1.0.0.bb")
        with open(stale, "w", encoding="utf-8") as handle:
            handle.write(recipes.HEADER + 'SUMMARY = "old"\n')
        kept = os.path.join(self.outdir, "hand-written.bb")
        with open(kept, "w", encoding="utf-8") as handle:
            handle.write('SUMMARY = "keep me"\n')
        root = {"dependencies": {"js-cookie": "3.0.1"}}
        self.generate(
            {
                "node_modules/js-cookie": self.entry(
                    "3.0.1", url, integrity_for(payload)
                )
            },
            root,
        )
        self.assertFalse(os.path.exists(stale))
        self.assertTrue(os.path.exists(kept))
        self.assertIn(
            "file://LICENSE;md5=",
            self.read("webui-vue-npm-js-cookie_3.0.1.bb"),
        )

    def test_bump_skips_unchanged_inputs(self):
        payload = package_tarball("mitt")
        url = "https://example.test/mitt.tgz"
        self.add(url, payload)
        meta = os.path.join(self.tmpdir, "meta-phosphor")
        recipe = "recipes-phosphor/webui/webui-vue_git.bb"
        os.makedirs(os.path.join(meta, os.path.dirname(recipe)))
        lock = {
            "lockfileVersion": 3,
            "packages": {
                "": {"dependencies": {"mitt": "3.0.1"}},
                "node_modules/mitt": self.entry(
                    "3.0.1", url, integrity_for(payload)
                ),
            },
        }
        lock_bytes = json.dumps(lock).encode("utf-8")
        out_rel = "recipes-phosphor/webui-vue-npm"

        def no_fetch(url):
            raise AssertionError("unchanged inputs fetched {}".format(url))

        def bump(data, fetch, vite_config=None):
            return recipes.update_recipes_for_bump(
                meta,
                recipe,
                data,
                self.licenses,
                vite_config=vite_config,
                fetch=fetch,
            )

        self.assertEqual(bump(lock_bytes, self.fetch), (out_rel, True))
        self.assertTrue(
            os.path.exists(
                os.path.join(meta, out_rel, "webui-vue-npm-mitt_3.0.1.bb")
            )
        )
        self.assertEqual(bump(lock_bytes, no_fetch), (out_rel, False))
        self.assertEqual(bump(lock_bytes + b"\n", self.fetch), (out_rel, True))
        self.assertEqual(
            bump(lock_bytes + b"\n", self.fetch, "import 'mitt';\n"),
            (out_rel, True),
        )

    @unittest.skipUnless(shutil.which("git"), "needs git")
    def test_autobump_regenerates_from_the_project_clone(self):
        payload = package_tarball("mitt", license_file=False)
        url = "https://example.test/mitt.tgz"
        self.add(url, payload)
        work = os.path.join(self.tmpdir, "work")
        project = os.path.join(work, "webui-vue")
        os.makedirs(project)
        lock = {
            "lockfileVersion": 3,
            "packages": {
                "": {"dependencies": {"mitt": "3.0.1"}},
                "node_modules/mitt": self.entry(
                    "3.0.1", url, integrity_for(payload)
                ),
            },
        }
        with open(
            os.path.join(project, "package-lock.json"), "w", encoding="utf-8"
        ) as handle:
            json.dump(lock, handle)
        with open(
            os.path.join(project, "vite.config.js"), "w", encoding="utf-8"
        ) as handle:
            handle.write("export default {};\n")
        git(project, "init", "-q")
        git(project, "add", "-A")
        git(project, "commit", "-qm", "webui-vue")
        sha = git(project, "rev-parse", "HEAD").strip()

        meta = os.path.join(work, "meta-phosphor")
        recipe = "recipes-phosphor/webui/webui-vue_git.bb"
        licenses = os.path.join(work, recipes.COMMON_LICENSE_DIRS[0])
        os.makedirs(os.path.join(meta, os.path.dirname(recipe)))
        os.makedirs(licenses)
        shutil.copy(os.path.join(self.licenses, "MIT"), licenses)
        with open(os.path.join(meta, recipe), "w", encoding="utf-8") as handle:
            handle.write('SRCREV = "{}"\n'.format(sha))
        git(meta, "init", "-q")
        git(meta, "add", "-A")
        git(meta, "commit", "-qm", "meta-phosphor")

        autobump = load_autobump()
        cwd = os.getcwd()
        os.chdir(work)
        try:
            with mock.patch.object(recipes, "default_fetch", self.fetch):
                note = autobump.regenerate_webui_vue_npm_recipes(
                    "meta-phosphor", recipe, "webui-vue", sha
                )
                again = autobump.regenerate_webui_vue_npm_recipes(
                    "meta-phosphor", recipe, "webui-vue", sha
                )
        finally:
            os.chdir(cwd)
        out_rel = "recipes-phosphor/webui-vue-npm"
        self.assertEqual(
            note, "Regenerated {} from package-lock.json.".format(out_rel)
        )
        self.assertEqual(again, "")
        self.assertEqual(
            sorted(git(meta, "diff", "--cached", "--name-only").split()),
            [
                out_rel + "/webui-vue-npm-mitt_3.0.1.bb",
                out_rel + "/webui-vue-npm.inc",
            ],
        )
        with open(
            os.path.join(meta, out_rel, "webui-vue-npm-mitt_3.0.1.bb"),
            encoding="utf-8",
        ) as handle:
            self.assertIn(
                "file://${COMMON_LICENSE_DIR}/MIT;md5="
                + hashlib.md5(MIT_TEXT).hexdigest(),
                handle.read(),
            )

    def test_autobump_finds_common_licenses_beside_the_layer(self):
        autobump = load_autobump()
        tree = os.path.join(self.tmpdir, "tree")
        layer = os.path.join(tree, "meta-phosphor")
        os.makedirs(layer)
        with self.assertRaises(ValueError) as caught:
            autobump.common_licenses_above(layer)
        self.assertIn(
            "common-licenses not found in " + layer, str(caught.exception)
        )
        beside = os.path.join(tree, recipes.COMMON_LICENSE_DIRS[0])
        os.makedirs(beside)
        self.assertEqual(autobump.common_licenses_above(layer), beside)
        with self.assertRaises(ValueError):
            recipes.find_common_licenses(layer)

    def test_main_writes_recipes_and_reports_errors(self):
        payload = package_tarball("mitt")
        url = "https://example.test/mitt.tgz"
        self.add(url, payload)
        lockfile = self.write_lock(
            {
                "node_modules/mitt": self.entry(
                    "3.0.1", url, integrity_for(payload)
                )
            },
            {"dependencies": {"mitt": "3.0.1"}},
        )
        config = os.path.join(self.tmpdir, "vite.config.js")
        with open(config, "w", encoding="utf-8") as handle:
            handle.write("import { defineConfig } from 'vite';\n")
        argv = [lockfile, self.outdir, "--common-licenses", self.licenses]
        with mock.patch.object(recipes, "default_fetch", self.fetch):
            with mock.patch("sys.stderr", new_callable=io.StringIO) as err:
                self.assertEqual(recipes.main(argv), 1)
            self.assertIn("vite.config.js imports vite", err.getvalue())
            os.remove(config)
            with mock.patch("sys.stdout", new_callable=io.StringIO):
                self.assertEqual(recipes.main(argv), 0)
        self.assertTrue(
            os.path.exists(
                os.path.join(self.outdir, "webui-vue-npm-mitt_3.0.1.bb")
            )
        )

    def test_integrity_mismatch_raises_before_write(self):
        payload = package_tarball("bad")
        url = "https://example.test/bad.tgz"
        self.payloads[url] = payload
        root = {"dependencies": {"bad": "1.0.0"}}
        with self.assertRaises(ValueError):
            self.generate(
                {
                    "node_modules/bad": self.entry(
                        "1.0.0", url, integrity_for(b"other")
                    )
                },
                root,
            )
        self.assertFalse(os.path.exists(self.outdir))


class FakeResult:
    def __init__(self, stdout=b""):
        self.stdout = stdout


class FakeGit:
    """Stands in for sh's git in find_and_process_bumps.

    fail names the calls that raise, as "ls-remote <project>" or
    "shortlog <project>". Commit messages are kept in commits.
    """

    def __init__(self, recipe_paths, new_sha, error, fail=()):
        self.recipe_paths = recipe_paths
        self.new_sha = new_sha
        self.error = error
        self.fail = set(fail)
        self.commits = []

    def __call__(self, *args, **kwargs):
        if args[0] == "ls-remote":
            if "ls-remote " + args[1].rsplit("/", 1)[-1] in self.fail:
                raise self.error()
            return FakeResult("{}\tHEAD\n".format(self.new_sha).encode())
        return "0123456789abcdef\n"

    def __getattr__(self, name):
        def command(*args, **kwargs):
            if name == "grep":
                return FakeResult("\n".join(self.recipe_paths).encode())
            if (
                name == "shortlog"
                and "shortlog " + kwargs["_cwd"] in self.fail
            ):
                raise self.error()
            if name == "commit":
                self.commits.append(args[0])
            return FakeResult(b"Author (1):\n      change\n")

        return command


class AutobumpBumpTest(unittest.TestCase):
    LAYER_RECIPES = {
        "recipes-phosphor/foo/phosphor-foo_git.bb": "phosphor-foo",
        "recipes-phosphor/webui/webui-vue_git.bb": "webui-vue",
    }

    def setUp(self):
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.tmpdir)

    def bump(self, fail=(), regenerate=None, through_main=False):
        """Bump two recipes in a fake meta-phosphor clone.

        Returns the reported status and the projects that got a commit.
        """
        autobump = load_autobump()
        work = tempfile.mkdtemp(dir=self.tmpdir)
        for path, project in self.LAYER_RECIPES.items():
            full = os.path.join(work, "meta-phosphor", path)
            os.makedirs(os.path.dirname(full))
            with open(full, "w", encoding="utf-8") as handle:
                handle.write(
                    'SRC_URI = "git://github.com/openbmc/{}.git;'
                    'branch=master;protocol=https"\n'
                    'SRCREV = "{}"\n'.format(project, "1" * 40)
                )
        fake = FakeGit(
            sorted(self.LAYER_RECIPES),
            "2" * 40,
            autobump.sh.ErrorReturnCode,
            fail,
        )
        regen = mock.Mock(
            return_value="Regenerated recipes-phosphor/webui-vue-npm.",
            side_effect=regenerate,
        )
        args = types.SimpleNamespace(
            ssh_config_host="gerrit-host",
            branch="master",
            noisy=False,
            dry_run=True,
            gerrit=lambda *query: FakeResult(),
        )
        argv = ["openbmc-autobump.py", "-d", "-m", "meta-phosphor", "host"]
        cwd = os.getcwd()
        os.chdir(work)
        try:
            with mock.patch.object(autobump, "git", fake), mock.patch.object(
                autobump, "regenerate_webui_vue_npm_recipes", regen
            ), mock.patch.object(sys, "argv", argv), mock.patch(
                "sys.stdout", new_callable=io.StringIO
            ), mock.patch(
                "sys.stderr", new_callable=io.StringIO
            ):
                if through_main:
                    ok = autobump.main()
                else:
                    ok = autobump.find_and_process_bumps("meta-phosphor", args)
        finally:
            os.chdir(cwd)
        return ok, [message.split(":")[0] for message in fake.commits]

    def test_only_a_failed_webui_vue_regeneration_fails_the_bumps(self):
        self.assertEqual(self.bump(), (True, ["phosphor-foo", "webui-vue"]))
        self.assertEqual(
            self.bump(regenerate=RuntimeError("no common-licenses")),
            (False, ["phosphor-foo"]),
        )
        for failure in ("ls-remote phosphor-foo", "shortlog phosphor-foo"):
            self.assertEqual(
                self.bump(fail=[failure]), (True, ["webui-vue"]), failure
            )

    def test_main_returns_the_bump_status(self):
        self.assertEqual(
            self.bump(through_main=True), (True, ["phosphor-foo", "webui-vue"])
        )
        self.assertEqual(
            self.bump(regenerate=RuntimeError("boom"), through_main=True),
            (False, ["phosphor-foo"]),
        )


if __name__ == "__main__":
    unittest.main()
