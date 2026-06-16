import { BRAND_NAME } from '../config/brand';

/** 同步系统导航栏标题为品牌名。 */
export function setBrandNavigationTitle(title: string = BRAND_NAME): void {
  wx.setNavigationBarTitle({ title });
}
