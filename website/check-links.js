#!/usr/bin/env node
/* Checks every link in the generated site: internal pages exist, internal
 * anchors exist, and assets are present.  External links are listed, not
 * fetched. */
'use strict';

const fs = require('fs');
const path = require('path');

const SITE = __dirname;
const pages = fs.readdirSync(SITE).filter((f) => f.endsWith('.html'));

let problems = 0;
const external = new Set();

const idsOf = (html) => new Set(
  Array.from(html.matchAll(/\sid="([^"]+)"/g)).map((m) => m[1])
);

const docs = {};
for (const page of pages) docs[page] = fs.readFileSync(path.join(SITE, page), 'utf8');

for (const page of pages) {
  const html = docs[page];
  const ids = idsOf(html);
  const refs = Array.from(html.matchAll(/(?:href|src)="([^"]+)"/g)).map((m) => m[1]);

  for (const ref of refs) {
    if (/^(https?:|mailto:|data:)/.test(ref)) { external.add(ref); continue; }

    if (ref.startsWith('#')) {
      const id = ref.slice(1);
      if (id && !ids.has(id)) {
        console.log(`  BROKEN ${page}  ->  ${ref}   (no such anchor on this page)`);
        problems++;
      }
      continue;
    }

    const [target, anchor] = ref.split('#');
    const file = target === '/' ? 'index.html' : target.replace(/^\//, '');
    const full = path.join(SITE, file);
    if (!fs.existsSync(full)) {
      console.log(`  BROKEN ${page}  ->  ${ref}   (no such file: website/${file})`);
      problems++;
      continue;
    }
    if (anchor) {
      const targetIds = docs[file] ? idsOf(docs[file]) : idsOf(fs.readFileSync(full, 'utf8'));
      if (!targetIds.has(anchor)) {
        console.log(`  BROKEN ${page}  ->  ${ref}   (no such anchor in ${file})`);
        problems++;
      }
    }
  }
}

console.log(`  ${pages.length} pages checked, ${external.size} external links (not fetched)`);
if (problems) {
  console.log(`  ${problems} broken link${problems === 1 ? '' : 's'}`);
  process.exit(1);
}
console.log('  no broken internal links');
