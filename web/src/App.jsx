import { useEffect, useMemo, useRef, useState } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';

const LEVELS = [
  { id: 'quick', label: 'Quick', depth: 5, movetime: 250 },
  { id: 'club', label: 'Club', depth: 7, movetime: 650 },
  { id: 'strong', label: 'Strong', depth: 9, movetime: 1400 },
  { id: 'max', label: 'Max', depth: 11, movetime: 3000 },
];

function colorName(color) {
  return color === 'w' ? 'White' : 'Black';
}

function getStatus(game, thinking, engineReady, engineError) {
  if (engineError) return 'Engine unavailable';
  if (!engineReady) return 'Loading Pickle…';
  if (game.isCheckmate()) return `${colorName(game.turn() === 'w' ? 'b' : 'w')} wins by checkmate`;
  if (game.isStalemate()) return 'Draw by stalemate';
  if (game.isThreefoldRepetition()) return 'Draw by repetition';
  if (game.isInsufficientMaterial()) return 'Draw by insufficient material';
  if (game.isDraw()) return 'Draw';
  if (thinking) return 'Pickle is thinking…';
  return `${colorName(game.turn())} to move${game.inCheck() ? ' — check' : ''}`;
}

function formatEval(cp) {
  const value = cp / 100;
  if (Math.abs(value) >= 99) return value > 0 ? '+M' : '-M';
  return `${value >= 0 ? '+' : ''}${value.toFixed(1)}`;
}

function EvalBar({ score }) {
  const clamped = Math.max(-700, Math.min(700, score));
  const whitePercent = 50 + (clamped / 700) * 45;
  return (
    <div className="eval-bar" aria-label={`Evaluation ${formatEval(score)}`}>
      <div className="eval-black" />
      <div className="eval-white" style={{ height: `${whitePercent}%` }} />
      <span className={score >= 0 ? 'eval-label on-white' : 'eval-label on-black'}>
        {formatEval(score)}
      </span>
    </div>
  );
}

function PlayerRow({ name, subtitle, engine, active, color }) {
  return (
    <div className={`player-row ${active ? 'active' : ''}`}>
      <div className={`avatar ${engine ? 'engine-avatar' : 'human-avatar'}`}>
        {engine ? 'P' : color === 'w' ? 'W' : 'B'}
      </div>
      <div className="player-copy">
        <strong>{name}</strong>
        <span>{subtitle}</span>
      </div>
      {active && <span className="turn-dot" title="Side to move" />}
    </div>
  );
}

function MoveList({ moves }) {
  const rows = [];
  for (let i = 0; i < moves.length; i += 2) {
    rows.push({ number: i / 2 + 1, white: moves[i], black: moves[i + 1] || '' });
  }

  return (
    <div className="move-list">
      {rows.length === 0 ? (
        <div className="empty-moves">Moves will appear here.</div>
      ) : (
        rows.map((row) => (
          <div className="move-row" key={row.number}>
            <span className="move-number">{row.number}.</span>
            <span>{row.white}</span>
            <span>{row.black}</span>
          </div>
        ))
      )}
    </div>
  );
}

