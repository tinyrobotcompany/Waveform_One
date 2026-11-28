import test from "node:test";
import assert from "node:assert/strict";
import * as fs from "node:fs";
import { join, resolve } from "node:path";
import { execFileSync } from "node:child_process";
import * as p4 from "../../scripts/package-p4-update.mjs";
import * as production from "../../scripts/build-production.mjs";
const root = resolve(".");
const commit = "a".repeat(40);
function fixture(t) {
  fs.mkdirSync(".artifacts", { recursive: true });
  const dir = fs.mkdtempSync(".artifacts/p4-test-");
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  return resolve(dir);
}
function image(version = "1.2.3") {
  const b = Buffer.alloc(1024);
  b[0] = 0xe9;
  b.writeUInt32LE(0xabcd5432, 32);
  b.write(version, 48);
  b.write("waveform_p4_display", 80);
  return b;
}
function artifacts(dir) {
  fs.writeFileSync(join(dir, p4.IMAGE), image());
  fs.writeFileSync(join(dir, "p4-dependencies.lock"), "locked");
  fs.writeFileSync(
    join(dir, "p4-update-build.json"),
    JSON.stringify({
      commit,
      version: "1.2.3",
      sha256: p4.digest(join(dir, p4.IMAGE)),
      dependency_lock_sha256: p4.digest(join(dir, "p4-dependencies.lock")),
    }),
  );
}
test("all production protections are mandatory", () => {
  const good =
    production.REQUIRED.map((n) => `CONFIG_${n}=y`).join("\n") +
    '\nCONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.production.csv"';
  production.verifyConfig(good);
  for (const n of production.REQUIRED)
    assert.throws(() =>
      production.verifyConfig(good.replace(`CONFIG_${n}=y`, "")),
    );
  for (const n of production.FORBIDDEN)
    assert.throws(() => production.verifyConfig(good + `\nCONFIG_${n}=y`));
  assert.throws(() =>
    production.verifyConfig(
      good.replace("partitions.production.csv", "partitions.csv"),
    ),
  );
});
test("SDK rejected before signing key or output access", () =>
  assert.throws(
    () => production.build("unused", "missing", "1.2.3", "missing-sdk"),
    /git|SDK|Command/,
  ));
