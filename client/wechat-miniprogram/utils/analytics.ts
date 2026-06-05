export function trackEvent(name: string, payload?: Record<string, string | number | boolean>): void {
  const line = payload ? `${name} ${JSON.stringify(payload)}` : name;
  console.info('[analytics]', line);
}
