/* Chopcap in JavaScript.
 *
 * This is a second implementation of the same language, written so the
 * playground on the website can actually run your code instead of pretending
 * to.  It mirrors src/lexer.c, src/parser.c and src/interp.c closely, and it
 * is checked against the very same tests/programs golden files as the C
 * implementation (see tests/js_parity.js), so the two agree.
 *
 * Differences from the real chopcap command, all of them because a browser
 * tab is not a computer you own:
 *   - the `files` module is not available
 *   - `ask` reads from a list of answers supplied up front
 *
 * Numbers follow the C implementation: whole numbers are BigInt, decimals
 * are ordinary JavaScript numbers.
 *
 * MIT licensed, part of the Chopcap project.
 */
(function (root) {
  'use strict';

  const VERSION = '0.1.0';

  /* ---------------------------------------------------------------- */
  /* Errors                                                            */
  /* ---------------------------------------------------------------- */

  class ChopcapError extends Error {
    constructor(line, message, hint) {
      super(message);
      this.line = line;
      this.chopMessage = message;
      this.hint = hint || null;
    }
  }

  const fail = (line, message, hint) => {
    throw new ChopcapError(line, message, hint);
  };

  /* ---------------------------------------------------------------- */
  /* Values                                                            */
  /* ---------------------------------------------------------------- */

  const isInt = (v) => typeof v === 'bigint';
  const isDec = (v) => typeof v === 'number';
  const isNum = (v) => isInt(v) || isDec(v);
  const isText = (v) => typeof v === 'string';
  const isList = (v) => Array.isArray(v);

  function typeName(v) {
    if (v === null || v === undefined) return 'nothing';
    if (typeof v === 'boolean') return 'boolean';
    if (isInt(v)) return 'number';
    if (isDec(v)) return 'decimal';
    if (isText(v)) return 'text';
    if (isList(v)) return 'list';
    if (v.kind === 'module') return 'module';
    return 'function';
  }

  /* Matches printf's "%.10g", which is what the C implementation uses. */
  function formatDecimal(d) {
    if (Number.isNaN(d)) return 'not-a-number';
    if (!Number.isFinite(d)) return d > 0 ? 'infinity' : '-infinity';
    if (d === 0) return Object.is(d, -0) ? '-0' : '0';
    const P = 10;
    let e = Math.floor(Math.log10(Math.abs(d)));
    // toPrecision can round up into the next power of ten.
    if (Math.abs(Number(d.toPrecision(P))) >= Math.pow(10, e + 1)) e += 1;
    if (e < -4 || e >= P) {
      let [m, ex] = d.toExponential(P - 1).split('e');
      if (m.includes('.')) m = m.replace(/0+$/, '').replace(/\.$/, '');
      const sign = ex[0] === '-' ? '-' : '+';
      const digits = ex.replace(/^[+-]/, '').padStart(2, '0');
      return `${m}e${sign}${digits}`;
    }
    let s = d.toFixed(Math.max(0, P - 1 - e));
    if (s.includes('.')) s = s.replace(/0+$/, '').replace(/\.$/, '');
    return s;
  }

  function toDisplay(v, quoted) {
    if (v === null || v === undefined) return 'nothing';
    if (typeof v === 'boolean') return v ? 'true' : 'false';
    if (isInt(v)) return v.toString();
    if (isDec(v)) return formatDecimal(v);
    if (isText(v)) {
      if (!quoted) return v;
      return '"' + v.replace(/\\/g, '\\\\').replace(/"/g, '\\"')
                    .replace(/\n/g, '\\n').replace(/\t/g, '\\t') + '"';
    }
    if (isList(v)) return '[' + v.map((x) => toDisplay(x, true)).join(', ') + ']';
    if (v.kind === 'module') return `<module ${v.name}>`;
    if (v.kind === 'native') return `<function ${v.name}>`;
    return `<function ${v.decl.name || 'anonymous'}>`;
  }

  const asText = (v) => toDisplay(v, false);
  const asRepr = (v) => toDisplay(v, true);

  function truthy(v) {
    if (v === null || v === undefined) return false;
    if (typeof v === 'boolean') return v;
    if (isInt(v)) return v !== 0n;
    if (isDec(v)) return v !== 0;
    if (isText(v)) return v.length > 0;
    if (isList(v)) return v.length > 0;
    return true;
  }

  function equal(a, b) {
    if (isNum(a) && isNum(b)) return Number(a) === Number(b);
    if (typeName(a) !== typeName(b)) return false;
    if (isList(a)) {
      if (a.length !== b.length) return false;
      return a.every((x, i) => equal(x, b[i]));
    }
    if (a === null || a === undefined) return b === null || b === undefined;
    return a === b;
  }

  const num = (v) => (isInt(v) ? Number(v) : v);

  /* ---------------------------------------------------------------- */
  /* Lexer                                                             */
  /* ---------------------------------------------------------------- */

  const KEYWORDS = new Set([
    'say', 'ask', 'if', 'else', 'while', 'for', 'in', 'fun',
    'return', 'break', 'skip', 'and', 'or', 'not', 'true', 'false',
    'nothing', 'use', 'as', 'try', 'catch', 'error'
  ]);

  function lex(src) {
    const toks = [];
    const indents = [0];
    let i = 0, line = 1, paren = 0, atLineStart = true;
    const n = src.length;
    const push = (type, value) => toks.push({ type, value, line });
    const isNameStart = (c) => /[A-Za-z_]/.test(c);
    const isNameChar = (c) => /[A-Za-z0-9_]/.test(c);

    while (i < n) {
      if (atLineStart && paren === 0) {
        let width = 0;
        while (i < n && (src[i] === ' ' || src[i] === '\t')) {
          width = src[i] === '\t' ? (Math.floor(width / 4) + 1) * 4 : width + 1;
          i++;
        }
        const c = src[i];
        if (!(c === undefined || c === '\n' || c === '#' || (c === '\r' && src[i + 1] === '\n'))) {
          const cur = indents[indents.length - 1];
          if (width > cur) {
            indents.push(width);
            push('INDENT');
          } else if (width < cur) {
            while (indents.length > 1 && width < indents[indents.length - 1]) {
              indents.pop();
              push('DEDENT');
            }
            if (width !== indents[indents.length - 1]) {
              fail(line, "this line's indentation does not match any open block.",
                   'Every line in the same block must start in the same column.');
            }
          }
        }
        atLineStart = false;
        if (i >= n) break;
      }

      const c = src[i];
      if (c === '\r') { i++; continue; }
      if (c === ' ' || c === '\t') { i++; continue; }
      if (c === '#') { while (i < n && src[i] !== '\n') i++; continue; }
      if (c === '\n') {
        i++;
        if (paren === 0) {
          const last = toks[toks.length - 1];
          if (last && last.type !== 'NEWLINE' && last.type !== 'INDENT' && last.type !== 'DEDENT') {
            push('NEWLINE');
          }
          atLineStart = true;
        }
        line++;
        continue;
      }

      if (c === '"' || c === "'") {
        const quote = c;
        const startLine = line;
        i++;
        let out = '';
        for (;;) {
          if (i >= n || src[i] === '\n') {
            fail(startLine, 'this text value is missing its closing quote.',
                 'Text values must open and close on the same line with ".');
          }
          let ch = src[i++];
          if (ch === quote) break;
          if (ch === '\\') {
            const e = src[i++];
            const map = { n: '\n', t: '\t', r: '\r', '\\': '\\', '"': '"', "'": "'" };
            if (!(e in map)) {
              fail(startLine, `\`\\${e}\` is not an escape Chopcap understands.`,
                   `Chopcap knows \\n, \\t, \\r, \\\\, \\" and \\'.`);
            }
            ch = map[e];
          }
          out += ch;
        }
        push('TEXT', out);
        continue;
      }

      if (/[0-9]/.test(c)) {
        let start = i;
        while (i < n && /[0-9_]/.test(src[i])) i++;
        let isFloat = false;
        if (src[i] === '.' && /[0-9]/.test(src[i + 1] || '')) {
          isFloat = true;
          i++;
          while (i < n && /[0-9_]/.test(src[i])) i++;
        }
        if (src[i] === 'e' || src[i] === 'E') {
          const save = i;
          i++;
          if (src[i] === '+' || src[i] === '-') i++;
          if (/[0-9]/.test(src[i] || '')) {
            isFloat = true;
            while (i < n && /[0-9]/.test(src[i])) i++;
          } else i = save;
        }
        const digits = src.slice(start, i).replace(/_/g, '');
        push(isFloat ? 'DEC' : 'INT', isFloat ? parseFloat(digits) : BigInt(digits));
        continue;
      }

      if (isNameStart(c)) {
        let start = i;
        while (i < n && isNameChar(src[i])) i++;
        const word = src.slice(start, i);
        push(KEYWORDS.has(word) ? word : 'NAME', word);
        continue;
      }

      i++;
      const two = c + (src[i] || '');
      if (two === '==' || two === '!=' || two === '<=' || two === '>=') {
        i++;
        push(two);
        continue;
      }
      if (c === '!') fail(line, '`!` is not a Chopcap operator.', 'Use `not` for negation, as in `not ready`.');
      if ('+-*/%^:,.=<>'.includes(c)) { push(c); continue; }
      if (c === '(' || c === '[') { paren++; push(c); continue; }
      if (c === ')' || c === ']') { if (paren) paren--; push(c); continue; }
      fail(line, `\`${c}\` is not something Chopcap understands here.`);
    }

    if (toks.length && toks[toks.length - 1].type !== 'NEWLINE') push('NEWLINE');
    while (indents.length > 1) { indents.pop(); push('DEDENT'); }
    push('EOF');
    return toks;
  }

  /* ---------------------------------------------------------------- */
  /* Parser                                                            */
  /* ---------------------------------------------------------------- */

  function describe(tok) {
    switch (tok.type) {
      case 'EOF': return 'the end of the file';
      case 'NEWLINE': return 'the end of the line';
      case 'INDENT': return 'an indented line';
      case 'DEDENT': return 'the end of the block';
      case 'TEXT': return 'a text value';
      case 'INT': case 'DEC': return 'a number';
      case 'NAME': return `\`${tok.value}\``;
      default: return `\`${tok.type}\``;
    }
  }

  function parse(toks) {
    let pos = 0;
    const cur = () => toks[pos];
    const at = (t) => toks[pos].type === t;
    const advance = () => (toks[pos].type === 'EOF' ? toks[pos] : toks[pos++]);
    const match = (t) => (at(t) ? (advance(), true) : false);
    const skipNewlines = () => { while (at('NEWLINE')) advance(); };
    /* `to` and `by` are only special inside a counting `for`, so they stay
     * usable as ordinary variable and parameter names everywhere else. */
    const matchWord = (word) =>
      (at('NAME') && cur().value === word ? (advance(), true) : false);

    function expect(t, what, hint) {
      if (match(t)) return true;
      fail(cur().line, `expected ${what} but found ${describe(cur())}.`, hint);
    }

    function blockAfterColon(what) {
      const hint = `Put a \`:\` at the end of the \`${what}\` line, then indent the lines below it.`;
      expect(':', '`:`', hint);
      expect('NEWLINE', 'the end of the line after `:`', hint);
      skipNewlines();
      expect('INDENT', 'an indented block', hint);
      const body = [];
      while (!at('DEDENT') && !at('EOF')) {
        if (at('NEWLINE')) { advance(); continue; }
        body.push(statement());
      }
      if (!at('EOF')) expect('DEDENT', 'the end of the block');
      return { type: 'Block', body };
    }

    function primary() {
      const t = cur();
      switch (t.type) {
        case 'INT': case 'DEC': advance(); return { type: 'Literal', value: t.value, line: t.line };
        case 'TEXT': advance(); return { type: 'Literal', value: t.value, line: t.line };
        case 'true': advance(); return { type: 'Literal', value: true, line: t.line };
        case 'false': advance(); return { type: 'Literal', value: false, line: t.line };
        case 'nothing': advance(); return { type: 'Literal', value: null, line: t.line };
        case 'NAME': advance(); return { type: 'Name', name: t.value, line: t.line };
        case 'ask': {
          advance();
          const stop = ['NEWLINE', 'EOF', ')', ':', ',', ']'];
          const prompt = stop.includes(cur().type) ? null : expression();
          return { type: 'Ask', prompt, line: t.line };
        }
        case '(': {
          advance();
          const e = expression();
          expect(')', '`)`');
          return e;
        }
        case '[': {
          advance();
          const items = [];
          skipNewlines();
          if (!at(']')) {
            do {
              skipNewlines();
              if (at(']')) break;
              items.push(expression());
              skipNewlines();
            } while (match(','));
          }
          expect(']', '`]` to close this list');
          return { type: 'ListLit', items, line: t.line };
        }
        default:
          fail(t.line, `expected a value but found ${describe(t)}.`);
      }
    }

    function postfix() {
      let node = primary();
      for (;;) {
        const t = cur();
        if (match('(')) {
          const args = [];
          skipNewlines();
          if (!at(')')) {
            do {
              skipNewlines();
              if (at(')')) break;
              args.push(expression());
              skipNewlines();
            } while (match(','));
          }
          expect(')', '`)` to close this call');
          node = { type: 'Call', callee: node, args, line: t.line };
        } else if (match('[')) {
          const index = expression();
          expect(']', '`]`');
          node = { type: 'Index', object: node, index, line: t.line };
        } else if (match('.')) {
          if (!at('NAME')) fail(cur().line, `expected a name after \`.\` but found ${describe(cur())}.`);
          const m = advance();
          node = { type: 'Member', object: node, name: m.value, line: m.line };
        } else {
          return node;
        }
      }
    }

    function power() {
      const base = postfix();
      const t = cur();
      if (match('^')) return { type: 'Binary', op: '^', left: base, right: unary(), line: t.line };
      return base;
    }

    function unary() {
      const t = cur();
      if (at('-') || at('not')) {
        advance();
        return { type: 'Unary', op: t.type, operand: unary(), line: t.line };
      }
      return power();
    }

    const LEVELS = [
      ['*', '/', '%'],
      ['+', '-'],
      ['==', '!=', '<', '<=', '>', '>=', 'in']
    ];

    function binaryLevel(level) {
      if (level < 0) return unary();
      let left = binaryLevel(level - 1);
      while (LEVELS[level].includes(cur().type)) {
        const t = advance();
        left = { type: 'Binary', op: t.type, left, right: binaryLevel(level - 1), line: t.line };
      }
      return left;
    }

    function andExpr() {
      let left = binaryLevel(2);
      while (at('and')) {
        const t = advance();
        left = { type: 'And', left, right: binaryLevel(2), line: t.line };
      }
      return left;
    }

    function expression() {
      let left = andExpr();
      while (at('or')) {
        const t = advance();
        left = { type: 'Or', left, right: andExpr(), line: t.line };
      }
      return left;
    }

    function endOfLine() {
      if (at('EOF') || at('DEDENT')) return;
      expect('NEWLINE', 'the end of the line');
    }

    function ifStatement(t) {
      const test = expression();
      const then = blockAfterColon('if');
      let otherwise = null;
      skipNewlines();
      if (at('else')) {
        advance();
        if (at('if')) { const i2 = advance(); otherwise = ifStatement(i2); }
        else otherwise = blockAfterColon('else');
      }
      return { type: 'If', test, then, otherwise, line: t.line };
    }

    function statement() {
      const t = cur();
      switch (t.type) {
        case 'say': {
          advance();
          const values = [];
          if (!at('NEWLINE') && !at('EOF')) {
            do { values.push(expression()); } while (match(','));
          }
          endOfLine();
          return { type: 'Say', values, line: t.line };
        }
        case 'if': advance(); return ifStatement(t);
        case 'while': {
          advance();
          const test = expression();
          return { type: 'While', test, body: blockAfterColon('while'), line: t.line };
        }
        case 'for': {
          advance();
          if (!at('NAME')) {
            fail(cur().line, `expected a variable name after \`for\` but found ${describe(cur())}.`,
                 'Write `for item in things:` or `for i in 1 to 10:`.');
          }
          const v = advance();
          expect('in', '`in`', 'Write `for item in things:`.');
          const first = expression();
          if (matchWord('to')) {
            const end = expression();
            const step = matchWord('by') ? expression() : null;
            return { type: 'ForRange', name: v.value, from: first, to: end, step,
                     body: blockAfterColon('for'), line: t.line };
          }
          return { type: 'ForIn', name: v.value, seq: first, body: blockAfterColon('for'), line: t.line };
        }
        case 'fun': {
          advance();
          if (!at('NAME')) {
            fail(cur().line, `expected a function name after \`fun\` but found ${describe(cur())}.`,
                 'Write `fun add(a, b):`.');
          }
          const nameTok = advance();
          expect('(', '`(`', 'Write `fun add(a, b):`.');
          const params = [];
          if (!at(')')) {
            do {
              if (!at('NAME')) fail(cur().line, `expected a parameter name but found ${describe(cur())}.`);
              params.push(advance().value);
            } while (match(','));
          }
          expect(')', '`)`');
          return { type: 'Fun', name: nameTok.value, params, body: blockAfterColon('fun'), line: t.line };
        }
        case 'return': {
          advance();
          const value = (at('NEWLINE') || at('EOF') || at('DEDENT')) ? null : expression();
          endOfLine();
          return { type: 'Return', value, line: t.line };
        }
        case 'break': advance(); endOfLine(); return { type: 'Break', line: t.line };
        case 'skip': advance(); endOfLine(); return { type: 'Skip', line: t.line };
        case 'error': {
          advance();
          const value = expression();
          endOfLine();
          return { type: 'Raise', value, line: t.line };
        }
        case 'try': {
          advance();
          const body = blockAfterColon('try');
          skipNewlines();
          expect('catch', '`catch`', 'Every `try:` block needs a `catch problem:` block after it.');
          const name = at('NAME') ? advance().value : null;
          return { type: 'Try', body, name, handler: blockAfterColon('catch'), line: t.line };
        }
        case 'use': {
          advance();
          let path = null, module = null;
          if (at('TEXT')) path = advance().value;
          else if (at('NAME')) module = advance().value;
          else {
            fail(cur().line, `expected a module name after \`use\` but found ${describe(cur())}.`,
                 'Write `use math` or `use "helpers.chop" as helpers`.');
          }
          let alias = null;
          if (match('as')) {
            if (!at('NAME')) fail(cur().line, `expected a name after \`as\` but found ${describe(cur())}.`);
            alias = advance().value;
          } else if (path !== null) {
            fail(t.line, 'loading a file needs a name: add `as something` at the end.',
                 'Write `use "helpers.chop" as helpers`.');
          }
          endOfLine();
          return { type: 'Use', module, path, alias, line: t.line };
        }
        case 'else':
          fail(t.line, '`else` here has no matching `if`.',
               'An `else` must follow an `if` block at the same indentation.');
          break;
        case 'catch':
          fail(t.line, '`catch` here has no matching `try`.',
               'A `catch` must follow a `try` block at the same indentation.');
          break;
        default: break;
      }

      const lhs = expression();
      if (at('=')) {
        const eq = advance();
        const value = expression();
        endOfLine();
        if (lhs.type === 'Name') return { type: 'Assign', name: lhs.name, value, line: lhs.line };
        if (lhs.type === 'Index') {
          return { type: 'IndexAssign', object: lhs.object, index: lhs.index, value, line: lhs.line };
        }
        fail(eq.line, 'this is not something you can assign a value to.',
             'Only variables and list positions can be assigned to.');
      }
      endOfLine();
      return { type: 'ExprStatement', expr: lhs, line: lhs.line };
    }

    const body = [];
    for (;;) {
      while (at('NEWLINE') || at('DEDENT') || at('INDENT')) {
        if (at('INDENT')) {
          fail(cur().line, 'this line is indented but nothing above it opened a block.',
               'Top-level lines should start at the left margin.');
        }
        advance();
      }
      if (at('EOF')) break;
      body.push(statement());
    }
    return { type: 'Block', body };
  }

  /* ---------------------------------------------------------------- */
  /* Environments                                                      */
  /* ---------------------------------------------------------------- */

  class Env {
    constructor(parent) {
      this.parent = parent || null;
      this.vars = new Map();
    }
    lookup(name) {
      for (let e = this; e; e = e.parent) if (e.vars.has(name)) return e;
      return null;
    }
    get(name) {
      const e = this.lookup(name);
      return e ? e.vars.get(name) : undefined;
    }
    define(name, value) { this.vars.set(name, value); }
    assign(name, value) {
      const e = this.lookup(name);
      if (!e) return false;
      e.vars.set(name, value);
      return true;
    }
    names() {
      const out = [];
      for (let e = this; e; e = e.parent) for (const k of e.vars.keys()) out.push(k);
      return out;
    }
  }

  function editDistance(a, b) {
    const la = a.length, lb = b.length;
    if (la > 64 || lb > 64) return 99;
    let prev = Array.from({ length: lb + 1 }, (_, j) => j);
    for (let i = 1; i <= la; i++) {
      const curr = [i];
      for (let j = 1; j <= lb; j++) {
        const cost = a[i - 1].toLowerCase() === b[j - 1].toLowerCase() ? 0 : 1;
        curr[j] = Math.min(prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost);
      }
      prev = curr;
    }
    return prev[lb];
  }

  function nearestName(env, name) {
    const limit = name.length <= 2 ? 0 : (name.length <= 4 ? 1 : (name.length <= 7 ? 2 : 3));
    if (limit === 0) return null;
    let best = null, bestD = Infinity;
    for (const cand of env.names()) {
      let d = editDistance(name, cand);
      if (d > limit && cand.includes(name)) d = 1;
      if (d <= limit && d < bestD) { bestD = d; best = cand; }
    }
    return best;
  }

  /* ---------------------------------------------------------------- */
  /* The standard library                                              */
  /* ---------------------------------------------------------------- */

  const ORDINALS = ['first', 'second', 'third', 'fourth', 'fifth', 'sixth'];
  const ordinal = (i) => ORDINALS[i] || 'next';

  function native(name, min, max, fn) {
    return { kind: 'native', name, min, max, fn };
  }

  function wantText(v, fn, i, line) {
    if (!isText(v)) fail(line, `\`${fn}\` needs text as its ${ordinal(i)} value, but got ${typeName(v)}.`);
    return v;
  }
  function wantList(v, fn, i, line) {
    if (!isList(v)) fail(line, `\`${fn}\` needs a list as its ${ordinal(i)} value, but got ${typeName(v)}.`);
    return v;
  }
  function wantNum(v, fn, i, line) {
    if (!isNum(v)) fail(line, `\`${fn}\` needs a number as its ${ordinal(i)} value, but got ${typeName(v)}.`);
    return num(v);
  }
  function wantInt(v, fn, i, line) {
    if (isInt(v)) return Number(v);
    if (isDec(v) && Number.isInteger(v)) return v;
    fail(line, `\`${fn}\` needs a whole number as its ${ordinal(i)} value, but got ${typeName(v)}.`);
  }

  /* Gives back a whole number when the result is exact, like the C build. */
  function numResult(d) {
    if (Number.isFinite(d) && Number.isInteger(d) && Math.abs(d) < 9e15) return BigInt(d);
    return d;
  }

  function clampRange(start, end, n) {
    if (start < 0) start += n;
    if (end < 0) end += n;
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (end < start) end = start;
    return [start, end];
  }

  function compareValues(x, y) {
    if (isNum(x) && isNum(y)) { const a = num(x), b = num(y); return a < b ? -1 : (a > b ? 1 : 0); }
    if (isText(x) && isText(y)) return x < y ? -1 : (x > y ? 1 : 0);
    return isNum(x) ? -1 : (isNum(y) ? 1 : 0);
  }

  function makeModule(name, entries) {
    const slots = new Map();
    for (const [key, value] of Object.entries(entries)) slots.set(key, value);
    return { kind: 'module', name, slots };
  }

  function textFind(hay, needle) {
    if (needle.length === 0) return 0n;
    const at = hay.indexOf(needle);
    return BigInt(at);
  }

  function joinValues(list, sep) {
    return list.map((v) => asText(v)).join(sep);
  }

  function makeStdlib(rt) {
    const modules = {};

    modules.math = () => makeModule('math', {
      pi: Math.PI,
      e: Math.E,
      sqrt: native('sqrt', 1, 1, (a, line) => {
        const x = wantNum(a[0], 'sqrt', 0, line);
        if (x < 0) fail(line, 'cannot take the square root of a negative number.', 'Square roots need a value of 0 or more.');
        return numResult(Math.sqrt(x));
      }),
      abs: native('abs', 1, 1, (a, line) => {
        if (isInt(a[0])) return a[0] < 0n ? -a[0] : a[0];
        return Math.abs(wantNum(a[0], 'abs', 0, line));
      }),
      floor: native('floor', 1, 1, (a, line) => BigInt(Math.floor(wantNum(a[0], 'floor', 0, line)))),
      ceil: native('ceil', 1, 1, (a, line) => BigInt(Math.ceil(wantNum(a[0], 'ceil', 0, line)))),
      round: native('round', 1, 2, (a, line) => {
        const x = wantNum(a[0], 'round', 0, line);
        const places = a.length > 1 ? wantInt(a[1], 'round', 1, line) : 0;
        // C's llround rounds halves away from zero.
        const half = (v) => (v < 0 ? -Math.round(-v) : Math.round(v));
        if (places <= 0) return BigInt(half(x));
        const f = Math.pow(10, places);
        return half(x * f) / f;
      }),
      pow: native('pow', 2, 2, (a, line) =>
        numResult(Math.pow(wantNum(a[0], 'pow', 0, line), wantNum(a[1], 'pow', 1, line)))),
      min: native('min', 1, -1, (a, line) =>
        numResult(Math.min(...a.map((v, i) => wantNum(v, 'min', i, line))))),
      max: native('max', 1, -1, (a, line) =>
        numResult(Math.max(...a.map((v, i) => wantNum(v, 'max', i, line))))),
      sin: native('sin', 1, 1, (a, line) => Math.sin(wantNum(a[0], 'sin', 0, line))),
      cos: native('cos', 1, 1, (a, line) => Math.cos(wantNum(a[0], 'cos', 0, line))),
      tan: native('tan', 1, 1, (a, line) => Math.tan(wantNum(a[0], 'tan', 0, line))),
      log: native('log', 1, 2, (a, line) => {
        const x = wantNum(a[0], 'log', 0, line);
        if (x <= 0) fail(line, '`log` needs a value greater than zero.');
        if (a.length > 1) {
          const base = wantNum(a[1], 'log', 1, line);
          if (base <= 0 || base === 1) fail(line, '`log` needs a base greater than zero and not 1.');
          return numResult(Math.log(x) / Math.log(base));
        }
        return Math.log(x);
      })
    });

    const textFns = {
      upper: native('upper', 1, 1, (a, line) => wantText(a[0], 'upper', 0, line).toUpperCase()),
      lower: native('lower', 1, 1, (a, line) => wantText(a[0], 'lower', 0, line).toLowerCase()),
      trim: native('trim', 1, 1, (a, line) => wantText(a[0], 'trim', 0, line).trim()),
      split: native('split', 1, 2, (a, line) => {
        const s = wantText(a[0], 'split', 0, line);
        if (a.length < 2 || (isText(a[1]) && a[1] === '')) {
          return s.split(/\s+/).filter((p) => p.length > 0);
        }
        return s.split(wantText(a[1], 'split', 1, line));
      }),
      join: native('join', 1, 2, (a, line) => {
        const list = wantList(a[0], 'join', 0, line);
        const sep = a.length > 1 ? wantText(a[1], 'join', 1, line) : '';
        return joinValues(list, sep);
      }),
      replace: native('replace', 3, 3, (a, line) => {
        const [s, from, to] = a.map((v, i) => wantText(v, 'replace', i, line));
        if (from === '') return s;
        return s.split(from).join(to);
      }),
      contains: native('contains', 2, 2, (a, line) =>
        wantText(a[0], 'contains', 0, line).includes(wantText(a[1], 'contains', 1, line))),
      starts: native('starts', 2, 2, (a, line) =>
        wantText(a[0], 'starts', 0, line).startsWith(wantText(a[1], 'starts', 1, line))),
      ends: native('ends', 2, 2, (a, line) =>
        wantText(a[0], 'ends', 0, line).endsWith(wantText(a[1], 'ends', 1, line))),
      find: native('find', 2, 2, (a, line) =>
        textFind(wantText(a[0], 'find', 0, line), wantText(a[1], 'find', 1, line))),
      slice: native('slice', 2, 3, (a, line) => {
        const s = wantText(a[0], 'slice', 0, line);
        let start = wantInt(a[1], 'slice', 1, line);
        let end = a.length > 2 ? wantInt(a[2], 'slice', 2, line) : s.length;
        [start, end] = clampRange(start, end, s.length);
        return s.slice(start, end);
      }),
      repeat: native('repeat', 2, 2, (a, line) => {
        const s = wantText(a[0], 'repeat', 0, line);
        let times = wantInt(a[1], 'repeat', 1, line);
        if (times < 0) times = 0;
        if (s.length * times > 50 * 1024 * 1024) {
          fail(line, '`repeat` would make a piece of text that is far too large.');
        }
        return s.repeat(times);
      }),
      reverse: native('reverse', 1, 1, (a, line) =>
        wantText(a[0], 'reverse', 0, line).split('').reverse().join('')),
      chars: native('chars', 1, 1, (a, line) => wantText(a[0], 'chars', 0, line).split('')),
      code: native('code', 1, 1, (a, line) => {
        const s = wantText(a[0], 'code', 0, line);
        if (s.length === 0) fail(line, '`code` needs at least one character.');
        return BigInt(s.charCodeAt(0));
      }),
      char: native('char', 1, 1, (a, line) => {
        const c = wantInt(a[0], 'char', 0, line);
        if (c < 0 || c > 255) fail(line, '`char` needs a number between 0 and 255.');
        return String.fromCharCode(c);
      }),
      length: native('length', 1, 1, (a, line) => BigInt(wantText(a[0], 'length', 0, line).length))
    };
    modules.text = () => makeModule('text', textFns);

    modules.lists = () => makeModule('lists', {
      length: native('length', 1, 1, (a, line) => BigInt(wantList(a[0], 'length', 0, line).length)),
      add: native('add', 2, 2, (a, line) => { wantList(a[0], 'add', 0, line).push(a[1]); return a[0]; }),
      insert: native('insert', 3, 3, (a, line) => {
        const l = wantList(a[0], 'insert', 0, line);
        let pos = wantInt(a[1], 'insert', 1, line);
        if (pos < 0) pos += l.length;
        pos = Math.max(0, Math.min(pos, l.length));
        l.splice(pos, 0, a[2]);
        return l;
      }),
      remove: native('remove', 2, 2, (a, line) => {
        const l = wantList(a[0], 'remove', 0, line);
        let pos = wantInt(a[1], 'remove', 1, line);
        if (pos < 0) pos += l.length;
        if (pos < 0 || pos >= l.length) {
          fail(line, `position ${wantInt(a[1], 'remove', 1, line)} is outside this list of length ${l.length}.`);
        }
        return l.splice(pos, 1)[0];
      }),
      contains: native('contains', 2, 2, (a, line) =>
        wantList(a[0], 'contains', 0, line).some((x) => equal(x, a[1]))),
      find: native('find', 2, 2, (a, line) => {
        const l = wantList(a[0], 'find', 0, line);
        for (let i = 0; i < l.length; i++) if (equal(l[i], a[1])) return BigInt(i);
        return -1n;
      }),
      copy: native('copy', 1, 1, (a, line) => wantList(a[0], 'copy', 0, line).slice()),
      reverse: native('reverse', 1, 1, (a, line) => wantList(a[0], 'reverse', 0, line).slice().reverse()),
      slice: native('slice', 2, 3, (a, line) => {
        const l = wantList(a[0], 'slice', 0, line);
        let start = wantInt(a[1], 'slice', 1, line);
        let end = a.length > 2 ? wantInt(a[2], 'slice', 2, line) : l.length;
        [start, end] = clampRange(start, end, l.length);
        return l.slice(start, end);
      }),
      sort: native('sort', 1, 1, (a, line) => {
        const l = wantList(a[0], 'sort', 0, line);
        const allText = l.every(isText);
        const allNum = l.every(isNum);
        if (!allText && !allNum && l.length > 1) {
          fail(line, '`sort` needs every item to be the same kind of value.',
               'Sort a list of numbers, or a list of text.');
        }
        return l.slice().sort(compareValues);
      }),
      sum: native('sum', 1, 1, (a, line) => {
        const l = wantList(a[0], 'sum', 0, line);
        let anyDec = false, total = 0, itotal = 0n;
        for (const v of l) {
          if (isInt(v)) { itotal += v; total += Number(v); }
          else if (isDec(v)) { anyDec = true; total += v; }
          else fail(line, `\`sum\` needs a list of numbers, but found ${typeName(v)}.`);
        }
        return anyDec ? total : itotal;
      }),
      min: native('min', 1, 1, (a, line) => listMinMax(a[0], line, true, 'min')),
      max: native('max', 1, 1, (a, line) => listMinMax(a[0], line, false, 'max')),
      join: textFns.join
    });

    function listMinMax(list, line, wantMin, fname) {
      const l = wantList(list, fname, 0, line);
      if (l.length === 0) fail(line, `\`${fname}\` needs a list with at least one item.`);
      let best = l[0];
      for (const v of l.slice(1)) {
        const c = compareValues(v, best);
        if (wantMin ? c < 0 : c > 0) best = v;
      }
      return best;
    }

    modules.random = () => makeModule('random', {
      seed: native('seed', 1, 1, (a, line) => { rt.rngSeed(wantInt(a[0], 'seed', 0, line)); return null; }),
      number: native('number', 2, 2, (a, line) => {
        let lo = wantInt(a[0], 'number', 0, line);
        let hi = wantInt(a[1], 'number', 1, line);
        if (lo > hi) [lo, hi] = [hi, lo];
        return BigInt(lo + Math.floor(rt.rngNext() * (hi - lo + 1)));
      }),
      decimal: native('decimal', 0, 0, () => rt.rngNext()),
      pick: native('pick', 1, 1, (a, line) => {
        const l = wantList(a[0], 'pick', 0, line);
        if (l.length === 0) fail(line, '`pick` needs a list with at least one item.');
        return l[Math.floor(rt.rngNext() * l.length)];
      }),
      shuffle: native('shuffle', 1, 1, (a, line) => {
        const out = wantList(a[0], 'shuffle', 0, line).slice();
        for (let i = out.length - 1; i > 0; i--) {
          const j = Math.floor(rt.rngNext() * (i + 1));
          [out[i], out[j]] = [out[j], out[i]];
        }
        return out;
      })
    });

    const pad = (n) => String(n).padStart(2, '0');
    modules.time = () => makeModule('time', {
      now: native('now', 0, 0, () => Date.now() / 1000),
      clock: native('clock', 0, 0, () => (typeof performance !== 'undefined' ? performance.now() : Date.now()) / 1000),
      sleep: native('sleep', 1, 1, (a, line) => {
        wantNum(a[0], 'sleep', 0, line);
        return null;   // a browser tab must not block; sleeping is a no-op here
      }),
      date: native('date', 0, 0, () => {
        const d = new Date();
        return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
      }),
      stamp: native('stamp', 0, 0, () => {
        const d = new Date();
        return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())} ` +
               `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
      }),
      year: native('year', 0, 0, () => BigInt(new Date().getFullYear())),
      month: native('month', 0, 0, () => BigInt(new Date().getMonth() + 1)),
      day: native('day', 0, 0, () => BigInt(new Date().getDate())),
      hour: native('hour', 0, 0, () => BigInt(new Date().getHours())),
      minute: native('minute', 0, 0, () => BigInt(new Date().getMinutes())),
      second: native('second', 0, 0, () => BigInt(new Date().getSeconds()))
    });

    if (rt.files) modules.files = () => makeModule('files', rt.files());

    return modules;
  }

  /* ---------------------------------------------------------------- */
  /* Interpreter                                                       */
  /* ---------------------------------------------------------------- */

  const BREAK = { signal: 'break' };
  const SKIP = { signal: 'skip' };
  class ReturnSignal { constructor(value) { this.value = value; } }

  class Interpreter {
    constructor(options) {
      this.options = options || {};
      this.out = [];
      this.pending = '';
      this.inputs = (this.options.input || []).slice();
      this.depth = 0;
      this.steps = 0;
      this.maxSteps = this.options.maxSteps || 20000000;
      this.rngState = 0x2545f491 ^ (Date.now() & 0xffffffff);
      this.modules = makeStdlib({
        rngSeed: (n) => { this.rngState = (n >>> 0) || 1; },
        rngNext: () => this.nextRandom(),
        files: this.options.files || null
      });
      this.globals = new Env(null);
      this.installBuiltins();
    }

    nextRandom() {
      // xorshift32 — deterministic once seeded, which keeps `random.seed` useful.
      let x = this.rngState >>> 0;
      x ^= x << 13; x >>>= 0;
      x ^= x >>> 17;
      x ^= x << 5; x >>>= 0;
      this.rngState = x;
      return x / 4294967296;
    }

    write(s) {
      this.pending += s;
      if (this.options.onOutput) {
        const parts = this.pending.split('\n');
        this.pending = parts.pop();
        for (const p of parts) this.options.onOutput(p);
      }
    }

    output() {
      return this.out.join('');
    }

    installBuiltins() {
      const g = this.globals;
      g.define('len', native('len', 1, 1, (a, line) => {
        if (isText(a[0])) return BigInt(a[0].length);
        if (isList(a[0])) return BigInt(a[0].length);
        fail(line, `cannot measure the length of ${typeName(a[0])}.`, '`len` works with text and lists.');
      }));
      g.define('text', native('text', 1, 1, (a) => asText(a[0])));
      g.define('number', native('number', 1, 1, (a, line) => this.toNumber(a[0], line)));
      g.define('whole', native('whole', 1, 1, (a, line) => {
        if (isInt(a[0])) return a[0];
        if (isDec(a[0])) return BigInt(Math.trunc(a[0]));
        if (isText(a[0])) {
          const n = this.toNumber(a[0], line);
          return isInt(n) ? n : BigInt(Math.trunc(n));
        }
        fail(line, `cannot turn ${typeName(a[0])} into a whole number.`);
      }));
      g.define('type', native('type', 1, 1, (a) => typeName(a[0])));
      g.define('push', native('push', 2, 2, (a, line) => {
        wantList(a[0], 'push', 0, line).push(a[1]);
        return a[0];
      }));
      g.define('pop', native('pop', 1, 1, (a, line) => {
        const l = wantList(a[0], 'pop', 0, line);
        if (l.length === 0) {
          fail(line, 'cannot take the last item from an empty list.',
               'Check the list is not empty before taking from it.');
        }
        return l.pop();
      }));
    }

    toNumber(v, line) {
      if (isNum(v)) return v;
      if (typeof v === 'boolean') return v ? 1n : 0n;
      if (isText(v)) {
        const s = v.trim();
        if (s === '' || !/^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$/.test(s)) {
          fail(line, `cannot turn "${v}" into a number.`,
               'Only text that looks like a number can be converted.');
        }
        const d = parseFloat(s);
        if (Number.isInteger(d) && !/[.eE]/.test(s) && Math.abs(d) < 9e15) return BigInt(s);
        return d;
      }
      fail(line, `cannot turn ${typeName(v)} into a number.`);
    }

    run(source) {
      const program = parse(lex(source));
      this.execBlock(program, this.globals);
    }

    /* ------------------------------------------------------------ */

    execBlock(block, env) {
      for (const stmt of block.body) {
        const signal = this.execStatement(stmt, env);
        if (signal) return signal;
      }
      return null;
    }

    execStatement(node, env) {
      if (++this.steps > this.maxSteps) {
        fail(node.line, 'this program ran for too long and was stopped.',
             'A loop is probably never finishing.');
      }
      switch (node.type) {
        case 'Say': {
          const parts = node.values.map((v) => asText(this.eval(v, env)));
          this.write(parts.join(' ') + '\n');
          return null;
        }
        case 'Assign': {
          const value = this.eval(node.value, env);
          if (!env.assign(node.name, value)) env.define(node.name, value);
          return null;
        }
        case 'IndexAssign': {
          const obj = this.eval(node.object, env);
          const idx = this.eval(node.index, env);
          const value = this.eval(node.value, env);
          if (!isList(obj)) {
            fail(node.line, `cannot change a position inside ${typeName(obj)}.`,
                 'Only list positions can be changed this way.');
          }
          obj[this.resolveIndex(idx, obj.length, 'list', node.line)] = value;
          return null;
        }
        case 'ExprStatement': this.eval(node.expr, env); return null;
        case 'If': {
          if (truthy(this.eval(node.test, env))) return this.execBlock(node.then, env);
          if (node.otherwise) {
            return node.otherwise.type === 'Block'
              ? this.execBlock(node.otherwise, env)
              : this.execStatement(node.otherwise, env);
          }
          return null;
        }
        case 'While': {
          while (truthy(this.eval(node.test, env))) {
            const signal = this.execBlock(node.body, env);
            if (signal === BREAK) break;
            if (signal === SKIP) continue;
            if (signal) return signal;
            if (++this.steps > this.maxSteps) {
              fail(node.line, 'this program ran for too long and was stopped.',
                   'A loop is probably never finishing.');
            }
          }
          return null;
        }
        case 'ForIn': {
          const seq = this.eval(node.seq, env);
          let items;
          if (isList(seq)) items = seq;
          else if (isText(seq)) items = seq.split('');
          else {
            fail(node.line, `cannot go through ${typeName(seq)} one item at a time.`,
                 'Use `for i in 1 to 10:` to count instead.');
          }
          for (let i = 0; i < items.length; i++) {
            const item = items[i];
            if (!env.assign(node.name, item)) env.define(node.name, item);
            const signal = this.execBlock(node.body, env);
            if (signal === BREAK) break;
            if (signal === SKIP) continue;
            if (signal) return signal;
          }
          return null;
        }
        case 'ForRange': {
          const from = this.eval(node.from, env);
          const to = this.eval(node.to, env);
          if (!isNum(from) || !isNum(to)) {
            fail(node.line, `counting needs two numbers, got ${typeName(from)} and ${typeName(to)}.`,
                 'Write `for i in 1 to 10:`.');
          }
          let step = 1;
          if (node.step) {
            const s = this.eval(node.step, env);
            if (!isNum(s)) {
              fail(node.line, `the step after \`by\` must be a number, got ${typeName(s)}.`,
                   'Write `for i in 10 to 1 by -1:`.');
            }
            step = Math.trunc(num(s));
            if (step === 0) {
              fail(node.line, 'the step after `by` cannot be zero.',
                   'A step of 0 would never reach the end.');
            }
          }
          const start = Math.trunc(num(from)), end = Math.trunc(num(to));
          for (let i = start; step > 0 ? i <= end : i >= end; i += step) {
            const item = BigInt(i);
            if (!env.assign(node.name, item)) env.define(node.name, item);
            const signal = this.execBlock(node.body, env);
            if (signal === BREAK) break;
            if (signal === SKIP) continue;
            if (signal) return signal;
          }
          return null;
        }
        case 'Fun':
          env.define(node.name, { kind: 'fun', decl: node, env });
          return null;
        case 'Return':
          return new ReturnSignal(node.value ? this.eval(node.value, env) : null);
        case 'Break': return BREAK;
        case 'Skip': return SKIP;
        case 'Raise': {
          const value = this.eval(node.value, env);
          fail(node.line, asText(value));
          return null;
        }
        case 'Try': {
          try {
            return this.execBlock(node.body, env);
          } catch (err) {
            if (!(err instanceof ChopcapError)) throw err;
            const scope = new Env(env);
            if (node.name) scope.define(node.name, err.chopMessage);
            return this.execBlock(node.handler, scope);
          }
        }
        case 'Use': {
          if (node.path !== null) {
            fail(node.line, `cannot open the file \`${node.path}\`.`,
                 'The playground runs in your browser, so it cannot load other files.');
          }
          const maker = this.modules[node.module];
          if (!maker) {
            const known = Object.keys(this.modules).join(', ');
            fail(node.line, `there is no module called \`${node.module}\`.`,
                 `Chopcap comes with: ${known}.`);
          }
          env.define(node.alias || node.module, maker());
          return null;
        }
        default:
          this.eval(node, env);
          return null;
      }
    }

    resolveIndex(idx, count, what, line) {
      if (!isInt(idx)) {
        fail(line, `cannot use ${typeName(idx)} as a position in ${what}.`,
             'Positions must be whole numbers, starting at 0.');
      }
      let i = Number(idx);
      const original = i;
      if (i < 0) i += count;
      if (i < 0 || i >= count) {
        if (count === 0) {
          fail(line, `position ${original} is outside this ${what}.`,
               'It is empty, so there is nothing at any position.');
        }
        fail(line, `position ${original} is outside this ${what} of length ${count}.`,
             'Valid positions run from 0 to the length minus one.');
      }
      return i;
    }

    eval(node, env) {
      switch (node.type) {
        case 'Literal': return node.value;
        case 'Name': {
          const scope = env.lookup(node.name);
          if (!scope) {
            const near = nearestName(env, node.name);
            fail(node.line, `\`${node.name}\` does not exist.`,
                 near ? `Did you mean \`${near}\`?` : `Give it a value first, like \`${node.name} = 1\`.`);
          }
          return scope.vars.get(node.name);
        }
        case 'ListLit': return node.items.map((item) => this.eval(item, env));
        case 'Ask': {
          if (node.prompt) this.write(asText(this.eval(node.prompt, env)));
          if (this.inputs.length === 0) return null;
          return this.inputs.shift();
        }
        case 'And': {
          const left = this.eval(node.left, env);
          return truthy(left) ? this.eval(node.right, env) : left;
        }
        case 'Or': {
          const left = this.eval(node.left, env);
          return truthy(left) ? left : this.eval(node.right, env);
        }
        case 'Unary': {
          const v = this.eval(node.operand, env);
          if (node.op === 'not') return !truthy(v);
          if (isInt(v)) return -v;
          if (isDec(v)) return -v;
          fail(node.line, `cannot make ${typeName(v)} negative.`);
          return null;
        }
        case 'Binary': return this.binary(node.op, this.eval(node.left, env), this.eval(node.right, env), node.line);
        case 'Index': {
          const obj = this.eval(node.object, env);
          const idx = this.eval(node.index, env);
          if (isList(obj)) return obj[this.resolveIndex(idx, obj.length, 'list', node.line)];
          if (isText(obj)) return obj[this.resolveIndex(idx, obj.length, 'text', node.line)];
          fail(node.line, `cannot take a position out of ${typeName(obj)}.`,
               'Only lists and text can be used with `[...]`.');
          return null;
        }
        case 'Member': {
          const obj = this.eval(node.object, env);
          if (!obj || obj.kind !== 'module') {
            fail(node.line, `${typeName(obj)} has no parts to reach with \`.\`.`,
                 '`.` is used to reach inside a module, like `math.sqrt`.');
          }
          if (!obj.slots.has(node.name)) {
            fail(node.line, `the module \`${obj.name}\` has nothing called \`${node.name}\`.`);
          }
          return obj.slots.get(node.name);
        }
        case 'Call': {
          const fn = this.eval(node.callee, env);
          const args = node.args.map((a) => this.eval(a, env));
          return this.call(fn, args, node.line);
        }
        default:
          fail(node.line, 'this expression is not something Chopcap can run.');
          return null;
      }
    }

    call(fn, args, line) {
      if (fn && fn.kind === 'native') {
        if (args.length < fn.min || (fn.max >= 0 && args.length > fn.max)) {
          if (fn.min === fn.max) {
            fail(line, `\`${fn.name}\` needs ${fn.min} value${fn.min === 1 ? '' : 's'} but got ${args.length}.`);
          }
          fail(line, `\`${fn.name}\` needs between ${fn.min} and ${fn.max} values but got ${args.length}.`);
        }
        return fn.fn(args, line);
      }
      if (!fn || fn.kind !== 'fun') {
        fail(line, `${typeName(fn)} is not a function.`, 'Only functions can be called with `(...)`.');
      }
      const decl = fn.decl;
      if (args.length !== decl.params.length) {
        fail(line, `\`${decl.name}\` needs ${decl.params.length} ` +
                   `value${decl.params.length === 1 ? '' : 's'} but got ${args.length}.`);
      }
      if (++this.depth > 900) {
        this.depth--;
        fail(line, `\`${decl.name}\` went too deep.`,
             'A function is probably calling itself without ever stopping.');
      }
      const local = new Env(fn.env);
      decl.params.forEach((p, i) => local.define(p, args[i]));
      try {
        const signal = this.execBlock(decl.body, local);
        if (signal instanceof ReturnSignal) return signal.value;
        return null;
      } finally {
        this.depth--;
      }
    }

    binary(op, a, b, line) {
      switch (op) {
        case '==': return equal(a, b);
        case '!=': return !equal(a, b);
        case 'in':
          if (isList(b)) return b.some((x) => equal(x, a));
          if (isText(b) && isText(a)) return b.includes(a);
          fail(line, `cannot look for ${typeName(a)} inside ${typeName(b)}.`,
               '`in` works with a list, or with text inside text.');
          break;
        default: break;
      }

      if (op === '+') {
        if (isNum(a) && isNum(b)) {
          if (isInt(a) && isInt(b)) return a + b;
          return num(a) + num(b);
        }
        if (isText(a) || isText(b)) return asText(a) + asText(b);
        if (isList(a) && isList(b)) return a.concat(b);
        fail(line, `cannot add ${typeName(a)} and ${typeName(b)}.`,
             'You can add numbers, join text, or join two lists.');
      }

      if (['<', '<=', '>', '>='].includes(op)) {
        if (isText(a) && isText(b)) {
          const c = a < b ? -1 : (a > b ? 1 : 0);
          return op === '<' ? c < 0 : op === '<=' ? c <= 0 : op === '>' ? c > 0 : c >= 0;
        }
        if (!isNum(a) || !isNum(b)) {
          fail(line, `cannot compare ${typeName(a)} with ${typeName(b)} using \`${op}\`.`,
               'Compare numbers with numbers, or text with text.');
        }
        const x = num(a), y = num(b);
        return op === '<' ? x < y : op === '<=' ? x <= y : op === '>' ? x > y : x >= y;
      }

      if (!isNum(a) || !isNum(b)) {
        fail(line, `cannot use \`${op}\` with ${typeName(a)} and ${typeName(b)}.`,
             'This operator only works with numbers.');
      }

      const bothInt = isInt(a) && isInt(b);
      switch (op) {
        case '-': return bothInt ? a - b : num(a) - num(b);
        case '*': return bothInt ? a * b : num(a) * num(b);
        case '/':
          if (num(b) === 0) {
            fail(line, 'cannot divide by zero.', 'Check the value on the right of the `/`.');
          }
          if (bothInt && a % b === 0n) return a / b;
          return num(a) / num(b);
        case '%':
          if (num(b) === 0) {
            fail(line, 'cannot take the remainder of a division by zero.',
                 'Check the value on the right of the `%`.');
          }
          return bothInt ? a % b : num(a) % num(b);
        case '^': {
          const r = Math.pow(num(a), num(b));
          if (bothInt && b >= 0n && Number.isInteger(r) && Math.abs(r) < 9e18) return BigInt(r);
          return r;
        }
        default:
          fail(line, 'unsupported operator.');
          return null;
      }
    }
  }

  /* ---------------------------------------------------------------- */
  /* Public API                                                        */
  /* ---------------------------------------------------------------- */

  /* The same shape the chopcap command prints, snippet and all. */
  function formatError(err, filename, source) {
    let out = '\nChopcap Error\n\n';
    if (err.line) {
      out += filename ? `  ${filename}, line ${err.line}:\n` : `  line ${err.line}:\n`;
      if (source) {
        const snippet = (source.split('\n')[err.line - 1] || '').replace(/\r$/, '').replace(/^[ \t]+/, '');
        if (snippet) out += `    ${snippet}\n`;
      }
      out += '\n';
    }
    out += '  ' + err.chopMessage + '\n';
    if (err.hint) out += '  ' + err.hint + '\n';
    return out;
  }

  /* Runs a program and gives back everything it printed.
   *   run(source, { input: ["Ada"], filename: "playground.chop" })
   *     -> { output, ok, error }
   */
  function run(source, options) {
    options = options || {};
    const lines = [];
    const interp = new Interpreter({
      input: options.input,
      maxSteps: options.maxSteps,
      onOutput: (line) => lines.push(line)
    });
    let error = null;
    try {
      interp.run(source);
    } catch (err) {
      if (err instanceof ChopcapError) error = err;
      else throw err;
    }
    if (interp.pending) { lines.push(interp.pending); interp.pending = ''; }
    let output = lines.join('\n');
    if (lines.length) output += '\n';
    if (error) output += formatError(error, options.filename || null, source) + '\n';
    return { output, ok: error === null, error };
  }

  const Chopcap = { VERSION, run, lex, parse, Interpreter, ChopcapError, formatError, asText, asRepr };

  if (typeof module !== 'undefined' && module.exports) module.exports = Chopcap;
  root.Chopcap = Chopcap;
})(typeof globalThis !== 'undefined' ? globalThis : this);