test("image identity, version, bounds and private material rejected", () => {
  p4.validateImage(image(), "1.2.3");
  for (const [b, v] of [
    [image(), "1.2.4"],
    [image(), "01.2.3"],
    [Buffer.alloc(1024), "1.2.3"],
    [Buffer.concat([image(), Buffer.alloc(0x800000)]), "1.2.3"],
    [
      Buffer.concat([
        image(),
        Buffer.from(
          "-----BEGIN PRIVATE KEY-----\n" +
            "A".repeat(64) +
            "\n-----END PRIVATE KEY-----",
        ),
      ]),
      "1.2.3",
    ],
  ])
    assert.throws(() => p4.validateImage(b, v));
});
test("provenance rejects absent and corrupt artifacts before signing", (t) => {
  const dir = fixture(t);
  artifacts(dir);
  p4.verifyBuild(dir, "1.2.3", commit);
  assert.throws(() => p4.verifyBuild(dir, "1.2.3", "bad"));
  assert.throws(() => p4.verifyBuild(dir, "1.2.4", commit));
  for (const name of [
    p4.IMAGE,
    "p4-dependencies.lock",
    "p4-update-build.json",
  ]) {
    const path = join(dir, name),
      old = fs.readFileSync(path);
    fs.unlinkSync(path);
    assert.throws(() => p4.verifyBuild(dir, "1.2.3", commit));
    fs.writeFileSync(path, "corrupt");
    assert.throws(() => p4.verifyBuild(dir, "1.2.3", commit));
    fs.writeFileSync(path, old);
  }
});
test("secure verification invokes official espsecure and requires trust key", () => {
  assert.throws(() => p4.verifySecureImage("image", null));
  let args;
  p4.verifySecureImage("image", "public", {
    run: (_cmd, a) => {
      args = a;
    },
  });
  assert.deepEqual(args.slice(1), [
    "verify-signature",
    "--version",
    "2",
    "--keyfile",
    "public",
    "image",
  ]);
});
test("record validates both mapping headers, rollback and device credentials before output", (t) => {
  const dir = fixture(t),
    build = join(dir, "build"),
    dist = join(dir, "dist");
  const headers = ["config/sdkconfig.h", "bootloader/config/sdkconfig.h"];
  const good =
    "#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1\n#define CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH 1\n";
  for (const h of headers) {
    fs.mkdirSync(join(build, h, ".."), { recursive: true });
    fs.writeFileSync(join(build, h), good);
  }
  fs.writeFileSync(join(build, "waveform_p4_display.bin"), image());
  const creds = join(build, "esp-idf/main/acrcloud_config.h");
  fs.mkdirSync(join(creds, ".."), { recursive: true });
  fs.writeFileSync(
    creds,
    ["HOST", "ACCESS_KEY", "ACCESS_SECRET"]
      .map((n) => `#define ACRCLOUD_${n} ""`)
      .join("\n"),
  );
  for (const h of headers) {
    fs.writeFileSync(
      join(build, h),
      "#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1",
    );
    assert.throws(() => p4.record(build, dist, "1.2.3", commit), /32-bit/);
    assert.equal(fs.existsSync(dist), false);
    fs.writeFileSync(join(build, h), good);
  }
  fs.writeFileSync(
    join(build, headers[0]),
    good.replace("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE", "MISSING"),
  );
  assert.throws(() => p4.record(build, dist, "1.2.3", commit), /Rollback/);
  fs.writeFileSync(join(build, headers[0]), good);
  p4.record(build, dist, "1.2.3", commit);
  assert.deepEqual(fs.readFileSync(join(dist, p4.IMAGE)), image());
  fs.appendFileSync(creds, "bad");
  assert.throws(
    () => p4.record(build, join(dir, "secret-dist"), "1.2.3", commit),
    /credentials/,
  );
});
test("actual CLI signs compatible assets, verifies signature and cleans temporary key", (t) => {
  const dir = fixture(t),
    repo = join(dir, "repo"),
    dist = join(dir, "dist");
  fs.mkdirSync(join(repo, "scripts"), { recursive: true });
  fs.copyFileSync(
    "scripts/package-p4-update.mjs",
    join(repo, "scripts/package-p4-update.mjs"),
  );
  const pub = join(repo, "firmware/esp32/p4_display/main/ota_public.pem");
  fs.mkdirSync(join(pub, ".."), { recursive: true });
  const key = join(dir, "key.pem");
  execFileSync(
    "openssl",
    [
      "genpkey",
      "-algorithm",
      "RSA",
      "-pkeyopt",
      "rsa_keygen_bits:2048",
      "-out",
      key,
    ],
    { stdio: "pipe" },
  );
  execFileSync("openssl", ["pkey", "-in", key, "-pubout", "-out", pub], {
    stdio: "pipe",
  });
  fs.mkdirSync(dist);
  artifacts(dist);
  const plan = join(dir, "plan.json");
  fs.writeFileSync(
    plan,
    JSON.stringify({ tag: "v1.2.3", commit, notes: "Changed" }),
  );
  const out = execFileSync(
    process.execPath,
    [
      join(repo, "scripts/package-p4-update.mjs"),
      "package",
      "--dist",
      dist,
      "--plan",
      plan,
    ],
    {
      env: {
        ...process.env,
        GITHUB_SHA: commit,
        UPDATE_SIGNING_KEY: fs.readFileSync(key, "utf8"),
      },
      encoding: "utf8",
    },
  );
  assert.ok(!out.includes("PRIVATE KEY"));
  assert.deepEqual(fs.readdirSync(join(repo, ".artifacts")), []);
  execFileSync("openssl", [
    "dgst",
    "-sha256",
    "-verify",
    pub,
    "-signature",
    join(dist, "waveform-p4-manifest.sig"),
    join(dist, "waveform-p4-manifest.json"),
  ]);
  const manifest = JSON.parse(
    fs.readFileSync(join(dist, "waveform-p4-manifest.json")),
  );
  assert.equal(manifest.layout, p4.LAYOUT);
  assert.equal(manifest.target, p4.TARGET);
  assert.equal(manifest.version, "1.2.3");
  assert.deepEqual(
    fs.readdirSync(dist).sort(),
    [
      p4.IMAGE,
      "waveform-p4-manifest.json",
      "waveform-p4-manifest.sig",
      "p4-update-build.json",
      "p4-dependencies.lock",
      "p4-release.json",
      "p4-notes.md",
      "P4-SHA256SUMS",
    ].sort(),
  );
  for (const line of fs
    .readFileSync(join(dist, "P4-SHA256SUMS"), "utf8")
    .trim()
    .split("\n")) {
    const [hash, name] = line.split("  ");
    assert.equal(p4.digest(join(dist, name)), hash);
  }
  assert.throws(() =>
    p4.packageUpdate(dist, { tag: "v1.2.3", commit: "bad" }, commit, key, pub),
  );
});
test("temporary signing key mode and cleanup on failure", (t) => {
  const dir = fixture(t);
  let key;
  p4.withSigningKey("test-only", dir, (path) => {
    key = path;
    assert.equal(fs.statSync(path).mode & 0o777, 0o600);
  });
  assert.equal(fs.existsSync(key), false);
  assert.throws(
    () =>
      p4.withSigningKey("test-only", dir, (path) => {
        key = path;
        throw Error("signing failed");
      }),
    /signing failed/,
  );
  assert.equal(fs.existsSync(key), false);
});
test("partition layouts retain data locations, encryption and slot bounds", () => {
  for (const productionMode of [false, true]) {
    const entries = fs
      .readFileSync(
        join(
          p4.APP,
          productionMode ? "partitions.production.csv" : "partitions.csv",
        ),
        "utf8",
      )
      .split("\n")
      .filter((l) => l.trim() && !l.startsWith("#"))
      .map((l) => l.split(",").map((s) => s.trim()))
      .map(([name, kind, sub, offset, size, flags]) => ({
        name,
        sub,
        offset: Number(offset),
        size: size.endsWith("M")
          ? Number(size.slice(0, -1)) * 1024 ** 2
          : Number(size),
        flags,
      }))
      .sort((a, b) => a.offset - b.offset);
    for (let i = 1; i < entries.length; i++)
      assert.ok(
        entries[i - 1].offset + entries[i - 1].size <= entries[i].offset,
      );
    assert.ok(entries.at(-1).offset + entries.at(-1).size <= 0x2000000);
    if (productionMode) {
      assert.ok(entries[0].offset >= 0x11000);
      const key = entries.find((e) => e.name === "nvs_keys");
      assert.deepEqual(
        [key.sub, key.size, key.flags],
        ["nvs_keys", 4096, "encrypted"],
      );
      for (const name of ["nvs", "storage", "phone_tls"])
        assert.equal(entries.find((e) => e.name === name).flags, "");
    } else {
      for (const [name, offset, size] of [
        ["nvs", 0x9000, 0x6000],
        ["music_model", 0x810000, 4 * 1024 ** 2],
        ["storage", 0xc10000, 3 * 1024 ** 2],
      ]) {
        const e = entries.find((e) => e.name === name);
        assert.deepEqual([e.offset, e.size], [offset, size]);
      }
      for (const n of ["ota_0", "ota_1"]) {
        const e = entries.find((e) => e.name === n);
        assert.equal(e.size, 0x800000);
        assert.equal(e.offset % 0x10000, 0);
      }
    }
  }
});
test("production orchestration builds and verifies both signed binaries, failing closed", (t) => {
  const dir = fixture(t),
    key = join(dir, "key.pem");
  fs.writeFileSync(key, "test", { mode: 0o600 });
  const good =
    production.REQUIRED.map((n) => `CONFIG_${n}=y`).join("\n") +
    '\nCONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.production.csv"';
  let calls = [];
  const output = join(dir, "factory");
  production.build(output, key, "1.2.3", "sdk", {
    checkSdk: () => {},
    run: (command, args, options) => {
      calls.push(args);
      assert.equal(options.env.WAVEFORM_DEVICE_ACRCLOUD, "1");
      if (calls.length === 1) fs.writeFileSync(join(output, "sdkconfig"), good);
    },
  });
  assert.equal(calls.length, 3);
  assert.ok(calls[0][0].endsWith("/idf.sh"));
  assert.ok(calls[1].at(-1).endsWith("/bootloader/bootloader.bin"));
  assert.ok(calls[2].at(-1).endsWith("/waveform_p4_display.bin"));
  assert.throws(
    () =>
      production.build(output, key, "1.2.3", "sdk", {
        checkSdk: () => {},
        run: () => {},
      }),
    /EEXIST/,
  );
  fs.chmodSync(key, 0o644);
  assert.throws(
    () =>
      production.build(join(dir, "bad"), key, "1.2.3", "sdk", {
        checkSdk: () => {},
      }),
    /private/,
  );
  assert.equal(fs.existsSync(join(dir, "bad")), false);
  assert.match(
    fs.readFileSync("scripts/espsecure.sh", "utf8"),
    /exec python -m espsecure/,
  );
});
test("CI uses Node production and packaging entry points and handles empty version", () => {
  const workflow = fs.readFileSync(".github/workflows/validate.yml", "utf8");
  assert.match(workflow, /node scripts\/build-production.mjs/);
  assert.match(workflow, /node scripts\/package-p4-update.mjs record/);
  assert.ok(workflow.includes("${{ inputs.release-version || '0.0.0' }}"));
  const assignment = workflow
    .split("  production-security:")[1]
    .match(/^\s*(version=.*)$/m)[1];
  for (const value of [undefined, "", "1.2.3"]) {
    const env = { ...process.env };
    delete env.WAVEFORM_RELEASE_VERSION;
    if (value !== undefined) env.WAVEFORM_RELEASE_VERSION = value;
    assert.equal(
      execFileSync("sh", ["-eu", "-c", assignment + '\nprintf %s "$version"'], {
        env,
        encoding: "utf8",
      }),
      value || "0.0.0",
    );
  }
});

