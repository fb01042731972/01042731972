-- ============================================================
-- 공용 일정 설정 (Supabase 게시판 프로젝트: yzgmqbnrsylxrwokrjii)
-- Supabase 대시보드 > SQL Editor 에 통째로 붙여넣고 Run 하세요.
-- ※ pg_cron 이 꺼져 있으면 Database > Extensions 에서 pg_cron 을 먼저 켜세요.
-- ============================================================

create table if not exists public.shared_schedule (
  id          text primary key,
  date        text not null,                       -- YYYY-MM-DD
  end_date    text,
  time        text,
  title       text not null check (char_length(title) <= 100),
  memo        text check (char_length(memo) <= 1000),
  link        text check (link is null or (char_length(link) <= 500 and link ~* '^https?://')),
  type        text,
  author      text check (char_length(author) <= 20),
  owner_hash  text not null,                       -- 작성자 PC 열쇠의 SHA-256 (열쇠 자체는 서버에 없음)
  created_at  timestamptz not null default now()
);
create index if not exists shared_schedule_date_idx on public.shared_schedule (date);

alter table public.shared_schedule enable row level security;

-- 누구나 보기 / 누구나 등록. 수정·삭제 정책은 일부러 만들지 않음(아래 함수로만 가능).
drop policy if exists shared_schedule_select on public.shared_schedule;
drop policy if exists shared_schedule_insert on public.shared_schedule;
create policy shared_schedule_select on public.shared_schedule for select to anon, authenticated using (true);
create policy shared_schedule_insert on public.shared_schedule for insert to anon, authenticated with check (true);

-- 수정: 작성자 본인(열쇠 일치)만
create or replace function public.cal_update(
  p_id text, p_key text, p_date text, p_end text, p_time text,
  p_title text, p_memo text, p_link text, p_type text
) returns boolean
language plpgsql security definer set search_path = public as $$
begin
  update public.shared_schedule
     set date = p_date, end_date = p_end, time = p_time,
         title = left(p_title, 100), memo = left(p_memo, 1000), link = p_link, type = p_type
   where id = p_id
     and owner_hash = encode(sha256(convert_to(p_key, 'utf8')), 'hex');
  return found;
end $$;

-- 삭제: 관리자 비밀번호 확인 후에만 (서버에서 SHA-256 비교)
create or replace function public.cal_delete_many(p_ids text[], p_pin text)
returns boolean
language plpgsql security definer set search_path = public as $$
begin
  if encode(sha256(convert_to(p_pin, 'utf8')), 'hex')
     <> '8f56743ebfc75e95f389831748bd8a90a266edc1510e754e1a8b0cd89efe38cf' then
    return false;
  end if;
  delete from public.shared_schedule where id = any(p_ids);
  return true;
end $$;

grant execute on function public.cal_update(text,text,text,text,text,text,text,text,text) to anon, authenticated;
grant execute on function public.cal_delete_many(text[], text) to anon, authenticated;

-- 실시간 반영
do $$ begin
  alter publication supabase_realtime add table public.shared_schedule;
exception when duplicate_object then null; end $$;

-- 일정 날짜(종료일이 있으면 종료일) 기준 1년 지난 일정 자동 삭제 — 매일 새벽 3시 10분(한국시간)
select cron.schedule(
  'shared_schedule_cleanup', '10 18 * * *',
  $$ delete from public.shared_schedule
      where coalesce(nullif(end_date, ''), date) < to_char(current_date - interval '1 year', 'YYYY-MM-DD') $$
);