export default function App() {
  const gameRef = useRef(new Chess());
  const workerRef = useRef(null);
  const requestSeqRef = useRef(0);
  const activeRequestRef = useRef(0);
  const pendingFenRef = useRef('');
  const pendingEngineColorRef = useRef('b');

  const [fen, setFen] = useState(gameRef.current.fen());
  const [humanColor, setHumanColor] = useState('w');
  const [orientation, setOrientation] = useState('white');
  const [levelId, setLevelId] = useState('strong');
  const [engineReady, setEngineReady] = useState(false);
  const [engineError, setEngineError] = useState('');
  const [thinking, setThinking] = useState(false);
  const [whiteEval, setWhiteEval] = useState(0);
  const [searchInfo, setSearchInfo] = useState({ depth: 0, nodes: 0 });

  const level = LEVELS.find((item) => item.id === levelId) || LEVELS[2];
  const game = gameRef.current;
  const moves = game.history();
  const verboseHistory = game.history({ verbose: true });
  const lastMove = verboseHistory[verboseHistory.length - 1];
  const status = getStatus(game, thinking, engineReady, engineError);

  function syncPosition() {
    setFen(gameRef.current.fen());
  }

  function applyUciMove(uci) {
    if (!uci || uci === '0000' || uci.length < 4) return false;
    try {
      gameRef.current.move({
        from: uci.slice(0, 2),
        to: uci.slice(2, 4),
        promotion: uci[4] || 'q',
      });
      syncPosition();
      return true;
    } catch {
      return false;
    }
  }

  useEffect(() => {
    const worker = new Worker('/pickle-worker.js');
    workerRef.current = worker;

    worker.onmessage = (event) => {
      const message = event.data || {};
      if (message.type === 'ready') {
        setEngineReady(true);
        setEngineError('');
        return;
      }
      if (message.type === 'error') {
        if (!message.requestId || message.requestId === activeRequestRef.current) {
          setEngineError(message.message || 'Pickle failed to load.');
          setThinking(false);
        }
        return;
      }
      if (message.type !== 'result' || message.requestId !== activeRequestRef.current) return;
      if (message.fen !== pendingFenRef.current || gameRef.current.fen() !== message.fen) return;

      const rootColor = pendingEngineColorRef.current;
      const scoreFromWhite = rootColor === 'w' ? message.score : -message.score;
      setWhiteEval(scoreFromWhite);
      setSearchInfo({ depth: message.depth || 0, nodes: message.nodes || 0 });
      setThinking(false);

      if (!applyUciMove(message.move) && !gameRef.current.isGameOver()) {
        setEngineError(`Pickle returned an invalid move: ${message.move || 'none'}`);
      }
    };

    worker.postMessage({ type: 'init' });
    return () => worker.terminate();
  }, []);

  useEffect(() => {
    const current = gameRef.current;
    if (!engineReady || engineError || thinking || current.isGameOver()) return;
    if (current.turn() === humanColor) return;

    const requestId = ++requestSeqRef.current;
    activeRequestRef.current = requestId;
    pendingFenRef.current = current.fen();
    pendingEngineColorRef.current = current.turn();
    setThinking(true);

    workerRef.current?.postMessage({
      type: 'search',
      requestId,
      fen: current.fen(),
      depth: level.depth,
      movetime: level.movetime,
    });
  }, [fen, humanColor, engineReady, engineError, thinking, level.depth, level.movetime]);

  function onPieceDrop({ sourceSquare, targetSquare }) {
    if (!targetSquare || thinking || gameRef.current.isGameOver()) return false;
    if (gameRef.current.turn() !== humanColor) return false;

    try {
      const piece = gameRef.current.get(sourceSquare);
      if (!piece || piece.color !== humanColor) return false;

      const targetRank = targetSquare[1];
      const promotion = piece.type === 'p' && (targetRank === '1' || targetRank === '8') ? 'q' : undefined;
      gameRef.current.move({ from: sourceSquare, to: targetSquare, promotion });
      syncPosition();
      return true;
    } catch {
      return false;
    }
  }

  function startNewGame(color = humanColor) {
    ++requestSeqRef.current;
    activeRequestRef.current = requestSeqRef.current;
    pendingFenRef.current = '';
    gameRef.current = new Chess();
    setHumanColor(color);
    setOrientation(color === 'w' ? 'white' : 'black');
    setWhiteEval(0);
    setSearchInfo({ depth: 0, nodes: 0 });
    setEngineError('');
    setThinking(false);
    setFen(gameRef.current.fen());
  }

  function undoTurn() {
    if (thinking || moves.length === 0) return;
    gameRef.current.undo();
    if (gameRef.current.history().length > 0 && gameRef.current.turn() !== humanColor) {
      gameRef.current.undo();
    }
    setWhiteEval(0);
    setSearchInfo({ depth: 0, nodes: 0 });
    syncPosition();
  }

  const squareStyles = useMemo(() => {
    if (!lastMove) return {};
    const highlight = { background: 'rgba(238, 214, 92, 0.44)' };
    return { [lastMove.from]: highlight, [lastMove.to]: highlight };
  }, [fen]);

  const boardOptions = useMemo(() => ({
    id: 'pickle-board',
    position: fen,
    onPieceDrop,
    boardOrientation: orientation,
    animationDurationInMs: 170,
    lightSquareStyle: { backgroundColor: '#e7e9cf' },
    darkSquareStyle: { backgroundColor: '#71905d' },
    squareStyles,
    boardStyle: {
      borderRadius: '7px',
      overflow: 'hidden',
      boxShadow: '0 18px 45px rgba(0, 0, 0, 0.28)',
    },
  }), [fen, orientation, squareStyles, thinking, humanColor]);

  const topColor = orientation === 'white' ? 'b' : 'w';
  const bottomColor = orientation === 'white' ? 'w' : 'b';
  const engineColor = humanColor === 'w' ? 'b' : 'w';

  return (
    <div className="app-shell">
      <header className="topbar">
        <a className="brand" href="https://github.com/DMDTague/Pickle" target="_blank" rel="noreferrer">
          <span className="brand-mark">P</span>
          <span>
            <strong>Pickle</strong>
            <small>C++ chess engine</small>
          </span>
        </a>
        <div className={`engine-state ${engineReady && !engineError ? 'online' : ''}`}>
          <span />
          {engineError ? 'engine error' : engineReady ? 'engine ready' : 'loading engine'}
        </div>
      </header>

      <main className="game-layout">
        <section className="board-column">
          <PlayerRow
            name={topColor === engineColor ? 'Pickle' : 'You'}
            subtitle={topColor === engineColor ? `${level.label} · depth ${level.depth}` : 'Human'}
            engine={topColor === engineColor}
            active={game.turn() === topColor && !game.isGameOver()}
            color={topColor}
          />

          <div className="board-area">
            <EvalBar score={whiteEval} />
            <div className="board-wrap">
              <Chessboard options={boardOptions} />
            </div>
          </div>

          <PlayerRow
            name={bottomColor === engineColor ? 'Pickle' : 'You'}
            subtitle={bottomColor === engineColor ? `${level.label} · depth ${level.depth}` : 'Human'}
            engine={bottomColor === engineColor}
            active={game.turn() === bottomColor && !game.isGameOver()}
            color={bottomColor}
          />
        </section>

        <aside className="side-panel">
          <div className="panel-header">
            <div>
              <span className="eyebrow">PLAY PICKLE</span>
              <h1>New game</h1>
            </div>
            <button className="icon-button" onClick={() => setOrientation((o) => o === 'white' ? 'black' : 'white')} title="Flip board">
              ↻
            </button>
          </div>

          <div className="status-card">
            <strong>{status}</strong>
            <span>
              {searchInfo.depth > 0
                ? `Last search: depth ${searchInfo.depth} · ${searchInfo.nodes.toLocaleString()} nodes`
                : 'Pickle runs locally in your browser.'}
            </span>
          </div>

          <div className="moves-title">
            <span>Moves</span>
            <span>{moves.length} ply</span>
          </div>
          <MoveList moves={moves} />

          <div className="controls">
            <label>
              <span>Strength</span>
              <select value={levelId} onChange={(e) => setLevelId(e.target.value)} disabled={thinking}>
                {LEVELS.map((item) => (
                  <option key={item.id} value={item.id}>{item.label} · d{item.depth}</option>
                ))}
              </select>
            </label>

            <div className="color-picker">
              <span>Play as</span>
              <div>
                <button className={humanColor === 'w' ? 'selected' : ''} onClick={() => startNewGame('w')}>White</button>
                <button className={humanColor === 'b' ? 'selected' : ''} onClick={() => startNewGame('b')}>Black</button>
              </div>
            </div>

            <div className="button-row">
              <button className="secondary" onClick={undoTurn} disabled={thinking || moves.length === 0}>Undo</button>
              <button className="primary" onClick={() => startNewGame(humanColor)}>New game</button>
            </div>
          </div>
        </aside>
      </main>

      <footer>
        <span>Pickle engine by Dylan Tague.</span>
        <span>Board component: react-chessboard · Rules: chess.js</span>
      </footer>
    </div>
  );
}
