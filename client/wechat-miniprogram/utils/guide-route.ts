import type { FeedItemContext } from '../services/gateway/types';
import type { ThemeKey } from './theme';

export interface GuideDetailRouteParams {
  guideCardId: string;
  recommendationId?: string;
  scene?: string;
  itemRank?: number;
  title?: string;
  reason?: string;
  theme?: ThemeKey;
  action?: string;
}

export function buildGuideDetailQuery(params: GuideDetailRouteParams): string {
  const q = [
    `guide_card_id=${encodeURIComponent(params.guideCardId)}`,
    `recommendation_id=${encodeURIComponent(params.recommendationId ?? '')}`,
    `scene=${encodeURIComponent(params.scene ?? 'home_feed')}`,
    `item_rank=${params.itemRank ?? 1}`,
    `title=${encodeURIComponent(params.title ?? '')}`,
    `reason=${encodeURIComponent(params.reason ?? '-')}`,
    `theme=${encodeURIComponent(params.theme ?? 'clothing')}`,
  ];
  if (params.action) {
    q.push(`action=${encodeURIComponent(params.action)}`);
  }
  return q.join('&');
}

export function guideDetailPath(params: GuideDetailRouteParams): string {
  return `/pages/guide-detail/guide-detail?${buildGuideDetailQuery(params)}`;
}

export function redirectPreparePath(context: FeedItemContext, title?: string): string {
  const q = [
    `guide_card_id=${encodeURIComponent(context.guideCardId)}`,
    `recommendation_id=${encodeURIComponent(context.recommendationId)}`,
    `scene=${encodeURIComponent(context.scene)}`,
    `item_rank=${context.itemRank}`,
    `title=${encodeURIComponent(title ?? '')}`,
  ].join('&');
  return `/pages/redirect-prepare/redirect-prepare?${q}`;
}

export function feedContextFromCard(card: {
  guideCardId: string;
  recommendationId: string;
  scene: string;
  itemRank: number;
}): FeedItemContext {
  return {
    guideCardId: card.guideCardId,
    recommendationId: card.recommendationId,
    scene: card.scene,
    itemRank: card.itemRank,
  };
}
