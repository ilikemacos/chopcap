#!/usr/bin/env node
/* The home page shows Chopcap programs next to the output they produce.
 * This runs every one of those programs through the real chopcap binary and
 * checks the printed output really is what the page claims.
 *
 *   node website/check-examples.js
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');
const os = require('os');

const ROOT = path.join(__dirname, '..');
const BIN = path.join(ROOT, 'build/chopcap');

if (!fs.existsSync(BIN)) {
  console.log("  (skipped: build/chopcap not found — run 'make' first)");
  process.exit(0);
}

const html = fs.readFileSync(path.join(__dirname, 'index.html'), 'utf8');

const unescape = (s) => s
  .replace(/&lt;/g, '<').replace(/&gt;/g, '>')
  .replace(/&quot;/g, '"').replace(/&#39;/g, "'")
  .replace(/&nbsp;/g, ' ').replace(/&amp;/g, '&');

/* Each pair is: a chopcap code block, then the output box that follows it. */
const pairs = [];
const blockRe = /<pre class="code"><code data-lang="chopcap">([\s\S]*?)<\/code><\/pre>[\s\S]*?<pre class="output-box">([\s\S]*?)<\/pre>/g;
let m;
while ((m = blockRe.exec(html)) !== null) {
  pairs.push({ code: unescape(m[1]), expected: unescape(m[2]).replace(/\s+$/, '') });
}

if (pairs.length === 0) {
  console.log('  no code/output pairs found on the home page — is the site built?');
  process.exit(1);
}

let ok = 0;
let bad = 0;

pairs.forEach(function (pair, i) {
  const file = path.join(os.tmpdir(), `chopcap-site-check-${process.pid}-${i}.chop`);
  fs.writeFileSync(file, pair.code + '\n');
  let actual;
  try {
    actual = execFileSync(BIN, [file], { encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe'] });
  } catch (err) {
    // A failing program still prints its friendly error, which some samples show.
    actual = (err.stdout || '') + (err.stderr || '');
  }
  fs.unlinkSync(file);

  // Error samples name the temporary file; the page names hello.chop.
  actual = actual.replace(new RegExp(`chopcap-site-check-${process.pid}-${i}\\.chop`, 'g'), 'hello.chop');
  actual = actual.replace(/^\s+|\s+$/g, '');
  const expected = pair.expected.replace(/^\s+|\s+$/g, '');

  if (actual === expected) {
    ok++;
  } else {
    bad++;
    console.log(`  MISMATCH in home page sample ${i + 1}:`);
    console.log('    the page says:');
    expected.split('\n').forEach((l) => console.log(`      ${l}`));
    console.log('    chopcap prints:');
    actual.split('\n').forEach((l) => console.log(`      ${l}`));
  }
});

console.log(`  ${ok} of ${pairs.length} home page samples print exactly what the page shows`);
if (bad) process.exit(1);
