let modulePromise = null;

const ENGINE_COMMIT = 'ca68999ab716665f477b9fecb529b632e50a4f2c';
const ENGINE_CDN = `https://cdn.jsdelivr.net/gh/DMDTague/Pickle@${ENGINE_COMMIT}/web/public/engine`;
const TABLEBASE_ENDPOINT = 'https://tablebase.lichess.ovh/standard';
const TABLEBASE_ATTEMPTS = 3;
const TABLEBASE_TIMEOUT_MS = 5500;
const TABLEBASE_BACKOFF_MS = [350, 800];
let engineBase = '/engine';

function resetEngine({ preferCdn = false } = {}) {
  modulePromise = null;
  if (preferCdn) engineBase = ENGINE_CDN;
}

function loadEngineFactory() {
  if (typeof self.createPickleModule === 'function') return;

  try {
    importScripts('/engine/pickle.js');
    engineBase = '/engine';
  } catch {
    importScripts(`${ENGINE_CDN}/pickle.js`);
    engineBase = ENGINE_CDN;
  }
}

async function getEngine() {
  if (!modulePromise) {
    loadEngineFactory();

    modulePromise = self.createPickleModule({
      locateFile: (path) => `${engineBase}/${path}`,
      noInitialRun: true,
    }).then((engine) => {
      engine.ccall('pickle_init', null, [], []);
      return engine;
    });
  }
  return modulePromise;
}

function pieceCountFromFen(fen) {
  const placement = String(fen || '').split(' ')[0] || '';
  let count = 0;
  for (const char of placement) {
    if ('prnbqkPRNBQK'.includes(char)) count += 1;
  }
  return count;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function invertTablebaseCategory(category) {
  const opposite = {
    win: 'loss',
    'syzygy-win': 'syzygy-loss',
    'maybe-win': 'maybe-loss',
    'cursed-win': 'blessed-loss',
    draw: 'draw',
    'blessed-loss': 'cursed-win',
    'maybe-loss': 'maybe-win',
    'syzygy-loss': 'syzygy-win',
    loss: 'win',
    unknown: 'unknown',
  };
  return opposite[category] || 'unknown';
}

function normalizeTablebaseMove(move) {
  return {
    uci: move.uci,
    san: move.san || move.uci,
    outcome: invertTablebaseCategory(move.category),
    rawCategory: move.category,
    dtz: move.dtz ?? null,
    dtm: move.dtm ?? null,
    zeroing: Boolean(move.zeroing),
    checkmate: Boolean(move.checkmate),
  };
}

async function probeTablebase(fen) {
  const pieceCount = pieceCountFromFen(fen);
  if (pieceCount < 2 || pieceCount > 7) return null;

  const url = `${TABLEBASE_ENDPOINT}?fen=${encodeURIComponent(fen)}`;

  for (let attempt = 0; attempt < TABLEBASE_ATTEMPTS; attempt += 1) {
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), TABLEBASE_TIMEOUT_MS);

    try {
      const response = await fetch(url, {
        method: 'GET',
        headers: { Accept: 'application/json' },
        signal: controller.signal,
        cache: 'no-store',
      });

      // 400/404 means this position is not probeable, not that Lichess is down.
      if (response.status === 400 || response.status === 404) return null;

      if (response.ok) {
        const data = await response.json();
        if (!data || data.category === 'unknown' || !Array.isArray(data.moves) || data.moves.length === 0) {
          return null;
        }

        const moves = data.moves
          .filter((move) => move?.uci && /^[a-h][1-8][a-h][1-8][qrbn]?$/.test(move.uci))
          .map(normalizeTablebaseMove);
        if (moves.length === 0) return null;

        return {
          move: moves[0].uci,
          category: data.category,
          dtz: data.dtz ?? null,
          dtm: data.dtm ?? null,
          moves,
        };
      }

      // Rate limits and server-side errors are transient. Retry them.
      if (response.status !== 429 && response.status < 500) return null;
    } catch {
      // Timeout/network failures are retried below. We intentionally do not
      // mark the service globally unavailable; the next position gets a fresh cycle.
    } finally {
      clearTimeout(timer);
    }

    if (attempt < TABLEBASE_ATTEMPTS - 1) {
      await sleep(TABLEBASE_BACKOFF_MS[attempt] || TABLEBASE_BACKOFF_MS.at(-1));
    }
  }

  return null;
}

function sourceName(code) {
  if (code === 2) return 'book';
  if (code === 1) return 'mate';
  return 'search';
}

async function runSearch(message) {
  const engine = await getEngine();

  // Immediate checkmate is absolute. Probe it locally before asking an
  // external tablebase so a mate in one can never be displaced by another
  // game-theoretically winning move.
  let immediateMate = '0000';
  try {
    immediateMate = engine.ccall(
      'pickle_mate_in_one',
      'string',
      ['string'],
      [message.fen],
    );
  } catch {
    immediateMate = '0000';
  }

  if (immediateMate && immediateMate !== '0000') {
    return {
      type: 'result',
      requestId: message.requestId,
      fen: message.fen,
      move: immediateMate,
      score: null,
      depth: 1,
      nodes: 0,
      source: 'mate',
      detail: 'mate-in-one',
      tablebaseMoves: [],
    };
  }

  const tablebase = await probeTablebase(message.fen);
  if (tablebase) {
    return {
      type: 'result',
      requestId: message.requestId,
      fen: message.fen,
      move: tablebase.move,
      score: null,
      depth: 0,
      nodes: 0,
      source: 'tablebase',
      detail: tablebase.category,
      dtz: tablebase.dtz,
      dtm: tablebase.dtm,
      tablebaseMoves: tablebase.moves,
    };
  }

  const move = engine.ccall(
    'pickle_best_move',
    'string',
    ['string', 'number', 'number'],
    [message.fen, message.depth, message.movetime],
  );

  let sourceCode = 0;
  try {
    sourceCode = engine.ccall('pickle_last_source', 'number', [], []);
  } catch {
    sourceCode = 0;
  }

  return {
    type: 'result',
    requestId: message.requestId,
    fen: message.fen,
    move,
    score: engine.ccall('pickle_last_score', 'number', [], []),
    depth: engine.ccall('pickle_last_depth', 'number', [], []),
    nodes: engine.ccall('pickle_last_nodes', 'number', [], []),
    source: sourceName(sourceCode),
    detail: '',
    tablebaseMoves: [],
  };
}

self.onmessage = async (event) => {
  const message = event.data || {};

  if (message.type === 'init') {
    try {
      await getEngine();
      self.postMessage({ type: 'ready' });
    } catch (firstError) {
      try {
        resetEngine({ preferCdn: true });
        await getEngine();
        self.postMessage({ type: 'ready', recovered: true });
      } catch (secondError) {
        resetEngine({ preferCdn: true });
        self.postMessage({
          type: 'error',
          message: `${String(firstError)} | retry: ${String(secondError)}`,
          recoverable: true,
        });
      }
    }
    return;
  }

  if (message.type !== 'search') return;

  try {
    self.postMessage(await runSearch(message));
  } catch (firstError) {
    try {
      resetEngine({ preferCdn: true });
      self.postMessage(await runSearch(message));
    } catch (secondError) {
      resetEngine({ preferCdn: true });
      self.postMessage({
        type: 'error',
        requestId: message.requestId,
        message: `${String(firstError)} | retry: ${String(secondError)}`,
        recoverable: true,
      });
    }
  }
};
