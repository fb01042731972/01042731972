-- ============================================================================
--  이진주방산업 — 관리자 이메일 로그인 + 서버 권한 설정 (Supabase SQL Editor에서 실행)
--
--  이 앱은 Supabase 프로젝트를 2개 씁니다. 각각 따로 실행하세요.
--    프로젝트 A (채팅)      : qabrelgdencnuicsxsnk  → [공통] + [A] 부분
--    프로젝트 B (게시판/일정): yzgmqbnrsylxrwokrjii → [공통] + [B] 부분
--
--  실행 전에 두 프로젝트 모두에서 (대시보드에서 직접):
--    1) Authentication → Users → Add user → 관리자 이메일 + 비밀번호
--       ("Auto Confirm User" 체크). 두 프로젝트에 같은 이메일/비밀번호로 만드세요.
--    2) Authentication → Sign In / Providers → Email → "Allow new users to sign up" 끄기
--       (아무도 스스로 가입하지 못하게 막는 중요한 설정입니다)
--    3) 비밀번호는 길고 복잡하게(12자 이상).
--    4) 2단계 인증(휴대폰 인증 앱): Authentication → Sign In / Providers 의 'Multi-Factor'(MFA) 에서
--       TOTP(앱 인증)가 켜져 있는지 확인하세요. (보통 기본으로 켜져 있습니다)
--       첫 로그인 때 화면에 뜨는 QR을 휴대폰 인증 앱(Google/Microsoft Authenticator)으로 스캔해 등록합니다.
--       프로젝트가 2개라서 앱에 항목도 2개(채팅 / 게시판·일정)가 생깁니다.
--    ※ 이미 이전 버전 SQL을 실행했다면, [공통] 부분만 다시 실행하면 됩니다(다시 실행해도 안전).
-- ============================================================================


-- ============================== [공통] A, B 모두 ==============================
-- ※ 아래 한 줄의 이메일을 관리자 이메일로 바꾸세요.
create table if not exists public.admin_emails (email text primary key);
alter table public.admin_emails enable row level security;   -- 정책 없음 = API로는 아무도 못 읽음
insert into public.admin_emails (email) values (lower('fb1234@naver.com'))
  on conflict do nothing;

-- 이메일이 관리자 목록에 있는지만 확인 (1차 확인용. 이것만으로는 아무 권한도 주지 않습니다)
create or replace function public.is_admin_email() returns boolean
language sql stable security definer set search_path = public as $$
  select exists (
    select 1 from public.admin_emails
    where email = lower(coalesce(auth.jwt() ->> 'email', ''))
  );
$$;
grant execute on function public.is_admin_email() to anon, authenticated;

-- 실제 관리자 권한: 관리자 이메일 + 인증 앱(2단계) 코드까지 확인된 로그인(aal2)일 때만 true
create or replace function public.is_admin() returns boolean
language sql stable security definer set search_path = public as $$
  select public.is_admin_email() and coalesce(auth.jwt() ->> 'aal', '') = 'aal2';
$$;
grant execute on function public.is_admin() to anon, authenticated;


-- ============================== [A] 채팅 프로젝트 ==============================
-- A-1) 관리자(로그인한 사람)에게 모든 데이터 읽기/수정/삭제 허용 — 기존 동작은 그대로 두는 '추가' 정책
do $$
declare t text;
begin
  foreach t in array array[
    'chat_messages','chat_guests','chat_profiles','chat_read_state','visit_log',
    'banned_ips','ip_labels','app_settings','matgo_employees','matgo_sessions','matgo_points_log'
  ] loop
    if to_regclass('public.' || t) is not null then
      execute format('drop policy if exists admin_all on public.%I', t);
      execute format('create policy admin_all on public.%I for all to authenticated using (public.is_admin()) with check (public.is_admin())', t);
    end if;
  end loop;
end $$;

-- A-2) 삭제된 대화 자동 보관 (관리자만 열람) — 삭제되기 전 내용을 따로 저장
create table if not exists public.chat_messages_deleted (
  id         bigserial primary key,
  deleted_at timestamptz not null default now(),
  msg        jsonb not null
);
alter table public.chat_messages_deleted enable row level security;
drop policy if exists admin_read on public.chat_messages_deleted;
create policy admin_read on public.chat_messages_deleted for select to authenticated using (public.is_admin());

create or replace function public.archive_deleted_chat() returns trigger
language plpgsql security definer set search_path = public as $$
begin
  begin
    insert into public.chat_messages_deleted (msg) values (to_jsonb(old));
  exception when others then
    null;   -- 보관에 실패해도 삭제 자체는 막지 않음
  end;
  return old;
end $$;

drop trigger if exists trg_archive_deleted_chat on public.chat_messages;
create trigger trg_archive_deleted_chat before delete on public.chat_messages
  for each row execute function public.archive_deleted_chat();


-- ============================== [B] 게시판/일정 프로젝트 ==============================
do $$
declare t text;
begin
  foreach t in array array['board_posts','board_comments','shared_schedule','shared_quote_snap'] loop
    if to_regclass('public.' || t) is not null then
      execute format('drop policy if exists admin_all on public.%I', t);
      execute format('create policy admin_all on public.%I for all to authenticated using (public.is_admin()) with check (public.is_admin())', t);
    end if;
  end loop;
end $$;


-- ============================================================================
--  [선택·2단계] 진짜로 '관리자만' 하게 막기 — 위 1단계를 적용하고 로그인 테스트를 마친 뒤에 하세요.
--
--  1단계는 관리자 권한을 '추가'만 합니다. 기존에 누구나(anon) 삭제할 수 있게 열려 있던 정책은
--  그대로이므로, 코드를 아는 사람은 여전히 삭제할 수 있습니다. 막으려면 기존 정책을 확인해 정리합니다.
--
--  (1) 현재 정책 확인:
--        select tablename, policyname, cmd, roles, qual
--        from pg_policies where schemaname = 'public' order by tablename, cmd;
--
--  (2) anon 이 delete 하게 열어둔 정책을 지움. 예) 채팅 삭제를 관리자 전용으로:
--        drop policy "<anon delete 정책 이름>" on public.chat_messages;
--      ※ 이렇게 하면 일반 직원의 '내가 쓴 글 삭제'도 막힙니다
--        (직원은 로그인 계정이 없어 서버가 '누가 쓴 글인지' 알 수 없기 때문입니다).
--        유지하려면 직원도 각자 계정으로 로그인하는 구조로 바꿔야 합니다.
--
--  (3) 일정 삭제 함수(cal_delete_admin / cal_delete_many / cal_delete_pw 등)는 지금 '비밀번호 해시'를
--      HTML에서 받아 확인합니다. 해시가 HTML에 들어 있으므로 누구나 호출할 수 있습니다.
--      함수 안에서 public.is_admin() 을 확인하도록 바꾸는 것이 안전합니다. (함수 소스를 보내 주시면
--      맞춰서 수정해 드립니다.)
-- ============================================================================
