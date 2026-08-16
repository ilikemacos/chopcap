/* Checks the JavaScript implementation used by the website playground
 * against the very same golden files as the C implementation.
 *
 *   node tests/js_parity.js
 *
 * A few tests are skipped because a browser cannot do what they test:
 * reading files, real sleeping, and loading other .chop files.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const Chopcap = require(path.join(ROOT, 'website/assets/chopcap.js'));
const PROGRAMS = path.join(ROOT, 'tests/programs');

const SKIP = {
  '16_use_file': 'loads another .chop file, which a browser cannot do',
  '22_err_module': 'lists the available modules, and the playground has no files module',
  '28_files': 'uses the files module, which the playground does not provide',
  '29_time': 'measures real sleeping, which a browser tab must not do',
  'helpers': 'is a module loaded by another test'
};

let pass = 0, fail = 0, skipped = 0;
const failures = [];

for (const file of fs.readdirSync(PROGRAMS).sort()) {
  if (!file.endsWith('.chop')) continue;
  const name = file.replace(/\.chop$/, '');

  if (SKIP[name]) {
    console.log(`  -- ${name}  (skipped: ${SKIP[name]})`);
    skipped++;
    continue;
  }

  const expectedPath = path.join(PROGRAMS, `${name}.expected`);
  if (!fs.existsSync(expectedPath)) {
    failures.push(`${name}: no .expected file`);
    fail++;
    continue;
  }

  const source = fs.readFileSync(path.join(PROGRAMS, file), 'utf8');
  const inPath = path.join(PROGRAMS, `${name}.in`);
  const input = fs.existsSync(inPath)
    ? fs.readFileSync(inPath, 'utf8').split('\n').filter((l, i, a) => i < a.length - 1 || l !== '')
    : [];

  const expected = fs.readFileSync(expectedPath, 'utf8').replace(/\n+$/, '');
  let actual;
  try {
    actual = Chopcap.run(source, { input, filename: file }).output.replace(/\n+$/, '');
  } catch (err) {
    failures.push(`${name}: threw ${err && err.stack ? err.stack.split('\n')[0] : err}`);
    fail++;
    continue;
  }

  if (actual === expected) {
    console.log(`  ok ${name}`);
    pass++;
  } else {
    console.log(`  FAIL ${name}`);
    const e = expected.split('\n');
    const a = actual.split('\n');
    for (let i = 0; i < Math.max(e.length, a.length); i++) {
      if (e[i] !== a[i]) {
        console.log(`      line ${i + 1}:`);
        console.log(`        C  : ${JSON.stringify(e[i])}`);
        console.log(`        JS : ${JSON.stringify(a[i])}`);
      }
    }
    failures.push(name);
    fail++;
  }
}

console.log();
console.log(`  ${pass} matched, ${fail} differed, ${skipped} skipped`);
if (fail > 0) {
  console.log('\n  Differences in: ' + failures.join(', '));
  process.exit(1);
}
