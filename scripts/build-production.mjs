#!/usr/bin/env node
// Manufacturing-only build verification. Never flashes or burns eFuses.
import * as fs from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync } from "node:child_process";
import { parseArgs } from "node:util";
import { verifySdk } from "./check-idf-security.mjs";
const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
export const APP = join(ROOT, "firmware/esp32/p4_display");
export const REQUIRED = [
  "SECURE_BOOT",
  "SECURE_BOOT_V2_ENABLED",
  "SECURE_BOOT_V2_RSA_ENABLED",
  "SECURE_BOOT_BUILD_SIGNED_BINARIES",
  "SECURE_FLASH_ENC_ENABLED",
  "SECURE_FLASH_ENCRYPTION_MODE_RELEASE",
  "NVS_ENCRYPTION",
  "NVS_SEC_KEY_PROTECT_USING_FLASH_ENC",
  "SECURE_ENABLE_SECURE_ROM_DL_MODE",
  "WAVEFORM_PRODUCTION_SECURITY",
  "BOOTLOADER_APP_ROLLBACK_ENABLE",
];
export const FORBIDDEN = [
  "SECURE_BOOT_INSECURE",
  "SECURE_BOOT_ALLOW_JTAG",
  "SECURE_INSECURE_ALLOW_DL_MODE",
  "SECURE_BOOT_ENABLE_AGGRESSIVE_KEY_REVOKE",
  "EFUSE_VIRTUAL",
  "SECURE_FLASH_ENCRYPTION_MODE_DEVELOPMENT",
];
export function verifyConfig(config) {
  const lines = new Set(config.split(/\r?\n/));
  for (const name of REQUIRED)
    if (!lines.has(`CONFIG_${name}=y`))
      throw Error(`Missing production protection: ${name}`);
  for (const name of FORBIDDEN)
    if (lines.has(`CONFIG_${name}=y`))
      throw Error(`Forbidden production setting: ${name}`);
  if (
    !lines.has(
      'CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.production.csv"',
    )
  )
    throw Error("Production partition layout required");
}
export function build(
  output,
  key,
  version,
  idf,
  { run = execFileSync, checkSdk = verifySdk } = {},
) {
  if (!/^\d+\.\d+\.\d+$/.test(version))
    throw Error("Semantic version required");
  checkSdk(idf);
  output = resolve(output);
  key = resolve(key);
  if (fs.statSync(key).mode & 0o077)
    throw Error("Signing key must be private (0600)");
  if (/[\n\r";]/.test(key)) throw Error("Unsafe key path");
  fs.mkdirSync(dirname(output), { recursive: true });
  fs.mkdirSync(output, { mode: 0o700 });
  const signing = join(output, "signing.defaults");
  fs.writeFileSync(signing, `CONFIG_SECURE_BOOT_SIGNING_KEY="${key}"\n`, {
    mode: 0o600,
    flag: "wx",
  });
  const env = {
    ...process.env,
    IDF_PATH: resolve(idf),
    WAVEFORM_DEVICE_ACRCLOUD: "1",
    WAVEFORM_RELEASE_VERSION: version,
  };
  run(
    "bash",
    [
      join(ROOT, "scripts/idf.sh"),
      "-C",
      APP,
      "-B",
      join(output, "build"),
      `-DSDKCONFIG=${join(output, "sdkconfig")}`,
      `-DSDKCONFIG_DEFAULTS=${join(APP, "sdkconfig.defaults")};${join(APP, "sdkconfig.defaults.production")};${signing}`,
      "build",
    ],
    { env, stdio: "inherit" },
  );
  verifyConfig(fs.readFileSync(join(output, "sdkconfig"), "utf8"));
  for (const binary of ["bootloader/bootloader.bin", "waveform_p4_display.bin"])
    run(
      "bash",
      [
        join(ROOT, "scripts/espsecure.sh"),
        "verify-signature",
        "--version",
        "2",
        "--keyfile",
        key,
        join(output, "build", binary),
      ],
      { env, stdio: "inherit" },
    );
  console.log(
    "Verified factory build. NOT flashed. First boot permanently enables hardware protections.",
  );
}
export function main(args = process.argv.slice(2)) {
  const { values: a } = parseArgs({
    args,
    options: Object.fromEntries(
      ["output", "signing-key", "version", "idf"].map((n) => [
        n,
        { type: "string" },
      ]),
    ),
  });
  if (!a.output || !a["signing-key"] || !a.version)
    throw Error("--output, --signing-key and --version are required");
  process.umask(0o077);
  build(
    a.output,
    a["signing-key"],
    a.version,
    a.idf ?? process.env.IDF_PATH ?? join(ROOT, ".artifacts/esp-idf"),
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
