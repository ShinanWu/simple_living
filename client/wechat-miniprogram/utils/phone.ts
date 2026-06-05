/** 将中国大陆 11 位手机号或已有 E.164 规范化为 +86… */
export function normalizePhoneToE164(raw: string): string | null {
  const trimmed = raw.trim().replace(/\s+/g, '');
  if (!trimmed) return null;
  if (/^\+[1-9]\d{7,14}$/.test(trimmed)) return trimmed;
  if (/^1\d{10}$/.test(trimmed)) return `+86${trimmed}`;
  return null;
}
