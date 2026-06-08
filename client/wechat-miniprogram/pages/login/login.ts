import type { IAppOption } from '../../app';
import { getGateway } from '../../services/gateway/runtime';
import { setWxOpenId, getWxOpenId } from '../../services/auth/storage';
import { GatewayBusinessError, loginErrorCopy } from '../../utils/errors';
import { normalizePhoneToE164 } from '../../utils/phone';
import { trackEvent } from '../../utils/analytics';
import { BRAND_LOGIN_TITLE } from '../../config/brand';

Page({
  data: {
    loginTitle: BRAND_LOGIN_TITLE,
    phone: '',
    otp: '',
    verificationId: '',
    showAdvanced: false,
    otpCooldown: 0,
    busy: false,
    loginMode: '' as '' | 'wechat' | 'phone',
    errorMessage: '',
  },

  returnUrl: '',
  _cooldownTimer: 0 as ReturnType<typeof setInterval> | 0,

  onLoad(query: Record<string, string | undefined>) {
    this.returnUrl = query.return_url ? decodeURIComponent(query.return_url) : '';
    if (!this.returnUrl && query.redirect === 'favorites') {
      this.returnUrl = '/pages/favorites/favorites';
    }
    trackEvent('login_panel_show');
  },

  onUnload() {
    if (this._cooldownTimer) clearInterval(this._cooldownTimer);
  },

  toggleAdvanced() {
    this.setData({ showAdvanced: !this.data.showAdvanced });
  },

  onPhoneInput(e: WechatMiniprogram.Input) {
    this.setData({ phone: e.detail.value });
  },

  onOtpInput(e: WechatMiniprogram.Input) {
    this.setData({ otp: e.detail.value });
  },

  onVerificationInput(e: WechatMiniprogram.Input) {
    this.setData({ verificationId: e.detail.value });
  },

  onSendOtp() {
    if (this.data.otpCooldown > 0) return;
    const e164 = normalizePhoneToE164(this.data.phone);
    if (!e164) {
      this.setData({ errorMessage: loginErrorCopy(10002).message });
      return;
    }
    wx.showModal({
      title: '验证码说明',
      content:
        '网关暂未提供独立下发验证码接口。请使用测试环境提供的 verification_id 与固定验证码，或在 Mock 模式下任意填写后登录。',
      showCancel: false,
      success: () => {
        this.startCooldown(60);
        if (!this.data.verificationId) {
          this.setData({
            verificationId: 'verify_01mockyq8y8y2r7a4h7s0x',
            showAdvanced: true,
          });
        }
      },
    });
  },

  startCooldown(seconds: number) {
    this.setData({ otpCooldown: seconds });
    if (this._cooldownTimer) clearInterval(this._cooldownTimer);
    this._cooldownTimer = setInterval(() => {
      const next = this.data.otpCooldown - 1;
      if (next <= 0) {
        clearInterval(this._cooldownTimer);
        this._cooldownTimer = 0;
        this.setData({ otpCooldown: 0 });
      } else {
        this.setData({ otpCooldown: next });
      }
    }, 1000);
  },

  async onWeChatLogin() {
    this.setData({ busy: true, loginMode: 'wechat', errorMessage: '' });
    trackEvent('login_wechat_click');
    try {
      const code = await this.wxLogin();
      const openId = getWxOpenId() || `wx_code_${code.slice(0, 8)}`;
      const pair = await getGateway().issueTokenWithWeChat(openId, code);
      if (pair.sessionId) setWxOpenId(openId);
      trackEvent('login_issue_token_success', { provider: 'wechat' });
      this.afterLoginSuccess();
    } catch (err) {
      this.handleLoginError(err);
    } finally {
      this.setData({ busy: false, loginMode: '' });
    }
  },

  wxLogin(): Promise<string> {
    return new Promise((resolve, reject) => {
      wx.login({
        success: (res) => {
          if (res.code) resolve(res.code);
          else reject(new GatewayBusinessError(20006, 'wx.login failed'));
        },
        fail: () => reject({ kind: 'offline' }),
      });
    });
  },

  async onPhoneLogin() {
    this.setData({ busy: true, loginMode: 'phone', errorMessage: '' });
    const e164 = normalizePhoneToE164(this.data.phone);
    if (!e164 || !this.data.otp || !this.data.verificationId) {
      this.setData({
        busy: false,
        loginMode: '',
        errorMessage: loginErrorCopy(10001).message,
      });
      return;
    }
    try {
      await getGateway().issueTokenWithPhone(e164, this.data.otp, this.data.verificationId);
      trackEvent('login_issue_token_success', { provider: 'phone_otp' });
      this.afterLoginSuccess();
    } catch (err) {
      this.handleLoginError(err);
    } finally {
      this.setData({ busy: false, loginMode: '' });
    }
  },

  handleLoginError(err: unknown) {
    if (err instanceof GatewayBusinessError) {
      this.setData({ errorMessage: loginErrorCopy(err.code).message });
      trackEvent('login_issue_token_fail', { code: err.code });
      return;
    }
    this.setData({ errorMessage: loginErrorCopy(90002).message });
  },

  afterLoginSuccess() {
    wx.showToast({ title: '登录成功', icon: 'success' });
    const app = getApp<IAppOption>();
    app.globalData.pendingLoginAction = null;

    if (this.returnUrl) {
      wx.redirectTo({
        url: this.returnUrl,
        fail: () => wx.reLaunch({ url: '/pages/home/home' }),
      });
      return;
    }
    wx.navigateBack({ fail: () => wx.reLaunch({ url: '/pages/home/home' }) });
  },
});
