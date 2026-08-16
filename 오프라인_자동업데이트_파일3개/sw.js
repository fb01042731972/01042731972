/* 이진주방산업 - 오프라인 실행 + 온라인 자동 최신화
   핵심 원칙: 온라인에서는 최신 index.html을 받아 캐시하고,
   오프라인에서는 마지막 정상 캐시를 사용합니다.
*/
const CACHE_NAME = 'jinjoo-app-v1';

self.addEventListener('install', event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(self.clients.claim());
});

// 프로그램 본체(index.html)는 온라인 우선, 실패하면 캐시 사용
self.addEventListener('fetch', event => {
  const req = event.request;
  if (req.method !== 'GET') return;

  const url = new URL(req.url);
  const isNavigation = req.mode === 'navigate' ||
    (req.headers.get('accept') || '').includes('text/html');

  if (isNavigation && url.origin === self.location.origin) {
    event.respondWith((async () => {
      try {
        const fresh = await fetch(req, {cache: 'no-store'});
        if (fresh && fresh.ok) {
          const cache = await caches.open(CACHE_NAME);
          await cache.put('./index.html', fresh.clone());
        }
        return fresh;
      } catch (e) {
        const cached = await caches.match('./index.html');
        return cached || new Response(
          '<!doctype html><meta charset="utf-8"><h3>오프라인 상태입니다.</h3><p>인터넷 연결 후 프로그램을 한 번 열어 주세요.</p>',
          {headers:{'Content-Type':'text/html; charset=utf-8'}}
        );
      }
    })());
    return;
  }

  // same-origin 정적 리소스가 있다면 온라인에서 받아 캐시하고 오프라인에서 사용
  if (url.origin === self.location.origin) {
    event.respondWith((async () => {
      const cached = await caches.match(req);
      try {
        const fresh = await fetch(req);
        if (fresh && (fresh.ok || fresh.type === 'opaque')) {
          const cache = await caches.open(CACHE_NAME);
          await cache.put(req, fresh.clone());
        }
        return fresh;
      } catch (e) {
        return cached || Response.error();
      }
    })());
  }
});

// 온라인 복귀 시 현재 index.html의 최신본을 백그라운드에서 캐시
self.addEventListener('message', event => {
  if (!event.data || event.data.type !== 'CHECK_LATEST') return;
  event.waitUntil((async () => {
    try {
      const response = await fetch('./index.html', {cache: 'no-store'});
      if (!response.ok) return;
      const cache = await caches.open(CACHE_NAME);
      await cache.put('./index.html', response.clone());
      const clients = await self.clients.matchAll({type:'window', includeUncontrolled:true});
      for (const client of clients) {
        client.postMessage({type:'LATEST_CACHED'});
      }
    } catch (e) {
      // 인터넷이 없으면 기존 캐시를 그대로 유지합니다.
    }
  })());
});
