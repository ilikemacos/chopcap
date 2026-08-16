#!/usr/bin/env node
/* Builds the Chopcap website.
 *
 *   node website/build.js
 *
 * Everything the site shows comes from the repository itself, so the site
 * cannot drift away from the language:
 *
 *   docs/*.md         -> docs.html      (one page, with a sidebar)
 *   examples/*.chop   -> examples.html  (the real programs, runnable)
 *   install.sh        -> install.sh     (so /install.sh serves the real thing)
 *   src/chopcap.h     -> the version number shown everywhere
 *   website/pages/*   -> index.html, playground.html
 *
 * No dependencies: a small Markdown subset is rendered below.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..');
const OUT = __dirname;
const REPO = 'https://github.com/ilikemacos/chopcap';

const VERSION = (function () {
  const header = fs.readFileSync(path.join(ROOT, 'src/chopcap.h'), 'utf8');
  const m = header.match(/CHOPCAP_VERSION\s+"([^"]+)"/);
  if (!m) throw new Error('could not find CHOPCAP_VERSION in src/chopcap.h');
  return m[1];
})();

const INSTALL_CMD = `curl -fsSL ${REPO.replace('https://github.com', 'https://raw.githubusercontent.com')}/main/install.sh | sh`;

/* ------------------------------------------------------------------ */
/* A small Markdown renderer — only the subset the docs actually use.  */
/* ------------------------------------------------------------------ */

const escapeHtml = (s) =>
  s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

function slug(text) {
  return text.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '');
}

