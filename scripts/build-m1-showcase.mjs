import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const output = path.join(root, 'reports', 'm1-showcase');
const read = name => JSON.parse(fs.readFileSync(path.join(root, 'benchmarks', 'milestones', name), 'utf8').replace(/^\uFEFF/, ''));
const start = read('start.json');
const m0 = read('m0.json');
const m1 = read('m1.json');
const usage = JSON.parse(fs.readFileSync(path.join(output, 'account-usage.json'), 'utf8'));
if (usage.used_percent < 0 || usage.used_percent > 100 || usage.used_percent + usage.remaining_percent !== 100)
  throw new Error('Invalid account usage snapshot');
const fields = ['input_tokens', 'cached_input_tokens', 'output_tokens', 'reasoning_output_tokens'];
const key = item => `${item.model}|${item.reasoning_effort}`;

function grouped(rows) {
  const result = new Map();
  for (const row of rows) {
    const id = key(row);
    const totals = result.get(id) ?? Object.fromEntries(fields.map(field => [field, 0]));
    for (const field of fields) totals[field] += row[field] ?? 0;
    result.set(id, totals);
  }
  return result;
}
function difference(after, before, id) {
  const newer = after.get(id) ?? {};
  const older = before.get(id) ?? {};
  const result = Object.fromEntries(fields.map(field => [field, (newer[field] ?? 0) - (older[field] ?? 0)]));
  result.fresh = result.input_tokens - result.cached_input_tokens;
  if (Object.values(result).some(value => value < 0) || result.reasoning_output_tokens > result.output_tokens)
    throw new Error(`Invalid token delta for ${id}`);
  return result;
}
const baseline = grouped(start.sessions);
const checkpoint0 = grouped(m0.models);
const checkpoint1 = grouped(m1.models);
const ids = ['gpt-6-sol|xhigh', 'gpt-6-sol|medium', 'gpt-6-sol|high'];
const roles = ids.map((id, index) => ({
  id, label: ['Coordinator', 'Builder', 'Reviewer'][index],
  effort: ['xhigh', 'medium', 'high'][index],
  ...difference(checkpoint1, baseline, id)
}));
const periods = [
  { label: 'M0 checkpoint', ...sum(ids.map(id => difference(checkpoint0, baseline, id))) },
  { label: 'M1 checkpoint', ...sum(ids.map(id => difference(checkpoint1, checkpoint0, id))) }
];
const overall = sum(roles);
for (const field of [...fields, 'fresh']) {
  if (periods[0][field] + periods[1][field] !== overall[field])
    throw new Error(`Milestone totals do not reconcile for ${field}`);
}
const cacheShare = overall.cached_input_tokens / overall.input_tokens * 100;
const fmt = value => new Intl.NumberFormat('en-US').format(value);
const short = value => value >= 1e6 ? `${(value / 1e6).toFixed(2)}M` : `${(value / 1e3).toFixed(1)}k`;
const pct = value => `${value.toFixed(1)}%`;
function sum(rows) {
  const result = Object.fromEntries(fields.map(field => [field, 0]));
  result.fresh = 0;
  for (const row of rows) {
    for (const field of fields) result[field] += row[field];
    result.fresh += row.fresh;
  }
  return result;
}
function line(x1, y1, x2, y2, color = '#555', opacity = 1, width = 1) {
  return `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" stroke="${color}" stroke-opacity="${opacity}" stroke-width="${width}"/>`;
}
function text(x, y, value, size = 13, color = '#d8e3ec', anchor = 'start', weight = 500) {
  return `<text x="${x}" y="${y}" fill="${color}" font-size="${size}" font-weight="${weight}" text-anchor="${anchor}" font-family="Inter, Segoe UI, Arial, sans-serif">${value}</text>`;
}

