// Firmware schema v1. Keep numbers in their published SI units.
export const finite = value => typeof value === 'number' && Number.isFinite(value);
export const format = (value, digits = 2) => finite(value) ? value.toFixed(digits) : '—';

export function parseTelemetry(text, deviceId) {
  const data = JSON.parse(text);
  if (!data || data.schema_version !== 1 || data.device_id !== deviceId ||
      !data.imu || !data.ds18b20 || !data.strain) {
    throw new Error('Unsupported telemetry schema or device/topic mismatch');
  }
  return data;
}

export function telemetryAge(record, now = Date.now()) {
  if (!record?.data) return Infinity;
  const elapsed = Math.max(0, now - record.receivedAt);
  if (finite(record.data.timestamp) && record.data.timestamp > 0) {
    return Math.max(elapsed, now - record.data.timestamp * 1000, 0);
  }
  // A retained payload without a device timestamp has unknown age.
  return record.retained ? Infinity : elapsed;
}

export function sensorFresh(record, sensor, staleAfterMs, now = Date.now()) {
  const age = record?.data?.[sensor]?.sample_age_ms;
  return finite(age) && age >= 0 && telemetryAge(record, now) + age <= staleAfterMs;
}

export function vectorValid(value) {
  return Array.isArray(value) && value.length === 3 && value.every(finite);
}

export function uptime(value) {
  if (!finite(value) || value < 0) return '—';
  const seconds = Math.floor(value / 1000);
  return `${Math.floor(seconds / 86400)}d ${Math.floor(seconds / 3600) % 24}h ${Math.floor(seconds / 60) % 60}m ${seconds % 60}s`;
}
