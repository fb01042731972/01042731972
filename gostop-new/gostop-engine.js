/**
 * gostop-engine.js
 * 맞고(고스톱) 게임 룰 엔진 - 순수 로직만 담당 (화면/네트워크 없음)
 *
 * 이 파일 하나로 카드 데이터, 패 나누기, 카드 내기/먹기 판정,
 * 흔들기/폭탄/따닥/쪽/뻑, 최종 점수 계산(광·열끗·고도리·띠·단·피·박)까지 처리합니다.
 * 화면(UI)이나 실시간 통신(Supabase Realtime)은 이 엔진 위에 별도로 얹습니다.
 */

// ===================== 1. 카드 데이터 (48장) =====================
// 카드 이미지 경로 (cards/ 폴더 기준 파일명). 실제 <img>에 쓸 때는 IMAGE_BASE_PATH를 앞에 붙여 사용하세요.
const IMAGE_BASE_PATH = 'cards/';

function buildDeck() {
  const cards = [];
  let uid = 0;
  // piIndex: 월별로 몇 번째 피 카드인지 자동 카운트 (1_pi_1.svg, 1_pi_2.svg 매칭용)
  const piCounter = {};
  const push = (month, type, extra = {}) => {
    let image;
    if (type === 'pi') {
      piCounter[month] = (piCounter[month] || 0) + 1;
      image = `${month}_pi_${piCounter[month]}.svg`;
    } else {
      image = `${month}_${type}.svg`;
    }
    cards.push({ id: `c${uid++}`, month, type, image, ...extra });
  };

  push(1, 'gwang'); push(1, 'tti', { ttiColor: 'hong' }); push(1, 'pi'); push(1, 'pi');
  push(2, 'yeol', { isGodori: true }); push(2, 'tti', { ttiColor: 'hong' }); push(2, 'pi'); push(2, 'pi');
  push(3, 'gwang'); push(3, 'tti', { ttiColor: 'hong' }); push(3, 'pi'); push(3, 'pi');
  push(4, 'yeol', { isGodori: true }); push(4, 'tti', { ttiColor: 'cho' }); push(4, 'pi'); push(4, 'pi');
  push(5, 'yeol'); push(5, 'tti', { ttiColor: 'cho' }); push(5, 'pi'); push(5, 'pi');
  push(6, 'yeol'); push(6, 'tti', { ttiColor: 'cheong' }); push(6, 'pi'); push(6, 'pi');
  push(7, 'yeol'); push(7, 'tti', { ttiColor: 'cho' }); push(7, 'pi'); push(7, 'pi');
  push(8, 'gwang'); push(8, 'yeol', { isGodori: true }); push(8, 'pi'); push(8, 'pi');
  push(9, 'yeol'); push(9, 'tti', { ttiColor: 'cheong' }); push(9, 'pi'); push(9, 'pi', { isSsangpi: true });
  push(10, 'yeol'); push(10, 'tti', { ttiColor: 'cheong' }); push(10, 'pi'); push(10, 'pi');
  push(11, 'gwang'); push(11, 'pi'); push(11, 'pi'); push(11, 'pi', { isSsangpi: true });
  push(12, 'gwang', { isBigwang: true }); push(12, 'yeol', { isGodori: true }); push(12, 'tti', { ttiColor: 'hong' }); push(12, 'pi', { isSsangpi: true });

  return cards;
}

function shuffle(arr) {
  const a = [...arr];
  for (let i = a.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [a[i], a[j]] = [a[j], a[i]];
  }
  return a;
}

// ===================== 2. 게임 상태 생성 =====================
function createGame(playerIds) {
  const playerCount = playerIds.length;
  if (playerCount !== 2 && playerCount !== 3) {
    throw new Error('맞고는 2인 또는 3인만 지원합니다.');
  }
  const deck = shuffle(buildDeck());
  const handSize = playerCount === 2 ? 10 : 7;
  const fieldSize = playerCount === 2 ? 8 : 6;

  const hands = {};
  playerIds.forEach((pid) => { hands[pid] = []; });

  let idx = 0;
  for (let r = 0; r < handSize; r++) {
    playerIds.forEach((pid) => { hands[pid].push(deck[idx++]); });
  }
  const field = deck.slice(idx, idx + fieldSize);
  idx += fieldSize;
  const drawPile = deck.slice(idx);

  const captured = {};
  playerIds.forEach((pid) => { captured[pid] = []; });

  return {
    playerIds, hands, field, drawPile, captured,
    turnIndex: 0, goCount: {}, shakes: {}, bombs: {}, log: [],
    finished: false, winnerId: null,
  };
}

function currentPlayer(game) { return game.playerIds[game.turnIndex]; }
function nextTurn(game) { game.turnIndex = (game.turnIndex + 1) % game.playerIds.length; }