function inputChart() {
  const max = 20_000_000, left = 164, right = 674, width = right - left;
  const ticks = [0, 5, 10, 15, 20];
  const grid = ticks.map(t => {
    const x = left + width * t / 20;
    return line(x, 25, x, 236, '#666666', t === 0 ? 0.7 : 0.22) + text(x, 258, `${t}M`, 12, '#a5a5a5', 'middle');
  }).join('');
  const bars = roles.map((role, index) => {
    const y = 60 + index * 77;
    const totalWidth = width * role.input_tokens / max;
    const cachedWidth = width * role.cached_input_tokens / max;
    return [
      text(0, y + 4, `Sol · ${role.effort}`, 16, '#f1f1f1', 'start', 600),
      text(0, y + 23, role.label.toLowerCase(), 12, '#aaa'),
      line(left, y, left + cachedWidth, y, '#dedede', 1, 4),
      `<line x1="${(left + cachedWidth).toFixed(3)}" y1="${y}" x2="${(left + totalWidth).toFixed(3)}" y2="${y}" stroke="#f2c679" stroke-width="5" stroke-linecap="round"/>`,
      `<circle cx="${(left + totalWidth).toFixed(3)}" cy="${y}" r="4.5" fill="#f2c679"/>`,
      text(744, y + 5, short(role.input_tokens), 15, '#f1f1f1', 'end', 600)
    ].join('');
  }).join('');
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 286" role="img" aria-labelledby="input-title input-desc">
<title id="input-title">Recorded input by M1 role</title><desc id="input-desc">Coordinator 19.02 million input tokens, 1.2 percent fresh. Builder 5.27 million, 2.2 percent fresh. Reviewer 1.59 million, 7.3 percent fresh. The remainder is cached input.</desc>
<rect width="760" height="286" fill="#050505"/>${grid}${bars}${line(left, 236, right, 236, '#999', 0.55)}${text((left + right) / 2, 282, 'Total recorded input · millions of tokens', 12, '#b8b8b8', 'middle')}
</svg>`;
}

function periodChart() {
  const left = 140, right = 674, width = right - left, max = 300_000;
  const grid = [0, 100, 200, 300].map(t => {
    const x = left + width * t / 300;
    return line(x, 24, x, 242, '#666', t === 0 ? 0.7 : 0.22) + text(x, 263, `${t}k`, 12, '#a5a5a5', 'middle');
  }).join('');
  const groups = periods.map((period, index) => {
    const top = 71 + index * 107;
    const freshWidth = width * period.fresh / max;
    const outputWidth = width * period.output_tokens / max;
    return [
      text(0, top + 4, index === 0 ? 'Start → M0' : 'M0 → M1', 16, '#f1f1f1', 'start', 600),
      text(0, top + 24, 'checkpoint interval', 12, '#aaa'),
      line(left, top - 10, left + freshWidth, top - 10, '#f2c679', 1, 3),
      `<circle cx="${(left + freshWidth).toFixed(3)}" cy="${top - 10}" r="4.5" fill="#f2c679"/>`,
      text(left + freshWidth + 10, top - 5, `${short(period.fresh)} fresh`, 13, '#f1f1f1', 'start', 600),
      line(left, top + 20, left + outputWidth, top + 20, '#b7b5de', 1, 3),
      `<circle cx="${(left + outputWidth).toFixed(3)}" cy="${top + 20}" r="4.5" fill="#b7b5de"/>`,
      text(left + outputWidth + 10, top + 25, `${short(period.output_tokens)} output`, 13, '#f1f1f1', 'start', 600)
    ].join('');
  }).join('');
  return `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 760 292" role="img" aria-labelledby="period-title period-desc">
<title id="period-title">Fresh input and output added by milestone interval</title><desc id="period-desc">Start to M0: 215.4 thousand fresh input and 61.1 thousand output, including 22.9 thousand reasoning. M0 to M1: 242.4 thousand fresh input and 59.6 thousand output, including 26.4 thousand reasoning.</desc>
<rect width="760" height="292" fill="#050505"/>${grid}${groups}${line(left, 242, right, 242, '#999', 0.55)}${text((left + right) / 2, 287, 'Tokens added in interval · thousands', 12, '#b8b8b8', 'middle')}
</svg>`;
}

const inputSvg = inputChart();
const periodSvg = periodChart();
const captured = new Date(m1.generated_at_utc).toLocaleString('en-GB', { timeZone: 'UTC', day: '2-digit', month: 'short', year: 'numeric', hour: '2-digit', minute: '2-digit' });
const usageCaptured = new Date(usage.captured_at_utc).toLocaleString('en-GB', { timeZone: 'UTC', day: '2-digit', month: 'short', hour: '2-digit', minute: '2-digit' });
const usageReset = new Date(usage.resets_at_utc).toLocaleString('en-GB', { timeZone: 'UTC', day: '2-digit', month: 'short', hour: '2-digit', minute: '2-digit' });


const tableRows = roles.map(role => `<tr><td>Sol · ${role.effort}</td><td>${short(role.cached_input_tokens)}</td><td>${short(role.fresh)}</td><td>${short(role.output_tokens)}</td><td>${short(role.reasoning_output_tokens)}</td></tr>`).join('');
const replacements = new Map([
  ['USAGE_USED', usage.used_percent], ['USAGE_REMAINING', usage.remaining_percent],
  ['USAGE_CAPTURED', usageCaptured], ['USAGE_RESET', usageReset],
  ['INPUT_SVG', inputSvg], ['PERIOD_SVG', periodSvg],
  ['TOTAL_INPUT', fmt(overall.input_tokens)], ['CACHE_SHARE', pct(cacheShare)],
  ['TOTAL_OUTPUT', fmt(overall.output_tokens)], ['REASONING', fmt(overall.reasoning_output_tokens)],
  ['TABLE_ROWS', tableRows], ['M1_CAPTURED', captured]
]);
const template = fs.readFileSync(path.join(output, 'template.html'), 'utf8');
const editorialHtml = [...replacements].reduce((page, [name, value]) => page.replaceAll(`{{${name}}}`, String(value)), template);
if (/{{[A-Z_]+}}/.test(editorialHtml)) throw new Error('Unfilled showcase template placeholder');

fs.mkdirSync(output, { recursive: true });
fs.writeFileSync(path.join(output, 'recorded-input.svg'), inputSvg);
fs.writeFileSync(path.join(output, 'milestone-increments.svg'), periodSvg);
fs.writeFileSync(path.join(output, 'index.html'), editorialHtml);
fs.writeFileSync(path.join(output, 'data.json'), JSON.stringify({
  source: ['benchmarks/milestones/start.json', 'benchmarks/milestones/m0.json', 'benchmarks/milestones/m1.json'],
  captured_at_utc: m1.generated_at_utc,
  roles, periods, overall, cache_share_percent: cacheShare,
  account_usage_snapshot: usage
}, null, 2) + '\n');
console.log(`Built ${output}`);
console.log(`Input ${fmt(overall.input_tokens)}; cached ${fmt(overall.cached_input_tokens)}; fresh ${fmt(overall.fresh)}; output ${fmt(overall.output_tokens)}; reasoning ${fmt(overall.reasoning_output_tokens)}`);
