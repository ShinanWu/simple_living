import { render, screen } from "@testing-library/react";
import { describe, expect, it } from "vitest";
import App from "./App";

describe("App shell", () => {
  it("renders bottom nav labels", () => {
    render(<App />);
    expect(screen.getByText("首页")).toBeTruthy();
    expect(screen.getByText("我的")).toBeTruthy();
  });
});
