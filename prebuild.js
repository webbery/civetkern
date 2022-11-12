console.info("┏ Build Dependencies")
console.info("")
console.info("  downloading gqlite")
var process = require('child_process');
function runCommand(hookScript, workingDirectory) {
  console.log(`${hookScript}`);
  return process.execSync(hookScript, {stdio: [this.stdin, this.stdout, this.stderr], encoding: 'buffer', cwd: workingDirectory});
}
let error
let fs = require('fs')
let os = require('os')
if (os.platform() === "win32") {
  if (fs.existsSync('gqlite')) {
    runCommand('git pull', './gqlite')
  } else {
    runCommand('git clone --recursive https://github.com/webbery/gqlite.git gqlite', '.')
  }
  runCommand('cmake -B build -DCMAKE_BUILD_TYPE=Release -DGQLITE_BUILD_SHARED=TRUE -DGQLITE_BUILD_TEST=FALSE gqlite', 'gqlite')
  runCommand('cmake --build build --config Release', 'gqlite')
} else {
  if (!fs.existsSync('gqlite')) {
    runCommand('git clone --recursive https://github.com/webbery/gqlite.git gqlite', '.')
  }
  runCommand('cmake -B gqlite/build -DCMAKE_BUILD_TYPE=Release -DGQLITE_BUILD_SHARED=TRUE -DGQLITE_BUILD_TEST=FALSE gqlite', '.')
    runCommand('cmake --build gqlite/build --config Release', '.')
}
