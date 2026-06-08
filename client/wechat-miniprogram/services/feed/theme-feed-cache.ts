import type { HomeCard } from '../gateway/types';
import { getGateway } from '../gateway/runtime';
import { THEMES, type ThemeKey } from '../../utils/theme';
import { GatewayBusinessError } from '../../utils/errors';

export type ThemeFeedLoadState = 'success' | 'empty' | 'error' | 'offline';

export type DisplayCard = HomeCard & { reasonDisplay: string };

export interface ThemeFeedSnapshot {
  loadState: ThemeFeedLoadState;
  cards: DisplayCard[];
  nextCursor: string | null;
  hasMore: boolean;
  errorMessage: string;
}

const cache: Partial<Record<ThemeKey, ThemeFeedSnapshot>> = {};
let coldPrefetchPromise: Promise<void> | null = null;

function mapCards(cards: HomeCard[]): DisplayCard[] {
  return cards.map((c) => ({
    ...c,
    reasonDisplay: c.reason === '-' ? '为你精选的轻量推荐' : c.reason,
  }));
}

function offlineError(err: unknown): boolean {
  return Boolean(err && typeof err === 'object' && (err as { kind?: string }).kind === 'offline');
}

/** 冷启动：并行刷新四主题首屏，结果写入内存缓存 */
export function startColdPrefetchAllThemes(): Promise<void> {
  if (coldPrefetchPromise) return coldPrefetchPromise;
  coldPrefetchPromise = Promise.all(
    THEMES.map((t) => refreshThemeFeed(t.key, { writeCache: true }))
  ).then(() => undefined);
  return coldPrefetchPromise;
}

export function getCachedThemeFeed(theme: ThemeKey): ThemeFeedSnapshot | undefined {
  return cache[theme];
}

export function hasCachedThemeFeed(theme: ThemeKey): boolean {
  return Boolean(cache[theme]);
}

/** 等待冷启动预拉取结束（若正在进行） */
export async function waitForColdPrefetch(): Promise<void> {
  if (coldPrefetchPromise) await coldPrefetchPromise;
}

/** 拉取单主题首屏（不传 limit，条数由后端 items 决定） */
export async function refreshThemeFeed(
  theme: ThemeKey,
  options: { writeCache?: boolean } = {}
): Promise<ThemeFeedSnapshot> {
  const writeCache = options.writeCache !== false;
  try {
    const res = await getGateway().getHomeFeed(theme, {});
    const cards = mapCards(res.cards);
    const snapshot: ThemeFeedSnapshot = cards.length
      ? {
          loadState: 'success',
          cards,
          nextCursor: res.nextCursor,
          hasMore: res.hasMore,
          errorMessage: '',
        }
      : {
          loadState: 'empty',
          cards: [],
          nextCursor: null,
          hasMore: false,
          errorMessage: '',
        };
    if (writeCache) cache[theme] = snapshot;
    return snapshot;
  } catch (err) {
    const snapshot: ThemeFeedSnapshot = offlineError(err)
      ? { loadState: 'offline', cards: [], nextCursor: null, hasMore: false, errorMessage: '' }
      : {
          loadState: 'error',
          cards: [],
          nextCursor: null,
          hasMore: false,
          errorMessage:
            err instanceof GatewayBusinessError ? err.message : '加载失败，请稍后重试',
        };
    if (writeCache) cache[theme] = snapshot;
    return snapshot;
  }
}

/** 游标追加一页（条数由后端返回决定） */
export async function appendThemeFeedPage(
  theme: ThemeKey,
  cursor: string,
  existing: DisplayCard[]
): Promise<Pick<ThemeFeedSnapshot, 'cards' | 'nextCursor' | 'hasMore'>> {
  const res = await getGateway().getHomeFeed(theme, { cursor });
  const appended = mapCards(res.cards);
  const cards = [...existing, ...appended];
  const patch = {
    cards,
    nextCursor: res.nextCursor,
    hasMore: res.hasMore,
  };
  const prev = cache[theme];
  if (prev) {
    cache[theme] = {
      ...prev,
      ...patch,
      loadState: cards.length ? 'success' : prev.loadState,
    };
  }
  return patch;
}

/** 仅测试重置 */
export function resetThemeFeedCacheForTests(): void {
  for (const key of Object.keys(cache) as ThemeKey[]) {
    delete cache[key];
  }
  coldPrefetchPromise = null;
}
