export type Theme = "clothing" | "food" | "housing" | "transport";

export const THEME_TABS: Array<{ key: Theme; label: string }> = [
  { key: "clothing", label: "衣" },
  { key: "food", label: "食" },
  { key: "housing", label: "住" },
  { key: "transport", label: "行" },
];
