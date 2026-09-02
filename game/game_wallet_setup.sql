-- ============================================================
-- 맞고게임 이용권(보유금액/이용시간) 시스템 - Supabase 테이블
-- ============================================================
-- 이미 쓰고 계신 Supabase 프로젝트의 SQL Editor에서 실행하세요.

-- 1) 직원별 지갑 (현재 보유금액 + 남은 이용시간)
create table if not exists game_wallets (
  user_id text primary key,           -- 로그인 시스템의 사용자 ID (LeChat과 동일한 값 사용 권장)
  display_name text,                  -- 화면에 보여줄 이름/닉네임
  money bigint not null default 0,    -- 보유 게임머니
  minutes_left integer not null default 0,  -- 남은 이용 가능 시간(분)
  updated_at timestamptz not null default now()
);

-- 2) 지급/차감 이력 (자동지급, 관리자 수동배분, 게임머니 정산, 시간 차감 등 전부 기록)
create table if not exists game_wallet_logs (
  id bigint generated always as identity primary key,
  user_id text not null references game_wallets(user_id) on delete cascade,
  type text not null,        -- 'auto_grant' | 'manual_grant' | 'game_settle' | 'time_deduct' | 'free_recharge'
  money_delta bigint not null default 0,     -- 이번 이력에서 변동된 금액 (+/-)
  minutes_delta integer not null default 0,  -- 이번 이력에서 변동된 시간(분) (+/-)
  note text,                  -- 예: "견적서 작성 자동지급", "관리자 홍길동이 수동 배분", "게임 종료 정산"
  created_by text,            -- 누가 지급했는지 (관리자 수동배분일 때 관리자 ID)
  created_at timestamptz not null default now()
);

create index if not exists idx_game_wallet_logs_user on game_wallet_logs(user_id, created_at desc);

-- 3) 관리자가 조정 가능한 설정값 (자동지급 금액, 무료충전 기준/금액 등)
create table if not exists game_settings (
  key text primary key,
  value bigint not null,
  updated_at timestamptz not null default now()
);

insert into game_settings (key, value) values
  ('auto_grant_money_per_quote', 500),   -- 견적서/작업지시서 1건당 자동 지급 금액
  ('auto_grant_minutes_per_quote', 5),   -- 견적서/작업지시서 1건당 자동 지급 시간(분)
  ('free_recharge_threshold', 0),        -- 보유금액이 이 값 이하이면 무료충전 대상
  ('free_recharge_amount', 1000),        -- 무료충전 시 채워주는 금액
  ('free_recharge_cooldown_hours', 24)   -- 무료충전 재사용 가능 간격(시간)
on conflict (key) do nothing;

-- ============================================================
-- Row Level Security: 필요에 따라 조정하세요.
-- 사내 전용 도구라 우선 전체 허용으로 열어둡니다. (LeChat과 동일한 보안 수준 가정)
-- ============================================================
alter table game_wallets enable row level security;
alter table game_wallet_logs enable row level security;
alter table game_settings enable row level security;

create policy "allow all - game_wallets" on game_wallets for all using (true) with check (true);
create policy "allow all - game_wallet_logs" on game_wallet_logs for all using (true) with check (true);
create policy "allow all - game_settings" on game_settings for all using (true) with check (true);
