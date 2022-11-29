var process = require('child_process');
function runCommand(hookScript, workingDirectory) {
  console.log(`${hookScript}`);
  return process.execSync(hookScript, {stdio: [this.stdin, this.stdout, this.stderr], encoding: 'buffer', cwd: workingDirectory});
}

let fs = require('fs')
let os = require('os')
if (os.platform() === "win32") {
  fs.copyFileSync('gqlite/build/Release/gqlite.dll', 'build/Release/gqlite.dll')
  // runCommand('copy gqlite/build/Release/gqlite.dll build/Release/', '.')
}
else if (os.platform() === "linux") {
  runCommand('cp gqlite/build/libgqlite.so build/Release/', '.')
}
else if (os.platform() === "darwin") {
  runCommand('cp gqlite/build/libgqlite.dylib build/Release/', '.')
  // runCommand('cp gqlite/build/Release/libgqlite.a build/Release/', '.')
}