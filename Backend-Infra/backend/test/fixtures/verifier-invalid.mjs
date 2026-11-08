const mode = process.argv[2] ?? "invalid";

if (mode === "malformed") {
  process.stdout.write("not-json");
  process.exitCode = 0;
} else if (mode === "internal") {
  process.stdout.write(JSON.stringify({ ok: false, kind: "internal" }));
  process.exitCode = 4;
} else if (mode === "unavailable") {
  process.stdout.write(JSON.stringify({ ok: false, kind: "unavailable" }));
  process.exitCode = 3;
} else if (mode === "signal") {
  process.kill(process.pid, "SIGTERM");
} else {
  process.stdout.write(JSON.stringify({ ok: false, kind: "invalid_token" }));
  process.exitCode = 2;
}
