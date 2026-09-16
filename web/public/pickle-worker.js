let modulePromise = null;

const ENGINE_COMMIT = 'b75b2b8774757c12567e5db01efb402875a76c9b';
const ENGINE_CDN = `https://cdn.jsdelivr.net/gh/DMDTague/Pickle@${ENGINE_COMMIT}/web/public/engine`;
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

async function runSearch(message) {
  const engine = await getEngine();
  const move = engine.ccall(
    'pickle_best_move',
    'string',
    ['string', 'number', 'number'],
    [message.fen, message.depth, message.movetime],
  );

  return {
    type: 'result',
    requestId: message.requestId,
    fen: message.fen,
    move,
    score: engine.ccall('pickle_last_score', 'number', [], []),
    depth: engine.ccall('pickle_last_depth', 'number', [], []),
    nodes: engine.ccall('pickle_last_nodes', 'number', [], []),
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
    // A fresh module is cheap compared with leaving the page permanently dead.
    // Retry once against the pinned known-good browser engine build.
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
