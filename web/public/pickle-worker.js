let modulePromise = null;

const ENGINE_COMMIT = 'b75b2b8774757c12567e5db01efb402875a76c9b';
const ENGINE_CDN = `https://cdn.jsdelivr.net/gh/DMDTague/Pickle@${ENGINE_COMMIT}/web/public/engine`;
let engineBase = '/engine';

function resetEngine() {
  modulePromise = null;
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

self.onmessage = async (event) => {
  const message = event.data || {};

  if (message.type === 'init') {
    try {
      await getEngine();
      self.postMessage({ type: 'ready' });
    } catch (error) {
      resetEngine();
      self.postMessage({ type: 'error', message: String(error), recoverable: true });
    }
    return;
  }

  if (message.type !== 'search') return;

  try {
    const engine = await getEngine();
    const move = engine.ccall(
      'pickle_best_move',
      'string',
      ['string', 'number', 'number'],
      [message.fen, message.depth, message.movetime],
    );

    const score = engine.ccall('pickle_last_score', 'number', [], []);
    const depth = engine.ccall('pickle_last_depth', 'number', [], []);
    const nodes = engine.ccall('pickle_last_nodes', 'number', [], []);

    self.postMessage({
      type: 'result',
      requestId: message.requestId,
      fen: message.fen,
      move,
      score,
      depth,
      nodes,
    });
  } catch (error) {
    resetEngine();
    self.postMessage({
      type: 'error',
      requestId: message.requestId,
      message: String(error),
      recoverable: true,
    });
  }
};
