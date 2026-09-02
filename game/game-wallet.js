/**
 * game-wallet.js
 * 맞고게임 보유금액/이용시간 지갑 시스템 - 공용 모듈
 *
 * 이진견적서 메인 앱과 게임 wrapper(index.html) 양쪽에서 공통으로 사용합니다.
 * Supabase 프로젝트의 URL/anon key만 채워 넣으면 바로 동작합니다.
 *
 * 사용 전 준비:
 *  1) game_wallet_setup.sql 을 Supabase SQL Editor에서 먼저 실행
 *  2) 아래 SUPABASE_URL / SUPABASE_ANON_KEY 를 실제 값으로 교체
 *     (이미 LeChat에서 쓰고 계신 것과 동일한 프로젝트 값이면 그대로 재사용 가능)
 */

const GameWallet = (() => {
  // ===== 여기 두 값을 실제 Supabase 프로젝트 값으로 교체하세요 =====
  const SUPABASE_URL = 'https://YOUR_PROJECT.supabase.co';
  const SUPABASE_ANON_KEY = 'YOUR_ANON_KEY';
  // ================================================================

  const REST_URL = `${SUPABASE_URL}/rest/v1`;
  const headers = {
    'apikey': SUPABASE_ANON_KEY,
    'Authorization': `Bearer ${SUPABASE_ANON_KEY}`,
    'Content-Type': 'application/json',
  };

  // 지갑이 없으면 새로 만들고, 있으면 그대로 반환
  async function ensureWallet(userId, displayName) {
    const res = await fetch(`${REST_URL}/game_wallets?user_id=eq.${encodeURIComponent(userId)}&select=*`, { headers });
    const rows = await res.json();
    if (rows.length > 0) return rows[0];

    const insertRes = await fetch(`${REST_URL}/game_wallets`, {
      method: 'POST',
      headers: { ...headers, 'Prefer': 'return=representation' },
      body: JSON.stringify([{ user_id: userId, display_name: displayName || userId, money: 0, minutes_left: 0 }]),
    });
    const created = await insertRes.json();
    return created[0];
  }

  // 현재 잔액 조회
  async function getBalance(userId) {
    const res = await fetch(`${REST_URL}/game_wallets?user_id=eq.${encodeURIComponent(userId)}&select=*`, { headers });
    const rows = await res.json();
    return rows[0] || null;
  }

  // 설정값 조회 (자동지급 금액 등)
  async function getSetting(key) {
    const res = await fetch(`${REST_URL}/game_settings?key=eq.${encodeURIComponent(key)}&select=value`, { headers });
    const rows = await res.json();
    return rows.length ? Number(rows[0].value) : 0;
  }

  // wallet의 money/minutes를 증감시키고 로그를 남기는 공통 내부 함수
  async function applyDelta(userId, moneyDelta, minutesDelta, type, note, createdBy) {
    const wallet = await ensureWallet(userId);
    const newMoney = Math.max(0, Number(wallet.money) + moneyDelta);
    const newMinutes = Math.max(0, Number(wallet.minutes_left) + minutesDelta);

    await fetch(`${REST_URL}/game_wallets?user_id=eq.${encodeURIComponent(userId)}`, {
      method: 'PATCH',
      headers,
      body: JSON.stringify({ money: newMoney, minutes_left: newMinutes, updated_at: new Date().toISOString() }),
    });

    await fetch(`${REST_URL}/game_wallet_logs`, {
      method: 'POST',
      headers,
      body: JSON.stringify([{
        user_id: userId,
        type,
        money_delta: moneyDelta,
        minutes_delta: minutesDelta,
        note: note || null,
        created_by: createdBy || null,
      }]),
    });

    return { money: newMoney, minutes_left: newMinutes };
  }

  // ① 자동 지급: 견적서/작업지시서 1건 저장 성공 시 호출
  //    예: GameWallet.autoGrant('user123', '견적서 작성')
  async function autoGrant(userId, note) {
    const money = await getSetting('auto_grant_money_per_quote');
    const minutes = await getSetting('auto_grant_minutes_per_quote');
    return applyDelta(userId, money, minutes, 'auto_grant', note || '업무 자동지급', 'system');
  }

  // ② 관리자 수동 배분: 관리자 화면에서 직원 선택 후 금액/시간 입력해서 호출
  //    예: GameWallet.manualGrant('user123', 3000, 30, '설 명절 보너스', '관리자ID')
  async function manualGrant(userId, money, minutes, note, adminId) {
    return applyDelta(userId, Number(money) || 0, Number(minutes) || 0, 'manual_grant', note || '관리자 수동 배분', adminId);
  }

  // ③ 게임 종료 시 최종 보유금액 정산 (ExternalInterface exitGame에서 호출)
  //    swf가 넘겨준 최종 금액(finalMoney)을 절대값으로 그대로 저장
  async function settleGameMoney(userId, finalMoney) {
    const wallet = await ensureWallet(userId);
    const delta = Number(finalMoney) - Number(wallet.money);
    return applyDelta(userId, delta, 0, 'game_settle', '게임 종료 정산', 'game');
  }

  // ④ 이용 시간 차감 (게임 창이 열려있는 동안 주기적으로 호출, 예: 1분마다 1씩)
  async function deductTime(userId, minutes) {
    return applyDelta(userId, 0, -Math.abs(Number(minutes) || 0), 'time_deduct', '게임 이용시간 차감', 'game');
  }

  // ⑤ 무료 충전: 보유금액이 기준 이하이고, 마지막 무료충전 후 쿨다운이 지났으면 충전
  async function tryFreeRecharge(userId) {
    const wallet = await ensureWallet(userId);
    const threshold = await getSetting('free_recharge_threshold');
    if (Number(wallet.money) > threshold) {
      return { recharged: false, reason: '아직 보유금액이 남아있습니다.' };
    }

    // 최근 무료충전 로그 확인 (쿨다운 체크)
    const cooldownHours = await getSetting('free_recharge_cooldown_hours');
    const logRes = await fetch(
      `${REST_URL}/game_wallet_logs?user_id=eq.${encodeURIComponent(userId)}&type=eq.free_recharge&order=created_at.desc&limit=1`,
      { headers }
    );
    const logs = await logRes.json();
    if (logs.length > 0) {
      const lastTime = new Date(logs[0].created_at).getTime();
      const hoursSince = (Date.now() - lastTime) / (1000 * 60 * 60);
      if (hoursSince < cooldownHours) {
        const remain = Math.ceil(cooldownHours - hoursSince);
        return { recharged: false, reason: `무료충전은 ${remain}시간 후에 다시 가능합니다.` };
      }
    }

    const amount = await getSetting('free_recharge_amount');
    const result = await applyDelta(userId, amount, 0, 'free_recharge', '무료 충전', 'system');
    return { recharged: true, ...result };
  }

  return {
    ensureWallet,
    getBalance,
    getSetting,
    autoGrant,
    manualGrant,
    settleGameMoney,
    deductTime,
    tryFreeRecharge,
  };
})();

if (typeof window !== 'undefined') {
  window.GameWallet = GameWallet;
}
