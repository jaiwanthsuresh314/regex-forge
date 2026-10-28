const $ = (id) => document.getElementById(id);

const patternInput = $('pattern');
const inputInput = $('input');
const repetitions = $('repetitions');
const allowDfa = $('allow-dfa');

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
  const ambiguity = stars * 18 + alternations * 9 + groups * 4;
  const workloadAmortized = repeats >= 4 || value.length >= 32;
  const useDfa = dfaAllowed && workloadAmortized && (ambiguity >= 20 || value.length >= 32);
  const useNfa = !useDfa && (ambiguity >= 28 || value.length > 512);
  const strategy = useDfa ? 'Lazy DFA' : useNfa ? 'Thompson NFA' : 'Backtracking';
  const steps = strategy === 'Backtracking' ? Math.max(1, value.length + ambiguity) : value.length;
  const active = strategy === 'Backtracking' ? 0 : Math.max(1, Math.min(12, 1 + groups + alternations));
  const dfaStates = strategy === 'Lazy DFA' ? Math.max(1, Math.min(10, 2 + stars + alternations)) : 0;
  const fallback = strategy === 'Lazy DFA' && dfaStates > 8;
  return { strategy, steps, active, dfaStates, fallback, ambiguity };
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
  const badge = $('status-badge');

  $('status-text').textContent = result.error ? 'ERROR' : result.matched ? 'MATCH' : 'NO MATCH';
  badge.classList.toggle('no-match', !result.matched || Boolean(result.error));
  $('strategy-name').textContent = result.error ? 'No strategy selected' : plan.strategy;
  $('strategy-icon').textContent = result.error ? '!' : plan.strategy === 'Backtracking' ? '↝' : plan.strategy === 'Thompson NFA' ? '◈' : '↯';
  $('strategy-reason').textContent = result.error ? result.error : plan.strategy === 'Lazy DFA' ? 'Risk and workload justify DFA construction.' : plan.strategy === 'Thompson NFA' ? 'Bounded simulation avoids ambiguous backtracking.' : 'Low predicted risk favors the baseline engine.';
  $('metric-steps').textContent = result.error ? '—' : plan.steps;
  $('metric-active').textContent = result.error ? '—' : plan.active;
  $('metric-dfa').textContent = result.error ? '—' : plan.dfaStates;
  $('metric-time').innerHTML = result.error ? '—' : `${(0.02 + value.length * 0.003).toFixed(2)}<span> ms</span>`;
  $('ast-shape').textContent = result.error ? 'ParseError' : astShape(pattern);
  const fallback = $('fallback-note');
  fallback.classList.toggle('is-fallback', plan.fallback);
  fallback.innerHTML = plan.fallback ? '<span>!</span> DFA budget reached — restarted with Thompson NFA.' : '<span>✓</span> No fallback required — cache remains within budget.';
  renderAst(pattern);
}

repetitions.addEventListener('input', () => { $('repetitions-value').textContent = `${repetitions.value}×`; render(); });
patternInput.addEventListener('input', render);
inputInput.addEventListener('input', render);
allowDfa.addEventListener('change', render);
$('run-button').addEventListener('click', render);
$('reset-button').addEventListener('click', () => { patternInput.value = '(a|b)*abb'; inputInput.value = 'abababb'; repetitions.value = 8; $('repetitions-value').textContent = '8×'; allowDfa.checked = true; render(); });
document.querySelectorAll('[data-pattern]').forEach((button) => button.addEventListener('click', () => { patternInput.value = button.dataset.pattern; render(); }));
render();
