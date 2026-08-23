-- ════════════════════════════════════════════════════════════
-- 화상통화 푸시 알림용 테이블
-- Supabase 대시보드 → SQL Editor 에서 이 전체를 붙여넣고 실행하세요.
-- ════════════════════════════════════════════════════════════

create table if not exists push_subscriptions (
  id bigint generated always as identity primary key,
  nickname text not null,
  endpoint text not null unique,
  subscription jsonb not null,
  created_at timestamptz not null default now()
);

create index if not exists idx_push_subscriptions_nickname
  on push_subscriptions (nickname);

-- RLS 활성화 (기존 채팅 테이블들과 동일한 정책 수준으로 맞춤:
-- 익명 키로 누구나 자기 구독을 등록/조회 가능하게 함 - 내부용 프로그램이므로 단순하게 열어둠)
alter table push_subscriptions enable row level security;

drop policy if exists "push_subscriptions_insert" on push_subscriptions;
create policy "push_subscriptions_insert"
  on push_subscriptions for insert
  to anon
  with check (true);

drop policy if exists "push_subscriptions_select" on push_subscriptions;
create policy "push_subscriptions_select"
  on push_subscriptions for select
  to anon
  using (true);

drop policy if exists "push_subscriptions_delete" on push_subscriptions;
create policy "push_subscriptions_delete"
  on push_subscriptions for delete
  to anon
  using (true);

drop policy if exists "push_subscriptions_update" on push_subscriptions;
create policy "push_subscriptions_update"
  on push_subscriptions for update
  to anon
  using (true);