function rewriteLink(href) {
  if (/^(https?:|#|mailto:)/.test(href)) return href;
  const md = href.match(/^([\w.-]+)\.md(#.*)?$/);
  if (md) return '#' + slug(md[1]);
  if (href === 'docs/' || href === '../docs' || href === '../docs/') return '#installation';
  if (href.startsWith('../')) return `${REPO}/blob/main/${href.replace(/^\.\.\//, '')}`;
  return `${REPO}/blob/main/docs/${href}`;
}

function inline(text) {
  let out = '';
  let i = 0;
  while (i < text.length) {
    const c = text[i];

    if (c === '`') {                                   // inline code
      const end = text.indexOf('`', i + 1);
      if (end > i) {
        out += '<code>' + escapeHtml(text.slice(i + 1, end)) + '</code>';
        i = end + 1;
        continue;
      }
    }

    if (c === '[') {                                   // [label](href)
      const close = text.indexOf(']', i);
      if (close > i && text[close + 1] === '(') {
        const paren = text.indexOf(')', close);
        if (paren > close) {
          const label = text.slice(i + 1, close);
          const href = text.slice(close + 2, paren);
          out += `<a href="${escapeHtml(rewriteLink(href))}">${inline(label)}</a>`;
          i = paren + 1;
          continue;
        }
      }
    }

    if (text.startsWith('**', i)) {                    // **bold**
      const end = text.indexOf('**', i + 2);
      if (end > i) {
        out += '<strong>' + inline(text.slice(i + 2, end)) + '</strong>';
        i = end + 2;
        continue;
      }
    }

    out += escapeHtml(c);
    i++;
  }
  return out;
}

function renderCells(row) {
  return row.trim().replace(/^\||\|$/g, '').split('|').map((c) => c.trim());
}

/* Renders a Markdown document. `shift` pushes every heading down a level so
   several documents can live on one page under one <h1>. */
function markdown(src, shift, onHeading) {
  const lines = src.split('\n');
  const out = [];
  let i = 0;

  const flushList = (items, ordered) => {
    const tag = ordered ? 'ol' : 'ul';
    out.push(`<${tag}>` + items.map((it) => `<li>${inline(it)}</li>`).join('') + `</${tag}>`);
  };

  while (i < lines.length) {
    const line = lines[i];

    if (line.trim() === '') { i++; continue; }

    if (line.startsWith('```')) {                       // fenced code
      const lang = line.slice(3).trim();
      const body = [];
      i++;
      while (i < lines.length && !lines[i].startsWith('```')) body.push(lines[i++]);
      i++;
      const attr = lang === 'chopcap' ? ' data-lang="chopcap"' : '';
      out.push(`<pre class="code"><code${attr}>${escapeHtml(body.join('\n'))}</code></pre>`);
      continue;
    }

    const heading = line.match(/^(#{1,6})\s+(.*)$/);
    if (heading) {
      const level = Math.min(6, heading[1].length + shift);
      const text = heading[2].trim();
      const id = slug(text.replace(/`/g, ''));
      if (onHeading) onHeading(level, text, id);
      out.push(`<h${level} id="${id}">${inline(text)}</h${level}>`);
      i++;
      continue;
    }

    if (/^(-{3,}|\*{3,})$/.test(line.trim())) { out.push('<hr>'); i++; continue; }

    if (line.startsWith('> ')) {                         // blockquote
      const body = [];
      while (i < lines.length && lines[i].startsWith('>')) body.push(lines[i++].replace(/^>\s?/, ''));
      out.push('<blockquote>' + markdown(body.join('\n'), shift) + '</blockquote>');
      continue;
    }

    if (line.trim().startsWith('|')) {                   // table
      const rows = [];
      while (i < lines.length && lines[i].trim().startsWith('|')) rows.push(lines[i++]);
      if (rows.length >= 2) {
        const head = renderCells(rows[0]);
        const body = rows.slice(2).map(renderCells);
        out.push('<table><thead><tr>' +
          head.map((c) => `<th>${inline(c)}</th>`).join('') +
          '</tr></thead><tbody>' +
          body.map((r) => '<tr>' + r.map((c) => `<td>${inline(c)}</td>`).join('') + '</tr>').join('') +
          '</tbody></table>');
        continue;
      }
      rows.forEach((r) => out.push(`<p>${inline(r)}</p>`));
      continue;
    }

    const bullet = line.match(/^[-*]\s+(.*)$/);
    const numbered = line.match(/^\d+\.\s+(.*)$/);
    if (bullet || numbered) {
      const ordered = Boolean(numbered);
      const items = [];
      while (i < lines.length) {
        const m = ordered ? lines[i].match(/^\d+\.\s+(.*)$/) : lines[i].match(/^[-*]\s+(.*)$/);
        if (m) { items.push(m[1]); i++; continue; }
        if (/^\s+\S/.test(lines[i]) && items.length) {   // continuation line
          items[items.length - 1] += ' ' + lines[i].trim();
          i++;
          continue;
        }
        break;
      }
      flushList(items, ordered);
      continue;
    }

    const para = [];                                     // paragraph
    while (i < lines.length && lines[i].trim() !== '' &&
           !lines[i].startsWith('```') && !lines[i].startsWith('#') &&
           !lines[i].trim().startsWith('|') && !lines[i].startsWith('> ') &&
           !/^[-*]\s+/.test(lines[i]) && !/^\d+\.\s+/.test(lines[i])) {
      para.push(lines[i++]);
    }
    if (para.length) out.push(`<p>${inline(para.join('\n'))}</p>`);
    else i++;
  }

  return out.join('\n');
}

/* ------------------------------------------------------------------ */
/* Page layout                                                         */
/* ------------------------------------------------------------------ */

const LOGO = `<svg viewBox="0 0 64 64" aria-hidden="true"><g transform="rotate(-45 32 32)"><rect x="25" y="1" width="14" height="28" rx="7" fill="var(--logo-accent)"/><rect x="25" y="35" width="14" height="28" rx="7" fill="var(--logo-ink)"/></g></svg>`;

function nav(current) {
  const item = (href, label, optional) =>
    `<a href="${href}"${current === label ? ' aria-current="page"' : ''}` +
    `${optional ? ' data-optional' : ''}>${label}</a>`;
  return `<nav class="site-nav">
        ${item('/docs.html', 'Docs')}
        ${item('/examples.html', 'Examples', true)}
        ${item('/playground.html', 'Playground')}
        <a href="${REPO}" rel="noopener" data-optional>GitHub</a>
        <button class="theme-toggle" type="button">☾</button>
      </nav>`;
}

function layout(opts) {
  return `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>${escapeHtml(opts.title)}</title>
<meta name="description" content="${escapeHtml(opts.description)}">
<meta name="color-scheme" content="light dark">
<meta property="og:title" content="${escapeHtml(opts.title)}">
<meta property="og:description" content="${escapeHtml(opts.description)}">
<meta property="og:type" content="website">
<link rel="icon" href="/assets/favicon.svg" type="image/svg+xml">
<link rel="stylesheet" href="/assets/style.css">
</head>
<body>
<header class="site-header">
  <div class="wrap">
    <a class="brand" href="/">${LOGO}<span>Chopcap</span><span class="version">${VERSION}</span></a>
    ${nav(opts.current)}
  </div>
</header>

${opts.body}

<footer class="site-footer">
  <div class="wrap">
    <div class="footer-grid">
      <div class="footer-about">
        <a class="brand" href="/">${LOGO}<span>Chopcap</span></a>
        <p>A very small programming language. Programming without the clutter.</p>
      </div>
      <div>
        <h5>Learn</h5>
        <ul>
          <li><a href="/docs.html#getting-started">Getting started</a></li>
          <li><a href="/docs.html#language-basics">Language basics</a></li>
          <li><a href="/docs.html#standard-library">Standard library</a></li>
          <li><a href="/docs.html#specification">Specification</a></li>
        </ul>
      </div>
      <div>
        <h5>Use</h5>
        <ul>
          <li><a href="/docs.html#installation">Installation</a></li>
          <li><a href="/docs.html#cli">CLI reference</a></li>
          <li><a href="/examples.html">Examples</a></li>
          <li><a href="/playground.html">Playground</a></li>
        </ul>
      </div>
      <div>
        <h5>Project</h5>
        <ul>
          <li><a href="${REPO}" rel="noopener">GitHub</a></li>
          <li><a href="${REPO}/releases" rel="noopener">Releases</a></li>
          <li><a href="${REPO}/blob/main/CHANGELOG.md" rel="noopener">Changelog</a></li>
          <li><a href="${REPO}/blob/main/LICENSE" rel="noopener">MIT License</a></li>
        </ul>
      </div>
    </div>
    <div class="footer-bottom">
      <span>Chopcap <code>${VERSION}</code> — an early release. MIT licensed.</span>
      <span>Built with a C compiler and not much else.</span>
    </div>
  </div>
</footer>

<script src="/assets/chopcap.js"></script>
<script src="/assets/site.js"></script>
</body>
</html>
`;
}

/* ------------------------------------------------------------------ */
/* Pages                                                               */
/* ------------------------------------------------------------------ */

const DOC_ORDER = [
  ['installation', 'Start here'],
  ['getting-started', null],
  ['language-basics', null],
  ['variables', 'The language'],
  ['types', null],
  ['conditions', null],
  ['loops', null],
  ['functions', null],
  ['lists', null],
  ['modules', null],
  ['error-handling', null],
  ['standard-library', 'Reference'],
  ['cli', null],
  ['examples', null],
  ['specification', null, 'Specification']
];

function buildDocs() {
  const sections = [];
  const navItems = [];

  for (const [name, group, navLabel] of DOC_ORDER) {
    const file = path.join(ROOT, 'docs', `${name}.md`);
    const src = fs.readFileSync(file, 'utf8');
    let title = null;
    const html = markdown(src, 1, (level, text, id) => {
      if (level === 2 && !title) title = { text, id };
    });
    if (!title) throw new Error(`${name}.md has no top-level heading`);
    // The anchor is the file name, so cross-document links resolve.
    const anchored = html.replace(`id="${title.id}"`, `id="${name}"`);
    navItems.push({ group, name, text: navLabel || title.text });
    sections.push(anchored);
  }

  let navHtml = '<div class="doc-nav-inner">';
  for (const item of navItems) {
    if (item.group) navHtml += `</div><h4>${item.group}</h4><div class="doc-nav-inner">`;
    navHtml += `<a href="#${item.name}">${escapeHtml(item.text)}</a>`;
  }
  navHtml += '</div>';
  navHtml = navHtml.replace('<div class="doc-nav-inner"></div>', '');

  const body = `<main class="wrap">
  <div class="doc-layout">
    <aside class="doc-nav">${navHtml}</aside>
    <article class="doc-body">
      <h1>Chopcap documentation</h1>
      <p>Everything about Chopcap ${VERSION} on one page. It is short on purpose —
      the whole language fits in an afternoon.</p>
      ${sections.join('\n')}
    </article>
  </div>
</main>`;

  fs.writeFileSync(path.join(OUT, 'docs.html'), layout({
    title: 'Documentation — Chopcap',
    description: `The complete manual for Chopcap ${VERSION}: installation, the language, the standard library, the CLI and the specification.`,
    current: 'Docs',
    body
  }));
  return navItems.length;
}

function buildExamples() {
  const dir = path.join(ROOT, 'examples');
  const files = fs.readdirSync(dir).filter((f) => f.endsWith('.chop')).sort();

  // Examples that read input come with answers so they can be run here.
  const ANSWERS = {
    'greeting.chop': ['Ada'],
    'guess.chop': ['50', '25', '37', '43', '40', '41', '42']
  };

  const cards = [];
  const panes = [];

  for (const file of files) {
    const source = fs.readFileSync(path.join(dir, file), 'utf8');
    const firstComment = (source.match(/^#\s*(.+)$/m) || [null, ''])[1];
    const id = 'ex-' + slug(file.replace('.chop', ''));
    const answers = ANSWERS[file] || [];

    cards.push(`<a class="card" href="#${id}">
        <h3><code>${escapeHtml(file)}</code></h3>
        <p>${escapeHtml(firstComment)}</p>
      </a>`);

    panes.push(`<section id="${id}">
    <div class="wrap">
      <h2>${escapeHtml(file)}</h2>
      <p class="lede">${escapeHtml(firstComment)}
        <a href="${REPO}/blob/main/examples/${file}" rel="noopener">View on GitHub</a></p>
      ${/use files/.test(source) ? '<p class="pg-label">This one uses the <code>files</code> module, which a browser cannot provide — run it with the <code>chopcap</code> command to see it work.</p>' : ''}
      ${answers.length ? `<p class="pg-label">This one asks questions. The answers used below are:
        <code>${escapeHtml(answers.join(', '))}</code></p>
      <textarea class="pg-inputs" id="${id}-inputs" hidden>${escapeHtml(answers.join('\n'))}</textarea>` : ''}
      <div class="pane" data-runner data-filename="${escapeHtml(file)}" data-autorun="no"${answers.length ? ` data-inputs="#${id}-inputs"` : ''}>
        <div class="pane-bar">
          <span class="dots"><i></i><i></i><i></i></span>
          <span>${escapeHtml(file)}</span>
          <button class="run" type="button">Run</button>
        </div>
        <textarea class="editor" spellcheck="false" rows="${Math.min(30, source.split('\n').length + 1)}">${escapeHtml(source.replace(/\s+$/, ''))}</textarea>
        <pre class="console"><span class="muted">Press Run to see what this prints.</span></pre>
      </div>
    </div>
  </section>`);
  }

  const body = `<main>
  <section style="border-top:none">
    <div class="wrap">
      <h2>Example programs</h2>
      <p class="lede">Every program here is a real file in the repository, run by the test
      suite on every change. You can edit and run any of them right on this page — the
      playground uses a JavaScript build of Chopcap that is checked against the same tests
      as the <code>chopcap</code> command.</p>
      <div class="cards">${cards.join('\n')}</div>
    </div>
  </section>
  ${panes.join('\n')}
</main>`;

  fs.writeFileSync(path.join(OUT, 'examples.html'), layout({
    title: 'Examples — Chopcap',
    description: 'Nine complete Chopcap programs you can read, edit and run in the browser.',
    current: 'Examples',
    body
  }));
  return files.length;
}

function buildPage(name, opts) {
  const body = fs.readFileSync(path.join(__dirname, 'pages', `${name}.html`), 'utf8')
    .replace(/\{\{VERSION\}\}/g, VERSION)
    .replace(/\{\{REPO\}\}/g, REPO)
    .replace(/\{\{INSTALL\}\}/g, INSTALL_CMD);
  fs.writeFileSync(path.join(OUT, `${name}.html`), layout(Object.assign({ body }, opts)));
}

function copyInstaller() {
  fs.copyFileSync(path.join(ROOT, 'install.sh'), path.join(OUT, 'install.sh'));
}

function favicon() {
  fs.writeFileSync(path.join(OUT, 'assets/favicon.svg'),
`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect width="64" height="64" rx="14" fill="#14161a"/>
  <g transform="rotate(-45 32 32)">
    <rect x="26" y="6" width="12" height="23" rx="6" fill="#f2542d"/>
    <rect x="26" y="35" width="12" height="23" rx="6" fill="#fbfaf8"/>
  </g>
</svg>
`);
}

/* ------------------------------------------------------------------ */

favicon();
const docCount = buildDocs();
const exampleCount = buildExamples();
buildPage('index', {
  title: 'Chopcap — programming without the clutter',
  description: `Chopcap ${VERSION} is a very small, very readable programming language for macOS. One binary, no dependencies, friendly errors.`,
  current: 'Home'
});
buildPage('playground', {
  title: 'Playground — Chopcap',
  description: 'Write and run Chopcap in your browser. No installation needed.',
  current: 'Playground'
});
copyInstaller();

console.log(`Chopcap website built for version ${VERSION}`);
console.log(`  index.html`);
console.log(`  docs.html       (${docCount} chapters from docs/)`);
console.log(`  examples.html   (${exampleCount} programs from examples/)`);
console.log(`  playground.html`);
console.log(`  install.sh      (copied from the repository root)`);
