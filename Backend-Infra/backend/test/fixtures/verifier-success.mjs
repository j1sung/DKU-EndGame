let input = "";
process.stdin.setEncoding("utf8");
process.stdin.on("data", (chunk) => {
  input += chunk;
});
process.stdin.on("end", () => {
  const request = JSON.parse(input);
  process.stdout.write(
    JSON.stringify({ ok: true, playerId: process.argv[2] ?? request.candidatePlayerId }),
  );
});
