-- ============================================================
-- 사내 포인트 라운지용 테이블 생성 SQL
-- Supabase 대시보드 → SQL Editor → 새 쿼리에 붙여넣고 "Run" 한 번만 누르면 됩니다.
-- 채팅 기능이 이미 쓰고 있는 것과 같은 Supabase 프로젝트에 실행하세요.
-- ============================================================

-- 1) 직원 계정
create table if not exists matgo_employees (
  id bigint generated always as identity primary key,
  username text unique not null,
  display_name text not null,
  password_hash text not null,   -- 평문 비밀번호는 저장하지 않음 (SHA-256 해시만 저장)
  active boolean not null default true,
  created_at timestamptz not null default now()
);

-- 2) 접속(이용시간) 기록
create table if not exists matgo_sessions (
  id bigint generated always as identity primary key,
  employee_id bigint not null references matgo_employees(id),
  login_at timestamptz not null default now(),
  last_seen_at timestamptz not null default now(),
  logout_at timestamptz
);

-- 3) 포인트 지급 내역
create table if not exists matgo_points_log (
  id bigint generated always as identity primary key,
  employee_id bigint not null references matgo_employees(id),
  points integer not null,
  reason text,
  granted_at timestamptz not null default now()
);

-- RLS 활성화 + 이 앱(채팅과 동일한 신뢰 모델: 링크를 아는 사람만 접근)이 자유롭게
-- 읽고 쓸 수 있도록 허용. 실제 접근 제어는 페이지 안의 로그인/관리자 비밀번호가 담당합니다.
alter table matgo_employees enable row level security;
alter table matgo_sessions enable row level security;
alter table matgo_points_log enable row level security;

create policy "matgo_employees_all" on matgo_employees for all using (true) with check (true);
create policy "matgo_sessions_all" on matgo_sessions for all using (true) with check (true);
create policy "matgo_points_log_all" on matgo_points_log for all using (true) with check (true);
