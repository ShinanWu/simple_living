/** 列表页展示用：ISO 8601 → 本地简短时间 */
export function formatDisplayTime(iso: string): string {
  if (!iso) return '';
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) return iso;
  const now = new Date();
  const sameDay =
    date.getFullYear() === now.getFullYear() &&
    date.getMonth() === now.getMonth() &&
    date.getDate() === now.getDate();
  const pad = (n: number) => String(n).padStart(2, '0');
  if (sameDay) {
    return `今天 ${pad(date.getHours())}:${pad(date.getMinutes())}`;
  }
  return `${date.getMonth() + 1}月${date.getDate()}日 ${pad(date.getHours())}:${pad(date.getMinutes())}`;
}
