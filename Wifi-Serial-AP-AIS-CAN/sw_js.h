// Service Worker for offline tile and Leaflet caching.
// Served at /sw.js by handle_sw_js() in the .ino.
// On first connected load the SW pre-caches Leaflet and intercepts all
// subsequent tile requests, storing them so the chart works fully offline.

const char swJS[] PROGMEM = R"=====(
const TC='tiles-v1', SC='static-v1';
self.addEventListener('install', e=>{
  e.waitUntil(
    caches.open(SC)
      .then(c=>c.addAll([
        'https://unpkg.com/leaflet@1.9.4/dist/leaflet.js',
        'https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'
      ]))
      .then(()=>self.skipWaiting())
  );
});
self.addEventListener('activate', e=>e.waitUntil(self.clients.claim()));
self.addEventListener('fetch', e=>{
  var u=e.request.url;
  if(!/openstreetmap\.org|openseamap\.org|unpkg\.com\/leaflet/.test(u)) return;
  var cn=/tile\.openstreetmap|openseamap/.test(u) ? TC : SC;
  e.respondWith(
    caches.match(e.request).then(hit=>{
      if(hit) return hit;
      return fetch(e.request).then(r=>{
        if(r.ok) caches.open(cn).then(c=>c.put(e.request,r.clone()));
        return r;
      }).catch(()=>new Response('',{status:503}));
    })
  );
});
)=====";
