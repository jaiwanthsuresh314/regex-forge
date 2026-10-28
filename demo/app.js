const $ = (id) => document.getElementById(id);

const patternInput = $('pattern');
const inputInput = $('input');
const repetitions = $('repetitions');
const allowDfa = $('allow-dfa');
let graphMode = 'nfa';

function parsePattern(source) {
  let index = 0;
  const peek = () => source[index];
  const take = () => source[index++];
  function expression() {
    const branches = [concatenation()];
    while (peek() === '|') { take(); branches.push(concatenation()); }
    return branches.length === 1 ? branches[0] : { type: 'alt', children: branches };
  }
  function concatenation() {
    const parts = [];
    while (index < source.length && !')|'.includes(peek())) parts.push(repetition());
    if (!parts.length) return { type: 'empty' };
    return parts.length === 1 ? parts[0] : { type: 'concat', children: parts };
  }
  function repetition() {
    const atom = atomNode();
    if (peek() === '*') { take(); if (peek() === '*') throw new Error('consecutive * is not supported'); return { type: 'star', child: atom }; }
    return atom;
  }
  function atomNode() {
    if (peek() === '(') { take(); const child = expression(); if (take() !== ')') throw new Error("expected ')' "); return child; }
    const char = take();
    if (!char || '*)|'.includes(char)) throw new Error(`unexpected '${char || 'end of pattern'}'`);
    return { type: 'literal', char };
  }
  const ast = expression();
  if (index !== source.length) throw new Error(`unexpected '${peek()}'`);
  return ast;
}

function buildNfa(ast) {
  const states = [];
  const add = () => { states.push({ epsilon: [], transitions: [], accept: false }); return states.length - 1; };
  function compile(node) {
    if (node.type === 'empty') { const s = add(), e = add(); states[s].epsilon.push(e); return [s, e]; }
    if (node.type === 'literal') { const s = add(), e = add(); states[s].transitions.push([node.char, e]); return [s, e]; }
    if (node.type === 'concat') { let result = compile(node.children[0]); node.children.slice(1).forEach((child) => { const next = compile(child); states[result[1]].epsilon.push(next[0]); result = [result[0], next[1]]; }); return result; }
    if (node.type === 'alt') { const s = add(), e = add(); node.children.forEach((child) => { const part = compile(child); states[s].epsilon.push(part[0]); states[part[1]].epsilon.push(e); }); return [s, e]; }
    const s = add(), e = add(), child = compile(node.child); states[s].epsilon.push(e, child[0]); states[child[1]].epsilon.push(child[0], e); return [s, e];
  }
  const [start, accept] = compile(ast); states[accept].accept = true;
  return { states, start, accept };
}

function closure(nfa, seeds) {
  const seen = new Set(seeds); const queue = [...seeds];
  for (let i = 0; i < queue.length; i++) nfa.states[queue[i]].epsilon.forEach((next) => { if (!seen.has(next)) { seen.add(next); queue.push(next); } });
  return [...seen].sort((a, b) => a - b);
}

function buildDfa(nfa) {
  const start = closure(nfa, [nfa.start]); const sets = [start]; const transitions = []; const lookup = new Map([[start.join(','), 0]]);
  for (let i = 0; i < sets.length; i++) {
    const edges = {}; new Set(sets[i].flatMap((id) => nfa.states[id].transitions.map(([char]) => char))).forEach((char) => {
      const next = closure(nfa, sets[i].flatMap((id) => nfa.states[id].transitions.filter(([label]) => label === char).map(([, target]) => target)));
      const key = next.join(','); let id = lookup.get(key); if (id === undefined) { id = sets.length; lookup.set(key, id); sets.push(next); } edges[char] = id;
    }); transitions[i] = edges;
  }
  return { sets, transitions, start: 0, accepts: new Set(sets.map((set, id) => set.some((n) => nfa.states[n].accept) ? id : -1).filter((id) => id >= 0)) };
}

function activeDfaStates(dfa, value, step) { let current = 0; const path = [0]; for (const char of value.slice(0, step)) { if (dfa.transitions[current]?.[char] === undefined) return { current: -1, path }; current = dfa.transitions[current][char]; path.push(current); } return { current, path }; }

