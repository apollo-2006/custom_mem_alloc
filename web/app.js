// Page logic for the allocator demo. All allocation goes through the C
// allocator; this file only records what it was asked for and draws the
// block list that print_heap_metadata() reports.
let HEADER = 0;  // measured at boot: 24 bytes on 64-bit targets, 12 on wasm32
const $ = (id) => document.getElementById(id);
let M, printed = [], live = new Map(), selected = null;

function log(text, cls = '') {
  const line = document.createElement('div');
  if (cls) line.className = cls;
  line.textContent = text;
  $('log').prepend(line);
}

async function boot(reason) {
  printed = [];
  const errors = [];
  M = await Allocator({
    print: (t) => printed.push(t),
    printErr: (t) => errors.push(t),
    onAbort: () => {},
  });
  M.__errors = errors;
  live.clear(); selected = null;
  // The arena is mapped lazily on the first my_malloc. Do that now with a probe,
  // which also measures sizeof(BlockHeader) for this target.
  const probe = M._my_malloc(1);
  M._my_free(probe);
  HEADER = probe - blocks()[0].addr;
  $('hdrLegend').textContent = `${HEADER}-byte header (sizeof(BlockHeader) on wasm32)`;
  if (reason) log(reason, 'muted');
  draw();
}

// Parse the allocator's own dump: "Block Address: 0x... | Size: N Bytes | Status: FREE"
function blocks() {
  printed = [];
  M._print_heap_metadata();
  return printed
    .map((l) => l.match(/Block Address: (0x[0-9a-f]+|\(nil\)|0) \| Size: (\d+) Bytes \| Status: (\w+)/i))
    .filter(Boolean)
    .map((m) => ({ addr: Number(m[1]), size: Number(m[2]), free: m[3] === 'FREE' }));
}

const hex = (n) => '0x' + n.toString(16);
const kb = (n) => (n >= 10240 ? (n / 1024).toFixed(0) + ' KB' : n.toLocaleString() + ' B');

function draw() {
  const list = blocks();
  const map = $('map'); map.textContent = '';
  const zoom = Number($('zoom').value);
  $('scaleEnd').textContent = zoom >= 1048576 ? '1 MB' : zoom / 1024 + ' KB';
  let used = 0, free = 0, largest = 0;
  if (list.length) {
    const base = list[0].addr;
    for (const b of list) {
      const start = b.addr - base;
      if (b.free) { free += b.size; largest = Math.max(largest, b.size); } else used += b.size;
      if (start >= zoom) continue;
      const put = (from, len, cls, title, ptr) => {
        const el = document.createElement('div');
        el.className = 'blk ' + cls;
        if (ptr !== undefined) el.dataset.ptr = ptr;
        el.style.left = (from / zoom * 100) + '%';
        el.style.width = Math.max(0.08, len / zoom * 100) + '%';
        el.title = title;
        map.append(el);
      };
      const payload = b.addr + HEADER;
      put(start, HEADER, 'hdr', `header at ${hex(b.addr)}`);
      put(start + HEADER, b.size, (b.free ? 'free' : 'used') + (payload === selected ? ' sel' : ''),
        `${b.free ? 'free' : 'allocated'} ${b.size} bytes at ${hex(payload)}`, b.free ? undefined : payload);
    }
  }
  $('sUsed').textContent = kb(used);
  $('sFree').textContent = kb(free);
  $('sLargest').textContent = kb(largest);
  $('sBlocks').textContent = list.length;
  $('sFrag').textContent = free ? Math.round((1 - largest / free) * 100) + '%' : '0%';

  const sizes = new Map(list.map((b) => [b.addr + HEADER, b.size]));
  const body = $('ptrs'); body.textContent = '';
  // The fragmentation demo leaves ~500 live pointers; rendering them all makes
  // every redraw slow for rows nobody scrolls to.
  const MAX_ROWS = 200;
  let shown = 0;
  for (const [ptr, req] of live) {
    if (shown++ === MAX_ROWS) {
      const tr = document.createElement('tr');
      tr.innerHTML = `<td colspan="4" class="muted">and ${live.size - MAX_ROWS} more</td>`;
      body.append(tr);
      break;
    }
    const tr = document.createElement('tr');
    tr.dataset.ptr = ptr;
    if (ptr === selected) tr.className = 'sel';
    tr.innerHTML = `<td>${hex(ptr)}</td><td>${req}</td><td>${sizes.get(ptr) ?? '?'}</td><td><button>free</button></td>`;
    tr.onmouseenter = () => select(ptr);
    tr.querySelector('button').onclick = () => doFree(ptr);
    body.append(tr);
  }
  $('noPtrs').hidden = live.size > 0;
}

