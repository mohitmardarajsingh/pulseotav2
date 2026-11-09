self.addEventListener('install', () => {
  self.skipWaiting();
});

self.addEventListener('activate', () => {
  self.registration.unregister()
    .then(() => self.clients.matchAll())
    .then((clients) => {
      clients.forEach((client) => {
        if (client.navigate) {
          client.navigate(client.url);
        }
      });
    });
});
