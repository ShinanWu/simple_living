/**
 * 微信小程序核心逻辑自测（Node + tsx，不依赖微信开发者工具）
 */
import assert from 'node:assert/strict';
import { normalizePhoneToE164 } from '../utils/phone';
import { loginErrorCopy, GatewayBusinessError } from '../utils/errors';
import { THEMES, nextTheme, themeByKey } from '../utils/theme';
import { parseEnvelope, assertEnvelopeSuccess } from '../services/gateway/parse-envelope';
import { MockGatewayAPI } from '../services/gateway/mock';

let passed = 0;

function test(name: string, fn: () => void | Promise<void>): Promise<void> {
  return Promise.resolve(fn()).then(
    () => {
      passed += 1;
      console.log(`✓ ${name}`);
    },
    (err) => {
      console.error(`✗ ${name}`);
      throw err;
    }
  );
}

async function main(): Promise<void> {
  await test('normalizePhoneToE164 +86', () => {
    assert.equal(normalizePhoneToE164('13800138000'), '+8613800138000');
  });

  await test('normalizePhoneToE164 rejects invalid', () => {
    assert.equal(normalizePhoneToE164('abc'), null);
  });

  await test('loginErrorCopy 20006', () => {
    const copy = loginErrorCopy(20006);
    assert.match(copy.message, /微信/);
  });

  await test('theme order', () => {
    assert.deepEqual(
      THEMES.map((t) => t.key),
      ['clothing', 'food', 'housing', 'transport']
    );
    assert.equal(nextTheme('clothing', 1), 'food');
    assert.equal(nextTheme('transport', -1), 'housing');
    assert.equal(themeByKey('food').title, '食');
  });

  await test('parseEnvelope success', () => {
    const env = parseEnvelope<{ session_id: string }>({
      success: true,
      code: 0,
      message: 'ok',
      data: { session_id: 'sess_x' },
    });
    const data = assertEnvelopeSuccess(env);
    assert.equal(data.session_id, 'sess_x');
  });

  await test('parseEnvelope failure throws', () => {
    const env = parseEnvelope<null>({
      success: false,
      code: 20006,
      message: 'bad',
      data: null,
    });
    assert.throws(() => assertEnvelopeSuccess(env), GatewayBusinessError);
  });

  await test('MockGateway home feed per theme', async () => {
    const api = new MockGatewayAPI();
    const res = await api.getHomeFeed('transport', { limit: 20 });
    assert.equal(res.cards.length, 2);
    assert.match(res.cards[0].guideCardId, /transport/);
  });

  await test('MockGateway wechat login flow', async () => {
    const api = new MockGatewayAPI();
    const pair = await api.issueTokenWithWeChat('openid_test', 'code_abc');
    assert.ok(pair.accessToken);
    const me = await api.getMeSummary();
    assert.equal(me.isLoggedIn, true);
  });

  await test('MockGateway favorite requires login before', async () => {
    const api = new MockGatewayAPI();
    await assert.rejects(() => api.addFavorite('guide_x'), GatewayBusinessError);
  });

  await test('MockGateway favorite after login', async () => {
    const api = new MockGatewayAPI();
    await api.issueTokenWithWeChat('u1', 'code1');
    const fav = await api.addFavorite('guide_food_001');
    assert.ok(fav.favoriteId);
    const list = await api.listFavorites({ limit: 10 });
    assert.equal(list.items.length, 1);
  });

  await test('MockGateway redirect_prepare', async () => {
    const api = new MockGatewayAPI();
    const res = await api.postRedirectPrepare({
      guideCardId: 'g1',
      recommendationId: 'r1',
      scene: 'home_feed',
      itemRank: 1,
    });
    assert.match(res.landingUrl, /^https:\/\//);
  });

  console.log(`\n${passed} tests passed`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