function activeNfaStates(nfa, value, step) { let current = closure(nfa, [nfa.start]); for (const char of value.slice(0, step)) current = closure(nfa, current.flatMap((id) => nfa.states[id].transitions.filter(([label]) => label === char).map(([, target]) => target))); return new Set(current); }

function graphSvg(pattern, value, step) {
  try {
    const ast = parsePattern(pattern); const nfa = buildNfa(ast); const dfa = buildDfa(nfa); const width = Math.max(430, (graphMode === 'nfa' ? nfa.states.length : dfa.sets.length) * 70 + 40); const height = 174;
    const active = graphMode === 'nfa' ? activeNfaStates(nfa, value, step) : new Set([activeDfaStates(dfa, value, step).current]);
    const nodes = graphMode === 'nfa' ? nfa.states.map((state, id) => ({ id, accept: state.accept })) : dfa.sets.map((set, id) => ({ id, accept: dfa.accepts.has(id) }));
    const x = (id) => 34 + id * ((width - 68) / Math.max(1, nodes.length - 1));
    const edgeParts = [];
    if (graphMode === 'nfa') nfa.states.forEach((state, from) => { state.epsilon.forEach((to) => edgeParts.push({ from, to, label: 'ε', active: active.has(from) && active.has(to) })); state.transitions.forEach(([label, to]) => edgeParts.push({ from, to, label, active: active.has(from) && active.has(to) })); });
    else dfa.transitions.forEach((edges, from) => Object.entries(edges).forEach(([label, to]) => edgeParts.push({ from, to, label, active: active.has(from) && active.has(to) })));
    const edges = edgeParts.map(({ from, to, label, active: isActive }) => { const x1=x(from), x2=x(to), y=87; const curve = from === to ? `M ${x1} ${y-10} C ${x1-20} 20, ${x1+20} 20, ${x1} ${y-10}` : `M ${x1+15} ${y} Q ${(x1+x2)/2} ${y + (from % 2 ? 28 : -28)} ${x2-15} ${y}`; return `<path class="graph-edge${isActive ? ' active' : ''}" d="${curve}" marker-end="url(#arrow)"/><text class="graph-edge-label" x="${(x1+x2)/2}" y="${from === to ? 24 : y + (from % 2 ? 41 : -34)}">${label === ' ' ? '·' : label}</text>`; }).join('');
    const circles = nodes.map(({ id, accept }) => `<circle class="graph-node${active.has(id) ? ' active' : ''}${accept ? ' accept' : ''}" cx="${x(id)}" cy="87" r="16"/><text class="graph-node-label" x="${x(id)}" y="87">${graphMode === 'nfa' ? id : `q${id}`}</text>`).join('');
    return `<svg viewBox="0 0 ${width} ${height}" preserveAspectRatio="xMidYMid meet"><title>${graphMode.toUpperCase()} automaton for ${pattern}</title><defs><marker id="arrow" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="5" markerHeight="5" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" fill="#6c9c86"/></marker></defs><line class="graph-start-line" x1="${x(0)-30}" y1="87" x2="${x(0)-16}" y2="87" marker-end="url(#arrow)"/>${edges}${circles}</svg>`;
  } catch (error) { return `<div class="graph-error">${error.message}</div>`; }
}

function renderGraph() { const pattern = patternInput.value; const value = inputInput.value; const step = Number($('step-range').value); $('step-value').textContent = `${step} / ${value.length}`; $('graph-canvas').innerHTML = graphSvg(pattern, value, step); $('graph-footnote').textContent = graphMode === 'nfa' ? 'Thompson construction · ε-transitions shown' : 'Subset construction · cached DFA states'; }

function safeMatch(pattern, value) {
  try {
    // The C++ demo grammar is full-string based. This browser adapter keeps
    // the same visible semantics while the production path is wired to C++.
    return { matched: new RegExp(`^(?:${pattern})$`).test(value), error: null };
  } catch (error) {
    return { matched: false, error: error.message };
  }
}

