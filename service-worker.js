// ════════════════════════════════════════════════════════════
// 이진주방산업 - 화상통화 푸시 알림용 서비스 워커
// index.html과 반드시 같은 폴더(같은 경로)에 업로드해야 합니다.
// ════════════════════════════════════════════════════════════

self.addEventListener('install', function (event) {
  self.skipWaiting();
});

self.addEventListener('activate', function (event) {
  event.waitUntil(self.clients.claim());
});

// 서버(Edge Function)로부터 푸시 메시지를 받으면 실행됨
// 브라우저/PC가 꺼져있지 않고 인터넷에 연결만 되어 있으면
// 프로그램(탭)을 전혀 켜두지 않아도 OS가 이 이벤트를 깨워서 실행합니다.
self.addEventListener('push', function (event) {
  let data = {};
  try {
    data = event.data ? event.data.json() : {};
  } catch (e) {
    data = { title: '알림', body: event.data ? event.data.text() : '' };
  }

  const title = data.title || '📹 화상통화 요청';
  const options = {
    body: data.body || '화상통화가 걸려왔습니다.',
    icon: data.icon || 'https://cdn-icons-png.flaticon.com/512/2278/2278992.png',
    badge: data.badge || 'https://cdn-icons-png.flaticon.com/512/2278/2278992.png',
    tag: data.tag || 'le-video-call', // 같은 tag면 알림이 중복되지 않고 갱신됨
    renotify: true,
    requireInteraction: true, // 사용자가 직접 닫을 때까지 화면에 유지 (전화 놓치지 않도록)
    vibrate: [300, 100, 300, 100, 300],
    data: {
      url: data.url || './',
      callId: data.callId || null
    }
  };

  event.waitUntil(self.registration.showNotification(title, options));
});

// 알림을 클릭하면 프로그램 탭을 열거나, 이미 열려있으면 그 탭으로 포커스 이동
self.addEventListener('notificationclick', function (event) {
  event.notification.close();
  const targetUrl = (event.notification.data && event.notification.data.url) || './';

  event.waitUntil(
    self.clients.matchAll({ type: 'window', includeUncontrolled: true }).then(function (clientList) {
      for (let i = 0; i < clientList.length; i++) {
        const client = clientList[i];
        if ('focus' in client) {
          client.focus();
          if ('navigate' in client) client.navigate(targetUrl);
          return;
        }
      }
      if (self.clients.openWindow) {
        return self.clients.openWindow(targetUrl);
      }
    })
  );
});
