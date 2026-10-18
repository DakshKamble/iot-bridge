import { finite, format, parseTelemetry, telemetryAge, sensorFresh, vectorValid, uptime } from './telemetry.mjs';

const $ = id => document.getElementById(id);
const defaults = window.DASHBOARD_CONFIG || {};
const staleAfterMs = finite(defaults.staleAfterMs) && defaults.staleAfterMs > 0 ? defaults.staleAfterMs : 15000;
const devices = new Map();
let client = null, connected = false, selected = '', messageCount = 0;
let activePrefix = '';

function text(id, value) { $(id).textContent = value; }
function pill(id, label, state = '') { text(id, label); $(id).dataset.state = state; }
function message(value, error = false) {
  text('connection-message', value);
  $('connection-message').dataset.error = String(error);
}
function activity(value) {
  const item = document.createElement('li');
  item.textContent = `${new Date().toLocaleTimeString()} · ${value}`;
  $('activity').prepend(item);
  while ($('activity').children.length > 8) $('activity').lastElementChild.remove();
}
function settings() {
  const host = $('host').value.trim();
  const port = Number($('port').value);
  const path = $('path').value.trim();
  const prefix = $('prefix').value.trim().replace(/^\/+|\/+$/g, '');
  if (!host || /[\s/:?#@]/.test(host)) throw new Error('Enter a broker hostname without a protocol or path.');
  if (!Number.isInteger(port) || port < 1 || port > 65535) throw new Error('Enter a valid WSS port.');
  if (!path.startsWith('/') || /[\s?#]/.test(path)) throw new Error('WebSocket path must start with / and contain no spaces, query or fragment.');
  if (!prefix || /[+#\u0000]/.test(prefix)) throw new Error('Topic prefix cannot contain MQTT wildcards.');
  return { url: `wss://${host}:${port}${path}`, prefix };
}
function preview() {
  try {
    const { url, prefix } = settings();
    text('endpoint', url);
    text('subscription', `${prefix}/+/telemetry and ${prefix}/+/status`);
  } catch { text('endpoint', 'Check connection settings'); }
}
function controls(busy) {
  for (const input of $('connection-form').querySelectorAll('input')) input.disabled = busy;
  $('connect').disabled = busy;
  $('disconnect').disabled = !busy;
}
function recordFor(id) {
  if (!devices.has(id)) {
    if (devices.size === 0) $('device').replaceChildren();
    devices.set(id, { status: 'unknown' });
    const option = document.createElement('option');
    option.value = id;
    option.textContent = id;
    $('device').append(option);
    if (!selected) { selected = id; $('device').value = id; }
    $('device').disabled = false;
  }
  return devices.get(id);
}
function resetDevices() {
  devices.clear(); selected = ''; messageCount = 0;
  $('device').replaceChildren(new Option('Waiting for a device', ''));
  $('device').disabled = true;
  render();
}
function render() {
  const record = devices.get(selected);
  const data = record?.data;
  const stale = !connected || record?.status === 'offline' || telemetryAge(record) > staleAfterMs;
  pill('device-state', !record ? 'No telemetry' : record.status === 'offline' ? 'Device offline' : !data ? 'Waiting for telemetry' : !connected ? 'Broker disconnected' : stale ? 'Stale telemetry' : 'Live telemetry', !record ? '' : stale ? 'warn' : data ? 'good' : 'warn');
  const ds = data?.ds18b20, strain = data?.strain, imu = data?.imu;
  const dsValid = ds?.valid === true && finite(ds.temperature_c);
  const sampled = strain?.sampled === true;
  const strainValid = sampled && finite(strain.output_mv);
  const imuValid = imu?.valid === true && vectorValid(imu.acceleration_m_s2) && vectorValid(imu.gyro_rad_s);
  const dsStale = stale || !sensorFresh(record, 'ds18b20', staleAfterMs);
  const strainStale = stale || !sensorFresh(record, 'strain', staleAfterMs);
  const imuStale = stale || !sensorFresh(record, 'imu', staleAfterMs);
  $('temperature-card').dataset.stale = String(dsStale);
  $('strain-card').dataset.stale = String(strainStale);
  $('motion-card').dataset.stale = String(imuStale);
  text('temperature', dsValid ? format(ds.temperature_c) : '—');
  text('temperature-state', !data ? 'Waiting for a probe reading' : !dsValid ? 'Probe unavailable / invalid reading' : `${dsStale ? 'Stale' : 'Valid'} probe reading · sample age ${format(telemetryAge(record) + ds.sample_age_ms, 0)} ms`);
  text('strain-voltage', strainValid ? format(strain.output_mv, 1) : '—');
  text('strain-adc', sampled && finite(strain.adc_raw) ? `${format(strain.adc_raw, 0)} / 4095` : '—');
  text('strain-value', !data ? '—' : strain?.calibrated !== true ? 'Uncalibrated' : sampled && finite(strain.microstrain) ? `${format(strain.microstrain)} µε` : '—');
  text('strain-state', !data ? 'Waiting for an analog sample' : !strainValid ? 'No analog sample available' : `${strainStale ? 'Stale sample · ' : ''}${strain.adc_near_rail === true ? 'ADC near rail · possible clipping / disconnected input' : 'Analog sampled · connection cannot be detected'} · sample age ${format(telemetryAge(record) + strain.sample_age_ms, 0)} ms`);
  $('strain-state').dataset.warning = String(strain?.adc_near_rail === true);
  text('imu-model', typeof imu?.model === 'string' ? imu.model : 'MPU6050 / MPU6500');
  text('imu-state', !data ? 'Waiting for data' : !imuValid ? 'Sensor unavailable' : imuStale ? 'Stale motion sample' : `Valid · sample age ${format(telemetryAge(record) + imu.sample_age_ms, 0)} ms`);
  ['x', 'y', 'z'].forEach((axis, index) => {
    text(`accel-${axis}`, imuValid ? format(imu.acceleration_m_s2[index], 3) : '—');
    text(`gyro-${axis}`, imuValid ? format(imu.gyro_rad_s[index], 3) : '—');
  });
  text('last-sample', !data ? '—' : finite(data.timestamp) ? new Date(data.timestamp * 1000).toLocaleString() : record.retained ? 'Unknown (retained)' : 'Device clock unavailable');
  text('rssi', finite(data?.wifi_rssi_dbm) ? `${data.wifi_rssi_dbm} dBm` : '—');
  text('sequence', finite(data?.sequence) ? data.sequence : '—');
  text('uptime', uptime(data?.uptime_ms));
  text('message-count', `${messageCount} messages`);
  text('raw-topic', record?.topic || 'No telemetry received yet');
  text('raw-json', data ? JSON.stringify(data, null, 2) : 'Connect to the broker to receive sensor data.');
}
function disconnect() {
  const previous = client;
  client = null; connected = false;
  if (previous) previous.end(true);
  controls(false);
  pill('broker-state', 'Broker disconnected');
  render();
}
function connect(event) {
  event?.preventDefault();
  let config;
  try { config = settings(); } catch (error) { message(error.message, true); return; }
  if (typeof window.mqtt?.connect !== 'function') { message('MQTT library could not load. Check vendor/mqtt.min.js and reload.', true); return; }
  disconnect(); resetDevices();
  activePrefix = config.prefix;
  controls(true);
  preview();
  pill('broker-state', 'Connecting', 'warn');
  message('Connecting through secure WebSockets…');
  activity('Connecting to broker');
  let session;
  try {
    session = window.mqtt.connect(config.url, {
      username: $('username').value, password: $('password').value,
      clientId: `dashboard-${crypto.randomUUID()}`,
      clean: true, keepalive: 30, connectTimeout: 10000, reconnectPeriod: 3000,
      resubscribe: false,
    });
    client = session;
  } catch (error) { disconnect(); message(error.message, true); return; }
  session.on('connect', () => {
    if (client !== session) return;
    pill('broker-state', 'Subscribing', 'warn');
    session.subscribe([`${activePrefix}/+/telemetry`, `${activePrefix}/+/status`], { qos: 1 }, (error, grants) => {
      if (client !== session) return;
      if (error || !grants || grants.length !== 2 || grants.some(grant => grant.qos > 2)) {
        connected = false;
        pill('broker-state', 'Subscription failed', 'bad');
        message('Broker rejected the subscriptions. Check topic permissions.', true);
        activity('Subscription failed'); render(); return;
      }
      connected = true;
      pill('broker-state', 'Broker connected', 'good');
      message('Subscribed. Waiting for the bridge to publish telemetry.');
      activity('Connected · telemetry and status subscribed'); render();
    });
  });
  session.on('message', (topic, payload, packet) => {
    if (client !== session) return;
    const relative = topic.startsWith(`${activePrefix}/`) ? topic.slice(activePrefix.length + 1).split('/') : [];
    if (relative.length !== 2 || !relative[0]) return;
    const [id, kind] = relative;
    const value = payload.toString();
    if (kind === 'status' && ['online', 'offline'].includes(value)) {
      recordFor(id).status = value;
    } else if (kind === 'telemetry') {
      try {
        const data = parseTelemetry(value, id);
        const record = recordFor(id);
        Object.assign(record, { data, topic, receivedAt: Date.now(), retained: packet?.retain === true });
        messageCount++;
        message('Receiving sensor telemetry.');
      } catch (error) { activity(`Ignored telemetry: ${error.message}`); }
    }
    render();
  });
  session.on('reconnect', () => {
    if (client !== session) return;
    connected = false; pill('broker-state', 'Reconnecting', 'warn');
    message('Retrying the broker connection…'); render();
  });
  session.on('close', () => {
    if (client !== session) return;
    connected = false; pill('broker-state', 'Connection lost', 'warn');
    message('Connection lost. Retrying automatically; check network, WSS port and credentials.', true);
    activity('Connection closed'); render();
  });
  session.on('error', error => {
    if (client !== session) return;
    message(`MQTT error: ${error.message}`, true);
    activity(`MQTT error: ${error.message}`);
  });
}

for (const [id, key] of [['host', 'host'], ['port', 'port'], ['path', 'path'], ['username', 'username'], ['password', 'password'], ['prefix', 'topicPrefix']]) {
  $(id).value = defaults[key] ?? '';
}
$('connection-form').addEventListener('submit', connect);
$('connection-form').addEventListener('input', preview);
$('disconnect').addEventListener('click', () => { disconnect(); message('Disconnected.'); activity('Disconnected by user'); });
$('device').addEventListener('change', () => { selected = $('device').value; render(); });
window.addEventListener('pagehide', disconnect);
setInterval(render, 1000);
preview(); render();
connect();