// Hovering a row only moves the highlight; nothing about the heap changed, so
// there is no reason to re-run print_heap_metadata and rebuild both views.
function select(ptr) {
  if (selected === ptr) return;
  selected = ptr;
  for (const tr of $('ptrs').children) tr.classList.toggle('sel', Number(tr.dataset.ptr) === ptr);
  for (const el of $('map').children) el.classList.toggle('sel', Number(el.dataset.ptr) === ptr);
}

function doMalloc(size, fn = '_my_malloc') {
  const p = fn === '_my_calloc' ? M._my_calloc(size, 1) : M[fn](size);
  if (p) { live.set(p, size); log(`${fn.slice(1)}(${size}) = ${hex(p)}`); }
  else log(`${fn.slice(1)}(${size}) = NULL`, 'err');
  return p;
}

function guarded(fn) {
  try { fn(); draw(); }
  catch (e) {
    const why = M.__errors.filter((l) => l.startsWith('my_free')).join(' ') || String(e.message || e);
    log(`aborted: ${why}`, 'err');
    boot('module restarted with a fresh arena');
  }
}

function doFree(ptr) {
  guarded(() => { M._my_free(ptr); live.delete(ptr); if (selected === ptr) selected = null; log(`my_free(${hex(ptr)})`); });
}

Allocator.ready = boot();

$('malloc').onclick = () => guarded(() => doMalloc(Math.max(1, Number($('size').value) | 0)));
$('calloc').onclick = () => guarded(() => doMalloc(Math.max(1, Number($('size').value) | 0), '_my_calloc'));
$('zoom').onchange = draw;
$('freeAll').onclick = () => guarded(() => { for (const p of [...live.keys()]) M._my_free(p); live.clear(); log('freed everything; neighbours coalesce back into one block'); });
$('churn').onclick = () => guarded(() => {
  for (let i = 0; i < 200; i++) {
    const keys = [...live.keys()];
    if (keys.length && Math.random() < 0.45) { const p = keys[(Math.random() * keys.length) | 0]; M._my_free(p); live.delete(p); }
    else { const s = 8 + ((Math.random() ** 3) * 2000) | 0; const p = M._my_malloc(s); if (p) live.set(p, s); }
  }
  log(`200 random calls, ${live.size} live pointers`);
});
$('frag').onclick = () => guarded(() => {
  for (const p of [...live.keys()]) M._my_free(p);
  live.clear();
  const made = [];
  for (;;) { const p = M._my_malloc(1000); if (!p) break; live.set(p, 1000); made.push(p); }
  made.forEach((p, i) => { if (i % 2 === 0) { M._my_free(p); live.delete(p); } });
  const free = blocks().filter((b) => b.free).reduce((a, b) => a + b.size, 0);
  const big = M._my_malloc(4096);
  log(`filled the arena with ${made.length} × my_malloc(1000), freed every other one`);
  log(`${kb(free)} free, but my_malloc(4096) = ${big ? hex(big) : 'NULL'}: first fit needs one hole that big`, big ? '' : 'err');
  if (big) live.set(big, 4096);
  $('zoom').value = '1048576';
});
$('dbl').onclick = () => guarded(() => {
  const p = doMalloc(64); doMalloc(64);
  M._my_free(p); live.delete(p); log(`my_free(${hex(p)})`);
  log(`my_free(${hex(p)}) again…`);
  M._my_free(p);
});
$('interior').onclick = () => guarded(() => {
  const p = doMalloc(128); doMalloc(16);
  log(`my_free(${hex(p + 32)}), 32 bytes into that block…`);
  M._my_free(p + 32);
});
$('huge').onclick = () => guarded(() => {
  // SIZE_MAX on wasm32 is 2^32 - 1. The unsigned wraparound this used to hit
  // would round it up to 0 and hand back a live pointer.
  const p = M._my_malloc(0xffffffff);
  log(`my_malloc(SIZE_MAX) = ${p ? hex(p) : 'NULL'}${p ? '' : ' (rejected before alignment rounding)'}`, p ? 'err' : 'ok');
});
$('bench').onclick = async () => {
  $('bench').disabled = true; $('bres').innerHTML = '<tr><td colspan="4" class="muted">running…</td></tr>';
  await new Promise((r) => setTimeout(r, 30));
  const ops = Number($('bops').value), max = Number($('bmax').value), rows = [];
  for (const [name, mine] of [['my_malloc', 1], ['emscripten malloc', 0]]) {
    let best = Infinity;
    for (let i = 0; i < 3; i++) best = Math.min(best, M._bench_run(mine, ops, max));
    rows.push(`<tr><td>${name}</td><td>${best.toFixed(1)}</td><td>${(1000 / best).toFixed(2)}</td><td>${M._bench_failed()}${M._bench_corrupt() ? ' CORRUPT' : ''}</td></tr>`);
  }
  $('bres').innerHTML = rows.join('');
  $('bench').disabled = false;
  draw();
};
