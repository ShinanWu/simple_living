import { readGatewayBaseUrl } from '../../config/env';
import { HttpGatewayAPI } from './http';
import type { GatewayAPI } from './types';

let singleton: GatewayAPI | null = null;

export function getGateway(): GatewayAPI {
  if (singleton) return singleton;
  singleton = new HttpGatewayAPI(readGatewayBaseUrl());
  return singleton;
}

export function resetGatewayForTests(): void {
  singleton = null;
}
