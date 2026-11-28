#!/usr/bin/env node
// Signed P4 OTA assets bound to the validated build and source commit.
import * as fs from "node:fs";
import { createHash } from "node:crypto";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync } from "node:child_process";
import { parseArgs } from "node:util";
const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
export const APP = join(ROOT, "firmware/esp32/p4_display");
export const TARGET = "waveshare-p4-7b-rev3-32mb";
export const LAYOUT = "waveform-ab8-https-v1";
export const SECURE_LAYOUT = "waveform-ab8-secure-v1";
export const IMAGE = "waveform-p4.bin";
export const digest = (path) =>
  createHash("sha256").update(fs.readFileSync(path)).digest("hex");
// Retain Python's ASCII JSON encoding for existing signed/publication artifacts.
function json(value, { sorted = false, pretty = false, spaced = false } = {}) {
  if (sorted)
    value = Object.fromEntries(
      Object.keys(value)
        .sort()
        .map((key) => [key, value[key]]),
    );
  let result = JSON.stringify(value, null, pretty ? 2 : undefined);
  if (spaced) result = result.replace(/":/g, '": ').replace(/,"/g, ', "');
  return (
    result.replace(
      /[\u007f-\uffff]/g,
      (c) => "\\u" + c.charCodeAt(0).toString(16).padStart(4, "0"),
    ) + "\n"
  );
}
export function validateImage(blob, version) {
  if (
    !/^(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})\.(0|[1-9][0-9]{0,5})$/.test(
      version,
    )
  )
    throw Error("Invalid release version");
  if (blob.length < 1024 || blob.length > 0x800000 || blob[0] !== 0xe9)
    throw Error("Invalid ESP application image or OTA slot overflow");
  const field = (start, end) =>
    blob.subarray(start, end).toString().split("\0")[0];
  if (
    blob.readUInt32LE(32) !== 0xabcd5432 ||
    field(48, 80) !== version ||
    field(80, 112) !== "waveform_p4_display"
  )
    throw Error("Image descriptor does not match the P4 release");
  if (
    /-----BEGIN (?:EC |RSA )?PRIVATE KEY-----[\r\n]+[A-Za-z0-9+/=\r\n]{64,}-----END (?:EC |RSA )?PRIVATE KEY-----/.test(
      blob.toString("latin1"),
    )
  )
    throw Error("Private key material in release image");
}
export function verifySecureImage(
  image,
  publicKey,
  { run = execFileSync } = {},
) {
  if (!publicKey)
    throw Error("Production OTA requires the approved secure-boot public key");
  run(
    "bash",
    [
      join(ROOT, "scripts/espsecure.sh"),
      "verify-signature",
      "--version",
      "2",
      "--keyfile",
      publicKey,
      image,
    ],
    { stdio: "inherit" },
  );
}
export function record(
  build,
  dist,
  version,
  commit,
  secureBootPublicKey = null,
) {
  const source = join(build, "waveform_p4_display.bin"),
    blob = fs.readFileSync(source);
  validateImage(blob, version);
  const config = fs.readFileSync(join(build, "config/sdkconfig.h"), "utf8");
  if (!config.includes("#define CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE 1"))
    throw Error("Rollback must be enabled");
  for (const header of ["config/sdkconfig.h", "bootloader/config/sdkconfig.h"])
    if (
      !fs
        .readFileSync(join(build, header), "utf8")
        .includes("#define CONFIG_BOOTLOADER_CACHE_32BIT_ADDR_QUAD_FLASH 1")
    )
      throw Error(`32-bit flash addressing must be enabled in ${header}`);
  const secure = config.includes(
    "#define CONFIG_WAVEFORM_PRODUCTION_SECURITY 1",
  );
  if (secure) verifySecureImage(source, secureBootPublicKey);
  else if (secureBootPublicKey)
    throw Error("Secure-boot key supplied for a development image");
  const credentials = fs.readFileSync(
    join(build, "esp-idf/main/acrcloud_config.h"),
    "utf8",
  );
  for (const name of ["HOST", "ACCESS_KEY", "ACCESS_SECRET"])
    if (!new RegExp(`^#define ACRCLOUD_${name} ""$`, "m").test(credentials))
      throw Error(
        "Public OTA images must use device ACRCloud credentials, not embed them",
      );
  fs.mkdirSync(dist, { recursive: true });
  fs.writeFileSync(join(dist, IMAGE), blob);
  fs.copyFileSync(
    join(APP, "dependencies.lock"),
    join(dist, "p4-dependencies.lock"),
  );
  fs.writeFileSync(
    join(dist, "p4-update-build.json"),
    json(
      {
        commit,
        version,
        sha256: digest(join(dist, IMAGE)),
        layout: secure ? SECURE_LAYOUT : LAYOUT,
        dependency_lock_sha256: digest(join(dist, "p4-dependencies.lock")),
      },
      { sorted: true, spaced: true },
    ),
  );
}
export function verifyBuild(dist, version, commit) {
  const provenance = JSON.parse(
    fs.readFileSync(join(dist, "p4-update-build.json"), "utf8"),
  );
  if (
    provenance.commit !== commit ||
    provenance.version !== version ||
    provenance.sha256 !== digest(join(dist, IMAGE)) ||
    provenance.dependency_lock_sha256 !==
      digest(join(dist, "p4-dependencies.lock"))
  )
    throw Error("P4 artifacts do not match the validated source/version");
  const blob = fs.readFileSync(join(dist, IMAGE));
  validateImage(blob, version);
  return { provenance, blob };
}
export function packageUpdate(
  dist,
  plan,
  commit,
  key,
  publicKey,
  secureBootPublicKey = null,
) {
  if (plan.commit !== commit)
    throw Error("Release plan does not match the validated source");
  const version = plan.tag.replace(/^v/, ""),
    { provenance, blob } = verifyBuild(dist, version, commit),
    layout = provenance.layout ?? LAYOUT;
  if (![LAYOUT, SECURE_LAYOUT].includes(layout))
    throw Error("Unknown P4 layout");
  if (layout === SECURE_LAYOUT)
    verifySecureImage(join(dist, IMAGE), secureBootPublicKey);
  else if (secureBootPublicKey)
    throw Error("Secure-boot key supplied for a development image");
  const manifest = {
    schema: 1,
    source_commit: commit,
    version,
    target: TARGET,
    layout,
    size: blob.length,
    sha256: createHash("sha256").update(blob).digest("hex"),
    dependency_lock_sha256: provenance.dependency_lock_sha256,
    url: `https://github.com/tinyrobotcompany/Waveform_One/releases/download/v${version}/${IMAGE}`,
  };
  const path = join(dist, "waveform-p4-manifest.json"),
    signature = join(dist, "waveform-p4-manifest.sig");
  fs.writeFileSync(path, json(manifest, { sorted: true }));
  execFileSync(
    "openssl",
    ["dgst", "-sha256", "-sign", key, "-out", signature, path],
    { stdio: "inherit" },
  );
  execFileSync(
    "openssl",
    ["dgst", "-sha256", "-verify", publicKey, "-signature", signature, path],
    { stdio: "inherit" },
  );
  fs.writeFileSync(join(dist, "p4-release.json"), json(plan, { pretty: true }));
  fs.writeFileSync(join(dist, "p4-notes.md"), plan.notes);
  const names = [
    IMAGE,
    "waveform-p4-manifest.json",
    "waveform-p4-manifest.sig",
    "p4-dependencies.lock",
    "p4-release.json",
    "p4-notes.md",
  ];
  fs.writeFileSync(
    join(dist, "P4-SHA256SUMS"),
    names.map((name) => `${digest(join(dist, name))}  ${name}\n`).join(""),
  );
}
export function withSigningKey(value, scratch, callback) {
  fs.mkdirSync(scratch, { recursive: true });
  const dir = fs.mkdtempSync(join(scratch, "p4-signing-"));
  fs.chmodSync(dir, 0o700);
  try {
    const key = join(dir, "signing.pem");
    fs.writeFileSync(key, value, { mode: 0o600, flag: "wx" });
    return callback(key);
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
}
export function main(args = process.argv.slice(2), env = process.env) {
  const { values: a, positionals } = parseArgs({
    args,
    allowPositionals: true,
    options: Object.fromEntries(
      ["dist", "build", "version", "secure-boot-public-key", "plan"].map(
        (n) => [n, { type: "string" }],
      ),
    ),
  });
  const action = positionals[0];
  if (
    positionals.length !== 1 ||
    !["record", "verify", "package"].includes(action) ||
    !a.dist ||
    !env.GITHUB_SHA
  )
    throw Error("Action, --dist and GITHUB_SHA are required");
  if (action === "record") {
    if (!a.build || !a.version)
      throw Error("record requires --build and --version");
    return record(
      a.build,
      a.dist,
      a.version,
      env.GITHUB_SHA,
      a["secure-boot-public-key"],
    );
  }
  if (!a.plan) throw Error("package requires --plan");
  const plan = JSON.parse(fs.readFileSync(a.plan, "utf8"));
  if (plan.commit !== env.GITHUB_SHA)
    throw Error("Release plan commit mismatch");
  if (action === "verify")
    return verifyBuild(a.dist, plan.tag.replace(/^v/, ""), env.GITHUB_SHA);
  if (!env.UPDATE_SIGNING_KEY) throw Error("UPDATE_SIGNING_KEY is required");
  return withSigningKey(
    env.UPDATE_SIGNING_KEY,
    join(ROOT, ".artifacts"),
    (key) =>
      packageUpdate(
        a.dist,
        plan,
        env.GITHUB_SHA,
        key,
        join(APP, "main/ota_public.pem"),
        a["secure-boot-public-key"],
      ),
  );
}
if (
  process.argv[1] &&
  resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  try {
    main();
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
