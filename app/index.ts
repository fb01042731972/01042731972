// ════════════════════════════════════════════════════════════
// Supabase Edge Function: send-call-push
// 브라우저는 직접 푸시를 못 보내므로, 이 서버 쪽 함수가 대신 발송합니다.
//
// [배포 방법]
// 1. Supabase 대시보드 → Edge Functions → "Deploy a new function"
//    또는 Supabase CLI가 있다면:
//      supabase functions deploy send-call-push
// 2. 이 폴더(send-call-push) 안의 index.ts 내용을 그대로 사용
// 3. 아래 3개의 Secret을 Supabase 대시보드 → Edge Functions → Secrets 에서 등록:
//      VAPID_PUBLIC_KEY  = (제공된 공개키)
//      VAPID_PRIVATE_KEY = (제공된 비공개키, 외부 노출 금지)
//      VAPID_SUBJECT      = mailto:you@example.com  (아무 이메일이나 가능)
// ════════════════════════════════════════════════════════════

import webpush from "npm:web-push@3.6.7";
import { createClient } from "npm:@supabase/supabase-js@2";

const VAPID_PUBLIC_KEY = Deno.env.get("VAPID_PUBLIC_KEY") || "";
const VAPID_PRIVATE_KEY = Deno.env.get("VAPID_PRIVATE_KEY") || "";
const VAPID_SUBJECT = Deno.env.get("VAPID_SUBJECT") || "mailto:admin@example.com";

const SUPABASE_URL = Deno.env.get("SUPABASE_URL") || "";
const SUPABASE_ANON_KEY = Deno.env.get("SUPABASE_ANON_KEY") || "";

webpush.setVapidDetails(VAPID_SUBJECT, VAPID_PUBLIC_KEY, VAPID_PRIVATE_KEY);

const CORS_HEADERS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
};

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") {
    return new Response("ok", { headers: CORS_HEADERS });
  }

  try {
    const body = await req.json();
    const to: string[] = Array.isArray(body.to) ? body.to : [body.to].filter(Boolean);
    const from: string = body.from || "알 수 없음";
    const callId: string = body.callId || "";
    const group: boolean = !!body.group;

    if (!to.length) {
      return new Response(JSON.stringify({ error: "no recipients" }), {
        status: 400,
        headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
      });
    }

    const supabase = createClient(SUPABASE_URL, SUPABASE_ANON_KEY);

    const { data: subs, error } = await supabase
      .from("push_subscriptions")
      .select("*")
      .in("nickname", to);

    if (error) throw error;

    const title = "📹 화상통화 요청";
    const bodyText = group
      ? `${from}님이 단체 화상통화에 초대했습니다.`
      : `${from}님이 화상통화를 걸었습니다.`;

    const payload = JSON.stringify({
      title,
      body: bodyText,
      tag: "le-video-call-" + callId,
      callId,
      url: "./",
    });

    const results = await Promise.allSettled(
      (subs || []).map(async (row: any) => {
        try {
          await webpush.sendNotification(row.subscription, payload);
        } catch (err: any) {
          // 구독이 만료/무효화된 경우(410, 404) 정리
          if (err && (err.statusCode === 410 || err.statusCode === 404)) {
            await supabase.from("push_subscriptions").delete().eq("id", row.id);
          }
          throw err;
        }
      })
    );

    const sent = results.filter((r) => r.status === "fulfilled").length;
    const failed = results.length - sent;

    return new Response(JSON.stringify({ ok: true, sent, failed, matchedSubscriptions: (subs || []).length }), {
      headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
    });
  } catch (e) {
    return new Response(JSON.stringify({ error: String(e && e.message || e) }), {
      status: 500,
      headers: { ...CORS_HEADERS, "Content-Type": "application/json" },
    });
  }
});
