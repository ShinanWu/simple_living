import { BRAND_NAME } from '../config/brand';

/** 强制同步系统导航栏标题（避免 app.json 缓存未刷新时仍显示旧名） */
export function setBrandNavigationTitle(title: string = BRAND_NAME): void {
  wx.setNavigationBarTitle({ title });
}
