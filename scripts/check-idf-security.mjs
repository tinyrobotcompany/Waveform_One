#!/usr/bin/env node
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync } from "node:child_process";

// Reviewed Tempo SDK: includes the upstream Content-Length truncation fix.
export const SDK_COMMIT = "051e4c4eb20346e043cdd47bc388fde75ff8af63";
export const PARSER_PATH = "components/esp_http_server/src/httpd_parse.c";
export const PARSER_SHA256 =
  "2121bc06c00656fd2ad85b1a8a7eb3fe5d6010ccf3756b57d5520c3b8286df3e";

export function verifySdk(
  sdk,
  { commit = SDK_COMMIT, parserSha256 = PARSER_SHA256 } = {},
) {
  const actual = execFileSync("git", ["-C", sdk, "rev-parse", "HEAD"], {
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  }).trim();
  if (actual !== commit)
    throw new Error(`ESP-IDF commit must be ${commit}; found ${actual}`);
  const hash = createHash("sha256")
    .update(readFileSync(join(sdk, PARSER_PATH)))
    .digest("hex");
  if (hash !== parserSha256)
    throw new Error(
      "ESP-IDF HTTP parser differs from the reviewed upstream fix",
    );
  try {
    execFileSync("git", ["-C", sdk, "diff", "--quiet", "HEAD", "--"], {stdio: "ignore"});
  } catch {
    throw new Error("ESP-IDF tracked files differ from the reviewed commit");
  }
}

if (
  process.argv[1] &&
  resolve(process.argv[1]) === fileURLToPath(import.meta.url)
) {
  try {
    if (process.argv.length !== 3)
      throw new Error(
        "Usage: node scripts/check-idf-security.mjs <ESP-IDF directory>",
      );
    verifySdk(resolve(process.argv[2]));
    console.log(`Verified ESP-IDF ${SDK_COMMIT} and fixed HTTP parser`);
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
