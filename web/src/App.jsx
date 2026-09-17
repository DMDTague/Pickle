import { useEffect, useMemo, useRef, useState } from 'react';
import { Chess } from 'chess.js';
import { Chessboard } from 'react-chessboard';

const LEVELS = [
  { id: 'quick', label: 'Quick', depth: 5, movetime: 250 },
  { id: 'club', label: 'Club', depth: 7, movetime: 650 },
  { id: 'strong', label: 'Strong', depth: 9, movetime: 1400 },
  { id: 'max', label: 'Max', depth: 11, movetime: 3000 },
];

const FILES = ['a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'];
const EMPTY_SEARCH_INFO = { depth: 0, nodes: 0, source: '', detail: '', tablebaseMoves: [] };

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

function pieceCountFromFen(fen) {
  const placement = String(fen || '').split(' ')[0] || '';
  let count = 0;
  for (const char of placement) {
    if ('prnbqkPRNBQK'.includes(char)) count += 1;
  }
  return count;
}

function humanizeOutcome(outcome) {
  const labels = {
    win: 'Win',
    'syzygy-win': 'Win',
    'maybe-win': 'Likely win',
    'cursed-win': 'Cursed win',
    draw: 'Draw',
    'blessed-loss': 'Blessed loss',
    'maybe-loss': 'Likely loss',
    'syzygy-loss': 'Loss',
    loss: 'Loss',
    unknown: 'Unknown',
  };
  return labels[outcome] || 'Unknown';
}

function outcomeClass(outcome) {
  if (outcome?.includes('win')) return 'win';
  if (outcome === 'draw') return 'draw';
  if (outcome?.includes('loss')) return 'loss';
  return 'unknown';
}