// ===================== 3. 카드 내기 / 먹기 판정 =====================
function findFieldMatches(field, month) { return field.filter((c) => c.month === month); }
function removeFromArray(arr, cardId) {
  const i = arr.findIndex((c) => c.id === cardId);
  if (i === -1) return null;
  return arr.splice(i, 1)[0];
}

function playCard(game, playerId, cardId, chosenFieldCardId = null) {
  if (game.finished) throw new Error('이미 종료된 게임입니다.');
  if (currentPlayer(game) !== playerId) throw new Error('당신의 차례가 아닙니다.');

  const hand = game.hands[playerId];
  const played = removeFromArray(hand, cardId);
  if (!played) throw new Error('손패에 없는 카드입니다.');

  const result = { played, capturedThisTurn: [], ppeok: false, drawnCard: null, drawCaptured: [], ttak: false, jjok: false };

  let fieldMatches = findFieldMatches(game.field, played.month);

  if (fieldMatches.length === 0) {
    game.field.push(played);
  } else if (fieldMatches.length === 1) {
    const got = removeFromArray(game.field, fieldMatches[0].id);
    result.capturedThisTurn.push(played, got);
  } else if (fieldMatches.length === 2) {
    const targetId = chosenFieldCardId || fieldMatches[0].id;
    const got = removeFromArray(game.field, targetId);
    result.capturedThisTurn.push(played, got);
  } else if (fieldMatches.length === 3) {
    // 뻑: 바닥에 같은 월 3장 있는 상태에서 4번째를 내면 아무것도 못 먹고 4장이 바닥에 쌓임
    game.field.push(played);
    result.ppeok = true;
  }

  if (game.drawPile.length > 0) {
    const drawn = game.drawPile.shift();
    result.drawnCard = drawn;
    let drawMatches = findFieldMatches(game.field, drawn.month);

    if (drawMatches.length === 0) {
      game.field.push(drawn);
    } else if (drawMatches.length === 1) {
      const got = removeFromArray(game.field, drawMatches[0].id);
      result.drawCaptured.push(drawn, got);
      if (result.capturedThisTurn.length === 0 && !result.ppeok) result.jjok = true;
      if (result.capturedThisTurn.length > 0 && drawn.month === played.month) result.ttak = true;
    } else if (drawMatches.length >= 2) {
      const targetId = drawMatches[0].id;
      const got = removeFromArray(game.field, targetId);
      result.drawCaptured.push(drawn, got);
    }
  }

  const allCapturedThisTurn = [...result.capturedThisTurn, ...result.drawCaptured];
  game.captured[playerId].push(...allCapturedThisTurn);

  game.log.push({ type: 'play', playerId, cardId, result: summarizeResult(result) });

  nextTurn(game);
  return result;
}

function summarizeResult(result) {
  return {
    played: result.played.id,
    captured: result.capturedThisTurn.map((c) => c.id),
    drawn: result.drawnCard ? result.drawnCard.id : null,
    drawCaptured: result.drawCaptured.map((c) => c.id),
    ppeok: result.ppeok, ttak: result.ttak, jjok: result.jjok,
  };
}

// ===================== 4. 흔들기 / 폭탄 선언 =====================
function declareShake(game, playerId, month) {
  const hand = game.hands[playerId];
  const sameMonth = hand.filter((c) => c.month === month);
  if (sameMonth.length < 3) throw new Error('흔들기는 같은 월 카드 3장을 가지고 있어야 선언할 수 있습니다.');
  if (!game.shakes[playerId]) game.shakes[playerId] = [];
  game.shakes[playerId].push({ month, at: game.log.length });
  game.log.push({ type: 'shake', playerId, month });
}

function declareBomb(game, playerId, month) {
  const hand = game.hands[playerId];
  const sameMonthInHand = hand.filter((c) => c.month === month);
  const sameMonthInField = findFieldMatches(game.field, month);
  if (sameMonthInHand.length < 3 || sameMonthInField.length < 1) {
    throw new Error('폭탄은 손에 같은 월 3장 + 바닥에 같은 월 1장이 있어야 선언할 수 있습니다.');
  }
  const takenFromHand = sameMonthInHand.slice(0, 3).map((c) => removeFromArray(hand, c.id));
  const takenFromField = removeFromArray(game.field, sameMonthInField[0].id);
  game.captured[playerId].push(...takenFromHand, takenFromField);

  if (!game.bombs[playerId]) game.bombs[playerId] = [];
  game.bombs[playerId].push({ month, at: game.log.length });
  game.log.push({ type: 'bomb', playerId, month });
}

