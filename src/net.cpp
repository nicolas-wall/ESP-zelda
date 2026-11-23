#if !defined(PINFINDER) && !defined(BUZZTEST)
#include "net.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "reminders.h"

extern Reminders reminders;
void pushNotification(const String& text);
void playHyruleTheme();

static WiFiManager wm;
static WebServer server(80);
static bool serverUp = false;

bool netConnected() { return WiFi.status() == WL_CONNECTED; }
bool netPortalActive() { return wm.getConfigPortalActive(); }
String netIP() { return WiFi.localIP().toString(); }
String netSSID() { return WiFi.SSID(); }
bool timeValid() { return time(nullptr) > 1700000000; }

void netResetWifi() {
  wm.resetSettings();
  delay(200);
  ESP.restart();
}

static String esc(const String& s) {
  String o;
  o.reserve(s.length() + 8);
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

static const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html lang="es"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Reloj de Hyrule</title><style>
body{margin:0;font-family:system-ui,sans-serif;background:#0d1f14;color:#e8e2c4}
main{max-width:480px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;color:#e6c34a;display:flex;align-items:center;gap:10px;margin:8px 0 4px}
h2{font-size:1rem;margin:0 0 6px;color:#e6c34a}
.card{background:#16301f;border:1px solid #2c5a3a;border-radius:12px;padding:14px;margin:14px 0}
label{display:block;font-size:.85rem;margin:10px 0 4px;color:#b9c9a8}
input,select,button{width:100%;box-sizing:border-box;padding:11px;border-radius:8px;border:1px solid #2c5a3a;background:#0d1f14;color:#e8e2c4;font-size:1rem}
button{background:#e6c34a;color:#1a1400;font-weight:700;border:0;margin-top:12px}
ul{list-style:none;padding:0;margin:0}
li{display:flex;align-items:center;gap:10px;padding:10px 0;border-bottom:1px solid #2c5a3a}
li:last-child{border:0}
.t{font-weight:700;color:#e6c34a;min-width:3.2em}.x{flex:1;overflow-wrap:anywhere}.r{display:block;font-size:.75rem;color:#9fb08c}
li form{margin:0}li button{width:auto;margin:0;padding:7px 11px;background:#7a3030;color:#fff}
.ok{background:#2c5a3a;border-radius:8px;padding:10px;margin-top:12px}
small,.muted{color:#9fb08c;font-size:.8rem}code{color:#e6c34a;overflow-wrap:anywhere}
</style></head><body><main>
<h1><svg width="30" height="26" viewBox="0 0 28 24" aria-hidden="true"><path fill="#e6c34a" d="M14 0 21 12H7zM7 12l7 12H0zM21 12l7 12H14z"/></svg>Reloj de Hyrule</h1>
)HTML";

static void handleRoot() {
  String h;
  h.reserve(6000);
  h += FPSTR(PAGE_HEAD);

  struct tm t;
  bool have = timeValid() && getLocalTime(&t, 0);
  if (have) {
    char buf[48];
    strftime(buf, sizeof(buf), "Hora del reloj: %H:%M", &t);
    h += "<div class=muted>";
    h += buf;
    h += "</div>";
  }
  if (server.hasArg("ok")) h += "<div class=ok>¡Listo! El hada ya lo sabe.</div>";
  if (server.hasArg("full")) h += "<div class=ok>No entran más recordatorios (máximo 12). Borrá alguno.</div>";

  h += F("<div class=card><h2>Mandar un mensaje ahora</h2>"
         "<form method=post action=/msg><input name=text maxlength=59 required placeholder='¡Hola desde el teléfono!'>"
         "<button>Enviar al hada</button></form></div>");

  h += F("<div class=card><h2>Música</h2>"
         "<form method=post action=/play><button>♪ Tocar el tema de Hyrule</button></form></div>");

  h += F("<div class=card><h2>Nuevo recordatorio</h2><form method=post action=/add>"
         "<label>Hora</label><input type=time name=time required>"
         "<label>Mensaje</label><input name=text maxlength=59 required placeholder='Tomar agua'>"
         "<label>Repetir</label><select name=rep><option value=0>Una vez</option>"
         "<option value=1>Todos los días</option><option value=2>Lunes a viernes</option></select>"
         "<button>Guardar</button></form></div>");

  h += F("<div class=card><h2>Recordatorios</h2>");
  int idx[MAX_REMINDERS];
  int n = have ? reminders.sorted(t, idx) : 0;
  if (!have)
    for (int i = 0; i < MAX_REMINDERS; i++)
      if (reminders.items[i].used) idx[n++] = i;
  if (n == 0) h += F("<div class=muted>Todavía no hay ninguno.</div>");
  h += "<ul>";
  for (int k = 0; k < n; k++) {
    const Reminder& r = reminders.items[idx[k]];
    char hm[8];
    snprintf(hm, sizeof(hm), "%02d:%02d", r.hour, r.minute);
    h += "<li><span class=t>";
    h += hm;
    h += "</span><span class=x>";
    h += esc(r.text);
    h += "<span class=r>";
    h += repeatName(r.repeat);
    h += "</span></span><form method=post action=/del><input type=hidden name=id value=";
    h += idx[k];
    h += "><button aria-label='Borrar'>✕</button></form></li>";
  }
  h += "</ul></div>";

  h += "<p><small>Atajo para el teléfono: abrí <code>http://";
  h += netIP();
  h += "/msg?text=Hola</code> y el mensaje aparece al instante. También podés entrar con <code>http://hyrule.local</code>.</small></p>";
  h += "</main></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

static void redirectHome(const char* q) {
  server.sendHeader("Location", String("/") + q);
  server.send(303);
}

static void handleAdd() {
  String tm = server.arg("time");
  String text = server.arg("text");
  text.trim();
  int h = tm.substring(0, 2).toInt(), m = tm.substring(3, 5).toInt();
  int rep = constrain(server.arg("rep").toInt(), 0, 2);
  if (tm.length() < 5 || h > 23 || m > 59 || text.isEmpty()) return redirectHome("");
  bool ok = reminders.add(h, m, rep, text);
  redirectHome(ok ? "?ok=1" : "?full=1");
}

static void handleDel() {
  reminders.remove(server.arg("id").toInt());
  redirectHome("");
}

static void handlePlay() {
  playHyruleTheme();
  if (server.method() == HTTP_POST) redirectHome("?ok=1");
  else server.send(200, "text/plain; charset=utf-8", "ok");
}

static void handleMsg() {
  String text = server.arg("text");
  text.trim();
  if (!text.isEmpty()) pushNotification(text);
  if (server.method() == HTTP_POST) redirectHome("?ok=1");
  else server.send(200, "text/plain; charset=utf-8", text.isEmpty() ? "falta ?text=" : "ok");
}

void netBegin() {
  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setConnectTimeout(12);
  wm.setTitle("Reloj de Hyrule");
  wm.autoConnect(WIFI_SETUP_AP);
}

void netLoop() {
  wm.process();
  if (!serverUp && netConnected()) {
    serverUp = true;
    WiFi.setSleep(false);  // sin ahorro de energía: las páginas largas llegaban cortadas
    configTzTime(TIMEZONE, "pool.ntp.org", "time.google.com");
    MDNS.begin("hyrule");
    server.on("/", HTTP_GET, handleRoot);
    server.on("/add", HTTP_POST, handleAdd);
    server.on("/del", HTTP_POST, handleDel);
    server.on("/msg", handleMsg);
    server.on("/play", handlePlay);
    server.begin();
    MDNS.addService("http", "tcp", 80);
  }
  if (serverUp) server.handleClient();
}
#endif
