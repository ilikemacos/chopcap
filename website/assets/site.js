/* Chopcap website behaviour: theme, copy buttons, syntax highlighting,
   the runnable code panes and the playground. No dependencies. */
(function () {
  'use strict';

  /* ------------------------------------------------------------ theme --- */

  const root = document.documentElement;
  const stored = (function () {
    try { return localStorage.getItem('chopcap-theme'); } catch (e) { return null; }
  })();
  if (stored === 'dark' || stored === 'light') root.setAttribute('data-theme', stored);

  function currentTheme() {
    const set = root.getAttribute('data-theme');
    if (set) return set;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  }

  function paintToggle(btn) {
    btn.textContent = currentTheme() === 'dark' ? '☀' : '☾';
    btn.setAttribute('aria-label', `Switch to ${currentTheme() === 'dark' ? 'light' : 'dark'} theme`);
  }

  document.querySelectorAll('.theme-toggle').forEach(function (btn) {
    paintToggle(btn);
    btn.addEventListener('click', function () {
      const next = currentTheme() === 'dark' ? 'light' : 'dark';
      root.setAttribute('data-theme', next);
      try { localStorage.setItem('chopcap-theme', next); } catch (e) { /* private mode */ }
      document.querySelectorAll('.theme-toggle').forEach(paintToggle);
    });
  });

  /* ------------------------------------------------------------- copy --- */

  document.querySelectorAll('[data-copy]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      const text = btn.getAttribute('data-copy');
      const done = function () {
        const was = btn.textContent;
        btn.textContent = 'Copied';
        btn.classList.add('done');
        setTimeout(function () { btn.textContent = was; btn.classList.remove('done'); }, 1600);
      };
      if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(done, fallback);
      } else fallback();

      function fallback() {
        const area = document.createElement('textarea');
        area.value = text;
        area.setAttribute('readonly', '');
        area.style.position = 'fixed';
        area.style.opacity = '0';
        document.body.appendChild(area);
        area.select();
        try { document.execCommand('copy'); done(); } catch (e) { /* nothing to do */ }
        document.body.removeChild(area);
      }
    });
  });

  /* ------------------------------------------------- syntax highlighting - */

  const KEYWORDS = [
    'say', 'ask', 'if', 'else', 'while', 'for', 'in', 'to', 'by', 'fun',
    'return', 'break', 'skip', 'and', 'or', 'not', 'true', 'false',
    'nothing', 'use', 'as', 'try', 'catch', 'error'
  ];
  const KEYWORD_SET = new Set(KEYWORDS);
  const esc = (s) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');

  /* A tiny tokeniser, only good enough to colour a code sample. */
  function highlight(src) {
    let out = '';
    let i = 0;
    while (i < src.length) {
      const c = src[i];
      if (c === '#') {
        let j = src.indexOf('\n', i);
        if (j === -1) j = src.length;
        out += '<span class="tok-com">' + esc(src.slice(i, j)) + '</span>';
        i = j;
        continue;
      }
      if (c === '"' || c === "'") {
        let j = i + 1;
        while (j < src.length && src[j] !== c && src[j] !== '\n') {
          if (src[j] === '\\') j++;
          j++;
        }
        j = Math.min(j + 1, src.length);
        out += '<span class="tok-str">' + esc(src.slice(i, j)) + '</span>';
        i = j;
        continue;
      }
      if (/[0-9]/.test(c) && !/[A-Za-z0-9_]/.test(src[i - 1] || '')) {
        let j = i;
        while (j < src.length && /[0-9._]/.test(src[j])) j++;
        out += '<span class="tok-num">' + esc(src.slice(i, j)) + '</span>';
        i = j;
        continue;
      }
      if (/[A-Za-z_]/.test(c)) {
        let j = i;
        while (j < src.length && /[A-Za-z0-9_]/.test(src[j])) j++;
        const word = src.slice(i, j);
        if (KEYWORD_SET.has(word)) out += '<span class="tok-key">' + word + '</span>';
        else if (src[j] === '(') out += '<span class="tok-fun">' + esc(word) + '</span>';
        else out += esc(word);
        i = j;
        continue;
      }
      out += esc(c);
      i++;
    }
    return out;
  }

  document.querySelectorAll('pre.code > code[data-lang="chopcap"]').forEach(function (el) {
    el.innerHTML = highlight(el.textContent);
  });

  /* --------------------------------------------------------- runnables --- */

  function renderOutput(target, result) {
    target.textContent = '';
    if (!result.output) {
      const span = document.createElement('span');
      span.className = 'muted';
      span.textContent = '(this program printed nothing)';
      target.appendChild(span);
      return;
    }
    if (result.ok) {
      target.textContent = result.output.replace(/\n+$/, '');
      return;
    }
    // Split the friendly error off so it can be coloured.
    const marker = result.output.indexOf('\nChopcap Error');
    const before = marker >= 0 ? result.output.slice(0, marker) : '';
    const errorPart = marker >= 0 ? result.output.slice(marker) : result.output;
    if (before) target.appendChild(document.createTextNode(before.replace(/\n+$/, '') + '\n'));
    const span = document.createElement('span');
    span.className = 'err';
    span.textContent = errorPart.replace(/^\n+/, '').replace(/\n+$/, '');
    target.appendChild(span);
  }

  function inputsFrom(el) {
    if (!el) return [];
    return el.value.split('\n').filter(function (line, idx, all) {
      return idx < all.length - 1 || line !== '';
    });
  }

  function wireRunner(pane) {
    const editor = pane.querySelector('.editor');
    const button = pane.querySelector('.run');
    const consoleEl = pane.querySelector('.console');
    if (!editor || !button || !consoleEl) return;
    const inputsEl = pane.dataset.inputs ? document.querySelector(pane.dataset.inputs) : null;

    function run() {
      if (typeof Chopcap === 'undefined') {
        consoleEl.textContent = 'The Chopcap runtime could not be loaded.';
        return;
      }
      button.disabled = true;
      try {
        const result = Chopcap.run(editor.value, {
          input: inputsFrom(inputsEl),
          filename: pane.dataset.filename || null,
          maxSteps: 4000000
        });
        renderOutput(consoleEl, result);
      } catch (err) {
        consoleEl.textContent = 'Something went wrong running that: ' + err;
      } finally {
        button.disabled = false;
      }
    }

    button.addEventListener('click', run);
    editor.addEventListener('keydown', function (e) {
      if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') { e.preventDefault(); run(); }
      if (e.key === 'Tab') {
        e.preventDefault();
        const start = editor.selectionStart;
        const end = editor.selectionEnd;
        editor.value = editor.value.slice(0, start) + '    ' + editor.value.slice(end);
        editor.selectionStart = editor.selectionEnd = start + 4;
      }
    });
    if (pane.dataset.autorun !== 'no') run();
  }

  document.querySelectorAll('[data-runner]').forEach(wireRunner);

  /* ---------------------------------------------------------- examples --- */

  document.querySelectorAll('[data-tabs]').forEach(function (group) {
    const tabs = group.querySelectorAll('.tab');
    const panels = group.querySelectorAll('[data-panel]');
    tabs.forEach(function (tab) {
      tab.addEventListener('click', function () {
        tabs.forEach((t) => t.setAttribute('aria-selected', String(t === tab)));
        const want = tab.getAttribute('data-for');
        panels.forEach(function (panel) {
          panel.hidden = panel.getAttribute('data-panel') !== want;
        });
      });
    });
  });

  /* --------------------------------------------------- playground picker - */

  const picker = document.querySelector('.pg-picker');
  if (picker) {
    const editor = document.querySelector('#pg-editor');
    const inputs = document.querySelector('#pg-inputs');
    picker.querySelectorAll('button').forEach(function (btn) {
      btn.addEventListener('click', function () {
        const source = document.querySelector('#sample-' + btn.dataset.sample);
        if (!source || !editor) return;
        editor.value = source.textContent.replace(/^\n/, '').replace(/\s+$/, '') + '\n';
        if (inputs) inputs.value = source.dataset.inputs ? source.dataset.inputs.split('|').join('\n') : '';
        picker.querySelectorAll('button').forEach((b) => b.setAttribute('aria-selected', String(b === btn)));
        const run = document.querySelector('#pg-pane .run');
        if (run) run.click();
      });
    });
  }

  /* ------------------------------------------------- docs active section - */

  const docNav = document.querySelector('.doc-nav');
  if (docNav && 'IntersectionObserver' in window) {
    const links = new Map();
    docNav.querySelectorAll('a[href^="#"]').forEach(function (a) {
      links.set(a.getAttribute('href').slice(1), a);
    });
    const headings = Array.from(document.querySelectorAll('.doc-body h2[id]'));
    const seen = new Set();
    const observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) seen.add(entry.target.id);
        else seen.delete(entry.target.id);
      });
      let active = null;
      for (const h of headings) if (seen.has(h.id)) { active = h.id; break; }
      if (!active) return;
      links.forEach(function (a, id) { a.classList.toggle('active', id === active); });
    }, { rootMargin: '-70px 0px -60% 0px' });
    headings.forEach((h) => observer.observe(h));
  }
})();
