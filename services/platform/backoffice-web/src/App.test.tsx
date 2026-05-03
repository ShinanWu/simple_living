import { afterEach, describe, expect, it } from "vitest";
import { cleanup, render, screen } from "@testing-library/react";
import App from "./App";

afterEach(() => {
  cleanup();
});

describe("App shell", () => {
  it("renders backoffice console", () => {
    render(<App />);
    expect(screen.getByText("运营管理平台（最小版）")).toBeTruthy();
  });
});
