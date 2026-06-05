import { GatewayBusinessError } from '../../utils/errors';
import type { ApiEnvelope } from './types';

export function parseEnvelope<T>(raw: unknown): ApiEnvelope<T> {
  if (!raw || typeof raw !== 'object') {
    throw new GatewayBusinessError(90001, 'invalid response');
  }
  const obj = raw as Record<string, unknown>;
  const success = Boolean(obj.success);
  const code = typeof obj.code === 'number' ? obj.code : 90001;
  const message = typeof obj.message === 'string' ? obj.message : 'unknown';
  const data = (obj.data ?? null) as T | null;
  return { success, code, message, data };
}

export function assertEnvelopeSuccess<T>(envelope: ApiEnvelope<T>): T {
  if (envelope.success && envelope.code === 0 && envelope.data !== null) {
    return envelope.data;
  }
  throw new GatewayBusinessError(envelope.code, envelope.message);
}
