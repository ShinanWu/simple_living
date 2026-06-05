Page({
  data: {
    safeUrl: '',
  },

  onLoad(query: Record<string, string | undefined>) {
    const raw = query.url ? decodeURIComponent(query.url) : '';
    if (raw.startsWith('https://')) {
      this.setData({ safeUrl: raw });
    } else {
      wx.showToast({ title: '仅支持 HTTPS 链接', icon: 'none' });
    }
  },
});
