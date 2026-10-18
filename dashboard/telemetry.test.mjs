import test from 'node:test';
import assert from 'node:assert/strict';
import { EventEmitter } from 'node:events';
import { parseTelemetry, telemetryAge, sensorFresh, format } from './telemetry.mjs';

const sample = (id = 'xiao-test') => ({
  schema_version: 1, device_id: id, timestamp: Math.floor(Date.now() / 1000),
  sequence: 0, uptime_ms: 42000, wifi_rssi_dbm: -61,
  imu: { valid: true, model: 'MPU6500', acceleration_m_s2: [0, -1.2, 9.80665], gyro_rad_s: [0.01, 0, -0.03], sample_age_ms: 50 },
  ds18b20: { valid: true, temperature_c: 26.125, sample_age_ms: 800 },
  strain: { sampled: true, calibrated: false, microstrain: null, adc_raw: 2048, output_mv: 1532.5, adc_near_rail: false, sample_age_ms: 50 },
});

test('firmware schema, units, zero and unknown values', () => {
  const data = sample();
  assert.deepEqual(parseTelemetry(JSON.stringify(data), data.device_id), data);
  assert.throws(() => parseTelemetry(JSON.stringify(data), 'different-device'));
  assert.throws(() => parseTelemetry('{', data.device_id));
  assert.throws(() => parseTelemetry('{"schema_version":2}', data.device_id));
  assert.equal(format(0, 3), '0.000');
  assert.equal(format(null), '—');
  assert.equal(format('12'), '—');
});

test('retained timestamps and individual sensor sample ages determine freshness', () => {
  const now = 1000000;
  const record = { data: { timestamp: 999, ds18b20: { sample_age_ms: 500 } }, receivedAt: now, retained: true };
  assert.equal(telemetryAge(record, now), 1000);
  assert.equal(sensorFresh(record, 'ds18b20', 15000, now), true);
  assert.equal(sensorFresh(record, 'ds18b20', 15000, now + 15000), false);
  record.data.timestamp = 900;
  assert.equal(sensorFresh(record, 'ds18b20', 15000, now), false);
  record.data.timestamp = null;
  assert.equal(telemetryAge(record, now), Infinity);
  record.retained = false;
  assert.equal(telemetryAge(record, now + 100), 100);
});

test('dashboard MQTT lifecycle and firmware display end to end with test transport', async () => {
  class Element {
    value = ''; textContent = ''; dataset = {}; disabled = false; children = []; handlers = {};
    append(child) { this.children.push(child); }
    prepend(child) { this.children.unshift(child); }
    replaceChildren(...children) { this.children = children; }
    querySelectorAll() { return []; }
    addEventListener(event, handler) { this.handlers[event] = handler; }
    get lastElementChild() { return { remove: () => this.children.pop() }; }
  }
  const elements = new Map();
  const element = id => { if (!elements.has(id)) elements.set(id, new Element()); return elements.get(id); };
  const sessions = [];
  let timer;
  globalThis.document = { getElementById: element, createElement: () => new Element() };
  globalThis.Option = class extends Element { constructor(label, value) { super(); this.textContent = label; this.value = value; } };
  globalThis.setInterval = fn => { timer = fn; };
  globalThis.window = {
    DASHBOARD_CONFIG: { host: 'example.com', port: 8084, path: '/mqtt', topicPrefix: 'iot-bridge', username: 'test', password: 'test' },
    addEventListener() {},
    mqtt: { connect(url, options) {
      assert.equal(url, 'wss://example.com:8084/mqtt');
      assert.equal(options.resubscribe, false);
      const session = new EventEmitter();
      session.subscribe = (topics, opts, cb) => {
        assert.deepEqual(topics, ['iot-bridge/+/telemetry', 'iot-bridge/+/status']);
        cb(null, topics.map(topic => ({ topic, qos: 1 })));
      };
      session.end = () => { session.ended = true; };
      sessions.push(session); return session;
    } },
  };
  await import('./app.js');
  const session = sessions[0];
  session.emit('connect');
  assert.equal(element('broker-state').textContent, 'Broker connected');
  const receive = (data, retained = false) => session.emit('message', `iot-bridge/${data.device_id}/telemetry`, Buffer.from(JSON.stringify(data)), { retain: retained });
  const data = sample();
  receive(data);
  assert.equal(element('temperature').textContent, '26.13');
  assert.equal(element('strain-voltage').textContent, '1532.5');
  assert.equal(element('strain-value').textContent, 'Uncalibrated');
  assert.equal(element('accel-z').textContent, '9.807');
  assert.equal(element('gyro-y').textContent, '0.000');
  assert.equal(element('sequence').textContent, 0);
  data.ds18b20 = { valid: false, temperature_c: null };
  data.imu.valid = false;
  data.strain = { ...data.strain, calibrated: true, microstrain: -42.5, adc_near_rail: true };
  receive(data);
  assert.equal(element('temperature').textContent, '—');
  assert.equal(element('accel-z').textContent, '—');
  assert.equal(element('strain-value').textContent, '-42.50 µε');
  assert.equal(element('strain-state').dataset.warning, 'true');
  data.timestamp -= 60;
  receive(data, true);
  assert.equal(element('device-state').textContent, 'Stale telemetry');
  const other = sample('xiao-other'); receive(other);
  element('device').value = other.device_id; element('device').handlers.change();
  assert.equal(element('temperature').textContent, '26.13');
  session.emit('message', 'iot-bridge/xiao-other/status', Buffer.from('offline'), {});
  assert.equal(element('device-state').textContent, 'Device offline');
  session.emit('close');
  assert.equal(element('broker-state').textContent, 'Connection lost');
  timer();
  element('disconnect').handlers.click();
  assert.equal(session.ended, true);
  assert.equal(element('connect').disabled, false);
  session.emit('connect'); // Late events must not revive a disconnected session.
  assert.equal(element('broker-state').textContent, 'Broker disconnected');
});
