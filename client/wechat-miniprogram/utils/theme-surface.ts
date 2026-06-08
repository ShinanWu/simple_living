/** 液态玻璃页面背景（与 cross-platform-interaction-consensus §6.2 对齐） */
export function pageBackgroundStyle(accent: string): string {
  return `linear-gradient(180deg, #fafafa 0%, color-mix(in srgb, ${accent} 8%, #f4f4f5) 55%, #ececee 100%)`;
}
