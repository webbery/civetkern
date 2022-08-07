console.info("┏ Build Dependencies")
console.info("")
console.info("  downloading gqlite")
var process = require('child_process');
function runCommand(hookScript) {
  console.log(`${hookScript}`);
  return process.execSync(hookScript, {stdio: [this.stdin, this.stdout, this.stderr], encoding: 'buffer'});
}
let error = runCommand('git clone --recursive https://github.com/webbery/gqlite.git gqlite')
error = runCommand('cmake -B gqlite/build -DCMAKE_BUILD_TYPE=Release')