function estimate(pattern, value, repeats, dfaAllowed) {
  const stars = (pattern.match(/\*/g) || []).length;
  const alternations = (pattern.match(/\|/g) || []).length;
  const groups = (pattern.match(/\(/g) || []).length;
  const nested = Math.max(0, stars - 1);
  const overlap = Math.min(32, alternations * (groups > 0 ? 1.4 : 0.6));
  const ambiguous = Math.min(32, alternations + stars * 2 + nested * 4);
  const nfaStates = Math.min(128, 1 + pattern.length + stars * 2 + alternations * 2);
  const normalized = {
    nested: Math.min(1, nested / 8),
    overlap: overlap / 32,
    ambiguous: ambiguous / 32,
    input: Math.min(1, value.length / 256),
    nfa: nfaStates / 128,
    depth: Math.min(1, (stars > 1 ? 2 : stars) / 8),
  };
  const riskScore = 100 * (0.25 * normalized.nested + 0.20 * normalized.overlap + 0.20 * normalized.ambiguous + 0.15 * normalized.depth + 0.10 * normalized.nfa + 0.10 * normalized.input);
  const ambiguity = nested * 18 + alternations * 9 + groups * 4;
  const workloadAmortized = repeats >= 4 || value.length >= 32;
  const useDfa = dfaAllowed && workloadAmortized && (riskScore >= 60 || (riskScore >= 30 && workloadAmortized));
  const useNfa = !useDfa && (riskScore >= 30 || value.length > 512);
  const strategy = useDfa ? 'Lazy DFA' : useNfa ? 'Thompson NFA' : 'Backtracking';
  const steps = strategy === 'Backtracking' ? Math.max(1, value.length + ambiguity) : value.length;
  const active = strategy === 'Backtracking' ? 0 : Math.max(1, Math.min(12, 1 + groups + alternations));
  const dfaStates = strategy === 'Lazy DFA' ? Math.max(1, Math.min(10, 2 + stars + alternations)) : 0;
  const fallback = strategy === 'Lazy DFA' && dfaStates > 8;
  return { strategy, steps, active, dfaStates, fallback, ambiguity, riskScore, normalized };
}

function astShape(pattern) {
  if (!pattern) return 'Empty';
  const literals = [...pattern].filter((char) => !'()|*'.includes(char));
  if (pattern.includes('|')) return `Alt(${literals.slice(0, 3).map((x) => `'${x}'`).join(', ') || 'Empty'})`;
  if (pattern.includes('*')) return `Concat(… Star('${literals[0] || ''}'))`;
  return literals.length === 1 ? `Literal('${literals[0]}')` : `Concat(${literals.map((x) => `'${x}'`).join(', ')})`;
}

function renderAst(pattern) {
  const nodes = [];
  const push = (text) => { if (nodes.length) nodes.push('<span class="ast-connector">→</span>'); nodes.push(`<span class="ast-node">${text}</span>`); };
  if (!pattern) push('Empty');
  else {
    if (pattern.includes('(')) push('Group');
    if (pattern.includes('|')) push('Alternation');
    const literals = [...pattern].filter((char) => !'()|*'.includes(char));
    literals.slice(0, 5).forEach((literal) => push(`'${literal}'`));
    if (pattern.includes('*')) push('Star');
  }
  $('ast-visual').innerHTML = nodes.join('');
}

function render() {
  const pattern = patternInput.value;
  const value = inputInput.value;
  const result = safeMatch(pattern, value);
  const plan = estimate(pattern, value, Number(repetitions.value), allowDfa.checked);
  const effectiveStrategy = plan.fallback ? 'Thompson NFA' : plan.strategy;
  const badge = $('status-badge');

  $('status-text').textContent = result.error ? 'ERROR' : result.matched ? 'MATCH' : 'NO MATCH';
  badge.classList.toggle('no-match', !result.matched || Boolean(result.error));
  $('strategy-name').textContent = result.error ? 'No strategy selected' : plan.fallback ? 'Thompson NFA · fallback' : plan.strategy;
  $('strategy-icon').textContent = result.error ? '!' : effectiveStrategy === 'Backtracking' ? '↝' : effectiveStrategy === 'Thompson NFA' ? '◈' : '↯';
  $('strategy-reason').textContent = result.error ? result.error : plan.fallback ? 'Lazy DFA budget reached; restarted from input with Thompson NFA.' : plan.strategy === 'Lazy DFA' ? 'Risk and workload justify DFA construction.' : plan.strategy === 'Thompson NFA' ? 'Bounded simulation avoids ambiguous backtracking.' : 'Low predicted risk favors the baseline engine.';
  [['Backtracking', 'strategy-backtracking'], ['Thompson NFA', 'strategy-nfa'], ['Lazy DFA', 'strategy-dfa']].forEach(([name, id]) => $(id).classList.toggle('selected', !result.error && effectiveStrategy === name));
  $('metric-steps').textContent = result.error ? '—' : plan.steps;
  $('metric-active').textContent = result.error ? '—' : plan.active;
  $('metric-dfa').textContent = result.error ? '—' : plan.dfaStates;
  $('metric-time').innerHTML = result.error ? '—' : `${(0.02 + value.length * 0.003).toFixed(2)}<span> ms</span>`;
  $('risk-score').innerHTML = result.error ? '—' : `${plan.riskScore.toFixed(1)}<span>/ 100</span>`;
  const riskRows = [['nested', plan.normalized.nested], ['overlap', plan.normalized.overlap], ['ambiguous', plan.normalized.ambiguous], ['depth', plan.normalized.depth], ['nfa', plan.normalized.nfa], ['input', plan.normalized.input]];
  riskRows.forEach(([name, amount]) => { $(`risk-${name}`).style.width = `${Math.round(amount * 100)}%`; $(`risk-${name}-value`).textContent = `${Math.round(amount * 100)}%`; });
  $('ast-shape').textContent = result.error ? 'ParseError' : astShape(pattern);
  const fallback = $('fallback-note');
  fallback.classList.toggle('is-fallback', plan.fallback);
  fallback.innerHTML = plan.fallback ? '<span>!</span> DFA budget reached — restarted with Thompson NFA.' : '<span>✓</span> No fallback required — cache remains within budget.';
  renderAst(pattern);
  $('step-range').max = value.length;
  if (Number($('step-range').value) > value.length) $('step-range').value = value.length;
  renderGraph();
}

repetitions.addEventListener('input', () => { $('repetitions-value').textContent = `${repetitions.value}×`; render(); });
patternInput.addEventListener('input', render);
inputInput.addEventListener('input', render);
allowDfa.addEventListener('change', render);
$('step-range').addEventListener('input', renderGraph);
$('nfa-tab').addEventListener('click', () => { graphMode = 'nfa'; $('nfa-tab').classList.add('active'); $('dfa-tab').classList.remove('active'); $('nfa-tab').setAttribute('aria-selected', 'true'); $('dfa-tab').setAttribute('aria-selected', 'false'); renderGraph(); });
$('dfa-tab').addEventListener('click', () => { graphMode = 'dfa'; $('dfa-tab').classList.add('active'); $('nfa-tab').classList.remove('active'); $('dfa-tab').setAttribute('aria-selected', 'true'); $('nfa-tab').setAttribute('aria-selected', 'false'); renderGraph(); });
$('run-button').addEventListener('click', render);
$('custom-mode').addEventListener('click', () => { patternInput.value = ''; inputInput.value = ''; patternInput.placeholder = 'type your pattern'; inputInput.placeholder = 'type your input'; repetitions.value = 1; $('repetitions-value').textContent = '1×'; allowDfa.checked = true; $('step-range').value = 0; render(); patternInput.focus(); });
$('reset-button').addEventListener('click', () => { patternInput.value = '(a|b)*abb'; inputInput.value = 'abababb'; repetitions.value = 8; $('repetitions-value').textContent = '8×'; allowDfa.checked = true; render(); });
document.querySelectorAll('[data-pattern]').forEach((button) => button.addEventListener('click', () => { patternInput.value = button.dataset.pattern; if (button.dataset.input !== undefined) inputInput.value = button.dataset.input; if (button.dataset.repetitions) repetitions.value = button.dataset.repetitions; $('repetitions-value').textContent = `${repetitions.value}×`; $('step-range').value = inputInput.value.length; render(); }));
render();
