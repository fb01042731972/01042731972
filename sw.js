/* 이진주방산업 오프라인/자동업데이트 서비스 워커 */
const CACHE_PREFIX = 'jinjoo-app-';
const CACHE_VERSION = 'v1';
const CACHE_NAME = CACHE_PREFIX + CACHE_VERSION;
const APP_SHELL = ['./', './index.html', './manifest.json'];

self.addEventListener('install', event => {
  event.waitUntil((async () => {
    const cache = await caches.open(CACHE_NAME);
    // 설치 시 최신 index를 받아 두어 이후 오프라인 실행이 가능하게 합니다.
    for (const url of APP_SHELL) {
      try {
        const req = new Request(url, { cache: 'no-store' });
        const res = await fetch(req);
        if (res.ok) await cache.put(url, res.clone());
      } catch (e) {
        // manifest 등이 없는 상태에서도 서비스 워커 설치가 실패하지 않도록 합니다.
      }
    }
    // 새 서비스 워커는 다음 실행에 자연스럽게 적용되도록 waiting 상태를 유지합니다.
  })());
});

self.addEventListener('activate', event => {
  event.waitUntil((async () => {
    const keys = await caches.keys();
    await Promise.all(keys.filter(k => k.startsWith(CACHE_PREFIX) && k !== CACHE_NAME).map(k => caches.delete(k)));
    await self.clients.claim();
  })());
});

function isApiRequest(url) {
  return /(^|\\.)api\\.github\\.com$/i.test(url.hostname);
}

self.addEventListener('fetch', event => {
  const req = event.request;
  if (req.method !== 'GET') return;
  const url = new URL(req.url);
  if (isApiRequest(url)) return; // GitHub API는 절대 캐시하지 않음

  // 페이지 이동: 온라인이면 항상 최신 페이지를 우선 받고, 실패하면 마지막 정상 버전을 사용.
  if (req.mode === 'navigate') {
    event.respondWith((async () => {
      const cache = await caches.open(CACHE_NAME);
      try {
        const res = await fetch(new Request(req, { cache: 'no-store' }));
        if (res && res.ok) {
          await cache.put('./index.html', res.clone());
          await cache.put('./', res.clone());
        }
        return res;
      } catch (e) {
        return (await cache.match('./index.html')) || (await cache.match('./')) || new Response('오프라인 상태입니다.', {status: 503});
      }
    })());
    return;
  }

  // 정적/외부 라이브러리: 캐시 우선, 없으면 네트워크에서 받아 캐시.
  event.respondWith((async () => {
    const cache = await caches.open(CACHE_NAME);
    const cached = await cache.match(req);
    if (cached) return cached;
    try {
      const res = await fetch(req);
      if (res && (res.ok || res.type === 'opaque')) {
        try { await cache.put(req, res.clone()); } catch (e) {}
      }
      return res;
    } catch (e) {
      return new Response('', {status: 503, statusText: 'Offline'});
    }
  })());
});
