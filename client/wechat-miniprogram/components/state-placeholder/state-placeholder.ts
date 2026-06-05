Component({
  properties: {
    title: { type: String, value: '' },
    actionTitle: { type: String, value: '重试' },
  },
  methods: {
    onAction() {
      this.triggerEvent('action');
    },
  },
});
