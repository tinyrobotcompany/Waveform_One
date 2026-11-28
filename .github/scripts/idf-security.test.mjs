import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import {
  mkdirSync,
  mkdtempSync,
  writeFileSync,
  rmSync,
  readFileSync,
} from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";
import { execFileSync, spawnSync } from "node:child_process";
import test from "node:test";
import {
  verifySdk,
  SDK_COMMIT,
  PARSER_SHA256,
  PARSER_PATH,
} from "../../scripts/check-idf-security.mjs";

const root = fileURLToPath(new URL("../../", import.meta.url));
function fixture(run) {
  const parent = join(root, ".artifacts");
  mkdirSync(parent, { recursive: true });
  const directory = mkdtempSync(join(parent, "sdk-test-"));
  try {
    execFileSync("git", ["init", "-q", directory]);
    mkdirSync(join(directory, "components/esp_http_server/src"), {
      recursive: true,
    });
    const parser = "fixture parser\n";
    writeFileSync(join(directory, PARSER_PATH), parser);
    writeFileSync(join(directory, "export.sh"), "# fixture export\n");
    execFileSync("git", ["-C", directory, "add", "."]);
    execFileSync("git", [
      "-C",
      directory,
      "-c",
      "user.name=Fixture",
      "-c",
      "user.email=fixture@example.invalid",
      "commit",
      "-qm",
      "fixture",
    ]);
    const commit = execFileSync("git", ["-C", directory, "rev-parse", "HEAD"], {
      encoding: "utf8",
    }).trim();
    return run(directory, {
      commit,
      parserSha256: createHash("sha256").update(parser).digest("hex"),
    });
  } finally {
    rmSync(directory, { recursive: true, force: true });
  }
}

test("production pin matches the reviewed Tempo SDK and fixed HTTP parser", () => {
  assert.equal(SDK_COMMIT, "051e4c4eb20346e043cdd47bc388fde75ff8af63");
  assert.equal(
    PARSER_SHA256,
    "2121bc06c00656fd2ad85b1a8a7eb3fe5d6010ccf3756b57d5520c3b8286df3e",
  );
});
test("accepts a matching checkout and parser", () =>
  fixture((sdk, pin) => verifySdk(sdk, pin)));
test("rejects another commit even with a matching parser", () =>
  fixture((sdk, pin) => {
    assert.throws(
      () => verifySdk(sdk, { ...pin, commit: "0".repeat(40) }),
      /commit/,
    );
  }));
test("rejects modified parser bytes at the expected commit", () =>
  fixture((sdk, pin) => {
    writeFileSync(join(sdk, PARSER_PATH), "modified\n");
    assert.throws(() => verifySdk(sdk, pin), /parser/);
  }));
test("rejects absent parser and nonexistent SDK", () =>
  fixture((sdk, pin) => {
    rmSync(join(sdk, PARSER_PATH));
    assert.throws(() => verifySdk(sdk, pin));
    assert.throws(() => verifySdk(join(sdk, "missing"), pin));
  }));
test("rejects modified SDK scripts even when commit and HTTP parser match", () =>
  fixture((sdk, pin) => {
    writeFileSync(join(sdk, "export.sh"), "echo changed\n");
    assert.throws(() => verifySdk(sdk, pin), /tracked files/);
    execFileSync("git", ["-C", sdk, "add", "export.sh"]);
    assert.throws(() => verifySdk(sdk, pin), /tracked files/);
  }));
test("release invokes the Node P4 packager after setting up pinned Node", () => {
  const workflow = readFileSync(join(root, ".github/workflows/release.yml"), "utf8");
  const release = workflow.split("\n  release:\n")[1];
  assert.match(release, /actions\/setup-node@[a-f0-9]{40}/);
  assert.match(release, /node scripts\/package-p4-update\.mjs verify/);
  assert.match(release, /node scripts\/package-p4-update\.mjs package/);
  assert.doesNotMatch(release, /python3 scripts\/package-p4-update\.py/);
});
test("CLI rejects missing arguments and an unsupported SDK", () =>
  fixture((sdk) => {
    for (const args of [[], [sdk]]) {
      const result = spawnSync(
        process.execPath,
        ["scripts/check-idf-security.mjs", ...args],
        { cwd: root, encoding: "utf8" },
      );
      assert.notEqual(result.status, 0);
    }
  }));
test("build wrapper refuses stale SDK before sourcing export or running build", () =>
  fixture((sdk) => {
    writeFileSync(join(sdk, "export.sh"), "echo UNSAFE_EXPORT\n");
    const result = spawnSync("bash", ["scripts/idf.sh", "build"], {
      cwd: root,
      env: { ...process.env, IDF_PATH: sdk },
      encoding: "utf8",
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /commit/);
    assert.doesNotMatch(result.stdout, /UNSAFE_EXPORT/);
  }));
test("setup refuses an existing unsupported checkout without changing it", () =>
  fixture((sdk, pin) => {
    writeFileSync(join(sdk, "install.sh"), "echo UNSAFE_INSTALL\n");
    const result = spawnSync("bash", ["scripts/setup-idf.sh"], {
      cwd: root,
      env: { ...process.env, IDF_PATH: sdk },
      encoding: "utf8",
    });
    assert.notEqual(result.status, 0);
    assert.match(result.stderr, /commit/);
    assert.doesNotMatch(result.stdout, /UNSAFE_INSTALL/);
    assert.equal(
      execFileSync("git", ["-C", sdk, "rev-parse", "HEAD"], {
        encoding: "utf8",
      }).trim(),
      pin.commit,
    );
  }));
test("both CI profiles run the same pinned SDK setup and verification", () => {
  const workflow = readFileSync(
    join(root, ".github/workflows/validate.yml"),
    "utf8",
  );
  for (const section of [
    workflow.split("\n  firmware:\n")[1].split("\n  production-security:\n")[0],
    workflow.split("\n  production-security:\n")[1],
  ]) {
    assert.match(section, /espressif\/idf:v6\.1\.1/);
    assert.match(section, /scripts\/setup-idf\.sh/);
    assert.match(section, /actions\/setup-node@/);
  }
  assert.match(workflow, /bash "\$GITHUB_WORKSPACE\/scripts\/idf\.sh" build/);
});