test("verify CLI needs no publisher key and rejects wrong source", (t) => {
  const dir = fixture(t);
  artifacts(dir);
  const plan = join(dir, "plan.json");
  fs.writeFileSync(plan, JSON.stringify({ tag: "v1.2.3", commit }));
  p4.main(["verify", "--dist", dir, "--plan", plan], { GITHUB_SHA: commit });
  assert.throws(
    () =>
      p4.main(["verify", "--dist", dir, "--plan", plan], {
        GITHUB_SHA: "wrong",
      }),
    /commit/,
  );
  assert.throws(
    () =>
      p4.main(["package", "--dist", dir, "--plan", plan], {
        GITHUB_SHA: commit,
      }),
    /UPDATE_SIGNING_KEY/,
  );
});

test("unknown and secure layouts fail before manifest signing", (t) => {
  const dir = fixture(t);
  artifacts(dir);
  const provenance = join(dir, "p4-update-build.json");
  const metadata = JSON.parse(fs.readFileSync(provenance));
  const plan = { tag: "v1.2.3", commit, notes: "fixture" };
  assert.throws(
    () =>
      p4.packageUpdate(
        dir,
        plan,
        commit,
        "missing-private",
        "missing-public",
        "boot-public",
      ),
    /development/,
  );
  for (const [layout, message] of [
    ["unknown", /Unknown P4 layout/],
    [p4.SECURE_LAYOUT, /approved secure-boot public key/],
  ]) {
    fs.writeFileSync(provenance, JSON.stringify({ ...metadata, layout }));
    assert.throws(
      () =>
        p4.packageUpdate(
          dir,
          plan,
          commit,
          "missing-private",
          "missing-public",
        ),
      message,
    );
    assert.equal(fs.existsSync(join(dir, "waveform-p4-manifest.json")), false);
  }
});

test("version must match the complete input, including trailing line breaks", () => {
  assert.throws(
    () => p4.validateImage(image("1.2.3\n"), "1.2.3\n"),
    /Invalid release version/,
  );
  assert.throws(
    () =>
      production.build("unused", "unused", "1.2.3\n", "unused", {
        checkSdk: () => {
          throw Error("SDK reached");
        },
      }),
    /Semantic version required/,
  );
});
