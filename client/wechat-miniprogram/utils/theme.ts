export type ThemeKey = 'clothing' | 'food' | 'housing' | 'transport';

export interface ThemeMeta {
  key: ThemeKey;
  title: string;
  slogan: string;
  accent: string;
  icon: string;
}

export const THEMES: ThemeMeta[] = [
  {
    key: 'clothing',
    title: '衣',
    slogan: '简单搭配，你也很美',
    accent: '#e91e8c',
    icon: '👔',
  },
  {
    key: 'food',
    title: '食',
    slogan: '好好吃饭，就是治愈',
    accent: '#f97316',
    icon: '🍜',
  },
  {
    key: 'housing',
    title: '住',
    slogan: '住得舒服，心才安定',
    accent: '#14b8a6',
    icon: '🏠',
  },
  {
    key: 'transport',
    title: '行',
    slogan: '轻松出发，步步从容',
    accent: '#3b82f6',
    icon: '🚗',
  },
];

export function themeByKey(key: ThemeKey): ThemeMeta {
  const found = THEMES.find((t) => t.key === key);
  return found ?? THEMES[0];
}

export function nextTheme(key: ThemeKey, direction: 1 | -1): ThemeKey {
  const idx = THEMES.findIndex((t) => t.key === key);
  const next = Math.min(Math.max(idx + direction, 0), THEMES.length - 1);
  return THEMES[next].key;
}

export function galleryForTheme(key: ThemeKey): { caption: string; emoji: string }[] {
  switch (key) {
    case 'clothing':
      return [
        { caption: '日常通勤搭配', emoji: '👔' },
        { caption: '舒适鞋履选择', emoji: '👟' },
        { caption: '轻便随身单品', emoji: '👜' },
      ];
    case 'food':
      return [
        { caption: '轻负担三餐', emoji: '🥗' },
        { caption: '一周菜单灵感', emoji: '🍱' },
        { caption: '下午茶时刻', emoji: '☕' },
      ];
    case 'housing':
      return [
        { caption: '小空间整理', emoji: '🏠' },
        { caption: '氛围照明建议', emoji: '💡' },
        { caption: '舒眠卧室布置', emoji: '🛏️' },
      ];
    case 'transport':
      return [
        { caption: '通勤路线优化', emoji: '🚗' },
        { caption: '公共交通衔接', emoji: '🚇' },
        { caption: '短途低碳出行', emoji: '🚲' },
      ];
    default:
      return [{ caption: '精选推荐', emoji: '✨' }];
  }
}
