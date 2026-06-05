export type ViewLoadState<T> =
  | { kind: 'loading' }
  | { kind: 'success'; data: T }
  | { kind: 'empty' }
  | { kind: 'error'; message: string }
  | { kind: 'offline' };

export interface LoginErrorCopy {
  title?: string;
  message: string;
  primaryCta: string;
  secondaryCta?: string;
}

export function loginErrorCopy(code: number): LoginErrorCopy {
  switch (code) {
    case 20006:
      return {
        title: '登录未成功',
        message: '验证码错误或已过期，请重新获取或重试微信授权。',
        primaryCta: '重试',
      };
    case 20005:
      return {
        title: '暂无法登录',
        message: '当前设备或环境受限，请稍后再试或更换网络。',
        primaryCta: '知道了',
      };
    case 10002:
      return {
        title: '信息有误',
        message: '请检查手机号格式或验证码是否完整。',
        primaryCta: '修改',
      };
    case 10001:
      return {
        title: '缺少信息',
        message: '请填写手机号或验证码后再试。',
        primaryCta: '去填写',
      };
    case 10005:
      return {
        title: '操作过于频繁',
        message: '请稍后再试验证码或登录。',
        primaryCta: '稍后重试',
      };
    case 20003:
      return {
        title: '请重新登录',
        message: '登录已过期，请重新登录。',
        primaryCta: '去登录',
        secondaryCta: '继续浏览',
      };
    case 90002:
      return {
        title: '网络繁忙',
        message: '服务响应超时，请检查网络后重试。',
        primaryCta: '重试',
        secondaryCta: '关闭',
      };
    case 90003:
    case 90001:
      return {
        title: '服务异常',
        message: '暂时无法连接服务，请稍后重试。',
        primaryCta: '重试',
        secondaryCta: '关闭',
      };
    default:
      return {
        message: '出了点问题，请稍后重试。',
        primaryCta: '重试',
      };
  }
}

export function isOfflineError(err: unknown): boolean {
  if (!err || typeof err !== 'object') return false;
  const e = err as { kind?: string; errMsg?: string };
  if (e.kind === 'offline') return true;
  if (typeof e.errMsg === 'string' && e.errMsg.includes('fail')) return true;
  return false;
}

export class GatewayBusinessError extends Error {
  readonly code: number;

  constructor(code: number, message: string) {
    super(message);
    this.code = code;
    this.name = 'GatewayBusinessError';
  }
}