function transferPi(game, fromPlayerId, toPlayerId) {
  const fromCaptured = game.captured[fromPlayerId];
  const piIndex = fromCaptured.findIndex((c) => c.type === 'pi' && !c.isSsangpi);
  const idx = piIndex !== -1 ? piIndex : fromCaptured.findIndex((c) => c.type === 'pi');
  if (idx === -1) return false;
  const [card] = fromCaptured.splice(idx, 1);
  game.captured[toPlayerId].push(card);
  return true;
}

// ===================== 5. 점수 계산 =====================
function calculateScore(capturedCards) {
  const gwangCards = capturedCards.filter((c) => c.type === 'gwang');
  const yeolCards = capturedCards.filter((c) => c.type === 'yeol');
  const ttiCards = capturedCards.filter((c) => c.type === 'tti');
  const piCards = capturedCards.filter((c) => c.type === 'pi');

  let score = 0;
  const breakdown = {};

  const gwangCount = gwangCards.length;
  let gwangScore = 0;
  if (gwangCount === 5) gwangScore = 15;
  else if (gwangCount === 4) gwangScore = 4;
  else if (gwangCount === 3) {
    const hasBigwang = gwangCards.some((c) => c.isBigwang);
    gwangScore = hasBigwang ? 2 : 3;
  }
  if (gwangScore > 0) { score += gwangScore; breakdown.gwang = gwangScore; }

  const godoriCount = yeolCards.filter((c) => c.isGodori).length;
  if (godoriCount >= 4) { score += 5; breakdown.godori = 5; }

  const yeolCount = yeolCards.length;
  if (yeolCount >= 5) { const s = yeolCount - 4; score += s; breakdown.yeol = s; }

  const ttiCount = ttiCards.length;
  if (ttiCount >= 5) { const s = ttiCount - 4; score += s; breakdown.tti = s; }

  ['hong', 'cheong', 'cho'].forEach((color) => {
    const count = ttiCards.filter((c) => c.ttiColor === color).length;
    if (count >= 3) { score += 3; breakdown[`dan_${color}`] = 3; }
  });

  const piCount = piCards.reduce((sum, c) => sum + (c.isSsangpi ? 2 : 1), 0);
  if (piCount >= 10) { const s = piCount - 9; score += s; breakdown.pi = s; }

  return { total: score, breakdown, counts: { gwang: gwangCount, yeol: yeolCount, tti: ttiCount, pi: piCount, godori: godoriCount } };
}

function shakeBombMultiplier(game, playerId) {
  const shakeCount = (game.shakes[playerId] || []).length;
  const bombCount = (game.bombs[playerId] || []).length;
  return Math.pow(2, shakeCount + bombCount);
}

function settleRound(game, winnerId) {
  const winnerScoreInfo = calculateScore(game.captured[winnerId]);
  let finalScore = winnerScoreInfo.total * shakeBombMultiplier(game, winnerId);

  const goCount = game.goCount[winnerId] || 0;
  finalScore += goCount;

  const penalties = {};
  game.playerIds.filter((pid) => pid !== winnerId).forEach((loserId) => {
    const loserScoreInfo = calculateScore(game.captured[loserId]);
    let multiplier = 1;
    if (winnerScoreInfo.breakdown.gwang && loserScoreInfo.counts.gwang === 0) multiplier *= 2;
    if (loserScoreInfo.counts.pi < 5) multiplier *= 2;
    penalties[loserId] = multiplier;
  });

  const maxMultiplier = Object.values(penalties).length ? Math.max(...Object.values(penalties)) : 1;
  const totalScore = finalScore * maxMultiplier;

  game.finished = true;
  game.winnerId = winnerId;

  return {
    winnerId, baseScore: winnerScoreInfo.total, breakdown: winnerScoreInfo.breakdown,
    shakeBombMultiplier: shakeBombMultiplier(game, winnerId), goCount,
    penaltyMultiplier: maxMultiplier, totalScore,
  };
}

function declareGo(game, playerId) {
  if (!game.goCount[playerId]) game.goCount[playerId] = 0;
  game.goCount[playerId] += 1;
  game.log.push({ type: 'go', playerId, count: game.goCount[playerId] });
}

function declareStop(game, playerId) { return settleRound(game, playerId); }

function canDeclare(game, playerId, minScore = 7) {
  const info = calculateScore(game.captured[playerId]);
  return info.total >= minScore;
}

const GoStopEngine = {
  buildDeck, shuffle, createGame, currentPlayer, playCard,
  declareShake, declareBomb, transferPi, calculateScore,
  settleRound, declareGo, declareStop, canDeclare, IMAGE_BASE_PATH,
};

if (typeof module !== 'undefined' && module.exports) {
  module.exports = GoStopEngine;
}
if (typeof window !== 'undefined') {
  window.GoStopEngine = GoStopEngine;
}