function EvalBar({ score, tablebase }) {
  const clamped = Math.max(-700, Math.min(700, score));
  const whitePercent = tablebase ? 50 : 50 + (clamped / 700) * 45;
  return (
    <div className={`eval-bar ${tablebase ? 'tablebase-mode' : ''}`} aria-label={tablebase ? 'Tablebase position' : `Evaluation ${formatEval(score)}`}>
      <div className="eval-black" />
      <div className="eval-white" style={{ height: `${whitePercent}%` }} />
      {tablebase ? (
        <span className="eval-tablebase-label">TB</span>
      ) : (
        <span className={score >= 0 ? 'eval-label on-white' : 'eval-label on-black'}>
          {formatEval(score)}
        </span>
      )}
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

function DecisionPanel({ info }) {
  if (info.source === 'tablebase') {
    return (
      <div className="decision-panel">
        <div className="decision-tabs">
          <span className="decision-tab active"><span className="decision-icon">▤</span>Tablebase</span>
          <span className="decision-meta">Syzygy · Lichess</span>
        </div>
        <div className="tablebase-list">
          {info.tablebaseMoves.map((move, index) => (
            <div className={`tablebase-row ${index === 0 ? 'best' : ''}`} key={`${move.uci}-${index}`}>
              <span className="tablebase-move">{move.san || move.uci}</span>
              <span className={`tablebase-outcome ${outcomeClass(move.outcome)}`}>
                {humanizeOutcome(move.outcome)}
              </span>
            </div>
          ))}
        </div>
        <div className="decision-footnote">Moves are ordered by the tablebase. No centipawn evaluation is shown.</div>
      </div>
    );
  }

  if (info.source === 'book') {
    return (
      <div className="decision-panel book-panel">
        <div className="decision-tabs">
          <span className="decision-tab active"><span className="decision-icon">▤</span>Book</span>
          <span className="decision-meta">Opening</span>
        </div>
        <div className="book-source">
          <strong>PickleBook</strong>
          <span>Lichess Elite · horizon-weighted for Pickle</span>
        </div>
      </div>
    );
  }

  return null;
}

function findKingSquare(game, kingColor) {
  for (const file of FILES) {
    for (let rank = 1; rank <= 8; rank += 1) {
      const square = `${file}${rank}`;
      const piece = game.get(square);
      if (piece?.type === 'k' && piece.color === kingColor) return square;
    }
  }
  return '';
}

export default function App() {
  const gameRef = useRef(new Chess());
  const workerRef = useRef(null);
  const searchTimerRef = useRef(null);
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
  const [searchInfo, setSearchInfo] = useState(EMPTY_SEARCH_INFO);
  const [workerEpoch, setWorkerEpoch] = useState(0);
  const [selectedSquare, setSelectedSquare] = useState('');
  const [moveOptionStyles, setMoveOptionStyles] = useState({});

  const level = LEVELS.find((item) => item.id === levelId) || LEVELS[2];
  const game = gameRef.current;
  const moves = game.history();
  const verboseHistory = game.history({ verbose: true });
  const lastMove = verboseHistory[verboseHistory.length - 1];
  const status = getStatus(game, thinking, engineReady, engineError);

  function clearSearchWatchdog() {
    if (searchTimerRef.current !== null) {
      window.clearTimeout(searchTimerRef.current);
      searchTimerRef.current = null;
    }
  }

  function clearSelection() {
    setSelectedSquare('');
    setMoveOptionStyles({});
  }

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
      clearSelection();
      syncPosition();
      return true;
    } catch {
      return false;
    }
  }

  function restartWorker() {
    clearSearchWatchdog();
    workerRef.current?.terminate();
    workerRef.current = null;
    setThinking(false);
    setEngineReady(false);
    setEngineError('');
    setWorkerEpoch((value) => value + 1);
  }

  useEffect(() => {
    const worker = new Worker(`/pickle-worker.js?v=${workerEpoch}`);
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
          clearSearchWatchdog();
          setThinking(false);
          if (message.recoverable) {
            window.setTimeout(restartWorker, 100);
          } else {
            setEngineError(message.message || 'Pickle failed to load.');
          }
        }
        return;
      }
      if (message.type !== 'result' || message.requestId !== activeRequestRef.current) return;
      if (message.fen !== pendingFenRef.current || gameRef.current.fen() !== message.fen) return;

      clearSearchWatchdog();
      const rootColor = pendingEngineColorRef.current;
      if (Number.isFinite(message.score)) {
        const scoreFromWhite = rootColor === 'w' ? message.score : -message.score;
        setWhiteEval(scoreFromWhite);
      }
      setSearchInfo({
        depth: message.depth || 0,
        nodes: message.nodes || 0,
        source: message.source || 'search',
        detail: message.detail || '',
        tablebaseMoves: Array.isArray(message.tablebaseMoves) ? message.tablebaseMoves : [],
      });
      setThinking(false);

      if (!applyUciMove(message.move) && !gameRef.current.isGameOver()) {
        setEngineError(`Pickle returned an invalid move: ${message.move || 'none'}`);
      }
    };

    worker.onerror = () => {
      window.setTimeout(restartWorker, 100);
    };

    worker.postMessage({ type: 'init' });
    return () => {
      worker.terminate();
      if (workerRef.current === worker) workerRef.current = null;
    };
  }, [workerEpoch]);

  useEffect(() => {
    const current = gameRef.current;
    if (!engineReady || engineError || thinking || current.isGameOver()) return;
    if (current.turn() === humanColor) return;

    clearSelection();
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

    clearSearchWatchdog();
    const searchFen = current.fen();
    const tablebaseEligible = pieceCountFromFen(searchFen) <= 7;
    const hardLimit = tablebaseEligible ? 30000 : Math.max(6000, level.movetime * 3);
    searchTimerRef.current = window.setTimeout(() => {
      if (activeRequestRef.current !== requestId) return;
      if (gameRef.current.fen() !== searchFen) return;
      restartWorker();
    }, hardLimit);
  }, [fen, humanColor, engineReady, engineError, thinking, level.depth, level.movetime]);

  useEffect(() => () => clearSearchWatchdog(), []);

  function showMoveOptions(square) {
    const current = gameRef.current;
    if (thinking || current.isGameOver() || current.turn() !== humanColor) {
      clearSelection();
      return false;
    }

    const piece = current.get(square);
    if (!piece || piece.color !== humanColor) {
      clearSelection();
      return false;
    }

    const legalMoves = current.moves({ square, verbose: true });
    if (legalMoves.length === 0) {
      clearSelection();
      return false;
    }

    const styles = {};
    for (const move of legalMoves) {
      const isCapture = Boolean(move.captured);
      styles[move.to] = isCapture
        ? {
            background: 'radial-gradient(circle, transparent 0 58%, rgba(19, 23, 17, 0.28) 60% 78%, transparent 80%)',
          }
        : {
            background: 'radial-gradient(circle, rgba(19, 23, 17, 0.30) 0 17%, transparent 19%)',
          };
    }

    styles[square] = {
      background: 'rgba(238, 214, 92, 0.58)',
      boxShadow: 'inset 0 0 0 2px rgba(82, 73, 29, 0.20)',
    };

    setSelectedSquare(square);
    setMoveOptionStyles(styles);
    return true;
  }

  function makeHumanMove(from, to) {
    if (!to || thinking || gameRef.current.isGameOver()) return false;
    if (gameRef.current.turn() !== humanColor) return false;

    try {
      const piece = gameRef.current.get(from);
      if (!piece || piece.color !== humanColor) return false;

      const targetRank = to[1];
      const promotion = piece.type === 'p' && (targetRank === '1' || targetRank === '8') ? 'q' : undefined;
      gameRef.current.move({ from, to, promotion });
      clearSelection();
      syncPosition();
      return true;
    } catch {
      return false;
    }
  }

  function onSquareClick({ square }) {
    if (thinking || gameRef.current.isGameOver()) return;
    if (gameRef.current.turn() !== humanColor) return;

    if (!selectedSquare) {
      showMoveOptions(square);
      return;
    }

    const legalMoves = gameRef.current.moves({ square: selectedSquare, verbose: true });
    const chosenMove = legalMoves.find((move) => move.to === square);

    if (chosenMove && makeHumanMove(selectedSquare, square)) return;

    const clickedPiece = gameRef.current.get(square);
    if (clickedPiece?.color === humanColor) {
      showMoveOptions(square);
    } else {
      clearSelection();
    }
  }

  function onPieceDrop({ sourceSquare, targetSquare }) {
    const moved = makeHumanMove(sourceSquare, targetSquare);
    if (!moved) clearSelection();
    return moved;
  }

  function startNewGame(color = humanColor) {
    clearSearchWatchdog();
    clearSelection();
    ++requestSeqRef.current;
    activeRequestRef.current = requestSeqRef.current;
    pendingFenRef.current = '';
    gameRef.current = new Chess();
    setHumanColor(color);
    setOrientation(color === 'w' ? 'white' : 'black');
    setWhiteEval(0);
    setSearchInfo(EMPTY_SEARCH_INFO);
    setEngineError('');
    setThinking(false);
    setFen(gameRef.current.fen());
  }

  function undoTurn() {
    if (thinking || moves.length === 0) return;
    clearSelection();
    gameRef.current.undo();
    if (gameRef.current.history().length > 0 && gameRef.current.turn() !== humanColor) {
      gameRef.current.undo();
    }
    setWhiteEval(0);
    setSearchInfo(EMPTY_SEARCH_INFO);
    syncPosition();
  }

  const squareStyles = useMemo(() => {
    const styles = {};

    if (lastMove) {
      const lastMoveHighlight = { background: 'rgba(238, 214, 92, 0.38)' };
      styles[lastMove.from] = lastMoveHighlight;
      styles[lastMove.to] = lastMoveHighlight;
    }

    if (gameRef.current.inCheck()) {
      const kingSquare = findKingSquare(gameRef.current, gameRef.current.turn());
      if (kingSquare) {
        styles[kingSquare] = {
          background: 'radial-gradient(circle, rgba(194, 67, 58, 0.72) 0%, rgba(170, 55, 49, 0.46) 58%, transparent 76%)',
        };
      }
    }

    return { ...styles, ...moveOptionStyles };
  }, [fen, lastMove, moveOptionStyles]);

  const boardOptions = useMemo(() => ({
    id: 'pickle-board',
    position: fen,
    onPieceDrop,
    onSquareClick,
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
  }), [fen, orientation, squareStyles, thinking, humanColor, selectedSquare]);

  const topColor = orientation === 'white' ? 'b' : 'w';
  const bottomColor = orientation === 'white' ? 'w' : 'b';
  const engineColor = humanColor === 'w' ? 'b' : 'w';
  const tablebaseMode = searchInfo.source === 'tablebase';

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
            <EvalBar score={whiteEval} tablebase={tablebaseMode} />
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
              {searchInfo.source === 'tablebase'
                ? 'Last move selected from the endgame tablebase.'
                : searchInfo.source === 'book'
                  ? 'Last move selected from Pickle’s opening book.'
                  : searchInfo.source === 'mate'
                    ? 'Last move was a forced mate in one.'
                    : searchInfo.depth > 0
                      ? `Last search: depth ${searchInfo.depth} · ${searchInfo.nodes.toLocaleString()} nodes`
                      : 'Pickle runs locally in your browser.'}
            </span>
          </div>

          <DecisionPanel info={searchInfo} />

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
